#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * get_multiplier - convert size specifier to an integer multiplier.
 * @str: the size specifier string
 *
 * This function parses the @str size specifier, which may be one of
 * 'KiB', 'MiB', or 'GiB' into an integer multiplier. Returns positive
 * size multiplier in case of success and %-1 in case of failure.
 */
int get_multiplier(const char *str)
{
    if (!str)
        return 1;

    /* Remove spaces before the specifier */
    while (*str == ' ' || *str == '\t')
        str += 1;

    if (!strcmp(str, "KiB"))
        return 1024;
    if (!strcmp(str, "MiB"))
        return 1024 * 1024;
    if (!strcmp(str, "GiB"))
        return 1024 * 1024 * 1024;

    return -1;
}

/**
 * ubiutils_get_bytes - convert a string containing amount of bytes into an
 * integer
 * @str: string to convert
 *
 * This function parses @str which may have one of 'KiB', 'MiB', or 'GiB'
 * size specifiers. Returns positive amount of bytes in case of success and %-1
 * in case of failure.
 */
long long get_bytes(const char *str)
{
    char *endp;
    long long bytes = strtoull(str, &endp, 0);

    if (endp == str || bytes < 0) {
        fprintf(stderr, "incorrect amount of bytes: \"%s\"\n", str);
        return -1;
    }

    if (*endp != '\0') {
        int mult = get_multiplier(endp);

        if (mult == -1) {
            fprintf(stderr, "bad size specifier: \"%s\" - "
                    "should be 'KiB', 'MiB' or 'GiB'\n", endp);
            return -1;
        }
        bytes *= mult;
    }

    return bytes;
}

/**
 * ubiutils_print_bytes - print bytes.
 * @bytes: variable to print
 * @bracket: whether brackets have to be put or not
 *
 * This is a helper function which prints amount of bytes in a human-readable
 * form, i.e., it prints the exact amount of bytes following by the approximate
 * amount of Kilobytes, Megabytes, or Gigabytes, depending on how big @bytes
 * is.
 */
void print_bytes(long long bytes, int bracket)
{
    const char *p;

    if (bracket)
        p = " (";
    else
        p = ", ";

    printf("%lld bytes", bytes);

    if (bytes > 1024 * 1024 * 1024)
        printf("%s%.1f GiB", p, (double)bytes / (1024 * 1024 * 1024));
    else if (bytes > 1024 * 1024)
        printf("%s%.1f MiB", p, (double)bytes / (1024 * 1024));
    else if (bytes > 1024 && bytes != 0)
        printf("%s%.1f KiB", p, (double)bytes / 1024);
    else
        return;

    if (bracket)
        printf(")");
}

