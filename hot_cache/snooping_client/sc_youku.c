/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * sc_youku.c
 * Original Author: zhaoyao@ruijie.com.cn, 2014-04-28
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

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

/* zhaoyao XXX: sc's private simple check */
int sc_yk_is_local_path(char *local_path)
{
    if (strstr(local_path, "youku_") != NULL) {
        if (strstr(local_path, "valf") != NULL) {       /* zhaoyao XXX: 广告暂不考虑 */
            return 0;
        }
        if (strstr(local_path, "ad_api_") != NULL) {    /* zhaoyao XXX: 广告暂不考虑 */
            return 0;
        }

        return 1;
    } else {
        return 0;
    }
}

/* zhaoyao XXX: sc's private simple check */
int sc_yk_is_local_path_pure_vid(char *local_path)
{
    int len, slash_num = 0;
    char *vid, *p, *uri_start;

    if (strstr(local_path, SC_NGX_ROOT_PATH) != NULL) {
        uri_start = local_path + SC_NGX_ROOT_PATH_LEN;
    } else {
        uri_start = local_path;
    }

    len = strlen(uri_start);
    if (len < 70 + 8) {
        return 0;
    }

    for (p = uri_start + len -1; *p != '/' && p >= uri_start; p--) {
        ;
    }
    if (*p != '/') {
        return 0;
    }

    vid = p + 1;
    len = strlen(vid);
    if (len != 70) {
        return 0;
    }

    for (p = uri_start; p < vid; p++) {
        if (*p == '/') {
            slash_num++;
        }
    }
    if (slash_num != 1) {
        return 0;
    }

    return 1;
}

int sc_yk_trans_vid_to_std_path(char *vid_path, char *std_path, unsigned int path_len)
{
    char *uri_start, *vid, *p;
    char *youku_tag = "youku_67731242A093F822806A633329_";

    if (vid_path == NULL || std_path == NULL) {
        hc_log_error("invalid input");
        return -1;
    }

    if (!sc_yk_is_local_path_pure_vid(vid_path)) {
        hc_log_error("invalid vid_path: %s", vid_path);
        return -1;
    }

    if (strlen(vid_path) + strlen(youku_tag) >= path_len) {
        hc_log_error("allow len %u, vid_path: %s", path_len, vid_path);
        return -1;
    }

    if (strstr(vid_path, SC_NGX_ROOT_PATH) != NULL) {
        uri_start = vid_path + SC_NGX_ROOT_PATH_LEN;
        strcat(std_path, SC_NGX_ROOT_PATH);
        p = std_path + SC_NGX_ROOT_PATH_LEN;
    } else {
        uri_start = vid_path;
        p = std_path;
    }

    vid = strchr(uri_start, '/') + 1;
    strncpy(p, uri_start, (unsigned long)vid - (unsigned long)uri_start);
    strcat(std_path, youku_tag);
    strcat(std_path, vid);

    return 0;
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
again:
            continue;
        }
        start = start + strlen(tag[i]);
        for (len = 0; isalnum(start[len]); len++) {
            ;
        }
        goto generate;
    }

    return -1;

generate:
    if (len != 13) {
        goto again;
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

static hc_result_t sc_yk_handle_cached(sc_res_info_ctnt_t *parsed)
{
    sc_res_info_mgmt_t *yk_ctrl_ld;
    hc_result_t ret;

    if (parsed == NULL) {
        hc_log_error("invalid input");
        return HC_ERR_INVALID;
    }

    yk_ctrl_ld = sc_ld_obtain_ctrl_ld_youku();
    if (yk_ctrl_ld == NULL) {
        hc_log_error("miss youku ctrl_ld");
        return HC_ERR_INTERNAL;
    }

    ret = sc_res_info_handle_cached(yk_ctrl_ld, parsed);
    if (ret != HC_SUCCESS) {
        hc_log_error("failed");
    }

    return ret;
}

int sc_youku_download(sc_res_info_ctnt_t *parsed)
{
    int ret;

    if (parsed == NULL) {
        return -1;
    }

    ret = sc_ngx_download(parsed->common.url, parsed->localpath);

    if (ret == HC_ERR_EXISTS) {
        ret = sc_yk_handle_cached(parsed);
    }
    
    if (ret != 0) {
        sc_res_flag_set_i_fail(&parsed->common);
    }

    return ret;
}

/*
 * Full path: .html -> getPlaylist -> getFlvpath -> real_url
 */
static int sc_get_yk_video_tradition(sc_res_info_mgmt_t *origin)
{
    char yk_url[HTTP_URL_MAX_LEN];                /* Youku video public URL */
    char pl_url[HTTP_URL_MAX_LEN];                /* getplaylist URL */
    char fp_url[HTTP_URL_MAX_LEN];                /* getflvpath URL */
    char real_url[HTTP_URL_MAX_LEN];              /* Youku video file's real URL */
    char *response = NULL;
    int i, j;
    int err = 0, status, ret;
    yk_stream_info_t *streams[STREAM_TYPE_TOTAL] = {NULL}, *strm;
    sc_res_info_ctnt_t *parsed;
    int download_index;

    sc_res_copy_url(yk_url, origin->common.url, HTTP_URL_MAX_LEN, 0); /* zhaoyao: do not care para in traditional way */

    response = malloc(RESP_BUF_LEN);
    if (response == NULL) {
        perror("Malloc failed");
        err = -1;
        goto out;
    }

    /*
     * Step 1 - getPlaylist and get flvpath URL.
     */
    memset(pl_url, 0, HTTP_URL_MAX_LEN);
    if (yk_url_to_playlist(yk_url, pl_url) != true) {
        hc_log_error("yk_url_to_playlist failed, url is:\n%s", yk_url);
        err = -1;
        goto out;
    }

    if (yk_http_session(pl_url, yk_url, response, RESP_BUF_LEN) < 0) {
        hc_log_error("yk_http_session faild, URL: %s\n", pl_url);
        err = -1;
        goto out;
    }

    if (http_parse_status_line(response, strlen(response), &status) < 0) {
        hc_log_error("Parse status line failed:\n%s", response);
        err = -1;
        goto out;
    }

    if (status == 200) {
//        hc_log_info("getPlaylist success!!! Response status code %d\n%s\n", status, response);
    } else {
        hc_log_error("getPlaylist's response status code %d:%s", status, response);
        err = -1;
        goto out;
    }

    memset(streams, 0, sizeof(streams));
    if (yk_parse_playlist(response, streams) != 0) {
        hc_log_error("Parse getPlaylist response failed:\n%s\n", response);
        err = -1;
        goto out;
    } else {
        //hc_log_info("Parse getPlaylist response success");
        //yk_debug_streams_all(streams);
    }

    download_index = sc_get_yk_download_video_type(streams);
    if (download_index < 0 && download_index >= STREAM_TYPE_TOTAL) {
        hc_log_error("sc_get_yk_download_video_type %d is invalid index\n", download_index);
        err = -1;
        goto out;
    }

    /* zhaoyao XXX: now we only care about the download_index type */
    for (i = download_index; i < download_index + 1; i++) {
        strm = streams[i];

        if (strm->segs == NULL) {   /* Has no segments info, innormal situation */
            hc_log_error("WARNING: stream %s has no segs\n", strm->type);
            err = -1;
            goto out;   /* zhaoyao XXX: since we only download_index, any fail here should ERROR */
            //continue;
        }
#if DEBUG
        hc_log_debug("Stream type: %s", strm->type);
#endif

        for (j = 0; j < STREAM_SEGS_MAX && strm->segs[j] != NULL; j++) {
            /*
             * Step 2 - getFlvpath and get real URL.
             */
            if (yk_seg_to_flvpath(strm->segs[j], fp_url) < 0) {
                hc_log_error("WARNING: yk_seg_to_flvpath failed\n");
                continue;
            }

            if (yk_http_session(fp_url, yk_url, response, RESP_BUF_LEN) < 0) {
                hc_log_error("yk_http_session faild, URL: %s\n", fp_url);
                err = -1;
                goto out;
            }

            if (http_parse_status_line(response, strlen(response), &status) < 0) {
                hc_log_error("Parse status line failed:\n%s", response);
                err = -1;
                goto out;
            }

            if (status == 200 || status == 302) {
                if (yk_parse_flvpath(response, real_url) < 0) {
                    hc_log_error("Parse getFlvpath response and get real URL failed\n");
                    err = -1;
                    goto out;
                }
#if DEBUG
                hc_log_debug("   Segment %-2d URL: %s", strm->segs[j]->no, real_url);
#endif
                /*
                 * Step 3 - using real URL to download.
                 */
                ret = sc_res_info_add_ctnt(sc_res_info_list, origin, real_url, &parsed);
                if (ret != 0) {
                    hc_log_error("add real_url\n\t%s\nto resource list failed, give up downloading...",
                                        real_url);
                    /*
                     * zhaoyao XXX: we can not using Nginx to download, without ri we can not track
                     *              downloaded resources.
                     */
                    continue;
                }
                ret = sc_youku_download(parsed);
                if (ret < 0) {
                    hc_log_error("   Segment %-2d inform Nginx failed\n", strm->segs[j]->no);
                }
            } else {
                hc_log_error("getFlvpath failed, status code %d:\n%s\n", status, response);
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

int sc_get_yk_video(sc_res_info_mgmt_t *origin)
{
    int ret;

    if (origin == NULL) {
        hc_log_error("need origin URL to parse real URL");
        return -1;
    }

    if (yk_is_tradition_url(origin->common.url)) {
        ret = sc_get_yk_video_tradition(origin);
    } else {
        hc_log_error("no support: %s", origin->common.url);
        ret = -1;
    }

    return ret;
}

static int sc_yk_add_symlink(sc_res_info_t *ri)
{
    int ret = 0;
    char shadow_url[HTTP_URL_MAX_LEN];
    char shadow_path[SC_RES_LOCAL_PATH_MAX_LEN];
    char *p, *q;

    if (ri == NULL) {
        return -1;
    }

    return ret;

    bzero(shadow_url, HTTP_URL_MAX_LEN);
    bzero(shadow_path, SC_RES_LOCAL_PATH_MAX_LEN);
    if (sc_url_is_yk(ri->url)) {
        for (p = ri->url, q = shadow_url; *p != '/'; p++, q++) {
            *q = *p;
        }
        for (p = p + 1; *p != '/'; p++) {
            ;
        }
        for (; *p != '\0'; p++, q++) {
            *q = *p;
        }
    } else {
        for (p = ri->url, q = shadow_url; *p != '/'; p++, q++) {
            /*zhaoyao TODO XXX: 临时延缓添加两条优酷url的工作*/
            ;
        }
    }

    /* zhaoyao TODO XXX: 创建软连接，添加url给Snooping模块 */

    return 0;
}

int sc_yk_add_ctnt_url(sc_res_info_ctnt_t *ctnt)
{
    int ret;
    sc_res_info_t *ri;

    if (ctnt == NULL) {
        return -1;
    }
    ri = &ctnt->common;
    if (!sc_res_site_is_youku(ri)) {
        return -1;
    }

    ret = sc_yk_add_symlink(ri);
    if (ret != 0) {
        hc_log_error("add symbol link of cached Youku video file failed: %s", ri->url);
    }

    ret = sc_res_notify_ri_url(ri);

    return ret;
}

int sc_yk_get_vf(char *vf_url, char *referer)
{
    int ret, status;
    char resp[RESP_BUF_LEN], resp2[BUFFER_LEN];
    char fp_url[HTTP_URL_MAX_LEN], *curr;
    char real_url[HTTP_URL_MAX_LEN];
    char vf_no_para_url[HTTP_URL_MAX_LEN];
    char vf_local_path[SC_RES_LOCAL_PATH_MAX_LEN];
    char *p, *q, *body;

    FILE *fp;
    int resp_len, body_len, wlen;

    if (vf_url == NULL || referer == NULL) {
        hc_log_error("input invalid");
        return -1;
    }

    bzero(vf_no_para_url, HTTP_URL_MAX_LEN);
    sc_res_copy_url(vf_no_para_url, vf_url, HTTP_URL_MAX_LEN, 0);

    ret = sc_snooping_do_del(-1, vf_no_para_url);
    if (ret != 0) {
        hc_log_error("WARNING: delete url in snooping failed: %s", vf_no_para_url);
    }

    bzero(resp, RESP_BUF_LEN);
    ret = yk_http_session(vf_url, referer, resp, RESP_BUF_LEN);
    if (ret != 0) {
        hc_log_error("url %s, referer %s, http session failed", vf_url, referer);
        return -1;
    }

    resp_len = strlen(resp);
    if (http_parse_status_line(resp, resp_len, &status) < 0) {
        hc_log_error("parse status line failed:\n%s", resp);
        return -1;
    }

    if (status == 200) {
    } else {
        hc_log_error("status %d failed:\n%s", status, resp);
        return -1;
    }

    body = strstr(resp, "{\"N\":"); /* zhaoyao XXX:去掉http头部，取到响应体 */
    if (body == NULL) {
        hc_log_error("invalid response: %s", resp);
        return -1;
    }
    body_len = strlen(body);

    bzero(vf_local_path, sizeof(vf_local_path));
    strcat(vf_local_path, SC_NGX_ROOT_PATH);
    for (p = vf_no_para_url, q = vf_local_path + SC_NGX_ROOT_PATH_LEN; *p != '\0'; p++, q++) {
        if (*p == '.') {
            *q = '_';
        } else {
            *q = *p;
        }
    }
#if DEBUG
    hc_log_debug("vf local path: %s", vf_local_path);
#endif
    fp = fopen(vf_local_path, "w");
    if (fp == NULL) {
        hc_log_error("open vf local file failed");
        return -1;
    }
    /* zhaoyao TODO XXX:代码写的很简陋，需改进 */
    wlen = fwrite(body, 1, body_len, fp);
    if (wlen < body_len) {
        hc_log_error("write %d, but vf is %d", wlen, body_len);
    }
    if (fclose(fp) == EOF) {
        hc_log_error("close %s failed", vf_local_path);
    }

    ret = util_json_to_ascii_string(resp, resp_len);
    if (ret != 0) {
        hc_log_error("transfer json to ascii failed");
        return -1;
    }
    resp_len = strlen(resp);    /* zhaoyao XXX:由于转码，更新响应的长度 */

    for (curr = resp; curr != NULL; ) {
        bzero(fp_url, HTTP_URL_MAX_LEN);
        curr = yk_parse_vf_response(curr, fp_url);

        /*
         * zhaoyao TODO:在后面的操作中，即使错误也继续，这需要改进。
         */

        bzero(resp2, BUFFER_LEN);
        if (yk_http_session(fp_url, referer, resp2, BUFFER_LEN) < 0) {
            hc_log_error("yk_http_session faild, URL: %s", fp_url);
            continue;
        }

        if (http_parse_status_line(resp2, strlen(resp2), &status) < 0) {
            hc_log_error("parse status line failed:\n%s", resp2);
            continue;
        }

        if (status != 200 && status != 302) {
            hc_log_error("server return %d", status);
            continue;
        }

        if (yk_parse_flvpath(resp2, real_url) != 0) {
            hc_log_error("parse getFlvpath response failed: %s", resp2);
            continue;
        }
#if DEBUG
        hc_log_debug("real_url: %120s", real_url);
#endif
        ret = sc_snooping_do_add(-1, real_url);
        if (ret != 0) {
            hc_log_error("add advertisement url to snooping failed");
            continue;
        }
    }

    ret = sc_snooping_do_add(-1, vf_no_para_url);
    if (ret != 0) {
        hc_log_error("add vf url to snooping failed");
        return -1;
    }

    return 0;
}

int sc_yk_init_vf_adv()
{
    int ret;
    char *vf_url = "valf.atm.youku.com/vf?vip=0&site=1&rt=MHwxMzk3NDQ0MzA3ODI1fFhOamd5TURrNU1qTXk=&p=1&vl=5272&fu=0&ct=c&cs=2034&paid=0&s=177092&td=0&sid=739744430145310e7d997&v=170524808&wintype=interior&u=97454045&vs=1.0&rst=flv&partnerid=null&dq=mp4&k=%u6536%u8D39&os=Windows%207&d=0&ti=%E5%A4%A7%E8%AF%9D%E5%A4%A9%E4%BB%99";
    char *referer = "http://v.youku.com/v_show/id_XNjgyMDk5MjMy.html";

    ret = sc_yk_get_vf(vf_url, referer);
    if (ret != 0) {
        hc_log_error("get Youku vf file failed");
        return -1;
    }

    return 0;
}

