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

int sc_url_is_sohu_file_url(char *url)
{
    if (strstr(url, "?file=/") != NULL) {
        return 1;
    } else {
        return 0;
    }
}

int sc_sohu_file_url_to_local_path(char *file_url, char *local_path, int len)
{
    char *p, *q;
    int first_slash = 1;

    if (file_url == NULL || local_path == NULL) {
        return -1;
    }

    if (!sc_url_is_sohu_file_url(file_url)) {
        return -1;
    }

    if (len <= strlen(file_url)) {
        return -1;
    }

    for (p = file_url, q = local_path; *p != '\0'; p++, q++) {
        if (first_slash && *p == '.') {
            *q = '_';
            continue;
        }
        if (*p == '/') {
            if (first_slash) {
                *q = *p;
                first_slash = 0;
            } else {
                *q = '_';
            }
            continue;
        }
        if (*p == '?') {
            *q = '_';
            continue;
        }
        *q = *p;
    }
    *q = '\0';

    return 0;
}

/*
 * hot.vrs.sohu.com/ipad1683703_4507722770245_4894024.m3u8?plat=0
 * ipad1683703_4507722770245_4894024.m3u8
 * 220.181.61.212/ipad?file=/233/180/zDIDW3pmePf3wxIGQKHut1.mp4
 * 118.123.211.22/sohu/ts/zC1LzEtlslvesEAgo6oGTGwUoKV7-wvwkGqy4akxs6q3jwvZSSMPX9xuTlmyqKsGXf5dXpAlsaiLzx?key=q3Zw9eUXpP3vaPi5gz4fmsYkuFctH8VFwXJ9BA..&r=OZ-AtpRgTC-1XfwNXlVLTlvAHdi64FVbtf5Lz3-A4pAdhlibsZoVo6Li&n=1&a=2001&cip=125.69.90.35&nid=299
 */
static int sc_get_sohu_video_m3u8(sc_res_info_origin_t *origin)
{
    int err = 0, status, ret;
    char m3u8_url[SC_RES_URL_MAX_LEN];
    char file_url[SC_RES_URL_MAX_LEN];
    char local_path[SC_RES_URL_MAX_LEN];
    char real_url[SC_RES_URL_MAX_LEN];
    sc_res_video_t vtype;
    sc_res_info_active_t *parsed;
    char *response = NULL, *curr;
    char response2[BUFFER_LEN];

    /* zhaoyao: do not care para in m3u8 way */
    sc_res_copy_url(m3u8_url, origin->common.url, SC_RES_URL_MAX_LEN, 0);

    response = malloc(RESP_BUF_LEN);
    if (response == NULL) {
        fprintf(stderr, "%s ERROR: allocate response buffer failed: %s\n", __func__, strerror(errno));
        err = -1;
        goto out;
    }

    if (sohu_http_session(m3u8_url, response, RESP_BUF_LEN) < 0) {
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
//        fprintf(stdout, "%s get response success:\n***************\n%s\n***************\n", __func__, response);
    } else {
        fprintf(stderr, "%s ERROR: m3u8 response status code %d:\n%s", __func__, status, response);
        err = -1;
        goto out;
    }

    for (curr = response; curr != NULL; ) {
        bzero(file_url, SC_RES_URL_MAX_LEN);
        curr = sohu_parse_m3u8_response(curr, file_url);

        /* zhaoyao XXX: file_url MUST end with ".flv" or ".mp4" */
        vtype = sc_res_video_type_obtain(file_url);
        if (!sc_res_video_type_is_valid(vtype)) {
            fprintf(stderr, "%s WARNING: %s type not support\n", __func__, file_url);
            continue;
        }

        bzero(local_path, SC_RES_URL_MAX_LEN);
        ret = sc_sohu_file_url_to_local_path(file_url, local_path, SC_RES_URL_MAX_LEN);
        if (ret != 0) {
            fprintf(stderr, "%s ERROR: sc_sohu_file_url_to_local_path failed, file_url:\n\t%s\n",
                                __func__, file_url);
            continue;
        }

        if (sohu_http_session(file_url, response2, BUFFER_LEN) < 0) {
            fprintf(stderr, "%s ERROR: sohu_http_session faild, URL: %s\n", __func__, file_url);
            continue;
        }

        if (http_parse_status_line(response2, &status) < 0) {
            fprintf(stderr, "%s ERROR: parse status line failed:\n%s", __func__, response2);
            continue;
        }

        if (status == 301) {
//            fprintf(stdout, "%s get response success:\n%s", __func__, response);
        } else {
            fprintf(stderr, "%s ERROR: file_url response status code %d:\n%s", __func__, status, response2);
            continue;
        }

        bzero(real_url, SC_RES_URL_MAX_LEN);
        ret = sohu_parse_file_url_response(response2, real_url);
        if (ret != 0) {
            fprintf(stderr, "%s ERROR: parse real_url failed, response is\n%s", __func__, response2);
            continue;
        }

#if DEBUG
        /*
         * zhaoyao XXX: using file_url to create parsed ri, and local_path to store file.
         * file_url:   220.181.61.212/ipad?file=/109/193/XKUNcCADy8eM9ypkrIfhU4.mp4
         * local_path: 220.181.61.212/ipad_file=_109_193_XKUNcCADy8eM9ypkrIfhU4.mp4
         * real_url:   118.123.211.11/sohu/ts/zC1LzEwlslvesEAgoE.........
         */
        fprintf(stdout, "%s file_url: %s\n", __func__, file_url);
        fprintf(stdout, "%s local_path: %s\n", __func__, local_path);
        fprintf(stdout, "%s real_url: %s\n", __func__, real_url);
#endif

        /* zhaoyao XXX TODO: need remembering segments count in origin */
        ret = sc_res_info_add_parsed(sc_res_info_list, origin, vtype, file_url, &parsed);
        if (ret != 0) {
            fprintf(stderr, "%s ERROR: add file_url\n\t%s\nto resource list failed, give up downloading...\n",
                                __func__, file_url);
            /*
             * zhaoyao XXX: we can not using Nginx to download, without ri we can not track
             *              downloaded resources.
             */
            continue;
        }

        ret = sc_ngx_download(real_url, local_path);
        if (ret < 0) {
            /* zhaoyao XXX TODO FIXME: paresd ri has added succesfully, we should make sure Nginx to download */
            fprintf(stderr, "%s ERROR: url %s inform Nginx failed\n", __func__, real_url);
        }

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

