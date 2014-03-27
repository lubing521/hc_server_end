/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * common.h
 * Original Author: zhaoyao@ruijie.com.cn, 2014-03-11
 *
 * Hot cache application's common header file.
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <endian.h>

#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/shm.h>

#include <pthread.h>

#define DEBUG               0

#define BUFFER_LEN          1024

#define MAX_HOST_NAME_LEN   32

#define HTTP_URL_PREFIX     "http://"
#define HTTP_URL_PRE_LEN    7       /* strlen("http://") */

#define INVALID_PTR         ((void *) -1)

#define true                1
#define false               0
typedef int bool;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

#endif /* __COMMON_H__ */

