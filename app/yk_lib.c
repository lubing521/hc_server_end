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

#include "common.h"
#include "yk_lib.h"
#include "net_util.h"
#include "sc_header.h"

volatile int download = 0;

static char yk_request_pattern[] = 
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Connection: close\r\n"
    "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) "
                "Chrome/33.0.1750.117 Safari/537.36\r\n"
    "Accept: */*\r\n"
    "Referer: http://%s\r\n"
    "Accept-Encoding: gzip,deflate,sdch\r\n"
    "Accept-Language: en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4\r\n\r\n";

static int yk_build_request(char *host, char *uri, char *referer, char *buf)
{
    int len;

    if (buf == NULL) {
        return -1;
    }
    
    /* zhaoyao TODO: checking request line ,header and length */
    len = strlen(host) + strlen(uri) + strlen(referer) + strlen(yk_request_pattern) - 3;
    if (len >= BUFFER_LEN) {
        fprintf(stderr, "%s request length (%d) exceed limit %d\n", __func__, len, BUFFER_LEN);
        return -1;
    }

    sprintf((char *)buf, (char *)yk_request_pattern, uri, host, referer);

    return 0;
}

static void yk_print_usage(char *cmd)
{
    printf("%s youku_url [dl]\n", cmd);
    printf("\tURL's format is http://v.youku.com/v_show/id_XNjgzMjc0MjY4.html\n\n");
}

/*
 * zhaoyao TODO: url's validity
 * v.youku.com/v_show/id_XNjgzMjc0MjY4.html
 */
static int yk_is_valid_url(char *yk_url)
{
    static char *tag1 = "youku.com";
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

    cur = strchr(cur, '?');
    if (cur != NULL) {
         *cur = '\0';    /* zhaoyao XXX TODO: Truncate parameters, side-effect */
    }

    return 1;
}

static int yk_http_session(char *url, char *referer, char *response)
{
    int sockfd = -1, err = 0;
    char host[MAX_HOST_NAME_LEN], *uri_start;
    char buffer[BUFFER_LEN];
    int nsend, nrecv, len, i, pre_len = 0;

    if (url == NULL || referer == NULL || response == NULL) {
        fprintf(stderr, "Input is invalid\n");
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

    memset(fp_url, 0, BUFFER_LEN);
    if (yk_get_fileurl(0, &play_list, &seg_data, false, 0, fp_url) != true) {
        fprintf(stderr, "yk_get_fileurl failed\n");
        return -1;
    }

    return 0;
}

/**
 * NAME: yk_get_video
 *
 * DESCRIPTION:
 *      调用接口；
 *      根据youku网视频的URL，解析得到视频资源真实的URL，并告知Nginx使用upstream方式下载视频资源。
 *
 * @url: -IN 视频的URL，注意是去掉"http://"的，例如v.youku.com/v_show/id_XNjg2MjA1NzQw.html
 *
 * RETURN: -1表示失败，0表示成功。
 */
int yk_get_video(char *url)
{
    char yk_url[BUFFER_LEN];                /* Youku video public URL */
    char pl_url[BUFFER_LEN];                /* getplaylist URL */
    char fp_url[BUFFER_LEN];                /* getflvpath URL */
    char real_url[BUFFER_LEN];              /* Youku video file's real URL */
    char *response = NULL;
    int i, j;
    int err = 0, status;
    yk_stream_info_t *streams[STREAM_TYPE_TOTAL] = {NULL}, *strm;

    if (url == NULL || strlen(url) <= HTTP_URL_PRE_LEN) {
        fprintf(stderr, "%s invalid input url\n", __func__);
        return -1;
    }
    memset(yk_url, 0, BUFFER_LEN);
    if (memcmp(url, HTTP_URL_PREFIX, HTTP_URL_PRE_LEN) == 0) {
        fprintf(stderr, "%s WARNING url cannot have \"http://\" prefix\n", __func__);
        memcpy(yk_url, url + HTTP_URL_PRE_LEN, strlen(url) - HTTP_URL_PRE_LEN);
    } else {
        memcpy(yk_url, url, strlen(url));
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
//        fprintf(stdout, "getPlaylist success!!! Response status code %d\n%s\n", status, response);
    } else {
        fprintf(stderr, "getPlaylist's response status code %d:%s", status, response);
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
            /*
             * Step 2 - getFlvpath and get real URL.
             */
            if (yk_seg_to_flvpath(strm->segs[j], fp_url) < 0) {
                fprintf(stderr, "WARNING: yk_seg_to_flvpath failed\n");
                continue;
            }

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
                if (download && gf_inform_ngx_download(NGINX_SERVER_IP_ADDR, real_url) < 0) {
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

int main(int argc, char *argv[])
{
    if ((argc == 3) && (memcmp(argv[2], "dl", 2) == 0)) {
        download = 1;
    }
    if (argc == 2 || argc == 3) {
        return yk_get_video(argv[1]);
    } else {
        yk_print_usage(argv[0]);
        return -1;
    }
}

