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

static int yk_http_session(char *url, const char *referer, char *response)
{
    int sockfd = -1, err = 0;
    char host[MAX_HOST_NAME_LEN], *uri_start;
    char buffer[BUFFER_LEN];
    int nsend, nrecv, len, i;

    if (url == NULL || referer == NULL || response == NULL) {
        fprintf(stderr, "Input is invalid\n");
        return -1;
    }

    memset(host, 0, MAX_HOST_NAME_LEN);
    for (i = 0; (*(url + HTTP_URL_PRE_LEN + i) != '/') && (*(url + HTTP_URL_PRE_LEN + i) != '\0'); i++) {
        host[i] = *(url + HTTP_URL_PRE_LEN + i);
    }
    if (*(url + HTTP_URL_PRE_LEN + i) == '\0') {
        fprintf(stderr, "URL is invalid: %s\n", url);
        return -1;
    }
    host[i] = '\0';
    uri_start = url + HTTP_URL_PRE_LEN + i;

    sockfd = host_connect(host);
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

    memset(response, 0, RESP_BUF_LEN);
    nrecv = recv(sockfd, response, RESP_BUF_LEN, MSG_WAITALL);
    if (nrecv <= 0) {
        perror("Recv failed or meet EOF");
        err = -1;
        goto out;
    }

out:
    close(sockfd);

    return err;
}

static int yk_seg_to_flvpath(const yk_segment_info_t *seg, char *fp_url)
{
    char fileids[YK_FILEID_LEN + 1];
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

    memset(fp_url, 0, BUFFER_LEN);
    if (yk_get_fileurl(0, &play_list, &seg_data, false, 0, fp_url) != true) {
        fprintf(stderr, "yk_get_fileurl failed\n");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    char *yk_url;                           /* Youku video public URL */
    char pl_url[BUFFER_LEN];                /* getplaylist URL */
    char fp_url[BUFFER_LEN];                /* getflvpath URL */
    char real_url[BUFFER_LEN];              /* Youku video file's real URL */
    char *response = NULL;
    int i, j;
    int err = 0, status;
    yk_stream_info_t *streams[STREAM_TYPE_TOTAL] = {NULL}, *strm;

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

    response = malloc(RESP_BUF_LEN);
    if (response == NULL) {
        perror("Malloc failed");
        err = -1;
        goto out;
    }

    /*
     * Step 1 - getPlaylist and get flvpath URL.
     */
    memset(pl_url, 0, BUFFER_LEN);
    /* zhaoyao FIXME TODO: yk_url_to_playlist has no string length control, overflow may occur */
    if (yk_url_to_playlist(yk_url, pl_url) != true) {
        fprintf(stderr, "yk_url_to_playlist failed, url is:\n%s\n", yk_url);
        err = -1;
        goto out;
    }

    if (yk_http_session(pl_url, yk_url, response) < 0) {
        fprintf(stderr, "yk_http_session faild, URL: %s\n", pl_url);
        err = -1;
        goto out;
    }

    if (http_parse_status_line(response, &status) < 0) {
        fprintf(stderr, "Parse status line failed:\n%s", response);
        err = -1;
        goto out;
    }

    if (status == 200) {
//        fprintf(stdout, "getPlaylist success!!! Response status code %d\n", status);
    } else {
        fprintf(stderr, "getPlaylist status code %d:%s", status, response);
        err = -1;
        goto out;
    }

    memset(streams, 0, sizeof(streams));
    if (yk_parse_playlist(response, streams) != 0) {
        fprintf(stderr, "Parse getPlaylist response failed:\n%s\n", response);
        err = -1;
        goto out;
    } else {
        //printf("Parse getPlaylist response success\n");
        //yk_debug_streams_all(streams);
    }

    /* zhaoyao XXX: now we only care about the first type stream */
    for (i = 0; i < 1 && streams[i] != NULL; i++) {
        strm = streams[i];

        if (strm->segs == NULL) {   /* Has no segments info, innormal situation */
            fprintf(stderr, "WARNING: stream %s has no segs\n", strm->type);
            continue;
        }

        printf("Stream type: %s\n", strm->type);

        for (j = 0; j < STREAM_SEGS_MAX && strm->segs[j] != NULL; j++) {
            if (yk_seg_to_flvpath(strm->segs[j], fp_url) < 0) {
                fprintf(stderr, "WARNING: yk_seg_to_flvpath failed\n");
                continue;
            }

            /*
             * Step 2 - getFlvpath and get real URL.
             */
            if (yk_http_session(fp_url, yk_url, response) < 0) {
                fprintf(stderr, "yk_http_session faild, URL: %s\n", pl_url);
                err = -1;
                goto out;
            }

            if (http_parse_status_line(response, &status) < 0) {
                fprintf(stderr, "Parse status line failed:\n%s", response);
                err = -1;
                goto out;
            }

            if (status == 200 || status == 302) {
                if (yk_parse_flvpath(response, real_url) < 0) {
                    fprintf(stderr, "Parse getFlvpath response and get real URL failed\n");
                    err = -1;
                    goto out;
                }
                printf("   Segment %-2d URL: %s\n", strm->segs[j]->no, real_url);
                if (gf_inform_ngx_download("192.168.46.89", real_url) < 0) {
                    fprintf(stderr, "   Segment %-2d inform Nginx failed\n", strm->segs[j]->no);
                }
            } else {
                fprintf(stderr, "getFlvpath failed, status code %d:\n%s\n", status, response);
                err = -1;
                goto out;
            }
        }
    }

out:
    free(response);
    yk_destroy_streams_all(streams);

    return err;
}

