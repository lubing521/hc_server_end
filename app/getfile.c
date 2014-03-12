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

#include "header.h"

static char default_hostip[] = "192.168.46.89";

static char request_pattern[] = "GET /getfile?%s HTTP/1.1\r\n"
                                "Host: %s\r\n"
                                "Connection: close\r\n\r\n";

static int gf_build_request(const char *ip, const char *uri, char *buf)
{
    if (buf == NULL) {
        return -1;
    }

    /* zhaoyao TODO: checking request line ,header and length */

    sprintf(buf, request_pattern, uri, ip);

    return 0;
}

static void gf_print_usage(char *cmd)
{
    printf("%s [hostip] URI\n\n", cmd);
    printf("NOTE: Default host IP address is %s\n", default_hostip);
    printf("      URI should like this - 222.73.245.205/youku/6A6.flv\n");
}

#if 0
int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;
    char *hostip;
    char *uri;
    char buffer[BUFFER_LEN];
    int nsend, nrecv;
    int err = 0, status;

    if (argc == 2) {
        hostip = default_hostip;
        uri = argv[1];
    } else if (argc == 3) {
        hostip = argv[1];
        uri = argv[2];
    } else {
        gf_print_usage(argv[0]);
        exit(-1);
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)80);
    sa.sin_addr.s_addr = inet_addr(hostip);
    salen = sizeof(struct sockaddr_in);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        exit(-1);
    }

    if (sock_conn_retry(sockfd, (struct sockaddr *)&sa, salen) < 0) {
        err = -1;
        goto out;
    }

    memset(buffer, 0, BUFFER_LEN);
    if (gf_build_request(hostip, uri, buffer) < 0) {
        fprintf(stderr, "gf_build_request failed\n");
        err = -1;
        goto out;
    }

    nsend = send(sockfd, buffer, BUFFER_LEN, 0);
    if (nsend != BUFFER_LEN) {
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
        fprintf(stderr, "Response status code %d:%s", status, buffer);
    }

out:
    close(sockfd);

    return err;
}
#endif

int gf_inform_ngx_download(char *ngx_ip, char *url)
{
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;
    char *hostip;
    char buffer[BUFFER_LEN];
    int nsend, nrecv;
    int err = 0, status;

    if (ngx_ip == NULL) {
        hostip = default_hostip;
    } else {
        hostip = ngx_ip;
    }
    if (url == NULL) {
        fprintf(stderr, "%s invalid argument\n", __func__);
        return -1;
    }

    if (strncmp(url, "http://", HTTP_URL_PRE_LEN) == 0) {
        url = url + HTTP_URL_PRE_LEN;
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)80);
    sa.sin_addr.s_addr = inet_addr(hostip);
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
    if (gf_build_request(hostip, url, buffer) < 0) {
        fprintf(stderr, "gf_build_request failed\n");
        err = -1;
        goto out;
    }

    nsend = send(sockfd, buffer, BUFFER_LEN, 0);
    if (nsend != BUFFER_LEN) {
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

