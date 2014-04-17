#include "common.h"
#include "sc_header.h"
#include "sc_resource.h"

sc_res_list_t *sc_res_info_list = NULL;
int sc_res_share_mem_shmid = -1;

/*
 * zhaoyao XXX: content type suffix MUST be at the end of str.
 */
sc_res_ctnt_t sc_res_content_type_obtain(char *str, int care_type)
{
    char *p;
    int len;

    if (!care_type) {
        return SC_RES_CTNT_NOT_CARE;
    }

    if (str == NULL) {
        hc_log_error("Invalid input");
        return SC_RES_CTNT_TYPE_MAX;
    }

    len = strlen(str);
    if (len < 3) {
        hc_log_error("Invalid input: %s", str);
        return SC_RES_CTNT_TYPE_MAX;
    }

    len = len - 3;
    p = str + len;
    if (strncmp(p, VIDEO_FLV_SUFFIX, VIDEO_FLV_SUFFIX_LEN) == 0) {
        return SC_RES_CTNT_VIDEO_FLV;
    }
    if (strncmp(p, VIDEO_MP4_SUFFIX, VIDEO_MP4_SUFFIX_LEN) == 0) {
        return SC_RES_CTNT_VIDEO_MP4;
    }

    len = strlen(str);
    len = len - 4;
    p = str + len;
    if (strncmp(p, TEXT_HTML_SUFFIX, TEXT_HTML_SUFFIX_LEN) == 0) {
        return SC_RES_CTNT_TEXT_HTML;
    }
    if (strncmp(p, TEXT_M3U8_SUFFIX, TEXT_M3U8_SUFFIX_LEN) == 0) {
        return SC_RES_CTNT_TEXT_M3U8;
    }

    hc_log_error("Unknown content type: %s", str);

    return SC_RES_CTNT_TYPE_MAX;
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
        sc_res_set_origin(&origin->common);
    }

    return origin;
}

static sc_res_info_mgmt_t *sc_res_info_get_mgmt_ctl_ld(sc_res_list_t *rl)
{
    sc_res_info_mgmt_t *ctl_ld;

    ctl_ld = sc_res_info_get_mgmt(rl);

    if (ctl_ld != NULL) {
        sc_res_set_ctl_ld(&ctl_ld->common);
    }

    return ctl_ld;
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

    return ctnt;
}

static sc_res_info_ctnt_t *sc_res_info_get_ctnt_normal(sc_res_list_t *rl)
{
    sc_res_info_ctnt_t *normal;

    normal = sc_res_info_get_ctnt(rl);
    if (normal != NULL) {
        sc_res_set_normal(&normal->common);
    }

    return normal;
}

static sc_res_info_ctnt_t *sc_res_info_get_ctnt_parsed(sc_res_list_t *rl)
{
    sc_res_info_ctnt_t *parsed;

    parsed = sc_res_info_get_ctnt(rl);
    if (parsed != NULL) {
        sc_res_set_parsed(&parsed->common);
    }

    return parsed;
}

static sc_res_info_ctnt_t *sc_res_info_get_ctnt_loaded(sc_res_list_t *rl)
{
    sc_res_info_ctnt_t *loaded;

    loaded = sc_res_info_get_ctnt(rl);
    if (loaded != NULL) {
        sc_res_set_loaded(&loaded->common);
    }

    return loaded;
}

static void sc_res_info_put(sc_res_list_t *rl, sc_res_info_t *ri)
{
    sc_res_info_mgmt_t *mgmt;
    sc_res_info_ctnt_t *ctnt;

    if (rl == NULL || ri == NULL) {
        return;
    }

    if (sc_res_is_mgmt(ri)) {
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
    } else if (sc_res_is_ctnt(ri)) {
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
        hc_log_error("unknown type 0x%lx", ri->flag);
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
    if (sc_res_is_youku(&ctnt->common)) {
        ret = sc_yk_url_to_local_path(ctnt->common.url, ctnt->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    } else if (sc_res_is_sohu(&ctnt->common)) {
        ret = sc_sohu_file_url_to_local_path(ctnt->common.url, ctnt->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    } else {
        hc_log_debug("using sc_res_url_to_local_path_default, url %s", ctnt->common.url);
        ret = sc_res_url_to_local_path_default(ctnt->common.url, ctnt->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    }

    return ret;
}

static int sc_res_recover_url_from_local_path(char *local_path, char *url)
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

    if (!sc_res_is_normal(ri) && !sc_res_is_mgmt(ri)) {
        hc_log_error("can not mark non-normal, non-origin nor non-ctl_ld ri's site directly");
        return ret;
    }

    if (sc_url_is_yk(ri->url)) {
        sc_res_set_youku(ri);
        ret = 0;
    } else if (sc_url_is_sohu(ri->url)) {
        sc_res_set_sohu(ri);
        ret = 0;
    } else {
        hc_log_error("unknown site: %s", ri->url);
    }

    return ret;
}

int sc_res_info_add_normal(sc_res_list_t *rl, char *url, sc_res_info_ctnt_t **ptr_ret)
{
    int len, ret;
    sc_res_info_ctnt_t *normal;
    sc_res_ctnt_t content_type;

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

    normal = sc_res_info_get_ctnt_normal(rl);
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

    content_type = sc_res_content_type_obtain(url, 1);
    sc_res_set_content_t(&normal->common, content_type);

    ret = sc_res_info_gen_ctnt_local_path(normal);
    if (ret != 0) {
        hc_log_error("generate local_path failed, url %s", normal->common.url);
        goto error;
    }

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
    sc_res_ctnt_t content_type;

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

    content_type = sc_res_content_type_obtain(url, 1);
    sc_res_set_content_t(&origin->common, content_type);

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
int sc_res_info_add_ctl_ld(sc_res_list_t *rl, char *url, sc_res_info_mgmt_t **ptr_ret)
{
    int len, ret;
    sc_res_info_mgmt_t *ctl_ld;
    sc_res_ctnt_t content_type;

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

    ctl_ld = sc_res_info_get_mgmt_ctl_ld(rl);
    if (ctl_ld == NULL) {
        hc_log_error("get free res_info failed");
        return -1;
    }

    sc_res_copy_url(ctl_ld->common.url, url, HTTP_URL_MAX_LEN, 1);
#if DEBUG
    hc_log_debug("copied url with parameter:%s", ctl_ld->common.url);
#endif
    /* zhaoyao: mark site should be very early. */
    ret = sc_res_info_mark_site(&ctl_ld->common);
    if (ret != 0) {
        hc_log_error("sc_res_info_mark_site failed, url %s", ctl_ld->common.url);
        goto error;
    }

    content_type = sc_res_content_type_obtain(url, 0);
    sc_res_set_content_t(&ctl_ld->common, content_type);

    if (ptr_ret != NULL) {
        *ptr_ret = ctl_ld;
    }

    return 0;

error:
    sc_res_info_del(sc_res_info_list, (sc_res_info_t *)ctl_ld);
    return -1;
}

int sc_res_info_add_parsed(sc_res_list_t *rl,
                           sc_res_info_mgmt_t *origin,
                           char *url,
                           sc_res_info_ctnt_t **ptr_ret)
{
    int len, ret;
    sc_res_info_ctnt_t *parsed;
    sc_res_ctnt_t content_type;

    if (rl == NULL || origin == NULL || url == NULL) {
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

    parsed = sc_res_info_get_ctnt_parsed(rl);
    if (parsed == NULL) {
        hc_log_error("get free res_info failed");
        return -1;
    }

    sc_res_copy_url(parsed->common.url, url, HTTP_URL_MAX_LEN, 1);
#if DEBUG
    hc_log_debug("copied url with parameter:%s", parsed->common.url);
#endif
    /* zhaoyao XXX: inherit site at very first time */
    sc_res_inherit_site(origin, parsed);
    content_type = sc_res_content_type_obtain(url, 1);
    sc_res_set_content_t(&parsed->common, content_type);

    ret = sc_res_info_gen_ctnt_local_path(parsed);
    if (ret != 0) {
        hc_log_error("generate local_path failed, file_url:\n\t%s", parsed->common.url);
        goto error;
    }

    if (origin->child_cnt == 0) {  /* First derivative is come */
        origin->child = parsed;
    } else {
        parsed->siblings = origin->child;
        origin->child = parsed;
    }
    origin->child_cnt++;
    parsed->parent = origin;

    if (ptr_ret != NULL) {
        *ptr_ret = parsed;
    }

    return 0;

error:
    sc_res_info_del(sc_res_info_list, (sc_res_info_t *)parsed);
    return -1;
}

int sc_res_info_add_loaded(sc_res_list_t *rl,
                           sc_res_info_mgmt_t *ctl_ld,
                           const char *fpath,
                           sc_res_info_ctnt_t **ptr_ret)
{
    int len, ret;
    sc_res_info_ctnt_t *loaded;
    sc_res_ctnt_t content_type;
    char old_url[HTTP_URL_MAX_LEN], *lp;

    if (rl == NULL || ctl_ld == NULL || fpath == NULL) {
        return -1;
    }

    lp = strstr(fpath, SC_NGX_ROOT_PATH);
    if (lp == NULL) {
        hc_log_error("invalid fpath %s", fpath);
        return -1;
    }
    lp = lp + SC_NGX_ROOT_PATH_LEN;

    bzero(old_url, HTTP_URL_MAX_LEN);
    ret = sc_res_recover_url_from_local_path(lp, old_url);
    if (ret != 0) {
        hc_log_error("recover old url from local path failed, %s", lp);
        return -1;
    }

    if (!sc_res_info_permit_adding(old_url)) {
        hc_log_error("unknown url %s", old_url);
        return -1;
    }

    len = strlen(old_url);
    if (len >= HTTP_URL_MAX_LEN) {
        hc_log_error("url is longer than MAX_LEN %d", HTTP_URL_MAX_LEN);
        return -1;
    }

    /* zhaoyao XXX: 解决bug， 如果在Snooping模块上已保存一个url，需要删除它，否则会重定向错误 */
    ret = sc_snooping_do_del(-1, old_url);
    if (ret != 0) {
        hc_log_error("flush url in Snooping Module failed: %s", old_url);
        return -1;
    }

    loaded = sc_res_info_get_ctnt_loaded(rl);
    if (loaded == NULL) {
        hc_log_error("get free res_info failed");
        return -1;
    }

    sc_res_copy_url(loaded->common.url, old_url, HTTP_URL_MAX_LEN, 1);
#if DEBUG
    hc_log_debug("copied url with parameter:%s", loaded->common.url);
#endif
    /* zhaoyao XXX: inherit site at very first time */
    sc_res_inherit_site(ctl_ld, loaded);
    content_type = sc_res_content_type_obtain(old_url, 1);
    sc_res_set_content_t(&loaded->common, content_type);

    /* zhaoyao XXX: 本地文件当然是已经stored了。 */
    sc_res_set_stored(&loaded->common);

    /* zhaoyao: 为loaded装填local path，当然很简单。。。 */
    strcpy(loaded->localpath, lp);

    if (ctl_ld->child_cnt == 0) {  /* First derivative is come */
        ctl_ld->child = loaded;
    } else {
        loaded->siblings = ctl_ld->child;
        ctl_ld->child = loaded;
    }
    ctl_ld->child_cnt++;
    loaded->parent = ctl_ld;

    if (ptr_ret != NULL) {
        *ptr_ret = loaded;
    }

    return 0;

//error:
//    sc_res_info_del(sc_res_info_list, (sc_res_info_t *)loaded);
//    return -1;
}

int sc_res_dup_loaded_to_parsed(sc_res_info_ctnt_t *loaded, sc_res_info_ctnt_t *parsed)
{
    if (loaded == NULL || parsed == NULL) {
        return -1;
    }

#if DEBUG
    hc_log_debug("\n\tloaded: %s\n\tparsed: %s", loaded->common.url, parsed->common.url);
    hc_log_debug("loaded local path: %s", loaded->localpath);

    if (sc_res_is_kf_crt(&loaded->common)) {
        hc_log_debug("loaded flv keyframe created");
    }
    if (sc_res_is_stored(&loaded->common)) {
        hc_log_debug("loaded is stored");
    }
    if (sc_res_is_notify(&loaded->common)) {
        hc_log_debug("loaded is notified to Snooping Module");
    }
#endif

    memcpy(parsed->common.url, loaded->common.url, HTTP_URL_MAX_LEN);
    if (sc_res_is_kf_crt(&loaded->common)) {
        memcpy(parsed->kf_info, loaded->kf_info, sizeof(parsed->kf_info));
        parsed->kf_num = loaded->kf_num;
    }
    memcpy(parsed->localpath, loaded->localpath, SC_RES_LOCAL_PATH_MAX_LEN);

    if (sc_res_is_stored(&loaded->common)) {
        sc_res_set_stored(&parsed->common);
    }
    if (sc_res_is_notify(&loaded->common)) {
        sc_res_set_notify(&parsed->common);
    }

    return 0;
}

int sc_res_remove_loaded(sc_res_info_ctnt_t *pre, sc_res_info_ctnt_t *ld)
{
    sc_res_info_mgmt_t *ctl_ld;

    if (ld == NULL) {
        return -1;
    }

    ctl_ld = ld->parent;
    if (ctl_ld == NULL) {
        hc_log_error("loaded has no parent");
        return -1;
    }

    if (pre == NULL) {
        if (ctl_ld->child_cnt != 1) {
            hc_log_error("invalid pre, ctl_ld has %lu child(ren)", ctl_ld->child_cnt);
            return -1;
        }

        ctl_ld->child = NULL;
        ctl_ld->child_cnt = 0;
        sc_res_info_del(sc_res_info_list, &ld->common);

        return 0;
    }

    if (ctl_ld->child_cnt < 2) {
        hc_log_error("ctl_ld has %lu child(ren), whild pre is not NULL", ctl_ld->child_cnt);
        return -1;
    }

    pre->siblings = ld->siblings;
    ctl_ld->child_cnt--;
    sc_res_info_del(sc_res_info_list, &ld->common);

    return 0;
}


void sc_res_info_del(sc_res_list_t *rl, sc_res_info_t *ri)
{
    sc_res_info_mgmt_t *mgmt;
    sc_res_info_ctnt_t *parsed;
    char print_buf[BUFFER_LEN];

    if (rl == NULL || ri == NULL) {
        return;
    }

    bzero(print_buf, BUFFER_LEN);
    if (sc_res_is_origin(ri)) {
        strcat(print_buf, "origin: ");
    } else if (sc_res_is_ctl_ld(ri)) {
        strcat(print_buf, "ctl_ld: ");
    } else if (sc_res_is_parsed(ri)) {
        strcat(print_buf, "parsed: ");
    } else if (sc_res_is_loaded(ri)) {
        strcat(print_buf, "loaded: ");
    } else if (sc_res_is_normal(ri)) {
        strcat(print_buf, "normal: ");
    } else {
        strcat(print_buf, "unknown: ");
    }
    strcat(print_buf, ri->url);
    hc_log_info("%s", print_buf);

    if (sc_res_is_mgmt(ri)) {
        mgmt = (sc_res_info_mgmt_t *)ri;
        if (mgmt->child_cnt != 0) {
            hc_log_error("delete origin or ctl_ld who has %lu children is not supported now",
                                mgmt->child_cnt);
        }
        sc_res_info_put(rl, ri);
        return;
    }

    if (sc_res_is_stored(ri) && !sc_res_is_loaded(ri)) {
        hc_log_error("\n%s\n\tstored local file is not deleted", ri->url);
    }

    if (sc_res_is_notify(ri) && !sc_res_is_loaded(ri)) {
        hc_log_error("\n%s\n\thas notified snooping module", ri->url);
    }

    if (sc_res_is_normal(ri)) {
        sc_res_info_put(rl, ri);
        return;
    }

    if (sc_res_is_parsed(ri)) {
        parsed = (sc_res_info_ctnt_t *)ri;
        if (parsed->parent != NULL) {
            hc_log_error("%s has parent\n", ri->url);
            mgmt = parsed->parent;
            if (mgmt->child_cnt > 1) {
                hc_log_error("%s's parent %s has more than one child", ri->url, mgmt->common.url);
            }
        }
        sc_res_info_put(rl, ri);
        return;
    }

    if (sc_res_is_loaded(ri)) {
        sc_res_info_put(rl, ri);
        return;
    }

    hc_log_error("unknown type 0x%lx", ri->flag);
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

    if (!sc_res_is_normal(ri) && !sc_res_is_parsed(ri)) {
        hc_log_error("only for normal and parsed");
        return -1;
    }

    ctnt = (sc_res_info_ctnt_t *)ri;

    if (sc_res_is_sohu(ri)) {
        ret = sc_sohu_download(ctnt);
    } else if (sc_res_is_youku(ri)) {
        ret = sc_youku_download(ctnt);
    } else {
        hc_log_error("unknown site file:\nreal_url: %s\nlocal_path: %s", ri->url, ctnt->localpath);
        ret = sc_ngx_download(ri->url, ctnt->localpath);
        if (ret != 0) {
            sc_res_set_i_fail(ri);
        }
    }

    if (ret != 0) {
        hc_log_error("%s failed\n", ri->url);
    }

    return ret;
}

int sc_res_add_ri_url(sc_res_info_t *ri)
{
    int ret;

    if (ri == NULL) {
        hc_log_error("invalid input");
        return -1;
    }

    if (!sc_res_is_stored(ri) || sc_res_is_notify(ri)) {
        hc_log_error("ri URL:\n\t%s\nflag:%lu can not be added", ri->url, ri->flag);
        return -1;
    }

    ret = sc_snooping_do_add(ri->id, ri->url);
    if (ret != 0) {
        hc_log_error("add res info url is failed");
        return -1;
    }

    sc_res_set_notify(ri);

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
    if (sc_res_is_youku(ri)) {
        ret = sc_yk_add_ctnt_url(ctnt);
    } else {
        /* zhaoyao: default case */
        ret = sc_res_add_ri_url(ri);
    }

    return ret;
}

static int sc_res_list_process_mgmt(sc_res_list_t *rl)
{
    if (rl == NULL) {
        return -1;
    }

    return 0;
}

static int sc_res_list_process_ctnt(sc_res_list_t *rl)
{
    sc_res_info_ctnt_t *curr;
    sc_res_info_t *ri;
    int i, err = 0, ret;

    if (rl == NULL) {
        return -1;
    }

    for (i = 0; i < SC_RES_INFO_NUM_MAX_CTNT; i++) {
        curr = &rl->ctnt[i];
        ri = &curr->common;

        if (ri->url[0] == '\0') {
            continue;
        }

        if (!sc_res_is_ctnt(ri)) {
            hc_log_error("Content type check wrong, type: 0x%lx", ri->flag);
            continue;
        }

        if (!sc_res_is_stored(ri)) {
            /*
             * zhaoyao XXX: add ctnt success, but inform Nginx to download failed,
             *              re-download in this situation.
             */
            if (sc_res_is_i_fail(ri)) {
                ret = sc_res_retry_download(ri);
                if (ret == 0) {
                    sc_res_unset_i_fail(ri);
                } else {
                    hc_log_error("inform Nginx re-download %s failed", ri->url);
                    err++;
                }
            } else if (sc_res_is_d_fail(ri)) {
                sc_res_unset_d_fail(ri);    /* zhaoyao: SC已经接管，可将d_fail置0 */
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

        if (!sc_res_is_notify(ri)) {
            ret = sc_res_add_ctnt_url(curr);
            if (ret != 0) {
                hc_log_error("inform Snooping Module add URL failed");
                err++;
            }
        }

        if (sc_res_is_flv(ri) && !sc_res_is_kf_crt(ri)) {
            ret = sc_kf_flv_create_info(curr);
            if (ret != 0) {
                hc_log_error("create FLV key frame information failed");
                err++;
            }
        }
    }

    return err;
}

int sc_res_list_process_func(sc_res_list_t *rl)
{
    int err = 0, ret;

    if (rl == NULL) {
        return -1;
    }

    ret = sc_res_list_process_mgmt(rl);
    if (ret < 0) {
        hc_log_error("sc_res_list_process_mgmt, return %d", ret);
        return ret;
    }

    err = err + ret;
    ret = sc_res_list_process_ctnt(rl);
    if (ret < 0) {
        hc_log_error("sc_res_list_process_ctnt, return %d", ret);
        return ret;
    }

    err = err + ret;

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
