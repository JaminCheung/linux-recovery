#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#define TAG_BASE "Test_momery--->"
static pthread_mutex_t log_init_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOGI(...)                                                              \
    do {                                                                       \
      int save_errno = errno;                                                  \
      pthread_mutex_lock(&log_init_lock);                                      \
      fprintf(stderr, "I/%s%s %d: ", TAG_BASE, LOG_TAG, __LINE__);             \
      errno = save_errno;                                                      \
      fprintf(stderr, __VA_ARGS__);                                            \
      fflush(stderr);                                                          \
      pthread_mutex_unlock(&log_init_lock);                                    \
      errno = save_errno;                                                      \
    } while (0)

#define LOG_TAG         "testcase-blockmanager"
#define MOUNT_POINT   "/mnt/"
#define PACKAGE_PREFIX "updateXXX/"
#define CHUNKSIZE       1024*1024*2

char *mtd_files [] = {
    "update001/x-loader-pad-with-sleep-lib.bin",
    "update002/xImage_001",
    "update003/xImage_002",
    "update004/xImage_003",
    "update005/sn.txt",
    "update006/mac.txt",
    "update010/system.jffs2_001",
    "update011/system.jffs2_002",
    "update012/system.jffs2_003",
    "update013/system.jffs2_004",
    "update014/system.jffs2_005",
    #endif
};

int main(int argc, char **argv) {
    char tmp[256];
    char *buf = NULL;
    struct stat aa_stat;
    int i, fd, readsize;
    buf = malloc(CHUNKSIZE);
    if (buf == NULL) {
        LOGI("malloc failed\n");
	return -1;
    }
    strcpy(tmp, "/mnt/update013/system.jffs2_004");
    for (i = 0; i < sizeof(mtd_files) / sizeof(mtd_files[0]); i++) {

        //strcpy(tmp, MOUNT_POINT);
        //strcat(tmp, mtd_files[i]);
	sprintf(tmp, "%s%s", MOUNT_POINT, mtd_files[i]);
        LOGI("%s is going to download\n", tmp);
        fd = open(tmp, O_RDONLY);
        if (fd <= 0) {
            LOGI("cannot open file %s\n", tmp);
            goto exit;
        }
        stat(tmp, &aa_stat);
        LOGI("\"%s\" file size is %d\n", tmp, (int)aa_stat.st_size);

        readsize = read(fd, buf, aa_stat.st_size);
        if (readsize != aa_stat.st_size) {
            LOGI("read filesize error, %d, %d\n", (int)aa_stat.st_size, readsize);
            goto exit;
        }
        if (fd) {
            LOGI("close opened fd \n");
            close(fd);
            fd = 0;
        }

   }
   printf("exit\n");
exit:
   if(buf)
   	free(buf);
   return 0;
}
