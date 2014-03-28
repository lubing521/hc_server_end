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

#define SC_RES_SHARE_MEM_ID        65511
#define SC_RES_SHARE_MEM_SIZE      (sizeof(sc_res_list_t) + (sizeof(sc_res_info_t) << SC_RES_NUM_MAX_SHIFT))
#define SC_RES_SHARE_MEM_MODE      0666

#define SC_RES_URL_MAX_LEN         512
#define SC_RES_NUM_MAX_SHIFT       10          /* Number is 1 << 10 */

/* URL type flag, also ri type flag */
#define SC_RES_FLAG_NORMAL    (0x00000001UL)   /* Snooping inform Nginx to directly download */
#define SC_RES_FLAG_ORIGIN    (0x00000002UL)   /* Original URL needed to be parsed */
#define SC_RES_FLAG_PARSED    (0x00000004UL)   /* Parsed URL from original one */

/* ri control flag */
#define SC_RES_FLAG_STORED    (0x00000010UL)   /* Resource is stored, can ONLY set by Nginx */
#define SC_RES_FLAG_D_FAIL    (0x00000020UL)   /* Resource download failed, set by Nginx, unset by SC */
#define SC_RES_FLAG_NOTIFY    (0x00000040UL)   /* Stored resource URL is notified to Snooping Module */
#define SC_RES_FLAG_KF_CRT    (0x00000080UL)   /* Resource's key frame information is created */
typedef struct sc_res_info_s {
    unsigned long id;
    unsigned long flag;
    char url[SC_RES_URL_MAX_LEN];

    /* Keyframe information */
    sc_kf_flv_info_t kf_info[SC_KF_FLV_MAX_NUM];
    unsigned long kf_num;

    /* origin URL has */
    unsigned long cnt;                 /* Derivative's quantity */
    struct sc_res_info_s *parsed;      /* Derivative of origin URL*/

    /* parsed type URL has */
    struct sc_res_info_s *parent;      /* My origin URL */
    struct sc_res_info_s *siblings;    /* Multi-parsed URL's siblings */
} sc_res_info_t;

#define sc_res_set_kf_crt(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_KF_CRT;  \
        }                                           \
    } while (0)

#define sc_res_set_d_fail(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_D_FAIL;  \
        }                                           \
    } while (0)

#define sc_res_set_stored(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_STORED;  \
        }                                           \
    } while (0)

#define sc_res_set_notify(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_NOTIFY;  \
        }                                           \
    } while (0)

#define sc_res_set_normal(ri)                       \
    do {                                            \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_NORMAL;  \
        }                                           \
    } while (0)

#define sc_res_set_origin(ri)                       \
    do {                                            \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_ORIGIN;  \
        }                                           \
    } while (0)

#define sc_res_set_parsed(ri)                       \
    do {                                            \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RES_FLAG_PARSED;  \
        }                                           \
    } while (0)

#define sc_res_unset_d_fail(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag &= ((~0UL) ^ SC_RES_FLAG_D_FAIL);  \
        }                                           \
    } while (0)
        
#define sc_res_is_kf_crt(ri)    ((ri)->flag & SC_RES_FLAG_KF_CRT)
#define sc_res_is_d_fail(ri)    ((ri)->flag & SC_RES_FLAG_D_FAIL)
#define sc_res_is_stored(ri)    ((ri)->flag & SC_RES_FLAG_STORED)
#define sc_res_is_notify(ri)    ((ri)->flag & SC_RES_FLAG_NOTIFY)
#define sc_res_is_normal(ri)    ((ri)->flag & SC_RES_FLAG_NORMAL)
#define sc_res_is_origin(ri)    ((ri)->flag & SC_RES_FLAG_ORIGIN)
#define sc_res_is_parsed(ri)    ((ri)->flag & SC_RES_FLAG_PARSED)

typedef struct sc_res_list_s {
    int total;                              /* Equals to 1 << SC_RES_NUM_MAX_SHIFT */
    sc_res_info_t *free;
    sc_res_info_t res[];
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
int sc_res_info_add_normal(sc_res_list_t *rl, char *url, sc_res_info_t **normal);
int sc_res_info_add_origin(sc_res_list_t *rl, char *url, sc_res_info_t **origin);
int sc_res_info_add_parsed(sc_res_list_t *rl,
                           sc_res_info_t *origin_ri,
                           char *url,
                           sc_res_info_t **parsed);
sc_res_info_t *sc_res_info_find(sc_res_list_t *rl, const char *url);
void sc_res_copy_url(char *url, char *o_url, unsigned int len, char with_para);

#endif /* __SC_RESOURCE_H__ */

