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

static char sc_ngx_get_pattern[] = "GET /getfile?%s%s HTTP/1.1\r\n"
                                   "Host: %s\r\n"
                                   "Connection: close\r\n\r\n";

static int sc_ngx_build_get(const char *ip,
                            const char *uri,
                            const char *local_path,
                            char *buf,
                            unsigned int len)
{
    char lp[SC_RES_LOCAL_PATH_MAX_LEN];

    if (buf == NULL || uri == NULL || local_path == NULL || ip == NULL) {
        return -1;
    }

    bzero(lp, SC_RES_LOCAL_PATH_MAX_LEN);
    if (strlen(local_path) >= SC_RES_LOCAL_PATH_MAX_LEN - 11) {
        /* zhaoyao XXX TODO: 本地路径名长度 */
        fprintf(stderr, "%s ERROR: local_path too long\n", __func__);
        return -1;
    }
    if (strchr(uri, '?')) {
        sprintf(lp, "&localpath=%s", local_path);
    } else {
        sprintf(lp, "?localpath=%s", local_path);
    }

    if (len <= (strlen(sc_ngx_get_pattern) + strlen(ip) + strlen(uri) + strlen(lp) - 6)) {
        return -1;
    }

    sprintf(buf, sc_ngx_get_pattern, uri, lp, ip);

#if DEBUG
    fprintf(stdout, "%s DEBUG request:\n%s\n", __func__, buf);
#endif

    return 0;
}

/**
 * NAME: sc_ngx_download
 *
 * DESCRIPTION:
 *      调用接口；
 *      输入资源真实的URL，告知Nginx使用upstream方式下载资源。
 *
 * @url:        -IN 资源真实的URL，注意是去掉"http://"的，例如58.211.22.175/youku/x/xxx.flv
 * @local_path: -IN 资源保存在本地的路径
 *
 * RETURN: -2表示资源已存在，-1表示失败，0表示成功。
 */
int sc_ngx_download(char *url, char *local_path)
{
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;
    char *ip_addr;
    char buffer[BUFFER_LEN];
    int nsend, nrecv, len;
    int err = 0, status;

    ip_addr = sc_ngx_default_ip_addr;

    if (url == NULL || local_path == NULL) {
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
    if (sc_ngx_build_get(ip_addr, url, local_path, buffer, BUFFER_LEN) < 0) {
        fprintf(stderr, "%s ERROR: sc_ngx_build_get failed\n", __func__);
        err = -1;
        goto out;
    }
    len = strlen(buffer) + 1; /* zhaoyao: plus terminator '\0' */

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

    if (http_parse_status_line(buffer, nrecv, &status) < 0) {
        fprintf(stderr, "Parse status line failed:\n%s", buffer);
        err = -1;
        goto out;
    }

    if (status == 200) {
        //fprintf(stdout, "Getfile success!!! Response status code %d\n", status);
    } else {
        if (status == 302) {
            if (strstr(buffer, "Location: http://20.0.0.99:8080") != NULL) {
                /* zhaoyao XXX TODO: 这不是错误，资源已被缓存且被设备成功重定向 */
                fprintf(stdout, "%s WARNING: resource already cached: %s\n", __func__, url);
                err = -2;   /* zhaoyao XXX TODO: 返回-2不代表失败，乃告知调用者资源不需要下载了 */
                goto out;
            }
        }
        fprintf(stderr, "%s ERROR: response status code %d:\n%s", status, buffer);
        err = -1;
        goto out;
    }

out:
    close(sockfd);

    return err;
}

