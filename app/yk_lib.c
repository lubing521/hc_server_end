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

#include "header.h"
#include "yk_lib.h"

static unsigned char yk_request_pattern[] = 
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Connection: close\r\n"
    "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) "
                "Chrome/33.0.1750.117 Safari/537.36\r\n"
    "Accept: */*\r\n"
    "Referer: %s\r\n"
    "Accept-Encoding: gzip,deflate,sdch\r\n"
    "Accept-Language: en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4\r\n\r\n";

static int yk_build_request(const char *host,
                            const char *uri,
                            const char *referer,
                            char *buf)
{
    if (buf == NULL) {
        return -1;
    }
    
    /* zhaoyao TODO: checking request line ,header and length */

    sprintf((char *)buf, (char *)yk_request_pattern, uri, host, referer);

    return 0;
}

static void yk_print_usage(char *cmd)
{
    printf("%s youku_url\n", cmd);
    printf("\tURL's format is http://v.youku.com/v_show/id_XNjgzMjc0MjY4.html\n\n");
}

/*
 * zhaoyao TODO: url's validity
 * http://v.youku.com/v_show/id_XNjgzMjc0MjY4.html
 */
static int yk_is_valid_url(const char *yk_url)
{
    return 1;
}

int main(int argc, char *argv[])
{
    int sockfd;
    char host[MAX_HOST_NAME_LEN];
    char *yk_url;                           /* Youku video public URL */
    char pl_url[BUFFER_LEN], *pl_uri_start; /* getplaylist URL */
    char fp_url[BUFFER_LEN], *fp_uri_start; /* getflvpath URL */
    char buffer[BUFFER_LEN];
    char *response = NULL;
    int nsend, nrecv, len, i, j;
    int err = 0, status;
    yk_stream_info_t *streams[STREAM_TYPE_TOTAL] = {NULL}, *strm;
    char fileids[YK_FILEID_LEN + 1];
    int seed;
    playlistdata_t play_list;
    video_seg_data_t seg_data;

    if (argc == 2) {
        yk_url = argv[1];
    } else {
        yk_print_usage(argv[0]);
        exit(-1);
    }

    if (!yk_is_valid_url(yk_url)) {
        fprintf(stderr, "Invalid URL of Youku video: %s\n", yk_url);
        exit(-1);
    }

    /*
     * Step 1 - getPlaylist.
     */
    memset(pl_url, 0, BUFFER_LEN);
    /* zhaoyao FIXME TODO: yk_url_to_playlist has no string length control, overflow may occur */
    if (yk_url_to_playlist((char *)yk_url, (char *)pl_url) != true) {
        fprintf(stderr, "yk_url_to_playlist failed, url is:\n%s\n", yk_url);
        exit(-1);
    }

    memset(host, 0, MAX_HOST_NAME_LEN);
    for (i = 0; (*(pl_url + HTTP_URL_PRE_LEN + i) != '/') && (*(pl_url + HTTP_URL_PRE_LEN + i) != '\0'); i++) {
        host[i] = *(pl_url + HTTP_URL_PRE_LEN + i);
    }
    if (*(pl_url + HTTP_URL_PRE_LEN + i) == '\0') {
        fprintf(stderr, "yk_url_to_playlist uri is invalid: %s\n", pl_url);
        exit(-1);
    }
    host[i] = '\0';
    pl_uri_start = pl_url + HTTP_URL_PRE_LEN + i;
    
    printf("getPlaylist's\n\tHost:%s\n\tURI:%s\n", host, pl_uri_start);

    sockfd = host_connect(host);
    if (sockfd < 0) {
        fprintf(stderr, "Can not connect to %s: http\n", host);
        err = -1;
        goto out;
    }

    memset(buffer, 0, BUFFER_LEN);
    if (yk_build_request(host, pl_uri_start, yk_url, buffer) < 0) {
        fprintf(stderr, "yk_build_request failed\n");
        err = -1;
        goto out;
    }

    len = strlen((char *)buffer);
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

    response = malloc(RESP_BUF_LEN);
    if (response == NULL) {
        perror("Malloc failed");
        err = -1;
        goto out;
    }
    memset(response, 0, RESP_BUF_LEN);
    nrecv = recv(sockfd, response, RESP_BUF_LEN, MSG_WAITALL);
    if (nrecv <= 0) {
        perror("Recv failed or meet EOF");
        err = -1;
        goto out;
    }
    close(sockfd);
    sockfd = -1;

    if (http_parse_status_line(response, &status) < 0) {
        fprintf(stderr, "Parse status line failed:\n%s", response);
        err = -1;
        goto out;
    }

    if (status == 200) {
        fprintf(stdout, "getPlaylist success!!! Response status code %d\n", status);
    } else {
        fprintf(stderr, "getPlaylist status code %d:%s", status, response);
        err = -1;
        goto out;
    }

    memset(streams, 0, sizeof(streams));
    if (yk_parse_playlist((char *)response, streams, &seed) != 0) {
        fprintf(stderr, "Parse getPlaylist response failed:\n%s\n", response);
        err = -1;
        goto out;
    } else {
        //printf("Parse getPlaylist response success, seed is %d\n", seed);
        //yk_debug_streams_all(streams);
    }

    for (i = 0; i < 1 && streams[i] != NULL; i++) {
        strm = streams[i];
        if (strm->segs == NULL) {   /* Has no segments info, innormal situation */
            fprintf(stderr, "WARNING: stream %s has no segs\n", strm->type);
            continue;
        }

        for (j = 0; j < 1 && strm->segs[j] != NULL; j++) {
            memset(fileids, 0, sizeof(fileids));
            if (yk_get_fileid(strm->streamfileids, strm->segs[j]->no, seed, fileids) == false) {
                fprintf(stderr, "yk_get_fileid failed\n");
                err = -1;
                goto out;
            }

            memset(&play_list, 0, sizeof(play_list));
        	memcpy(play_list.fileType, strm->type, 3);
        	play_list.drm = false;
        	play_list.sid[0] = '0';
        	play_list.sid[1] = '0';

        	memset(&seg_data, 0, sizeof(seg_data));
        	memcpy(seg_data.fileId, fileids, strlen(fileids));
        	memcpy(seg_data.key, strm->segs[j]->k, strlen(strm->segs[j]->k));

            memset(fp_url, 0, BUFFER_LEN);
            if (yk_get_fileurl(0, &play_list, &seg_data, false, 0, fp_url) != true) {
                fprintf(stderr, "yk_get_fileurl failed\n");
                err = -1;
                goto out;
            }
            printf("getFlvpath_url: %s\n", fp_url);
        }
    }

    /*
     * Step 2 - getFlvpath.
     */
    memset(host, 0, MAX_HOST_NAME_LEN);
    for (i = 0; (*(fp_url + HTTP_URL_PRE_LEN + i) != '/') && (*(fp_url + HTTP_URL_PRE_LEN + i) != '\0'); i++) {
        host[i] = *(fp_url + HTTP_URL_PRE_LEN + i);
    }
    if (*(fp_url + HTTP_URL_PRE_LEN + i) == '\0') {
        fprintf(stderr, "yk_get_fileurl Flvpath is invalid: %s\n", fp_url);
        exit(-1);
    }
    host[i] = '\0';
    fp_uri_start = fp_url + HTTP_URL_PRE_LEN + i;
    printf("getFlvpath's\n\tHost:%s\n\tURI:%s\n", host, fp_uri_start);

    sockfd = host_connect(host);
    if (sockfd < 0) {
        fprintf(stderr, "Can not connect to %s: http\n", host);
        err = -1;
        goto out;
    }

    memset(buffer, 0, BUFFER_LEN);
    if (yk_build_request(host, fp_uri_start, yk_url, buffer) < 0) {
        fprintf(stderr, "yk_build_request failed\n");
        err = -1;
        goto out;
    }

    len = strlen((char *)buffer);
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

    memset(response, 0, RESP_BUF_LEN);
    nrecv = recv(sockfd, response, RESP_BUF_LEN, MSG_WAITALL);
    if (nrecv <= 0) {
        perror("Recv failed or meet EOF");
        err = -1;
        goto out;
    }
    close(sockfd);
    sockfd = -1;

    if (http_parse_status_line(response, &status) < 0) {
        fprintf(stderr, "Parse status line failed:\n%s", response);
        err = -1;
        goto out;
    }

    if (status == 200) {
        fprintf(stdout, "getFlvpath success!!! Response status code %d:\n%s\n", status,response);
    } else {
        fprintf(stderr, "getFlvpath status code %d:\n%s\n", status, response);
        err = -1;
        goto out;
    }

out:
    free(response);
    close(sockfd);
    yk_destroy_streams_all(streams);

    return err;
}

