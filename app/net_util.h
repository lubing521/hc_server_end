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

#ifndef __NET_UTIL_H__
#define __NET_UTIL_H__

#include "common.h"

#define TCP_CONN_MAX_RETRY_TIME     16      /* TCP connect maximum retry time (sec) */

#define LF                  ((unsigned char) 10)
#define CR                  ((unsigned char) 13)

int sock_conn_retry(int sockfd, const struct sockaddr *addr, socklen_t alen);
int sock_init_server(int type, const struct sockaddr *addr, socklen_t alen, int qlen);

int http_host_connect(const char *host);    /* Connect http 80 port */
int http_parse_status_line(char *buf, int len, int *status);

int util_json_to_ascii_string(char *buf, int len);

#endif /* __NET_UTIL_H__ */

