#ifndef DROP_PAGE_CACHE
#define DROP_PAGE_CACHE 1

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <unistd.h>

extern int drop_page_cache(char *filename);

#endif
