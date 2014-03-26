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

#include "sc_header.h"

#define SC_RES_SHARE_MEM_ID        65507
#define SC_RES_SHARE_MEM_SIZE      (sizeof(sc_res_list_t) + (sizeof(sc_res_info_t) << SC_RES_NUM_MAX_SHIFT))
#define SC_RES_SHARE_MEM_MODE      0666

#define SC_RES_URL_MAX_LEN         256
#define SC_RES_NUM_MAX_SHIFT       10          /* Number is 1 << 10 */

#define SC_RES_FLAG_NORMAL    (0x00000001UL)   /* Snooping inform Nginx to directly download */
#define SC_RES_FLAG_ORIGIN    (0x00000002UL)   /* Original URL needed to be parsed */
#define SC_RES_FLAG_STORED    (0x00000004UL)   /* Resource is stored in local file system */
#define SC_RES_FLAG_NOTIFY    (0x00000008UL)   /* Stored resource URL is notified to Snooping Module */
#define SC_RES_FLAG_KF_CRT    (0x00000010UL)   /* Resource's key frame information is created */
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
        
#define sc_res_is_kf_crt(ri)    ((ri)->flag & SC_RES_FLAG_KF_CRT)
#define sc_res_is_stored(ri)    ((ri)->flag & SC_RES_FLAG_STORED)
#define sc_res_is_notify(ri)    ((ri)->flag & SC_RES_FLAG_NOTIFY)
#define sc_res_is_normal(ri)    ((ri)->flag & SC_RES_FLAG_NORMAL)
#define sc_res_is_origin(ri)    ((ri)->flag & SC_RES_FLAG_ORIGIN)

typedef struct sc_res_list_s {
    int total;                              /* Equals to 1 << SC_RES_NUM_MAX_SHIFT */
    sc_res_info_t *free;
    sc_res_info_t res[];
} sc_res_list_t;

extern sc_res_list_t *sc_res_info_list;
sc_res_list_t *sc_res_list_alloc_and_init();
int sc_res_list_destroy_and_uninit();
void *sc_res_list_process_thread(void *arg);
int sc_res_info_add_normal(sc_res_list_t *rl, char *url, sc_res_info_t **normal);
int sc_res_info_add_origin(sc_res_list_t *rl, char *url, sc_res_info_t **origin);
int sc_res_info_add_parsed(sc_res_list_t *rl,
                           sc_res_info_t *origin_ri,
                           char *url,
                           sc_res_info_t **parsed);
sc_res_info_t *sc_res_info_find(sc_res_list_t *rl, const char *url);
void sc_res_copy_url(char *url, char *o_url, char with_para);
int sc_res_map_url_to_file_path(char *url, char *fpath);

#endif /* __SC_RESOURCE_H__ */

