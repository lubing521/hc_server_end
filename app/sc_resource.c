#include "common.h"
#include "sc_header.h"
#include "sc_resource.h"

sc_res_list_t *sc_res_info_list = NULL;
int sc_res_share_mem_shmid = -1;

/*
 * zhaoyao XXX: content type suffix MUST be at the end of str.
 */
sc_res_ctnt_t sc_res_content_type_obtain(char *str)
{
    char *p;
    int len;

    if (str == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return SC_RES_CTNT_TYPE_MAX;
    }

    len = strlen(str);
    if (len < 3) {
        fprintf(stderr, "%s ERROR: invalid input %s\n", __func__, str);
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

    len = len - 4;
    p = str + len;
    if (strncmp(p, TEXT_HTML_SUFFIX, TEXT_HTML_SUFFIX_LEN) == 0) {
        return SC_RES_CTNT_TEXT_HTML;
    }
    if (strncmp(p, TEXT_M3U8_SUFFIX, TEXT_M3U8_SUFFIX_LEN) == 0) {
        return SC_RES_CTNT_TEXT_M3U8;
    }

    fprintf(stderr, "%s: unknown content type: %s\n", __func__, str);

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
        fprintf(stderr, "%s ERROR overflow, buffer len %u, but copied %u\n", __func__, len, (unsigned int)p - (unsigned int)url);
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
        fprintf(stderr, "%s shmget failed, memory size %d: %s", __func__, mem_size, strerror(errno));
        return -1;
    }

    if ((shmptr = shmat(shmid, 0, 0)) == (void *)-1) {
        fprintf(stderr, "%s shmat failed: %s", __func__, strerror(errno));
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
        fprintf(stderr, "%s allocate failed\n", __func__);
        return -1;
    }

    rl->origin[0].common.id = (unsigned long)INVALID_PTR;
    for (i = 1; i < SC_RES_INFO_NUM_MAX_ORGIIN; i++) {
        rl->origin[i].common.id = (unsigned long)(&(rl->origin[i - 1]));
    }
    rl->origin_free = (&(rl->origin[i - 1]));

    rl->active[0].common.id = (unsigned long)INVALID_PTR;
    for (i = 1; i < SC_RES_INFO_NUM_MAX_ACTIVE; i++) {
        rl->active[i].common.id = (unsigned long)(&(rl->active[i - 1]));
    }
    rl->active_free = (&(rl->active[i - 1]));

    *prl = rl;

    return ret;
}

int sc_res_list_destroy_and_uninit(int shmid)
{
    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
        fprintf(stderr, "%s shmctl remove %d failed: %s", __func__, shmid, strerror(errno));
        return -1;
    }

    sc_res_info_list = NULL;

    return 0;
}

static sc_res_info_origin_t *sc_res_info_get_origin(sc_res_list_t *rl)
{
    sc_res_info_origin_t *ri;

    if (rl == NULL) {
        return NULL;
    }

    if (rl->origin_free == INVALID_PTR) {
        fprintf(stderr, "%s ERROR: resource info totally ran out, already used %d\n",
                            __func__, rl->origin_cnt);
        return NULL;
    }

    ri = rl->origin_free;
    rl->origin_free = (sc_res_info_origin_t *)(ri->common.id);
    rl->origin_cnt++;

    memset(ri, 0, sizeof(sc_res_info_origin_t));
    ri->common.id = ((unsigned long)ri - (unsigned long)(rl->origin)) / sizeof(sc_res_info_origin_t);

    sc_res_set_origin(&ri->common);

    return ri;
}

static sc_res_info_active_t *sc_res_info_get_active(sc_res_list_t *rl)
{
    sc_res_info_active_t *ri;

    if (rl == NULL) {
        return NULL;
    }

    if (rl->active_free == INVALID_PTR) {
        fprintf(stderr, "%s ERROR: resource info totally ran out, already used %d\n",
                            __func__, rl->active_cnt);
        return NULL;
    }

    ri = rl->active_free;
    rl->active_free = (sc_res_info_active_t *)(ri->common.id);
    rl->active_cnt++;

    memset(ri, 0, sizeof(sc_res_info_active_t));
    ri->common.id = ((unsigned long)ri - (unsigned long)(rl->active)) / sizeof(sc_res_info_active_t);

    return ri;
}

static void sc_res_info_put(sc_res_list_t *rl, sc_res_info_t *ri)
{
    sc_res_info_origin_t *origin;
    sc_res_info_active_t *active;

    if (rl == NULL || ri == NULL) {
        return;
    }

    if (sc_res_is_origin(ri)) {
        if (rl->origin_cnt == 0) {
            fprintf(stderr, "%s ERROR: rl->origin_cnt = 0, can not put\n", __func__);
            return;
        }
        origin = (sc_res_info_origin_t *)ri;
        memset(origin, 0, sizeof(sc_res_info_origin_t));
        origin->common.id = (unsigned long)(rl->origin_free);
        rl->origin_free = origin;
        rl->origin_cnt--;
        return;
    } else if (sc_res_is_normal(ri) || sc_res_is_parsed(ri)) {
        if (rl->active_cnt == 0) {
            fprintf(stderr, "%s ERROR: rl->active_cnt = 0, can not put\n", __func__);
            return;
        }
        active = (sc_res_info_active_t *)ri;
        memset(active, 0, sizeof(sc_res_info_active_t));
        active->common.id = (unsigned long)(rl->active_free);
        rl->active_free = active;
        rl->active_cnt--;
        return;
    } else {
        fprintf(stderr, "%s ERROR: unknown type 0x%lx\n", __func__, ri->flag);
        return;
    }
}

/*
 * zhaoyao XXX: Snooping module Client's private simple check.
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

    fprintf(stderr, "%s is forbidden, URL: %s\n", __func__, url);

    return 0;
}

static int sc_res_url_to_local_path_default(char *url, char *local_path, int len)
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

static int sc_res_info_gen_active_local_path(sc_res_info_active_t *active)
{
    int ret = -1;

    if (active == NULL) {
        return -1;
    }

    if (sc_url_is_yk(active->common.url)) {
        ret = sc_yk_url_to_local_path(active->common.url, active->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    } else if (sc_url_is_sohu_file_url(active->common.url)) {
        ret = sc_sohu_file_url_to_local_path(active->common.url, active->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    } else {
        fprintf(stdout, "%s DEBUG: using sc_res_url_to_local_path_default, url %s\n", __func__, active->common.url);
        ret = sc_res_url_to_local_path_default(active->common.url, active->localpath, SC_RES_LOCAL_PATH_MAX_LEN);
    }

    return ret;
}

int sc_res_info_add_normal(sc_res_list_t *rl, char *url, sc_res_info_active_t **normal)
{
    int len, ret;
    sc_res_info_active_t *active;
    sc_res_ctnt_t content_type;

    if (rl == NULL || url == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return -1;
    }

    if (!sc_res_info_permit_adding(url)) {
        fprintf(stderr, "%s: unknown url %s\n", __func__, url);
        return -1;
    }

    len = strlen(url);
    if (len >= SC_RES_URL_MAX_LEN) {
        fprintf(stderr, "%s ERROR: url is longer than MAX_LEN %d\n", __func__, SC_RES_URL_MAX_LEN);
        return -1;
    }

    active = sc_res_info_get_active(rl);
    if (active == NULL) {
        fprintf(stderr, "%s ERROR: get free res_info failed\n", __func__);
        return -1;
    }

    sc_res_set_normal(&active->common);
    sc_res_copy_url(active->common.url, url, SC_RES_URL_MAX_LEN, 1);
#if DEBUG
    fprintf(stdout, "%s: copied url with parameter:%s\n", __func__, active->common.url);
#endif
    content_type = sc_res_content_type_obtain(url);
    sc_res_set_content_t(&active->common, content_type);

    ret = sc_res_info_gen_active_local_path(active);
    if (ret != 0) {
        fprintf(stderr, "%s ERROR: sc_res_info_gen_active_local_path failed, url %s\n", __func__, active->common.url);
        goto error;
    }

    if (normal != NULL) {
        *normal = active;
    }

    return 0;

error:
    sc_res_info_del(sc_res_info_list, (sc_res_info_t *)active);
    return -1;
}

int sc_res_info_add_origin(sc_res_list_t *rl, char *url, sc_res_info_origin_t **origin)
{
    int len;
    sc_res_info_origin_t *ri;
    sc_res_ctnt_t content_type;

    if (rl == NULL || url == NULL) {
        return -1;
    }

    if (!sc_res_info_permit_adding(url)) {
        fprintf(stderr, "%s: unknown url %s\n", __func__, url);
        return -1;
    }

    len = strlen(url);
    if (len >= SC_RES_URL_MAX_LEN) {
        fprintf(stderr, "%s ERROR: url is longer than MAX_LEN %d\n", __func__, SC_RES_URL_MAX_LEN);
        return -1;
    }

    ri = sc_res_info_get_origin(rl);
    if (ri == NULL) {
        fprintf(stderr, "%s ERROR: get free res_info failed\n", __func__);
        return -1;
    }

    sc_res_copy_url(ri->common.url, url, SC_RES_URL_MAX_LEN, 1);
#if DEBUG
    fprintf(stdout, "%s: copied url with parameter:%s\n", __func__, ri->common.url);
#endif

    content_type = sc_res_content_type_obtain(url);
    sc_res_set_content_t(&ri->common, content_type);

    if (origin != NULL) {
        *origin = ri;
    }

    return 0;
}

int sc_res_info_add_parsed(sc_res_list_t *rl,
                           sc_res_info_origin_t *origin,
                           char *url,
                           sc_res_info_active_t **parsed)
{
    int len;
    sc_res_info_active_t *active;
    sc_res_ctnt_t content_type;

    if (rl == NULL || origin == NULL || url == NULL) {
        return -1;
    }

    if (!sc_res_info_permit_adding(url)) {
        fprintf(stderr, "%s: unknown url %s\n", __func__, url);
        return -1;
    }

    len = strlen(url);
    if (len >= SC_RES_URL_MAX_LEN) {
        fprintf(stderr, "%s ERROR: url is longer than MAX_LEN %d\n", __func__, SC_RES_URL_MAX_LEN);
        return -1;
    }

    active = sc_res_info_get_active(rl);
    if (active == NULL) {
        fprintf(stderr, "%s ERROR: get free res_info failed\n", __func__);
        return -1;
    }

    sc_res_set_parsed(&active->common);
    sc_res_copy_url(active->common.url, url, SC_RES_URL_MAX_LEN, 1);
#if DEBUG
    fprintf(stdout, "%s: copied url with parameter:%s\n", __func__, active->common.url);
#endif
    content_type = sc_res_content_type_obtain(url);
    sc_res_set_content_t(&active->common, content_type);

    if (origin->child_cnt == 0) {  /* First derivative is come */
        origin->child = active;
    } else {
        active->siblings = origin->child;
        origin->child = active;
    }
    origin->child_cnt++;
    active->parent = origin;

    if (parsed != NULL) {
        *parsed = active;
    }

    return 0;
}

void sc_res_info_del(sc_res_list_t *rl, sc_res_info_t *ri)
{
    if (rl == NULL || ri == NULL) {
        return;
    }

    if (sc_res_is_origin(ri)) {
        /* zhaoyao XXX TODO FIXME: huge parsed URL stuff need to be done before put it */
        sc_res_info_put(rl, ri);
        return;
    }

    if (sc_res_is_stored(ri)) {
        fprintf(stderr, "%s WARNING: \n%s\n\tstored local file is not deleted\n", __func__, ri->url);
    }

    if (sc_res_is_notify(ri)) {
        fprintf(stderr, "%s WARNING: \n%s\n\thas notified snooping module\n", __func__, ri->url);
    }

    if (sc_res_is_normal(ri)) {
        sc_res_info_put(rl, ri);
        return;
    }

    if (sc_res_is_parsed(ri)) {
        /*
         * zhaoyao XXX TODO FIXME: huge origin URL stuff need to be done before put it,
         *                         should tell its parent.
         */
        sc_res_info_put(rl, ri);
        return;
    }

    fprintf(stderr, "%s ERROR: unknown type 0x%lx\n", __func__, ri->flag);
}

sc_res_info_active_t *sc_res_info_find_active(sc_res_list_t *rl, const char *url)
{
    sc_res_info_active_t *curr;
    int i;

    if (rl == NULL || url == NULL) {
        return NULL;
    }

    for (i = 0; i < SC_RES_INFO_NUM_MAX_ACTIVE; i++) {
        curr = &rl->active[i];
        if (strcmp(url, curr->common.url) == 0) {
            return curr;
        }
    }

    return NULL;
}

sc_res_info_origin_t *sc_res_info_find_origin(sc_res_list_t *rl, const char *url)
{
    sc_res_info_origin_t *curr;
    int i;

    if (rl == NULL || url == NULL) {
        return NULL;
    }

    for (i = 0; i < SC_RES_INFO_NUM_MAX_ORGIIN; i++) {
        curr = &rl->origin[i];
        if (strcmp(url, curr->common.url) == 0) {
            return curr;
        }
    }

    return NULL;
}

/*
 * zhaoyao: exact matching, finding in all res_info, TODO XXX add fuzzy matching or pattern matching.
 */
sc_res_info_t *sc_res_info_find(sc_res_list_t *rl, const char *url)
{
    sc_res_info_active_t *active;
    sc_res_info_origin_t *origin;

    active = sc_res_info_find_active(rl, url);
    if (active != NULL) {
        return (sc_res_info_t *)active;
    }

    origin = sc_res_info_find_origin(rl, url);
    if (origin != NULL) {
        return (sc_res_info_t *)origin;
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
    sc_res_info_active_t *active;

    if (ri == NULL) {
        return -1;
    }

    if (!sc_res_is_normal(ri) && !sc_res_is_parsed(ri)) {
        return -1;
    }

    /* zhaoyao TODO: sohu video should generate real_url based on ri->url */
    if (sc_url_is_sohu_file_url(ri->url)) {
        fprintf(stderr, "%s sohu video retry download not suppoted now...\n", __func__);
        return 0;
    }

    active = (sc_res_info_active_t *)ri;
    ret = sc_ngx_download(ri->url, active->localpath);
    if (ret != 0) {
        fprintf(stderr, "%s ERROR: %s failed\n", __func__, ri->url);
    }

    return ret;
}

static int sc_res_list_process_origin(sc_res_list_t *rl)
{
    /*
     * zhaoyao TODO: origin process
     */
     
    if (rl == NULL) {
        return -1;
    }

    return 0;
}

static int sc_res_list_process_active(sc_res_list_t *rl)
{
    sc_res_info_active_t *curr;
    sc_res_info_t *ri;
    int i, err = 0, ret;

    if (rl == NULL) {
        return -1;
    }

    for (i = 0; i < SC_RES_INFO_NUM_MAX_ACTIVE; i++) {
        curr = &rl->active[i];
        ri = &curr->common;

        if (ri->url[0] == '\0') {
            continue;
        }

        if (!sc_res_is_normal(ri) && !sc_res_is_parsed(ri)) {
            fprintf(stderr, "%s ERROR: active type check wrong, type: 0x%lx\n", __func__, ri->flag);
            continue;
        }

        if (!sc_res_is_stored(ri)) {
            /* zhaoyao XXX: timeout, and re-download */
            if (sc_res_is_d_fail(ri)) {   /* Nginx tell us to re-download it */
                ret = sc_res_retry_download(ri);
                if (ret != 0) {
                    fprintf(stderr, "%s inform Nginx re-download %s failed\n", __func__, ri->url);
                    err++;
                } else {
                    fprintf(stdout, "%s inform Nginx re-download %s success\n", __func__, ri->url);
                    sc_res_unset_d_fail(ri);
                }
            }
            continue;
        }

        if (!sc_res_is_notify(ri)) {
            fprintf(stderr, "%s use sc_snooping_do_add %s\n", __func__, ri->url);
            ret = sc_snooping_do_add(ri);
            if (ret != 0) {
                fprintf(stderr, "%s inform Snooping Module add URL failed\n", __func__);
                err++;
            }
        }

        if (sc_res_is_flv(ri) && !sc_res_is_kf_crt(ri)) {
            fprintf(stderr, "%s use sc_kf_flv_create_info %s\n", __func__, ri->url);
            ret = sc_kf_flv_create_info(curr);
            if (ret != 0) {
                fprintf(stderr, "%s create FLV key frame information failed\n", __func__);
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

    ret = sc_res_list_process_origin(rl);
    if (ret < 0) {
        fprintf(stderr, "%s ERROR: sc_res_list_process_origin, return %d\n", __func__, ret);
        return ret;
    }

    err = err + ret;
    ret = sc_res_list_process_active(rl);
    if (ret < 0) {
        fprintf(stderr, "%s ERROR: sc_res_list_process_active, return %d\n", __func__, ret);
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
            fprintf(stderr, "%s exit...\n", __func__);
            break;
        } else if (ret > 0) {
            fprintf(stderr, "%s problem occured %d time(s)...\n", __func__, ret);
        }

        sleep(3);
    }

    return ((void *)0);
}
