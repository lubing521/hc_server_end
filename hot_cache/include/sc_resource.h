/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * sc_resource.h
 * Original Author: zhaoyao@ruijie.com.cn, 2014-03-11
 *
 * Hot cache application's resource header file.
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#ifndef __SC_RESOURCE_H__
#define __SC_RESOURCE_H__

#include <string.h>

#include "common.h"
#include "sc_header.h"

#define SC_RES_SHARE_MEM_ID        65510
#define SC_RES_SHARE_MEM_SIZE      (sizeof(sc_res_list_t))
#define SC_RES_SHARE_MEM_MODE      0666

#define SC_RES_INFO_NUM_MAX_MGMT   (0x1 << 10)
#define SC_RES_INFO_NUM_MAX_CTNT   (0x1 << 12)

#define SC_RES_LOCAL_PATH_MAX_LEN  256

typedef enum sc_res_gen_e {
    /* using sc_res_info_ctnt_t */
    SC_RES_GEN_T_CONTENT = 0,       /* Content of all resources */

    /* using sc_res_info_mgmt_t */
    SC_RES_GEN_T_ORIGIN,            /* Original URL needed to be parsed */
    SC_RES_GEN_T_NORMAL,            /* Snooping inform Nginx to directly download */
    SC_RES_GEN_T_CTRL_LD,           /* Special RI to control loaded local resources who can not find Origin parent */

    SC_RES_GEN_T_MAX,
} sc_res_gen_t;
#define sc_res_gen_set_t(ri, type)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->gen = type; \
        }                                           \
    } while (0)

typedef enum sc_res_site_e {
    SC_RES_SITE_YOUKU = 0,       /* www.youku.com */
    SC_RES_SITE_SOHU,            /* www.sohu.com */

    SC_RES_SITE_MAX,
} sc_res_site_t;
#define sc_res_site_set_t(ri, type)                       \
    do {                                            \
        if ((ri) != NULL) {                         \
            (ri)->site = type; \
        }                                           \
    } while (0)

typedef enum sc_res_mime_e {
    SC_RES_MIME_TEXT_HTML = 0,
    SC_RES_MIME_TEXT_M3U8,
    SC_RES_MIME_VIDEO_FLV,
    SC_RES_MIME_VIDEO_MP4,

    SC_RES_MIME_NOT_CARE,

    SC_RES_MIME_TYPE_MAX,
} sc_res_mime_t;
#define sc_res_mime_set_t(ri, type)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->mime = type; \
        }                                           \
    } while (0)

/* ri control flag */
#define SC_RES_FLAG_STORED    (0x00000100UL)   /* Resource is stored, can ONLY set by Nginx */
#define SC_RES_FLAG_D_FAIL    (0x00000200UL)   /* Resource download failed, set by Nginx, unset by SC */
#define SC_RES_FLAG_I_FAIL    (0x00000400UL)   /* Inform Nginx to download failed, set and unset by SC */
#define SC_RES_FLAG_NOTIFY    (0x00000800UL)   /* Stored resource URL is notified to Snooping Module */

typedef struct sc_res_info_s {
    unsigned long id;
    unsigned long flag;
    unsigned long gen;

    u16           site;
    u16           mime;

    /*
     * zhaoyao XXX: 对于origin，由于来自Snooping模块的URL千差万别，需要根据不同视频网站构建统一的URL
     *              格式，origin本来对Snooping模块，且只要具备恢复视频资源信息的功能即可；
     *
     *              对于ctrl_ld，由于是我们自己创建的管理节点，其url可由主页地址表示；
     *
     *              对于parsed和normal，该URL必须与资源在本地存储的路径(local_path)对应起来，而且该
     *              url还会通知Snooping模块做重定向；
     *              对于loaded，其和parsed、normal一致。
     *              注意: 对于sohu视频，parsed的URL仍然不是最终的资源URL。
     */
    char url[HTTP_URL_MAX_LEN];
} sc_res_info_t;

/*
 * sc_res_info_mgmt_t is used for managing sc_res_info_ctnt_t, it is logic ri information.
 */
typedef struct sc_res_info_mgmt_s {
    /* zhaoyao XXX: common must be the very first member */
    struct sc_res_info_s common;

    /* Multi-thread coherence guarantee */
    pthread_rwlock_t rwlock;

    unsigned long child_cnt;
    struct sc_res_info_ctnt_s *child;
} sc_res_info_mgmt_t;

/*
 * sc_res_info_ctnt_t is used for holding real resources (contents) information.
 */
typedef struct sc_res_info_ctnt_s {
    /* zhaoyao XXX: common must be the very first member */
    struct sc_res_info_s common;

    struct sc_res_info_mgmt_s *parent;
    struct sc_res_info_ctnt_s *siblings;

    /* zhaoyao XXX: stored resources' stored local path, file path is from $ROOT/localpath */
    char localpath[SC_RES_LOCAL_PATH_MAX_LEN];
} sc_res_info_ctnt_t;

#define sc_res_flag_set_d_fail(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_D_FAIL;  \
        }                                           \
    } while (0)

#define sc_res_flag_set_i_fail(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_I_FAIL;  \
        }                                           \
    } while (0)

#define sc_res_flag_set_stored(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_STORED;  \
        }                                           \
    } while (0)

#define sc_res_flag_set_notify(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_NOTIFY;  \
        }                                           \
    } while (0)

#define sc_res_flag_unset_d_fail(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag &= ((~0UL) ^ SC_RES_FLAG_D_FAIL);  \
        }                                           \
    } while (0)

#define sc_res_flag_unset_i_fail(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag &= ((~0UL) ^ SC_RES_FLAG_I_FAIL);  \
        }                                           \
    } while (0)

#define sc_res_flag_is_d_fail(ri)    ((ri)->flag & SC_RES_FLAG_D_FAIL)
#define sc_res_flag_is_i_fail(ri)    ((ri)->flag & SC_RES_FLAG_I_FAIL)
#define sc_res_flag_is_stored(ri)    ((ri)->flag & SC_RES_FLAG_STORED)
#define sc_res_flag_is_notify(ri)    ((ri)->flag & SC_RES_FLAG_NOTIFY)

#define sc_res_gen_set_normal(ri)                       \
    do {                                            \
        sc_res_gen_set_t((ri), SC_RES_GEN_T_NORMAL);  \
    } while (0)

#define sc_res_gen_set_origin(ri)                       \
    do {                                            \
        sc_res_gen_set_t((ri), SC_RES_GEN_T_ORIGIN);  \
    } while (0)

#define sc_res_gen_set_ctrl_ld(ri)                       \
    do {                                            \
        sc_res_gen_set_t((ri), SC_RES_GEN_T_CTRL_LD);  \
    } while (0)

#define sc_res_gen_set_content(ri)                       \
    do {                                            \
        sc_res_gen_set_t((ri), SC_RES_GEN_T_CONTENT);  \
    } while (0)

static inline int sc_res_gen_is_normal(sc_res_info_t *ri)
{
    if (ri != NULL) {
        return (ri->gen == SC_RES_GEN_T_NORMAL);
    }

    return 0;
}
static inline int sc_res_gen_is_origin(sc_res_info_t *ri)
{
    if (ri != NULL) {
        return (ri->gen == SC_RES_GEN_T_ORIGIN);
    }

    return 0;
}
static inline int sc_res_gen_is_ctrl_ld(sc_res_info_t *ri)
{
    if (ri != NULL) {
        return (ri->gen == SC_RES_GEN_T_CTRL_LD);
    }

    return 0;
}
static inline int sc_res_gen_is_content(sc_res_info_t *ri)
{
    if (ri != NULL) {
        return (ri->gen == SC_RES_GEN_T_CONTENT);
    }

    return 0;
}

static inline int sc_res_info_is_mgmt(sc_res_info_t *ri)
{
    if (sc_res_gen_is_origin(ri)  ||
        sc_res_gen_is_ctrl_ld(ri) ||
        sc_res_gen_is_normal(ri)) {

        return 1;
    }

    return 0;
}
static inline int sc_res_info_is_ctnt(sc_res_info_t *ri)
{
    return sc_res_gen_is_content(ri);
}

#define sc_res_site_set_youku(ri)                       \
    do {                                            \
        sc_res_site_set_t((ri), SC_RES_SITE_YOUKU);  \
    } while (0)

#define sc_res_site_set_sohu(ri)                       \
    do {                                            \
        sc_res_site_set_t((ri), SC_RES_SITE_SOHU);  \
    } while (0)

static inline sc_res_site_t sc_res_site_obtain(sc_res_info_t *ri)
{
    if (ri != NULL) {
        return ri->site;
    }

    return SC_RES_SITE_MAX;
}
static inline int sc_res_site_is_youku(sc_res_info_t *ri)
{
    return (sc_res_site_obtain(ri) == SC_RES_SITE_YOUKU);
}
static inline int sc_res_site_is_sohu(sc_res_info_t *ri)
{
    return (sc_res_site_obtain(ri) == SC_RES_SITE_SOHU);
}
static inline void sc_res_site_inherit(sc_res_info_mgmt_t *parent, sc_res_info_ctnt_t *child)
{
    sc_res_site_t site;

    site = sc_res_site_obtain((sc_res_info_t *)parent);

    sc_res_site_set_t((sc_res_info_t *)child, site);
}

static inline int sc_res_mime_is_flv(sc_res_info_t *ri)
{
    if (ri != NULL) {
        return (ri->mime == SC_RES_MIME_VIDEO_FLV);
    }

    return 0;
}
static inline int sc_res_mime_is_mp4(sc_res_info_t *ri)
{
    if (ri != NULL) {
        return (ri->mime == SC_RES_MIME_VIDEO_MP4);
    }

    return 0;
}
static inline int sc_res_mime_is_html(sc_res_info_t *ri)
{
    if (ri != NULL) {
        return (ri->mime == SC_RES_MIME_TEXT_HTML);
    }

    return 0;
}
static inline int sc_res_mime_is_m3u8(sc_res_info_t *ri)
{
    if (ri != NULL) {
        return (ri->mime == SC_RES_MIME_TEXT_M3U8);
    }

    return 0;
}

typedef struct sc_res_list_s {
    int mgmt_cnt;
    struct sc_res_info_mgmt_s mgmt[SC_RES_INFO_NUM_MAX_MGMT];
    struct sc_res_info_mgmt_s *mgmt_free;

    int ctnt_cnt;
    struct sc_res_info_ctnt_s ctnt[SC_RES_INFO_NUM_MAX_CTNT];
    struct sc_res_info_ctnt_s *ctnt_free;
} sc_res_list_t;

/*
 * zhaoyao: faster, and truncate parameter in url;
 *          fpath buffer size is len.
 */
static inline int sc_res_map_to_file_path(sc_res_info_ctnt_t *ctnt, char *fpath, unsigned int len)
{
    if (ctnt == 0 || fpath == 0) {
        return -1;
    }
    if (len <= strlen(ctnt->localpath) + SC_NGX_ROOT_PATH_LEN) {
        return -1;
    }

    bzero(fpath, len);
    fpath = strcat(fpath, SC_NGX_ROOT_PATH);
    fpath = strcat(fpath, ctnt->localpath);

    return 0;
}

extern sc_res_list_t *sc_res_info_list;
int sc_res_list_alloc_and_init(sc_res_list_t **prl);
int sc_res_list_destroy_and_uninit();
void *sc_res_list_process_thread(void *arg);

int sc_res_info_add_normal(sc_res_list_t *rl, char *url, sc_res_info_mgmt_t **ptr_ret);
int sc_res_info_add_origin(sc_res_list_t *rl, char *url, sc_res_info_mgmt_t **ptr_ret);
int sc_res_info_add_ctrl_ld(sc_res_list_t *rl, char *url, sc_res_info_mgmt_t **ptr_ret);
int sc_res_info_add_ctnt(sc_res_list_t *rl,
                         sc_res_info_mgmt_t *mgmt,
                         char *url,
                         sc_res_info_ctnt_t **ptr_ret);
void sc_res_info_del(sc_res_list_t *rl, sc_res_info_t *ri);


sc_res_info_ctnt_t *sc_res_info_find_ctnt(sc_res_list_t *rl, const char *url);
sc_res_info_mgmt_t *sc_res_info_find_mgmt(sc_res_list_t *rl, const char *url);

void sc_res_copy_url(char *url, char *o_url, unsigned int len, char with_para);
int sc_res_gen_origin_url(char *req_url, char *origin_url);
int sc_res_url_to_local_path_default(char *url, char *local_path, int len);
hc_result_t sc_res_info_handle_cached(sc_res_info_mgmt_t *ctrl_ld, sc_res_info_ctnt_t *parsed);
int sc_res_notify_ri_url(sc_res_info_t *ri);
int sc_res_recover_url_from_local_path(char *local_path, char *url);

#endif /* __SC_RESOURCE_H__ */

