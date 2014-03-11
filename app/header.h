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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_LEN          512

#define MAX_HOST_NAME_LEN   32

#define MAX_SLEEP           32     /* TCP connect maximum retry time (sec) */
#define HTTP_URL_PRE_LEN    7                   /* strlen("http://") */

#define LF     ((unsigned char) 10)
#define CR     ((unsigned char) 13)

#define true 1
#define false 0
typedef int bool;

int sock_conn_retry(int sockfd, const struct sockaddr *addr, socklen_t alen);
int host_connect(const char *hostname);

int http_parse_status_line(char *buf, int *status);

