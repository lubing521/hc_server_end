/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * getfile.c
 * Original Author: zhaoyao@ruijie.com.cn, 2014-03-11
 *
 * Send hot cache file URI to Nginx, and inform Nginx /getfile module to download corresponding
 * file from third party (e.g. Youku) with upstream method.
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#include "common.h"
#include "sc_header.h"
#include "net_util.h"

static char sc_ngx_default_ip_addr[] = SC_NGX_DEFAULT_IP_ADDR;
static uint16_t sc_ngx_default_port = SC_NGX_DEFAULT_PORT;

static char sc_ngx_get_pattern[] = "GET /getfile?%s HTTP/1.1\r\n"
                                   "Host: %s\r\n"
                                   "Connection: close\r\n\r\n";

static int sc_ngx_build_get(const char *ip, const char *uri, char *buf)
{
    if (buf == NULL) {
        return -1;
    }

    /* zhaoyao XXX TODO: checking request line ,header and length */

    sprintf(buf, sc_ngx_get_pattern, uri, ip);

    return 0;
}

/**
 * NAME: sc_ngx_download
 *
 * DESCRIPTION:
 *      调用接口；
 *      输入资源真实的URL，告知Nginx使用upstream方式下载资源。
 *
 * @ngx_ip: -IN Nginx用于响应下载请求的IP地址
 * @url:    -IN 资源真实的URL，注意是去掉"http://"的，例如58.211.22.175/youku/x/xxx.flv
 *
 * RETURN: -1表示失败，0表示成功。
 */
int sc_ngx_download(char *ngx_ip, char *url)
{
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;
    char *ip_addr;
    char buffer[BUFFER_LEN];
    int nsend, nrecv, len;
    int err = 0, status;

    if (ngx_ip == NULL) {
        ip_addr = sc_ngx_default_ip_addr;
    } else {
        ip_addr = ngx_ip;
    }
    if (url == NULL) {
        fprintf(stderr, "%s invalid argument\n", __func__);
        return -1;
    }

    if (memcmp(url, HTTP_URL_PREFIX, HTTP_URL_PRE_LEN) == 0) {
        fprintf(stderr, "%s WARNING: input url should not begin with \"http://\"\n", __func__);
        url = url + HTTP_URL_PRE_LEN;
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)sc_ngx_default_port);
    sa.sin_addr.s_addr = inet_addr(ip_addr);
    salen = sizeof(struct sockaddr_in);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        return -1;
    }

    if (sock_conn_retry(sockfd, (struct sockaddr *)&sa, salen) < 0) {
        err = -1;
        goto out;
    }

    memset(buffer, 0, BUFFER_LEN);
    if (sc_ngx_build_get(ip_addr, url, buffer) < 0) {
        fprintf(stderr, "sc_ngx_build_get failed\n");
        err = -1;
        goto out;
    }
    len = strlen(buffer);
    if (len >= BUFFER_LEN) {
        fprintf(stderr, "%s WARNING: sc_ngx_build_get length %d too long\n", __func__, len);
        err = -1;
        goto out;
    }
	len = len + 1; /* zhaoyao: plus terminator '\0' */

    nsend = send(sockfd, buffer, len, 0);
    if (nsend != len) {
        perror("Send failed");
        err = -1;
        goto out;
    }

    memset(buffer, 0, BUFFER_LEN);
    nrecv = recv(sockfd, buffer, BUFFER_LEN, MSG_WAITALL);
    if (nrecv <= 0) {
        perror("Recv failed or meet EOF");
        err = -1;
        goto out;
    }

    if (http_parse_status_line(buffer, &status) < 0) {
        fprintf(stderr, "Parse status line failed:\n%s", buffer);
        err = -1;
        goto out;
    }

    if (status == 200) {
        fprintf(stdout, "Getfile success!!! Response status code %d\n", status);
    } else {
        fprintf(stderr, "Getfile failed, response status code %d:%s", status, buffer);
        err = -1;
        goto out;
    }

out:
    close(sockfd);

    return err;
}

