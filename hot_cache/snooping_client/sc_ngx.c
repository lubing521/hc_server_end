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
    char *para1 = "&localpath=%s";
    char *para2 = "?localpath=%s";
    int para_len;

    if (buf == NULL || uri == NULL || local_path == NULL || ip == NULL) {
        return -1;
    }

    para_len = strlen(para1) - 2;
    bzero(lp, SC_RES_LOCAL_PATH_MAX_LEN);
    if (strlen(local_path) >= SC_RES_LOCAL_PATH_MAX_LEN - para_len) {
        hc_log_error("local_path too long");
        return -1;
    }
    if (strchr(uri, '?')) {
        sprintf(lp, para1, local_path);
    } else {
        sprintf(lp, para2, local_path);
    }

    if (len <= (strlen(sc_ngx_get_pattern) + strlen(ip) + strlen(uri) + strlen(lp) - 6)) {
        return -1;
    }

    sprintf(buf, sc_ngx_get_pattern, uri, lp, ip);

#if DEBUG
    hc_log_debug("request: %s", buf);
#endif

    return 0;
}

/**
 * NAME: sc_ngx_download
 *
 * DESCRIPTION:
 *      ���ýӿڣ�
 *      ������Դ��ʵ��URL����֪Nginxʹ��upstream��ʽ������Դ��
 *
 * @url:        -IN ��Դ��ʵ��URL��ע����ȥ��"http://"�ģ�����58.211.22.175/youku/x/xxx.flv
 * @local_path: -IN ��Դ�����ڱ��ص�·��
 *
 * RETURN: -2��ʾ��Դ�Ѵ��ڣ�-1��ʾʧ�ܣ�0��ʾ�ɹ���
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
        hc_log_error("Invalid argument");
        return -1;
    }

    if (memcmp(url, HTTP_URL_PREFIX, HTTP_URL_PRE_LEN) == 0) {
        hc_log_debug("Input url should not begin with \"http://\"");
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
        hc_log_error("sc_ngx_build_get failed");
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
        hc_log_error("Parse status line failed:\n%s", buffer);
        err = -1;
        goto out;
    }

    if (status == 200) {
        ;
    } else {
        if (status == 302) {
            if (strstr(buffer, "Location: http://20.0.0.99:8080") != NULL) {
                /* zhaoyao XXX TODO: �ⲻ�Ǵ�����Դ�ѱ������ұ��豸�ɹ��ض��� */
                hc_log_info("WARNING: resource already cached: %s", url);
                err = -2;   /* zhaoyao XXX TODO: ����-2������ʧ�ܣ��˸�֪��������Դ����Ҫ������ */
                goto out;
            }
        }
        hc_log_error("Response status code %d:\n%s", status, buffer);
        err = -1;
        goto out;
    }

out:
    close(sockfd);

    return err;
}
