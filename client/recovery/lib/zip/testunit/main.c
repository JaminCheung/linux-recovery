#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
# include <unistd.h>
# include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <utils/log.h>
#include <lib/zip/minizip/zip.h>
#include <lib/zip/minizip/unzip.h>

#define LOG_TAG "test_minizip"

#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (8192)
#define MAXFILENAME (256)

void do_help() {
    printf(
            "Usage : test_minizip [-x] [-p password] file.zip [file_to_extr.] [-d extractdir]\n\n"
                    "  -e  Extract without pathname (junk paths)\n" \
                    "  -x  Extract with pathname\n"
                    "  -d  directory to extract into\n"
                    "  -p  extract crypted file using password\n\n");
}

int mymkdir(const char* dirname) {
    int ret = 0;

    ret = mkdir(dirname, 0775);

    return ret;
}

int makedir(const char *newdir) {
    char *buffer;
    char *p;
    int len = (int) strlen(newdir);

    if (len <= 0)
        return 0;

    buffer = (char*) malloc(len + 1);
    if (buffer == NULL) {
        printf("Error allocating memory\n");
        return UNZ_INTERNALERROR;
    }
    strcpy(buffer, newdir);

    if (buffer[len - 1] == '/') {
        buffer[len - 1] = '\0';
    }
    if (mymkdir(buffer) == 0) {
        free(buffer);
        return 1;
    }

    p = buffer + 1;
    while (1) {
        char hold;

        while (*p && *p != '\\' && *p != '/')
            p++;
        hold = *p;
        *p = 0;
        if ((mymkdir(buffer) == -1) && (errno == ENOENT)) {
            LOGE("couldn't create directory %s", buffer);
            free(buffer);
            return 0;
        }
        if (hold == 0)
            break;
        *p++ = hold;
    }
    free(buffer);
    return 1;
}

int do_extract_currentfile(unzFile uf, const int* popt_extract_without_path,
        const char* password) {
    char filename_inzip[256];
    char* filename_withoutpath;
    char* p;
    int err = UNZ_OK;
    FILE *fout = NULL;
    void* buf;
    uInt size_buf;

    unz_file_info64 file_info;
    err = unzGetCurrentFileInfo64(uf, &file_info, filename_inzip,
            sizeof(filename_inzip), NULL, 0, NULL, 0);

    if (err != UNZ_OK) {
        LOGE("error %d with zipfile in unzGetCurrentFileInfo", err);
        return err;
    }

    size_buf = WRITEBUFFERSIZE;
    buf = (void*) malloc(size_buf);
    if (buf == NULL) {
        LOGE("Error allocating memory");
        return UNZ_INTERNALERROR;
    }

    p = filename_withoutpath = filename_inzip;
    while ((*p) != '\0') {
        if (((*p) == '/') || ((*p) == '\\'))
            filename_withoutpath = p + 1;
        p++;
    }

    if ((*filename_withoutpath) == '\0') {
        if ((*popt_extract_without_path) == 0) {
            LOGI("creating directory: %s", filename_inzip);
            mymkdir(filename_inzip);
        }

    } else {
        const char* write_filename;

        if ((*popt_extract_without_path) == 0)
            write_filename = filename_inzip;
        else
            write_filename = filename_withoutpath;

        err = unzOpenCurrentFilePassword(uf, password);
        if (err != UNZ_OK)
            LOGE("error %d with zipfile in unzOpenCurrentFilePassword", err);

        if (err == UNZ_OK) {
            fout = fopen(write_filename, "wb");
            /* some zipfile don't contain directory alone before file */
            if ((fout == NULL) && ((*popt_extract_without_path) == 0)
                    && (filename_withoutpath != (char*) filename_inzip)) {
                char c = *(filename_withoutpath - 1);
                *(filename_withoutpath - 1) = '\0';
                makedir(write_filename);
                *(filename_withoutpath - 1) = c;
                fout = fopen(write_filename, "wb");
            }

            if (fout == NULL) {
                LOGE("error opening %s", write_filename);
            }
        }

        if (fout != NULL) {
            LOGI(" extracting: %s", write_filename);

            do {
                err = unzReadCurrentFile(uf, buf, size_buf);
                if (err < 0) {
                    LOGE("error %d with zipfile in unzReadCurrentFile\n", err);
                    break;
                }
                if (err > 0)
                    if (fwrite(buf, err, 1, fout) != 1) {
                        LOGE("error in writing extracted file\n");
                        err = UNZ_ERRNO;
                        break;
                    }
            } while (err > 0);

            if (fout)
                fclose(fout);
        }

        if (err == UNZ_OK) {
            err = unzCloseCurrentFile(uf);
            if (err != UNZ_OK) {
                LOGE("error %d with zipfile in unzCloseCurrentFile\n", err);
            }
        } else
            unzCloseCurrentFile(uf); /* don't lose the error */
    }

    free(buf);
    return err;
}

int do_extract(unzFile uf, int opt_extract_without_path, const char* password) {
    uLong i;
    unz_global_info64 gi;
    int err;

    err = unzGetGlobalInfo64(uf, &gi);
    if (err != UNZ_OK)
        LOGE("error %d with zipfile in unzGetGlobalInfo", err);

    for (i = 0; i < gi.number_entry; i++) {
        if (do_extract_currentfile(uf, &opt_extract_without_path,
                password) != UNZ_OK)
            break;

        if ((i + 1) < gi.number_entry) {
            err = unzGoToNextFile(uf);
            if (err != UNZ_OK) {
                LOGE("error %d with zipfile in unzGoToNextFile", err);
                break;
            }
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    const char *zipfilename = NULL;
    const char *password = NULL;
    char filename_try[MAXFILENAME + 16] = "";
    int i;
    int ret_value = 0;
    int opt_do_extract = 1;
    int opt_extractdir = 0;
    int opt_do_extract_withoutpath = 0;
    const char *dirname = NULL;
    unzFile uf = NULL;

    if (argc == 1) {
        do_help();
        return 0;
    } else {
        for (i = 1; i < argc; i++) {
            if ((*argv[i]) == '-') {
                const char *p = argv[i] + 1;

                while ((*p) != '\0') {
                    char c = *(p++);
                    ;
                    if ((c == 'x') || (c == 'X'))
                        opt_do_extract = 1;
                    if ((c == 'e') || (c == 'E'))
                        opt_do_extract = opt_do_extract_withoutpath = 1;
                    if ((c == 'd') || (c == 'D')) {
                        opt_extractdir = 1;
                        dirname = argv[i + 1];
                    }

                    if (((c == 'p') || (c == 'P')) && (i + 1 < argc)) {
                        password = argv[i + 1];
                        i++;
                    }
                }
            } else {
                if (zipfilename == NULL)
                    zipfilename = argv[i];
            }
        }
    }

    if (zipfilename != NULL) {
        strncpy(filename_try, zipfilename, MAXFILENAME - 1);
        /* strncpy doesnt append the trailing NULL, of the string is too long. */
        filename_try[ MAXFILENAME] = '\0';

        uf = unzOpen64(zipfilename);
        if (uf == NULL) {
            strcat(filename_try, ".zip");
            uf = unzOpen64(filename_try);
        }
    }

    if (uf == NULL) {
        LOGE("Cannot open %s or %s.zip", zipfilename, zipfilename);
        return 1;
    }
    LOGE("%s opened", filename_try);

    if (opt_do_extract == 1) {
        if (opt_extractdir && chdir(dirname)) {
            LOGE("Error changing into %s, aborting", dirname);
            exit(-1);
        }

        ret_value = do_extract(uf, opt_do_extract_withoutpath, password);
    }

    unzClose(uf);

    return ret_value;
}
