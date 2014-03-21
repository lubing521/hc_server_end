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

#define TCP_CONN_MAX_RETRY_TIME     16      /* TCP connect maximum retry time (sec) */

int sock_conn_retry(int sockfd, const struct sockaddr *addr, socklen_t alen);

int http_host_connect(const char *host);    /* Connect http 80 port */
int http_parse_status_line(char *buf, int *status);

