/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * header.h
 * Original Author: zhaoyao@ruijie.com.cn, 2014-03-11
 *
 * Hot cache application's common header file.
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#ifndef __OS_UTIL_H__
#define __OS_UTIL_H__

#include <ftw.h>

#include "common.h"

#define OS_DIR_WALK_OPEN_FD_MAX_NO 1000     /* zhaoyao: 在遍历目录时，同时打开文件的最大量 */

int os_dir_walk(const char *dirpath);

#endif /* __OS_UTIL_H__ */

