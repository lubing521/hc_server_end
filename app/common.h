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

#define DEBUG                   0

#define BUFFER_LEN              1024

#define RESP_BUF_LEN           (0x1 << 15)         /* 32KB */

#define MAX_HOST_NAME_LEN       32

#define HTTP_URL_PREFIX        "http://"
#define HTTP_URL_PRE_LEN        7       /* strlen("http://") */
#define HTTP_URL_MAX_LEN        512

#define YOUKU_WEBSITE_URL      "www.youku.com"
#define SOHU_WEBSITE_URL       "www.sohu.com"

#define TEXT_HTML_SUFFIX       "html"
#define TEXT_HTML_SUFFIX_LEN    4
#define TEXT_M3U8_SUFFIX       "m3u8"
#define TEXT_M3U8_SUFFIX_LEN    4
#define VIDEO_FLV_SUFFIX       "flv"
#define VIDEO_FLV_SUFFIX_LEN    3
#define VIDEO_MP4_SUFFIX       "mp4"
#define VIDEO_MP4_SUFFIX_LEN    3

#define INVALID_PTR             ((void *) -1)

#define true                    1
#define false                   0
typedef int bool;

#define hc_log_info(fmt, arg...) \
    do { \
        fprintf(stdout, "*INFO*  %s: " fmt "\n", __func__, ##arg); \
    } while (0)

#define hc_log_debug(fmt, arg...) \
    do { \
        fprintf(stderr, "*DEBUG* %s[%d]: " fmt "\n", __func__, __LINE__, ##arg); \
    } while (0)

#define hc_log_error(fmt, arg...) \
    do { \
        fprintf(stderr, "*ERROR* %s[%d]: " fmt "\n", __func__, __LINE__, ##arg); \
    } while (0)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

#endif /* __COMMON_H__ */

