#include "common.h"
#include "sc_header.h"
#include "net_util.h"
#include "sohu_lib.h"

/* zhaoyao XXX: sc's private simple check */
int sc_url_is_sohu(char *url)
{
    if (strstr(url, "sohu") != NULL) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * hot.vrs.sohu.com/ipad1683703_4507722770245_4894024.m3u8?plat=0
 * ipad1683703_4507722770245_4894024.m3u8
 * real_url
 */
static int sc_get_sohu_video_m3u8(sc_res_info_origin_t *origin)
{
    int err = 0, status;
    char m3u8_url[SC_RES_URL_MAX_LEN], real_url[SC_RES_URL_MAX_LEN];
    sc_res_video_t vtype;
//    sc_res_info_active_t *parsed;
    char *response = NULL, *curr;

    /* zhaoyao: do not care para in m3u8 way */
    sc_res_copy_url(m3u8_url, origin->common.url, SC_RES_URL_MAX_LEN, 0);

    response = malloc(RESP_BUF_LEN);
    if (response == NULL) {
        fprintf(stderr, "%s ERROR: allocate response buffer failed: %s\n", __func__, strerror(errno));
        err = -1;
        goto out;
    }

    if (sohu_http_session(m3u8_url, response) < 0) {
        fprintf(stderr, "sohu_http_session faild, URL: %s\n", m3u8_url);
        err = -1;
        goto out;
    }

    if (http_parse_status_line(response, &status) < 0) {
        fprintf(stderr, "%s ERROR: parse status line failed:\n%s", __func__, response);
        err = -1;
        goto out;
    }

    if (status == 200) {
//        fprintf(stdout, "%s get response success:\n%s", __func__, response);
    } else {
        fprintf(stderr, "%s ERROR: m3u8 response status code %d:\n%s", __func__, status, response);
        err = -1;
        goto out;
    }

    for (curr = response; curr != NULL; ) {
        bzero(real_url, SC_RES_URL_MAX_LEN);
        curr = sohu_parse_m3u8_response(curr, real_url);

        /* zhaoyao XXX: real_url MUST end with ".flv" or ".mp4" */
        vtype = sc_res_video_type_obtain(real_url);
        if (!sc_res_video_type_is_valid(vtype)) {
            fprintf(stderr, "%s WARNING: %s type not support\n", __func__, real_url);
            continue;
        }

        fprintf(stdout, "%s real_url: %s\n", __func__, real_url);

#if 0
        /* zhaoyao XXX TODO: need remembering segments count in origin */
        ret = sc_res_info_add_parsed(sc_res_info_list, origin, vtype, real_url, &parsed);
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
            /* zhaoyao XXX TODO FIXME: paresd ri has added succesfully, we should make sure Nginx to download */
            fprintf(stderr, "%s ERROR: url %s inform Nginx failed\n", __func__, real_url);
        }
#endif
    }

out:
    free(response);

    return err;
}

int sc_get_sohu_video(sc_res_info_origin_t *origin)
{
    int ret = 0;

    if (origin == NULL) {
        fprintf(stderr, "%s need origin URL to parse real URL\n", __func__);
        return -1;
    }

    if (sohu_is_m3u8_url(origin->common.url)) {
        ret = sc_get_sohu_video_m3u8(origin);
    } else {
        fprintf(stderr, "%s ERROR: no support: %s\n", __func__, origin->common.url);
        ret = -1;
    }

    return ret;
}

