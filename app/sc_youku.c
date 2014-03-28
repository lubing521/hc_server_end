#include "common.h"
#include "sc_header.h"
#include "net_util.h"
#include "yk_lib.h"

int sc_url_is_yk(char *url)
{
    if (strstr(url, "youku") != NULL) {
        return 1;
    } else {
        return 0;
    }
}

int sc_get_yk_video(char *url, sc_res_info_t *origin)
{
    char yk_url[BUFFER_LEN];                /* Youku video public URL */
    char pl_url[BUFFER_LEN];                /* getplaylist URL */
    char fp_url[BUFFER_LEN];                /* getflvpath URL */
    char real_url[BUFFER_LEN];              /* Youku video file's real URL */
    char *response = NULL;
    int i, j;
    int err = 0, status, ret;
    yk_stream_info_t *streams[STREAM_TYPE_TOTAL] = {NULL}, *strm;
    sc_res_info_t *parsed;

    if (origin == NULL) {
        fprintf(stderr, "%s need origin URL to parse real URL\n", __func__);
        return -1;
    }

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
        fprintf(stderr, "%s ERROR: invalid URL of Youku video: %s\n", __func__, yk_url);
        return -1;
    }

    strtok(yk_url, "?");    /* zhaoyao XXX: always truncating parameters. */

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

    /* zhaoyao XXX TODO: now we only care about the first type stream */
    for (i = 0; i < 1 && streams[i] != NULL; i++) {
        strm = streams[i];

        if (strm->segs == NULL) {   /* Has no segments info, innormal situation */
            fprintf(stderr, "WARNING: stream %s has no segs\n", strm->type);
            continue;
        }
#if DEBUG
        printf("Stream type: %s\n", strm->type);
#endif

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
#if DEBUG
                printf("   Segment %-2d URL: %s\n", strm->segs[j]->no, real_url);
#endif
                /*
                 * Step 3 - using real URL to download.
                 */
                /* zhaoyao XXX TODO: need remembering segments count in origin */
                ret = sc_res_info_add_parsed(sc_res_info_list, origin, real_url, &parsed);
                if (ret != 0) {
                    fprintf(stderr, "%s ERROR: add real_url\n\t%s\nto resource list failed, give up downloading...\n",
                                        __func__, real_url);
                    /*
                     * zhaoyao XXX: we can not using Nginx to download, without ri we can not track
                     *              downloaded resources.
                     */
                    continue;
                }
                ret = sc_ngx_download(NULL, real_url);
                if (ret < 0) {
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

