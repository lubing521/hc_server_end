
#include "sohu_lib.h"
#include "net_util.h"
#include "sc_header.h"

static char sohu_request_pattern[] =
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Connection: close\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
    "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) "
                "Chrome/33.0.1750.154 Safari/537.36\r\n"
    "Accept-Encoding: gzip,deflate,sdch\r\n"
    "Accept-Language: en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4\r\n\r\n";

int sohu_build_request(char *host, char *uri, char *buf)
{
    int len;

    if (buf == NULL) {
        return -1;
    }

    len = strlen(host) + strlen(uri) + strlen(sohu_request_pattern) - 3;
    if (len >= BUFFER_LEN) {
        fprintf(stderr, "%s request length (%d) exceed limit %d\n", __func__, len, BUFFER_LEN);
        return -1;
    }

    sprintf((char *)buf, (char *)sohu_request_pattern, uri, host);

    return 0;
}

/*
 * hot.vrs.sohu.com/ipad1683703_4507722770245_4894024.m3u8
 */
int sohu_http_session(char *url, char *response)
{
    int sockfd = -1, err = 0;
    char host[MAX_HOST_NAME_LEN], *uri_start;
    char buffer[BUFFER_LEN];
    int nsend, nrecv, len, i;

    if (url == NULL || response == NULL) {
        fprintf(stderr, "%s ERROR: input is invalid\n", __func__);
        return -1;
    }

    memset(host, 0, MAX_HOST_NAME_LEN);
    for (i = 0; (*(url + i) != '/') && (*(url + i) != '\0'); i++) {
        host[i] = *(url + i);
    }
    if (*(url + i) == '\0' || i >= MAX_HOST_NAME_LEN) {
        fprintf(stderr, "%s ERROR: URL is invalid: %s\n", __func__, url);
        return -1;
    }
    host[i] = '\0';
    uri_start = url + i;

    sockfd = http_host_connect(host);
    if (sockfd < 0) {
        fprintf(stderr, "%s ERROR: can not connect to %s: http\n", __func__, host);
        return -1;
    }

    memset(buffer, 0, BUFFER_LEN);
    if (sohu_build_request(host, uri_start, buffer) < 0) {
        fprintf(stderr, "%s ERROR: sohu_build_request failed\n", __func__);
        err = -1;
        goto out;
    }
    
    len = strlen(buffer);
    if (len > BUFFER_LEN) {
        fprintf(stderr, "%s ERROR: request too long, host %s, uri %s\n", __func__, host, uri_start);
        err = -1;
        goto out;
    }
    nsend = send(sockfd, buffer, len, 0);
    if (nsend != len) {
        fprintf(stderr, "%s ERROR: send %d bytes failed: %s\n", __func__, nsend, strerror(errno));
        err = -1;
        goto out;
    }

    memset(response, 0, RESP_BUF_LEN);
    nrecv = recv(sockfd, response, RESP_BUF_LEN, MSG_WAITALL);
    if (nrecv <= 0) {
        fprintf(stderr, "%s ERROR: recv failed or meet EOF, %d: %s\n", __func__, nrecv, strerror(errno));
        err = -1;
        goto out;
    }

out:
    close(sockfd);

    return err;
}

/*
 * hot.vrs.sohu.com/ipad1683703_4507722770245_4894024.m3u8?plat=0
 */
int sohu_is_m3u8_url(char *sohu_url)
{
    static char *tag1 = "sohu.com";
    static char *tag2 = ".m3u8";
    char *cur;

    if (sohu_url == NULL) {
        return 0;
    }

    cur = sohu_url;
    cur = strstr(cur, tag1);
    if (cur == NULL) {
        return 0;
    }

    cur = strstr(cur, tag2);
    if (cur == NULL) {
        return 0;
    }

    return 1;
}

char *sohu_parse_m3u8_response(char *curr, char *real_url)
{
    char *ret = NULL, *p;
    char *tag1 = "file=";
    char *tag2 = "#EXT-X-DISCONTINUITY";
    unsigned long len;

    if (curr == NULL || real_url == NULL) {
        return ret;
    }

    curr = strstr(curr, HTTP_URL_PREFIX);
    if (curr == NULL) {
        return ret;
    }

    curr = curr + HTTP_URL_PRE_LEN;
    p = strstr(curr, tag1);
    if (p == NULL) {
        return ret;
    }
    for ( ; *p != '&' && *p != '\0'; p++) {
        ;
    }
    len = (unsigned long)p - (unsigned long)curr;
    if (len >= SC_RES_URL_MAX_LEN) {
        fprintf(stderr, "%s ERROR: parsed url len %lu, exceed limit %u\n", __func__, len, SC_RES_URL_MAX_LEN);
    } else {
        strncpy(real_url, curr, len);
    }

    ret = strstr(curr, tag2);

    return ret;
}


