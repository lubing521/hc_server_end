/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * yk_lib.c
 * Original Author: zhaoyao@ruijie.com.cn, 2014-03-11
 *
 * Youku video protocol main file.
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#include "yk_lib.h"
#include "net_util.h"

static char yk_request_pattern[] = 
//    "GET %s HTTP/1.1\r\n"
    "GET %s HTTP/1.0\r\n"
    "Host: %s\r\n"
    "Connection: close\r\n"
    "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) "
                "Chrome/33.0.1750.117 Safari/537.36\r\n"
    "Accept: */*\r\n"
    "Referer: http://%s\r\n"
//    "Accept-Encoding: gzip,deflate,sdch\r\n"
    "Accept-Encoding:\r\n"
    "Accept-Language: en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4\r\n\r\n";

int yk_build_request(char *host, char *uri, char *referer, char *buf)
{
    int len;

    if (buf == NULL) {
        return -1;
    }

    len = strlen(host) + strlen(uri) + strlen(referer) + strlen(yk_request_pattern) - 6;
    if (len >= BUFFER_LEN) {
        fprintf(stderr, "%s request length (%d) exceed limit %d\n", __func__, len, BUFFER_LEN);
        return -1;
    }

    sprintf((char *)buf, (char *)yk_request_pattern, uri, host, referer);

    return 0;
}

void yk_print_usage(char *cmd)
{
    printf("%s youku_url [dl]\n", cmd);
    printf("\tURL's format is http://v.youku.com/v_show/id_XNjgzMjc0MjY4.html\n\n");
}

/*
 * v.youku.com/v_show/id_XNjgzMjc0MjY4.html
 */
int yk_is_tradition_url(char *yk_url)
{
    static char *tag1 = "v.youku.com";
    static char *tag2 = "/id_";
    char *cur;

    if (yk_url == NULL) {
        return 0;
    }

    cur = yk_url;
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

int yk_seg_to_flvpath(const yk_segment_info_t *seg, char *fp_url)
{
    char fileids[YK_FILEID_LEN + 1];    /* plus terminator '\0' */
    playlistdata_t play_list;
    video_seg_data_t seg_data;
    yk_stream_info_t *strm;
    
    if (seg == NULL || fp_url == NULL || seg->stream == NULL) {
        return -1;
    }

    strm = seg->stream;

    memset(fileids, 0, sizeof(fileids));
    if (yk_get_fileid(strm->streamfileids, seg->no, strm->seed, fileids) == false) {
        fprintf(stderr, "yk_get_fileid failed\n");
        return -1;
    }

    memset(&play_list, 0, sizeof(play_list));
	memcpy(play_list.fileType, strm->type, 3);
	play_list.drm = false;
	play_list.sid[0] = '0';
	play_list.sid[1] = '0';

	memset(&seg_data, 0, sizeof(seg_data));
	memcpy(seg_data.fileId, fileids, strlen(fileids));
	memcpy(seg_data.key, seg->k, strlen(seg->k));

    memset(fp_url, 0, HTTP_URL_MAX_LEN);
    if (yk_get_fileurl(0, &play_list, &seg_data, false, 0, fp_url) != true) {
        fprintf(stderr, "yk_get_fileurl failed\n");
        return -1;
    }

    return 0;
}

int yk_http_session(char *url, char *referer, char *response, unsigned long resp_len)
{
    int sockfd = -1, err = 0;
    char host[MAX_HOST_NAME_LEN], *uri_start;
    char buffer[BUFFER_LEN];
    int nsend, nrecv, len, i, pre_len = 0;

    if (url == NULL || referer == NULL || response == NULL) {
        fprintf(stderr, "Input is invalid\n");
        return -1;
    }

    if (resp_len < BUFFER_LEN) {
        fprintf(stderr, "%s ERROR: buffer too small(%lu), minimum should be %d\n",
                            __func__, resp_len, BUFFER_LEN);
        return -1;
    }

    if (memcmp(url, HTTP_URL_PREFIX, HTTP_URL_PRE_LEN) == 0) {
        pre_len = HTTP_URL_PRE_LEN;
    }

    memset(host, 0, MAX_HOST_NAME_LEN);
    for (i = 0; (*(url + pre_len + i) != '/') && (*(url + pre_len + i) != '\0'); i++) {
        host[i] = *(url + pre_len + i);
    }
    if (*(url + pre_len + i) == '\0') {
        fprintf(stderr, "URL is invalid: %s\n", url);
        return -1;
    }
    host[i] = '\0';
    uri_start = url + pre_len + i;

    sockfd = http_host_connect(host);
    if (sockfd < 0) {
        fprintf(stderr, "Can not connect to %s: http\n", host);
        return -1;
    }

    memset(buffer, 0, BUFFER_LEN);
    if (yk_build_request(host, uri_start, referer, buffer) < 0) {
        fprintf(stderr, "yk_build_request failed\n");
        err = -1;
        goto out;
    }
    
    len = strlen(buffer);
    if (len > BUFFER_LEN) {
        fprintf(stderr, "Request is too long, error!!!\n");
        err = -1;
        goto out;
    }
    nsend = send(sockfd, buffer, len, 0);
    if (nsend != len) {
        perror("Send failed");
        err = -1;
        goto out;
    }

    memset(response, 0, resp_len);
    nrecv = recv(sockfd, response, resp_len, MSG_WAITALL);
    if (nrecv <= 0) {
        perror("Recv failed or meet EOF");
        err = -1;
        goto out;
    }
    if (nrecv == resp_len) {
        fprintf(stderr, "%s WARNING: receive %d bytes, response buffer is full!!!\n", __func__, nrecv);
    }

out:
    close(sockfd);

    return err;
}

char *yk_parse_vf_response(char *curr, char *fp_url)
{
    char *ret = NULL, *p;
    char *tag = "\"RS\"";
    unsigned long len;

    if (curr == NULL || fp_url == NULL) {
        fprintf(stderr, "%s ERROR: Invalid input, curr is NULL\n", __func__);
        return ret;
    }

    p = strstr(curr, tag);
    if (p == NULL) {
        fprintf(stderr, "%s ERROR: do not find %s\n", __func__, tag);
        return ret;
    }

    p = strstr(p, HTTP_URL_PREFIX);
    if (p == NULL) {
        fprintf(stderr, "%s ERROR: do not find %s\n", __func__, HTTP_URL_PREFIX);
        return ret;
    }
    p = p + HTTP_URL_PRE_LEN;
    curr = p;

    for ( ; *p != '"' && *p != '\0'; p++) {
        ;
    }
    len = (unsigned long)p - (unsigned long)curr;
    if (len >= HTTP_URL_MAX_LEN) {
        fprintf(stderr, "%s ERROR: parsed url len %lu, exceed limit %u\n", __func__, len, HTTP_URL_MAX_LEN);
    } else {
        strncpy(fp_url, curr, len);
    }

    ret = strstr(curr, tag);

    return ret;
}


