#include "common.h"
#include "sc_header.h"
#include "net_util.h"
#include "yk_lib.h"

static char sc_yk_origin_url_pattern[] = "v.youku.com/v_show/id_%s.html";

/* zhaoyao XXX: sc's private simple check */
int sc_url_is_yk(char *url)
{
    if (strstr(url, "youku") != NULL) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * ri->url: 101.226.245.141/youku/657F3DA0E044A84268416F5901/030008120C5315B40E04A805CF07DDC55D635C-F0F8-8F9D-F095-A049DF9C59DA.mp4
 * local_path: 101_226_245_141/youku_657F3DA0E044A84268416F5901_030008120C5315B40E04A805CF07DDC55D635C-F0F8-8F9D-F095-A049DF9C59DA.mp4
 */
int sc_yk_url_to_local_path(char *url, char *local_path, int len)
{
    return sc_res_url_to_local_path_default(url, local_path, len);
}

int sc_yk_gen_origin_url(char *req_url, char *origin_url)
{
    char buf[16];
    char *tag[] = { "v_show/id_",
                    "&video_id=",
                    "&id=",
                    "&vid=",
                    "&local_vid=",
                    NULL };
    char *start = NULL;
    int i, len = 0;

    if (req_url == NULL || origin_url == NULL) {
        return -1;
    }

    for (i = 0; tag[i] != NULL; i++) {
        start = strstr(req_url, tag[i]);
        if (start == NULL) {
            continue;
        }
        start = start + strlen(tag[i]);
        for (len = 0; isalnum(start[len]); len++) {
            ;
        }
        goto generate;
    }

error:
    return -1;

generate:
    if (len != 13) {
        goto error;
    }
    bzero(buf, sizeof(buf));
    strncpy(buf, start, len);
    sprintf(origin_url, sc_yk_origin_url_pattern, buf);

    return 0;
}


static int sc_get_yk_download_video_type(yk_stream_info_t *streams[])
{
    int i, ret = -1;

    if (streams == NULL) {
        return -1;
    }

    for (i = 0; i < STREAM_TYPE_TOTAL && streams[i] != NULL; i++) {
        if (strncmp(streams[i]->type, VIDEO_FLV_SUFFIX, VIDEO_FLV_SUFFIX_LEN) == 0) {
            ret = i;
        }
        if (strncmp(streams[i]->type, VIDEO_MP4_SUFFIX, VIDEO_MP4_SUFFIX_LEN) == 0) {
            ret = i;
            /* zhaoyao: .mp4 type video has the highest priority */
            goto out;
        }
    }

out:
    return ret;
}

/*
 * Full path: .html -> getPlaylist -> getFlvpath -> real_url
 */
static int sc_get_yk_video_tradition(sc_res_info_origin_t *origin)
{
    char yk_url[BUFFER_LEN];                /* Youku video public URL */
    char pl_url[BUFFER_LEN];                /* getplaylist URL */
    char fp_url[BUFFER_LEN];                /* getflvpath URL */
    char real_url[BUFFER_LEN];              /* Youku video file's real URL */
    char *response = NULL;
    int i, j;
    int err = 0, status, ret;
    yk_stream_info_t *streams[STREAM_TYPE_TOTAL] = {NULL}, *strm;
    sc_res_info_active_t *parsed;
    int download_index;

    sc_res_copy_url(yk_url, origin->common.url, BUFFER_LEN, 0); /* zhaoyao: do not care para in traditional way */

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

    download_index = sc_get_yk_download_video_type(streams);
    if (download_index < 0 && download_index >= STREAM_TYPE_TOTAL) {
        fprintf(stderr, "%s ERROR: sc_get_yk_download_video_type %d is invalid index\n",
                            __func__, download_index);
        err = -1;
        goto out;
    }

    /* zhaoyao XXX: now we only care about the download_index type */
    for (i = download_index; i < download_index + 1; i++) {
        strm = streams[i];

        if (strm->segs == NULL) {   /* Has no segments info, innormal situation */
            fprintf(stderr, "WARNING: stream %s has no segs\n", strm->type);
            err = -1;
            goto out;   /* zhaoyao XXX: since we only download_index, any fail here should ERROR */
            //continue;
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
                /*
                 * zhaoyao XXX TODO: should not call this directly, generate local path should be
                 *                   done in sc_res_info_add_parsed.
                 */
                ret = sc_res_info_gen_active_local_path(parsed);
                if (ret != 0) {
                    fprintf(stderr, "%s ERROR: generate local_path failed, url %s\n", __func__, real_url);
                    err = -1;
                    goto out;
                }
                ret = sc_ngx_download(real_url, parsed->localpath);
                if (ret < 0) {
                    /* zhaoyao XXX TODO FIXME: paresd ri has added succesfully, we should make sure Nginx to download */
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

int sc_get_yk_video(sc_res_info_origin_t *origin)
{
    int ret;

    if (origin == NULL) {
        fprintf(stderr, "%s need origin URL to parse real URL\n", __func__);
        return -1;
    }

    if (yk_is_tradition_url(origin->common.url)) {
        ret = sc_get_yk_video_tradition(origin);
    } else {
        fprintf(stderr, "%s ERROR: no support: %s\n", __func__, origin->common.url);
        ret = -1;
    }

    return ret;
}

