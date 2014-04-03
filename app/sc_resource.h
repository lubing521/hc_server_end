/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * header.h
 * Original Author: zhaoyao@ruijie.com.cn, 2014-03-11
 *
 * Hot cache application's common header file.
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#ifndef __SC_RESOURCE_H__
#define __SC_RESOURCE_H__

#include <string.h>

#include "sc_header.h"

#define SC_RES_SHARE_MEM_ID        65513
#define SC_RES_SHARE_MEM_SIZE      (sizeof(sc_res_list_t))
#define SC_RES_SHARE_MEM_MODE      0666

#define SC_RES_INFO_NUM_MAX_ORGIIN (0x1 << 8)
#define SC_RES_INFO_NUM_MAX_ACTIVE (0x1 << 10)

#define SC_RES_URL_MAX_LEN         512

#define SC_RES_GEN_T_SHIFT  24
#define SC_RES_GEN_T_MASK   0xFF
typedef enum sc_res_gen_e {
    SC_RES_GEN_T_NORMAL = 0,        /* Snooping inform Nginx to directly download */
    SC_RES_GEN_T_ORIGIN,            /* Original URL needed to be parsed */
    SC_RES_GEN_T_PARSED,            /* Parsed URL from original one */

    SC_RES_GEN_T_MAX,
} sc_res_gen_t;
#define sc_res_set_gen_t(ri, type)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= (((type) & SC_RES_GEN_T_MASK) << SC_RES_GEN_T_SHIFT); \
        }                                           \
    } while (0)

#define SC_RES_CTNT_T_SHIFT  0
#define SC_RES_CTNT_T_MASK   0XFF
typedef enum sc_res_ctnt_e {
    SC_RES_CTNT_TEXT_HTML = 0,
    SC_RES_CTNT_VIDEO_FLV,
    SC_RES_CTNT_VIDEO_MP4,

    SC_RES_CTNT_UNSPEC,

    SC_RES_CTNT_TYPE_MAX,
} sc_res_ctnt_t;
#define sc_res_set_content_t(ri, type)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= (((type) & SC_RES_CTNT_T_MASK) << SC_RES_CTNT_T_SHIFT); \
        }                                           \
    } while (0)

/*
 * zhaoyao XXX TODO FIXME: sc_res_video_t should be replaced by sc_res_ctnt_t.
 */
typedef enum sc_res_video_e {
    SC_RES_VIDEO_FLV = 0,
    SC_RES_VIDEO_MP4,

    SC_RES_VIDEO_MAX,
} sc_res_video_t;

static inline int sc_res_video_type_is_valid(sc_res_video_t vtype)
{
    if (vtype < SC_RES_VIDEO_MAX) {
        return 1;
    }

    return 0;
}

/* ri control flag */
#define SC_RES_F_CTRL_STORED    (0x00000100UL)   /* Resource is stored, can ONLY set by Nginx */
#define SC_RES_F_CTRL_D_FAIL    (0x00000200UL)   /* Resource download failed, set by Nginx, unset by SC */
#define SC_RES_F_CTRL_NOTIFY    (0x00000400UL)   /* Stored resource URL is notified to Snooping Module */
#define SC_RES_F_CTRL_KF_CRT    (0x00000800UL)   /* Resource's key frame information is created */

typedef struct sc_res_info_s {
    unsigned long id;
    unsigned long flag;
    char url[SC_RES_URL_MAX_LEN]; /* zhaoyao XXX: stored file's local path is generated from it. */
} sc_res_info_t;

typedef struct sc_res_info_origin_s {
    /* zhaoyao XXX: common must be the very first member */
    struct sc_res_info_s common;

    unsigned long child_cnt[SC_RES_VIDEO_MAX];
    struct sc_res_info_active_s *child[SC_RES_VIDEO_MAX];
} sc_res_info_origin_t;

/*
 * sc_res_info_active_t is both used for SC_RES_GEN_T_NORMAL and SC_RES_GEN_T_PARSED.
 */
typedef struct sc_res_info_active_s {
    /* zhaoyao XXX: common must be the very first member */
    struct sc_res_info_s common;

    struct sc_res_info_origin_s *parent;
    struct sc_res_info_active_s *siblings;

    /* zhaoyao XXX FIXME: at current time, we treat all active as video */
    enum sc_res_video_e vtype;

    sc_kf_flv_info_t kf_info[SC_KF_FLV_MAX_NUM];
    unsigned long kf_num;
} sc_res_info_active_t;

#define sc_res_set_kf_crt(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_F_CTRL_KF_CRT;  \
        }                                           \
    } while (0)

#define sc_res_set_d_fail(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_F_CTRL_D_FAIL;  \
        }                                           \
    } while (0)

#define sc_res_set_stored(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_F_CTRL_STORED;  \
        }                                           \
    } while (0)

#define sc_res_set_notify(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_F_CTRL_NOTIFY;  \
        }                                           \
    } while (0)

#define sc_res_unset_d_fail(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag &= ((~0UL) ^ SC_RES_F_CTRL_D_FAIL);  \
        }                                           \
    } while (0)

#define sc_res_set_normal(ri)                       \
    do {                                            \
        sc_res_set_gen_t((ri), SC_RES_GEN_T_NORMAL);  \
    } while (0)

#define sc_res_set_origin(ri)                       \
    do {                                            \
        sc_res_set_gen_t((ri), SC_RES_GEN_T_ORIGIN);  \
    } while (0)

#define sc_res_set_parsed(ri)                       \
    do {                                            \
        sc_res_set_gen_t((ri), SC_RES_GEN_T_PARSED);  \
    } while (0)

#define sc_res_is_kf_crt(ri)    ((ri)->flag & SC_RES_F_CTRL_KF_CRT)
#define sc_res_is_d_fail(ri)    ((ri)->flag & SC_RES_F_CTRL_D_FAIL)
#define sc_res_is_stored(ri)    ((ri)->flag & SC_RES_F_CTRL_STORED)
#define sc_res_is_notify(ri)    ((ri)->flag & SC_RES_F_CTRL_NOTIFY)

static inline int sc_res_is_normal(sc_res_info_t *ri)
{
    int type;

    if (ri != NULL) {
        type = (ri->flag >> SC_RES_GEN_T_SHIFT) & SC_RES_GEN_T_MASK;
        return (type == SC_RES_GEN_T_NORMAL);
    }

    return 0;
}
static inline int sc_res_is_origin(sc_res_info_t *ri)
{
    int type;

    if (ri != NULL) {
        type = (ri->flag >> SC_RES_GEN_T_SHIFT) & SC_RES_GEN_T_MASK;
        return (type == SC_RES_GEN_T_ORIGIN);
    }

    return 0;
}
static inline int sc_res_is_parsed(sc_res_info_t *ri)
{
    int type;

    if (ri != NULL) {
        type = (ri->flag >> SC_RES_GEN_T_SHIFT) & SC_RES_GEN_T_MASK;
        return (type == SC_RES_GEN_T_PARSED);
    }

    return 0;
}

typedef struct sc_res_list_s {
    int origin_cnt;
    struct sc_res_info_origin_s origin[SC_RES_INFO_NUM_MAX_ORGIIN];
    struct sc_res_info_origin_s *origin_free;

    int active_cnt;
    struct sc_res_info_active_s active[SC_RES_INFO_NUM_MAX_ACTIVE];
    struct sc_res_info_active_s *active_free;
} sc_res_list_t;

/*
 * zhaoyao: faster, and truncate parameter in url;
 *          fpath buffer size is len.
 */
static inline int sc_res_map_url_to_file_path(char *url, char *fpath, unsigned int len)
{
    char *p;
    int i, first_slash = 1;

    if (url == 0 || fpath == 0) {
        return -1;
    }
    if (strncmp(url, "http://", 7) == 0) {
        url = url + 7;
    }
    if (len <= strlen(url) + SC_NGX_ROOT_PATH_LEN) {
        return -1;
    }

    bzero(fpath, len);
    fpath = strcat(fpath, SC_NGX_ROOT_PATH);
    fpath = fpath + SC_NGX_ROOT_PATH_LEN;
    for (p = url, i = 0; *p != '?' && *p != '\0'; p++, i++) {
        if (first_slash && *p == '.') {
            fpath[i] = '_';
            continue;
        }
        if (*p == '/') {
            if (first_slash) {
                fpath[i] = *p;
                first_slash = 0;
            } else {
                fpath[i] = '_';
            }
        } else {
            fpath[i] = *p;
        }
    }

    return 0;
}

extern sc_res_list_t *sc_res_info_list;
int sc_res_list_alloc_and_init(sc_res_list_t **prl);
int sc_res_list_destroy_and_uninit();
void *sc_res_list_process_thread(void *arg);
int sc_res_info_add_normal(sc_res_list_t *rl, char *url, sc_res_info_active_t **normal);
int sc_res_info_add_origin(sc_res_list_t *rl, char *url, sc_res_info_origin_t **origin);
int sc_res_info_add_parsed(sc_res_list_t *rl,
                           sc_res_info_origin_t *origin,
                           sc_res_video_t vtype,
                           char *url,
                           sc_res_info_active_t **parsed);
sc_res_info_t *sc_res_info_find(sc_res_list_t *rl, const char *url);
sc_res_info_active_t *sc_res_info_find_active(sc_res_list_t *rl, const char *url);
sc_res_info_origin_t *sc_res_info_find_origin(sc_res_list_t *rl, const char *url);
void sc_res_copy_url(char *url, char *o_url, unsigned int len, char with_para);
sc_res_video_t sc_res_video_type_obtain(char *str);
int sc_res_get_local_path(sc_res_info_t *ri, char *local_path);

#endif /* __SC_RESOURCE_H__ */

