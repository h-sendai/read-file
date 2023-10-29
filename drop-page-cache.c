#include "drop-page-cache.h"

int drop_page_cache(char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        warn("open in drop_page_cache");
        return -1;
    }
    if (fdatasync(fd) < 0) {
        warn("fdatasync in drop_page_cache");
    }

    int n = posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    if (n != 0) {
        warnx("posix_fadvise POSIX_FADV_DONTNEED fail");
        return -1;
    }

    return 0;
}
