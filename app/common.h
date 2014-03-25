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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/shm.h>

#include <pthread.h>

#define BUFFER_LEN          512

#define MAX_HOST_NAME_LEN   32

#define HTTP_URL_PREFIX     "http://"
#define HTTP_URL_PRE_LEN    7       /* strlen("http://") */

#define LF                  ((unsigned char) 10)
#define CR                  ((unsigned char) 13)

#define true                1
#define false               0
typedef int bool;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;


