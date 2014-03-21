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

#include "common.h"

#define MAX_SLEEP           32      /* TCP connect maximum retry time (sec) */

int sock_conn_retry(int sockfd, const struct sockaddr *addr, socklen_t alen);
int host_connect(const char *hostname);

int http_parse_status_line(char *buf, int *status);

