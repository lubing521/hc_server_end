/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * sc_resource.c
 * Original Author: zhaoyao@ruijie.com.cn, 2014-04-28
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#include "common.h"
#include "sc_header.h"
#include "sc_resource.h"

sc_res_list_t *sc_res_info_list = NULL;
int sc_res_share_mem_shmid = -1;

/*
 * zhaoyao XXX: mime type suffix MUST be at the end of str.
 */
static sc_res_mime_t sc_res_mime_type_obtain(char *str, int care_type)
{
    char *p;
    int len;

    if (!care_type) {
        return SC_RES_MIME_NOT_CARE;
    }

    if (str == NULL) {
        hc_log_error("Invalid input");
        return SC_RES_MIME_TYPE_MAX;
    }

    len = strlen(str);
    if (len < 3) {
        hc_log_error("Invalid input: %s", str);
        return SC_RES_MIME_TYPE_MAX;
    }

    len = len - 3;
    p = str + len;
    if (strncmp(p, VIDEO_FLV_SUFFIX, VIDEO_FLV_SUFFIX_LEN) == 0) {
        return SC_RES_MIME_VIDEO_FLV;
    }
    if (strncmp(p, VIDEO_MP4_SUFFIX, VIDEO_MP4_SUFFIX_LEN) == 0) {
        return SC_RES_MIME_VIDEO_MP4;
    }

    len = strlen(str);
    len = len - 4;
    p = str + len;
    if (strncmp(p, TEXT_HTML_SUFFIX, TEXT_HTML_SUFFIX_LEN) == 0) {
        return SC_RES_MIME_TEXT_HTML;
    }
    if (strncmp(p, TEXT_M3U8_SUFFIX, TEXT_M3U8_SUFFIX_LEN) == 0) {
        return SC_RES_MIME_TEXT_M3U8;
    }

    hc_log_error("Unknown mime type: %s", str);

    return SC_RES_MIME_TYPE_MAX;
}

/*
 * zhaoyao XXX: not copy "http://", omitting parameter in o_url or not, is depending on with_para.
 */
void sc_res_copy_url(char *url, char *o_url, unsigned int len, char with_para)
{
    char *start = o_url, *p, *q;

    if (url == NULL || o_url == NULL) {
        return;
    }

    if (strncmp(start, HTTP_URL_PREFIX, HTTP_URL_PRE_LEN) == 0) {
        start = start + HTTP_URL_PRE_LEN;
    }

    if (with_para) {
        for (p = url, q = start; *q != '\0'; p++, q++) {
            *p = *q;
        }
    } else {
        for (p = url, q = start; *q != '?' && *q != '\0'; p++, q++) {
            *p = *q;
        }
    }

    if (url + len < p) {
        hc_log_error("Overflow, buffer len %u, but copied %u", len, (unsigned int)p - (unsigned int)url);
    }
}

static int sc_res_list_alloc_share_mem(sc_res_list_t **prl)
{
    int mem_size;
    void *shmptr;
    int shmid;

    if (prl == NULL) {
        return -1;
    }

    mem_size = SC_RES_SHARE_MEM_SIZE;
    if ((shmid = shmget(SC_RES_SHARE_MEM_ID, mem_size, SC_RES_SHARE_MEM_MODE | IPC_CREAT)) < 0) {
        hc_log_error("shmget failed, memory size %d: %s", mem_size, strerror(errno));
        return -1;
    }

    if ((shmptr = shmat(shmid, 0, 0)) == (void *)-1) {
        hc_log_error("shmat failed: %s", strerror(errno));
        sc_res_list_destroy_and_uninit(shmid);
        return -1;
    }
    memset(shmptr, 0, mem_size);

    *prl = shmptr;

    return shmid;
}

int sc_res_list_alloc_and_init(sc_res_list_t **prl)
{
    sc_res_list_t *rl;
    int ret, i;

    if (prl == NULL) {
        return -1;
    }

    ret = sc_res_list_alloc_share_mem(&rl);
    if (ret < 0) {
        hc_log_error("allocate failed");
        return -1;
    }

    rl->mgmt[0].common.id = (unsigned long)INVALID_PTR;
    for (i = 1; i < SC_RES_INFO_NUM_MAX_MGMT; i++) {
        rl->mgmt[i].common.id = (unsigned long)(&(rl->mgmt[i - 1]));
    }
    rl->mgmt_free = (&(rl->mgmt[i - 1]));

    rl->ctnt[0].common.id = (unsigned long)INVALID_PTR;
    for (i = 1; i < SC_RES_INFO_NUM_MAX_CTNT; i++) {
        rl->ctnt[i].common.id = (unsigned long)(&(rl->ctnt[i - 1]));
    }
    rl->ctnt_free = (&(rl->ctnt[i - 1]));

    *prl = rl;

    return ret;
}

int sc_res_list_destroy_and_uninit(int shmid)
{
    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
        hc_log_error("shmctl remove %d failed: %s", shmid, strerror(errno));
        return -1;
    }

    sc_res_info_list = NULL;

    return 0;
}

static sc_res_info_mgmt_t *sc_res_info_get_mgmt(sc_res_list_t *rl)
{
    sc_res_info_mgmt_t *mgmt;

    if (rl == NULL) {
        return NULL;
    }

    if (rl->mgmt_free == INVALID_PTR) {
        hc_log_error("resource info totally ran out, already used %d", rl->mgmt_cnt);
        return NULL;
    }

    mgmt = rl->mgmt_free;
    rl->mgmt_free = (sc_res_info_mgmt_t *)(mgmt->common.id);
    rl->mgmt_cnt++;

    memset(mgmt, 0, sizeof(sc_res_info_mgmt_t));
    mgmt->common.id = ((unsigned long)mgmt - (unsigned long)(rl->mgmt)) / sizeof(sc_res_info_mgmt_t);

    return mgmt;
}

static sc_res_info_mgmt_t *sc_res_info_get_mgmt_origin(sc_res_list_t *rl)
{
    sc_res_info_mgmt_t *origin;

    origin = sc_res_info_get_mgmt(rl);

    if (origin != NULL) {
        sc_res_gen_set_origin(&origin->common);
    }

    return origin;
}

static sc_res_info_mgmt_t *sc_res_info_get_mgmt_normal(sc_res_list_t *rl)
{
    sc_res_info_mgmt_t *normal;

    normal = sc_res_info_get_mgmt(rl);

    if (normal != NULL) {
        sc_res_gen_set_normal(&normal->common);
    }

    return normal;
}

static sc_res_info_mgmt_t *sc_res_info_get_mgmt_ctrl_ld(sc_res_list_t *rl)
{
    sc_res_info_mgmt_t *ctrl_ld;

    ctrl_ld = sc_res_info_get_mgmt(rl);

    if (ctrl_ld != NULL) {
        sc_res_gen_set_ctrl_ld(&ctrl_ld->common);
    }

    return ctrl_ld;
}

static sc_res_info_ctnt_t *sc_res_info_get_ctnt(sc_res_list_t *rl)
{
    sc_res_info_ctnt_t *ctnt;

    if (rl == NULL) {
        return NULL;
    }

    if (rl->ctnt_free == INVALID_PTR) {
        hc_log_error("resource info totally ran out, already used %d", rl->ctnt_cnt);
        return NULL;
    }

    ctnt = rl->ctnt_free;
    rl->ctnt_free = (sc_res_info_ctnt_t *)(ctnt->common.id);
    rl->ctnt_cnt++;

    memset(ctnt, 0, sizeof(sc_res_info_ctnt_t));
    ctnt->common.id = ((unsigned long)ctnt - (unsigned long)(rl->ctnt)) / sizeof(sc_res_info_ctnt_t);

    sc_res_gen_set_content(&ctnt->common);

    return ctnt;
}

static void sc_res_info_put(sc_res_list_t *rl, sc_res_info_t *ri)
{
    sc_res_info_mgmt_t *mgmt;
    sc_res_info_ctnt_t *ctnt;

    if (rl == NULL || ri == NULL) {
        return;
    }

    if (sc_res_info_is_mgmt(ri)) {
        if (rl->mgmt_cnt == 0) {
            hc_log_error("rl->mgmt_cnt = 0, can not put");
            return;
        }
        mgmt = (sc_res_info_mgmt_t *)ri;
        memset(mgmt, 0, sizeof(sc_res_info_mgmt_t));
        mgmt->common.id = (unsigned long)(rl->mgmt_free);
        rl->mgmt_free = mgmt;
        rl->mgmt_cnt--;
        return;
    } else if (sc_res_info_is_ctnt(ri)) {
        if (rl->ctnt_cnt == 0) {
            hc_log_error("rl->ctnt_cnt = 0, can not put");
            return;
        }
        ctnt = (sc_res_info_ctnt_t *)ri;
        memset(ctnt, 0, sizeof(sc_res_info_ctnt_t));
        ctnt->common.id = (unsigned long)(rl->ctnt_free);
        rl->ctnt_free = ctnt;
        rl->ctnt_cnt--;
        return;
    } else {
        hc_log_error("unknown type 0x%lx", ri->gen);
        return;
    }
}

/*
 * zhaoyao XXX: 检查url能否添加到资源列表，掌管蓝天门，若通过他的检查，起码该url能被SC识别。
 */
static int sc_res_info_permit_adding(char *url)
{
    if (sc_url_is_yk(url)) {
        return 1;
    }
    if (sc_url_is_sohu(url)) {
        return 1;
    }
    if (sc_url_is_sohu_file_url(url)) {
        return 1;
    }

    hc_log_error("is forbidden, URL: %s", url);

    return 0;
}

int sc_res_url_to_local_path_default(char *url, char *local_path, int len)
{
    char *p, *q;
    int first_slash = 1;

    if (url == NULL || local_path == NULL) {
        return -1;
    }

    if (len <= strlen(url)) {
        return -1;
    }

    for (p = url, q = local_path; *p != '\0' && *p != '?'; p++, q++) {
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
        *q = *p;
    }
    *q = '\0';

    return 0;
}

static int sc_res_info_gen_ctnt_local_path(sc_res_info_ctnt_t *ctnt)
{
    int ret = -1;

    if (ctnt == NULL) {
        return -1;
    }

    bzero(ctnt->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    if (sc_res_site_is_youku(&ctnt->common)) {
        ret = sc_yk_url_to_local_path(ctnt->common.url, ctnt->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    } else if (sc_res_site_is_sohu(&ctnt->common)) {
        ret = sc_sohu_file_url_to_local_path(ctnt->common.url, ctnt->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    } else {
        hc_log_debug("using sc_res_url_to_local_path_default, url %s", ctnt->common.url);
        ret = sc_res_url_to_local_path_default(ctnt->common.url, ctnt->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    }

    return ret;
}

int sc_res_recover_url_from_local_path(char *local_path, char *url)
{
    int lp_len, meet_slash = 0;
    char *p, *q;
    char *sohu_file_tag = "ipad/file=/", *sohu_tag_p;

    if (local_path == NULL || url == NULL) {
        hc_log_error("invalid input");
        return -1;
    }

    lp_len = strlen(local_path);
    if (lp_len >= HTTP_URL_MAX_LEN) {
        hc_log_error("local path %d, longer than limit %d", lp_len, HTTP_URL_MAX_LEN);
        return -1;
    }

    for (p = local_path, q = url; *p != '\0'; p++, q++) {
        if (*p == '/') {
            meet_slash = 1;
            *q = '/';
            continue;
        }
        if (*p == '_') {
            if (!meet_slash) {
                *q = '.';
            } else {
                *q = '/';
            }
            continue;
        }
        *q = *p;
    }
    *q = '\0';

    sohu_tag_p = strstr(url, sohu_file_tag);
    if (sohu_tag_p != NULL) {
        sohu_tag_p[4] = '?';
    }

    return 0;
}

static int sc_res_info_mark_site(sc_res_info_t *ri)
{
    int ret = -1;

    if (ri == NULL) {
        return ret;
    }

    if (!sc_res_info_is_mgmt(ri)) {
        hc_log_error("can not mark non-mgmt ri's site directly");
        return ret;
    }

    if (sc_url_is_yk(ri->url)) {
        sc_res_site_set_youku(ri);
        ret = 0;
    } else if (sc_url_is_sohu(ri->url)) {
        sc_res_site_set_sohu(ri);
        ret = 0;
    } else {
        hc_log_error("unknown site: %s", ri->url);
    }

    return ret;
}

int sc_res_info_add_normal(sc_res_list_t *rl, char *url, sc_res_info_mgmt_t **ptr_ret)
{
    int len, ret;
    sc_res_info_mgmt_t *normal;
    sc_res_mime_t mime_type;

    if (rl == NULL || url == NULL) {
        hc_log_error("invalid input");
        return -1;
    }

    if (!sc_res_info_permit_adding(url)) {
        hc_log_error("unknown url %s", url);
        return -1;
    }

    len = strlen(url);
    if (len >= HTTP_URL_MAX_LEN) {
        hc_log_error("url is longer than MAX_LEN %d", HTTP_URL_MAX_LEN);
        return -1;
    }

    normal = sc_res_info_get_mgmt_normal(rl);
    if (normal == NULL) {
        hc_log_error("get free res_info failed");
        return -1;
    }

    sc_res_copy_url(normal->common.url, url, HTTP_URL_MAX_LEN, 1);
#if DEBUG
    hc_log_debug("copied url with parameter: %s", normal->common.url);
#endif
    /* zhaoyao: mark site should be very early. */
    ret = sc_res_info_mark_site(&normal->common);
    if (ret != 0) {
        hc_log_error("sc_res_info_mark_site failed, url %s", normal->common.url);
        goto error;
    }

    mime_type = sc_res_mime_type_obtain(url, 1);
    sc_res_mime_set_t(&normal->common, mime_type);

    if (ptr_ret != NULL) {
        *ptr_ret = normal;
    }

    return 0;

error:
    sc_res_info_del(sc_res_info_list, (sc_res_info_t *)normal);
    return -1;
}

int sc_res_info_add_origin(sc_res_list_t *rl, char *url, sc_res_info_mgmt_t **ptr_ret)
{
    int len, ret;
    sc_res_info_mgmt_t *origin;
    sc_res_mime_t mime_type;

    if (rl == NULL || url == NULL) {
        return -1;
    }

    if (!sc_res_info_permit_adding(url)) {
        hc_log_error("unknown url %s", url);
        return -1;
    }

    len = strlen(url);
    if (len >= HTTP_URL_MAX_LEN) {
        hc_log_error("url is longer than MAX_LEN %d", HTTP_URL_MAX_LEN);
        return -1;
    }

    origin = sc_res_info_get_mgmt_origin(rl);
    if (origin == NULL) {
        hc_log_error("get free res_info failed");
        return -1;
    }

    sc_res_copy_url(origin->common.url, url, HTTP_URL_MAX_LEN, 1);
#if DEBUG
    hc_log_debug("copied url with parameter: %s", origin->common.url);
#endif
    /* zhaoyao: mark site should be very early. */
    ret = sc_res_info_mark_site(&origin->common);
    if (ret != 0) {
        hc_log_error("sc_res_info_mark_site failed, url %s", origin->common.url);
        goto error;
    }

    mime_type = sc_res_mime_type_obtain(url, 1);
    sc_res_mime_set_t(&origin->common, mime_type);

    if (ptr_ret != NULL) {
        *ptr_ret = origin;
    }

    return 0;

error:
    sc_res_info_del(sc_res_info_list, (sc_res_info_t *)origin);
    return -1;
}

/*
 * zhaoyao XXX: url是视频网站的主页地址
 */
int sc_res_info_add_ctrl_ld(sc_res_list_t *rl, char *url, sc_res_info_mgmt_t **ptr_ret)
{
    int len, ret;
    sc_res_info_mgmt_t *ctrl_ld;
    sc_res_mime_t mime_type;

    if (rl == NULL || url == NULL) {
        return -1;
    }

    if (!sc_res_info_permit_adding(url)) {
        hc_log_error("unknown url %s", url);
        return -1;
    }

    len = strlen(url);
    if (len >= HTTP_URL_MAX_LEN) {
        hc_log_error("url is longer than MAX_LEN %d", HTTP_URL_MAX_LEN);
        return -1;
    }

    ctrl_ld = sc_res_info_get_mgmt_ctrl_ld(rl);
    if (ctrl_ld == NULL) {
        hc_log_error("get free res_info failed");
        return -1;
    }

    sc_res_copy_url(ctrl_ld->common.url, url, HTTP_URL_MAX_LEN, 1);
#if DEBUG
    hc_log_debug("copied url with parameter: %s", ctrl_ld->common.url);
#endif
    /* zhaoyao: mark site should be very early. */
    ret = sc_res_info_mark_site(&ctrl_ld->common);
    if (ret != 0) {
        hc_log_error("sc_res_info_mark_site failed, url %s", ctrl_ld->common.url);
        goto error;
    }

    mime_type = sc_res_mime_type_obtain(url, 0);
    sc_res_mime_set_t(&ctrl_ld->common, mime_type);

    if (ptr_ret != NULL) {
        *ptr_ret = ctrl_ld;
    }

    return 0;

error:
    sc_res_info_del(sc_res_info_list, (sc_res_info_t *)ctrl_ld);
    return -1;
}

int sc_res_info_add_ctnt(sc_res_list_t *rl,
                         sc_res_info_mgmt_t *mgmt,
                         char *url,
                         sc_res_info_ctnt_t **ptr_ret)
{
    int len, ret;
    sc_res_info_ctnt_t *ctnt;
    sc_res_mime_t mime_type;

    if (rl == NULL || mgmt == NULL || url == NULL) {
        return -1;
    }

    if (!sc_res_info_permit_adding(url)) {
        hc_log_error("unknown url %s", url);
        return -1;
    }

    len = strlen(url);
    if (len >= HTTP_URL_MAX_LEN) {
        hc_log_error("url is longer than MAX_LEN %d", HTTP_URL_MAX_LEN);
        return -1;
    }

    if (sc_res_gen_is_ctrl_ld(&mgmt->common)) {
        /*
         * zhaoyao XXX: 解决bug，对于在Snooping模块已保存的url，需删除它，然后再添加，
         *              否则重定向错误。
         *              该问题只存在于服务器启动时加载的本地资源，因此依靠mgmt类型来做选择性操作。
         */
        ret = sc_snooping_do_del(-1, url);
        if (ret != 0) {
            hc_log_error("flush url in Snooping Module failed: %s", url);
            return -1;
        }
    }

    ctnt = sc_res_info_get_ctnt(rl);
    if (ctnt == NULL) {
        hc_log_error("get free res_info failed");
        return -1;
    }

    sc_res_copy_url(ctnt->common.url, url, HTTP_URL_MAX_LEN, 1);
#if DEBUG
    hc_log_debug("copied url with parameter:%s", ctnt->common.url);
#endif

    /* zhaoyao XXX: inherit site at very first time */
    sc_res_site_inherit(mgmt, ctnt);
    mime_type = sc_res_mime_type_obtain(url, 1);
    sc_res_mime_set_t(&ctnt->common, mime_type);

    if (sc_res_gen_is_ctrl_ld(&mgmt->common)) {
        /* zhaoyao XXX: 本地文件当然是已经stored了。 */
        sc_res_flag_set_stored(&ctnt->common);
    }

    ret = sc_res_info_gen_ctnt_local_path(ctnt);
    if (ret != 0) {
        hc_log_error("generate local_path failed, file_url:\n\t%s", ctnt->common.url);
        goto error;
    }

    if (mgmt->child_cnt == 0) {  /* First derivative is come */
        mgmt->child = ctnt;
    } else {
        ctnt->siblings = mgmt->child;
        mgmt->child = ctnt;
    }
    mgmt->child_cnt++;
    ctnt->parent = mgmt;

    if (ptr_ret != NULL) {
        *ptr_ret = ctnt;
    }

    return 0;

error:
    sc_res_info_del(sc_res_info_list, (sc_res_info_t *)ctnt);
    return -1;

}

static hc_result_t sc_res_info_dup_ctnt(sc_res_info_ctnt_t *from, sc_res_info_ctnt_t *to)
{
    if (from == NULL || to == NULL) {
        return HC_ERR_INVALID;
    }

#if DEBUG
    hc_log_debug("\n\tfrom: %s\n\tto: %s", from->common.url, to->common.url);
    hc_log_debug("from's local path: %s", from->localpath);

    if (sc_res_flag_is_stored(&from->common)) {
        hc_log_debug("loaded is stored");
    }
    if (sc_res_flag_is_notify(&from->common)) {
        hc_log_debug("from is notified to Snooping Module");
    }
#endif

    memcpy(to->common.url, from->common.url, HTTP_URL_MAX_LEN);
    memcpy(to->localpath, from->localpath, SC_RES_LOCAL_PATH_MAX_LEN);

    if (sc_res_flag_is_stored(&from->common)) {
        sc_res_flag_set_stored(&to->common);
    }
    if (sc_res_flag_is_notify(&from->common)) {
        sc_res_flag_set_notify(&to->common);
    }

    return HC_SUCCESS;
}

static hc_result_t sc_res_info_remove_loaded(sc_res_info_ctnt_t *pre, sc_res_info_ctnt_t *ld)
{
    sc_res_info_mgmt_t *ctrl_ld;

    if (ld == NULL) {
        return HC_ERR_INVALID;
    }

    ctrl_ld = ld->parent;
    if (ctrl_ld == NULL) {
        hc_log_error("loaded has no parent");
        return HC_ERR_INTERNAL;
    }

    if (pre == NULL) {
        if (ctrl_ld->child != ld) {
            hc_log_error("invalid pre, ctrl_ld has %lu child(ren)", ctrl_ld->child_cnt);
            return HC_ERR_INTERNAL;
        }
        if (ctrl_ld->child_cnt == 0) {
            hc_log_error("FATAL, ctrl_ld child count is 0");
            return HC_ERR_INTERNAL;
        }

        ctrl_ld->child = ld->siblings;
        ctrl_ld->child_cnt--;
        sc_res_info_del(sc_res_info_list, &ld->common);

        return HC_SUCCESS;
    }

    if (ctrl_ld->child_cnt < 2) {
        hc_log_error("ctrl_ld has %lu child(ren), while pre is not NULL", ctrl_ld->child_cnt);
        return HC_ERR_INTERNAL;
    }

    pre->siblings = ld->siblings;
    ctrl_ld->child_cnt--;
    sc_res_info_del(sc_res_info_list, &ld->common);

    return HC_SUCCESS;
}

/*
 * zhaoyao XXX: 对于已经存在的资源，根据loaded更新parsed，并删除掉loaded
 */
hc_result_t sc_res_info_handle_cached(sc_res_info_mgmt_t *ctrl_ld, sc_res_info_ctnt_t *parsed)
{
    sc_res_info_ctnt_t *ld, *pre_ld;
    char *vid, *p, *url;
    int url_len;
    hc_result_t ret;

    if (ctrl_ld == NULL || parsed == NULL) {
        hc_log_error("invalid input");
        return HC_ERR_INVALID;
    }

    url = parsed->common.url;
    url_len = strlen(url);
    for (p = url + url_len - 1; *p != '/' && p > url; p--) {
        ;
    }
    if (*p != '/') {
        hc_log_error("invalid url: %s", url);
        return HC_ERR_INTERNAL;
    }

    vid = p + 1;

    for (pre_ld = NULL, ld = ctrl_ld->child; ld != NULL; ) {
        if (strstr(ld->common.url, vid) != NULL) {
            break;
        }
        pre_ld = ld;
        ld = ld->siblings;
    }
    if (ld == NULL) {
        /* zhaoyao XXX: 这种情况比较特殊，可能是设备运行很长时间，且服务器中途删除过资源并重启过。 */
        hc_log_error("do not find corresponding loaded ri, url: %s", url);
        return HC_ERR_INTERNAL;
    }

    ret = sc_res_info_dup_ctnt(ld, parsed);
    if (ret != HC_SUCCESS) {
        hc_log_error("copy information from loaded to parsed failed, url: %s", url);
        return ret;
    }

    ret = sc_res_info_remove_loaded(pre_ld, ld);
    if (ret != HC_SUCCESS) {
        hc_log_error("remove loaded failed, url: %s", url);
        return ret;
    }

    return HC_SUCCESS;
}

static void sc_res_info_del_mgmt(sc_res_list_t *rl, sc_res_info_mgmt_t *mgmt)
{
    char print_buf[BUFFER_LEN];

    if (rl == NULL || mgmt == NULL) {
        return;
    }

    bzero(print_buf, BUFFER_LEN);
    if (sc_res_gen_is_origin(&mgmt->common)) {
        strcat(print_buf, "origin: ");
    } else if (sc_res_gen_is_ctrl_ld(&mgmt->common)) {
        strcat(print_buf, "ctrl_ld: ");
    } else if (sc_res_gen_is_normal(&mgmt->common)) {
        strcat(print_buf, "normal: ");
    } else {
        strcat(print_buf, "unknown: ");
    }
    strcat(print_buf, mgmt->common.url);
    hc_log_info("%s", print_buf);

    if (mgmt->child_cnt != 0) {
        /* zhaoyao XXX TODO: 支持 */
        hc_log_error("delete origin or ctrl_ld who has %lu children is not supported now",
                            mgmt->child_cnt);
    }

    sc_res_info_put(rl, &mgmt->common);
}

static void sc_res_info_del_ctnt(sc_res_list_t *rl, sc_res_info_ctnt_t *ctnt)
{
    /* zhaoyao TODO XXX: 支持 */
    sc_res_info_mgmt_t *mgmt;
    char print_buf[BUFFER_LEN];

    if (rl == NULL || ctnt == NULL) {
        return;
    }

    mgmt = ctnt->parent;

    bzero(print_buf, BUFFER_LEN);
    if (mgmt != NULL) {
        if (sc_res_gen_is_origin(&mgmt->common)) {
            strcat(print_buf, "origin , ");
        } else if (sc_res_gen_is_ctrl_ld(&mgmt->common)) {
            strcat(print_buf, "ctrl_ld, ");
        } else if (sc_res_gen_is_normal(&mgmt->common)) {
            strcat(print_buf, "normal , ");
        } else {
            strcat(print_buf, "unknown, ");
        }
    } else {
        strcat(print_buf, "no parent, ");
    }

    strcat(print_buf, ctnt->common.url);

    hc_log_info("%s", print_buf);

    if ((sc_res_flag_is_stored(&ctnt->common)) &&
        (mgmt != NULL) &&
        (!sc_res_gen_is_ctrl_ld(&mgmt->common))) {
        hc_log_error("\n%s\n\tstored local file is not deleted", ctnt->common.url);
    }

    if ((sc_res_flag_is_notify(&ctnt->common)) &&
        (mgmt != NULL) &&
        (!sc_res_gen_is_ctrl_ld(&mgmt->common))) {
        hc_log_error("\n%s\n\thas notified snooping module", ctnt->common.url);
    }

    if ((mgmt != NULL) &&
        (!sc_res_gen_is_ctrl_ld(&mgmt->common))) {
        hc_log_error("%s has parent\n", ctnt->common.url);
        if (mgmt->child_cnt > 1) {
            hc_log_error("%s's parent %s has more than one child", ctnt->common.url, mgmt->common.url);
        }
    }

    sc_res_info_put(rl, &ctnt->common);
}

void sc_res_info_del(sc_res_list_t *rl, sc_res_info_t *ri)
{
    if (rl == NULL || ri == NULL) {
        return;
    }

    if (sc_res_info_is_mgmt(ri)) {
        sc_res_info_del_mgmt(rl, (sc_res_info_mgmt_t *)ri);
        return;
    }

    if (sc_res_info_is_ctnt(ri)) {
        sc_res_info_del_ctnt(rl, (sc_res_info_ctnt_t *)ri);
        return;
    }

    hc_log_error("unknown type 0x%lx", ri->gen);
}

sc_res_info_ctnt_t *sc_res_info_find_ctnt(sc_res_list_t *rl, const char *url)
{
    sc_res_info_ctnt_t *curr;
    int i;

    if (rl == NULL || url == NULL) {
        return NULL;
    }

    for (i = 0; i < SC_RES_INFO_NUM_MAX_CTNT; i++) {
        curr = &rl->ctnt[i];
        if (strcmp(url, curr->common.url) == 0) {
            return curr;
        }
    }

    return NULL;
}

sc_res_info_mgmt_t *sc_res_info_find_mgmt(sc_res_list_t *rl, const char *url)
{
    sc_res_info_mgmt_t *curr;
    int i;

    if (rl == NULL || url == NULL) {
        return NULL;
    }

    for (i = 0; i < SC_RES_INFO_NUM_MAX_MGMT; i++) {
        curr = &rl->mgmt[i];
        if (strcmp(url, curr->common.url) == 0) {
            return curr;
        }
    }

    return NULL;
}

int sc_res_gen_origin_url(char *req_url, char *origin_url)
{
    int ret;

    if (req_url == NULL || origin_url == NULL) {
        return -1;
    }

    if (sc_url_is_yk(req_url)) {
        ret = sc_yk_gen_origin_url(req_url, origin_url);
        return ret;
    } else if (sc_url_is_sohu(req_url)) {
        ret = sc_sohu_gen_origin_url(req_url, origin_url);
        return ret;
    } else {
        return -1;
    }
}

static int sc_res_retry_download(sc_res_info_t *ri)
{
    int ret;
    sc_res_info_ctnt_t *ctnt;

    if (ri == NULL) {
        hc_log_error("input invalid");
        return -1;
    }

    hc_log_info("%120s", ri->url);

    if (!sc_res_info_is_ctnt(ri)) {
        hc_log_error("only for content");
        return -1;
    }

    ctnt = (sc_res_info_ctnt_t *)ri;

    if (sc_res_site_is_sohu(ri)) {
        ret = sc_sohu_download(ctnt);
    } else if (sc_res_site_is_youku(ri)) {
        ret = sc_youku_download(ctnt);
    } else {
        hc_log_error("unknown site file:\nreal_url: %s\nlocal_path: %s", ri->url, ctnt->localpath);
        ret = sc_ngx_download(ri->url, ctnt->localpath);
        if (ret != 0) {
            sc_res_flag_set_i_fail(ri);
        }
    }

    if (ret != 0) {
        hc_log_error("%s failed\n", ri->url);
    }

    return ret;
}

int sc_res_notify_ri_url(sc_res_info_t *ri)
{
    int ret;

    if (ri == NULL) {
        hc_log_error("invalid input");
        return -1;
    }

    if (!sc_res_info_is_ctnt(ri)) {
        hc_log_error("only for content");
        return -1;
    }

    if (!sc_res_flag_is_stored(ri) || sc_res_flag_is_notify(ri)) {
        hc_log_error("ri URL:\n\t%s\nflag:%lu can not be added", ri->url, ri->flag);
        return -1;
    }

    ret = sc_snooping_do_add(ri->id, ri->url);
    if (ret != 0) {
        hc_log_error("add res info url is failed");
        return -1;
    }

    sc_res_flag_set_notify(ri);

    return 0;
}


static int sc_res_add_ctnt_url(sc_res_info_ctnt_t *ctnt)
{
    int ret;
    sc_res_info_t *ri;

    if (ctnt == NULL) {
        return -1;
    }

    ri = &ctnt->common;
    if (sc_res_site_is_youku(ri)) {
        ret = sc_yk_add_ctnt_url(ctnt);
    } else {
        /* zhaoyao: default case */
        ret = sc_res_notify_ri_url(ri);
    }

    return ret;
}

static int sc_res_list_process_ctnt(sc_res_info_mgmt_t *mgmt)
{
    sc_res_info_ctnt_t *curr;
    sc_res_info_t *ri;
    int err = 0, ret;

    if (mgmt == NULL) {
        return -1;
    }

    for (curr = mgmt->child; curr != NULL; curr = curr->siblings) {
        ri = &curr->common;

        if (ri->url[0] == '\0') {
            hc_log_error("invalid url");
            err++;
            continue;
        }

        if (!sc_res_flag_is_stored(ri)) {
            if (sc_res_flag_is_i_fail(ri)) {
                /*
                 * zhaoyao XXX: add ctnt success, but inform Nginx to download failed,
                 *              re-download in this situation.
                 */
                ret = sc_res_retry_download(ri);
                if (ret == 0) {
                    sc_res_flag_unset_i_fail(ri);
                } else {
                    hc_log_error("inform Nginx re-download %s failed", ri->url);
                    err++;
                }
            } else if (sc_res_flag_is_d_fail(ri)) {
                sc_res_flag_unset_d_fail(ri);    /* zhaoyao: SC已经接管，可将d_fail置0 */
                ret = sc_res_retry_download(ri);
                if (ret == 0) {
                    ;
                } else {
                    hc_log_error("inform Nginx re-download %s failed", ri->url);
                    err++;
                }
            } else {
                /* zhaoyao XXX: 文件正在下载中 */
            }
            continue;
        }

        if (!sc_res_flag_is_notify(ri)) {
            ret = sc_res_add_ctnt_url(curr);
            if (ret != 0) {
                hc_log_error("inform Snooping Module add URL failed");
                err++;
            }
        }
    }

    return err;
}

static int sc_res_list_process_mgmt(sc_res_list_t *rl)
{
    sc_res_info_mgmt_t *curr;
    int i, err = 0, ret;

    if (rl == NULL) {
        return -1;
    }

    for (i = 0; i < SC_RES_INFO_NUM_MAX_MGMT; i++) {
        curr = &(rl->mgmt[i]);
        if (curr->common.id >= SC_RES_INFO_NUM_MAX_MGMT) {
            /* zhaoyao XXX: 跳过空闲的mgmt */
            continue;
        }

        ret = sc_res_list_process_ctnt(curr);
        if (ret < 0) {
            hc_log_error("%s", curr->common.url);
            return ret;
        }

        err = err + ret;
    }

    return err;
}

int sc_res_list_process_func(sc_res_list_t *rl)
{
    int err;

    if (rl == NULL) {
        return -1;
    }

    err = sc_res_list_process_mgmt(rl);
    if (err < 0) {
        hc_log_error("sc_res_list_process_mgmt, return %d", err);
        return err;
    }

    return err;
}

void *sc_res_list_process_thread(void *arg)
{
    int ret;

    while (1) {
        ret = sc_res_list_process_func(sc_res_info_list);
        if (ret < 0) {
            hc_log_error("exit...");
            break;
        } else if (ret > 0) {
            hc_log_error("problem occured %d time(s)...", ret);
        }

        sleep(3);
    }

    return ((void *)0);
}
