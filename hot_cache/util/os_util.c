/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * os_util.c
 * Original Author: zhaoyao@ruijie.com.cn, 2014-04-28
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#include "os_util.h"
#include "sc_header.h"

static int os_dir_walk_func(const char *fpath, const struct stat *sb,int typeflag)
{
    int ret = 0;

    if (typeflag == FTW_F) {
        ret = sc_ld_file_process((char *)fpath);
        return ret;
    } else {
        return 0;
    }
}

int os_dir_walk(const char *dirpath)
{
    int ret;

    if (dirpath == NULL) {
        return -1;
    }

    ret = ftw(dirpath, os_dir_walk_func, OS_DIR_WALK_OPEN_FD_MAX_NO);

    return ret;
}

int os_file_rename(const char *oldname, const char *newname)
{
    return rename(oldname, newname);
}

int os_file_remove(const char *pathname)
{
    return remove(pathname);
}


