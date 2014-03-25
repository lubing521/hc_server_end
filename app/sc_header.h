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

#define SC_NGX_DEFAULT_IP_ADDR          "127.0.0.1"
#define SC_NGX_DEFAULT_PORT             8089

#define SC_CLIENT_DEFAULT_IP_ADDR       "0.0.0.0"
#define SC_SNOOP_MOD_DEFAULT_IP_ADDR    "20.0.0.1"

#define SC_RESOURCE_SHARE_MEM_ID        65509
#define SC_RESOURCE_SHARE_MEM_SIZE      (sizeof(sc_resource_list_t) + (sizeof(sc_resource_info_t) << SC_RESOURCE_NUM_MAX_SHIFT))
#define SC_RESOURCE_SHARE_MEM_MODE      0666

#define SC_RESOURCE_URL_MAX_LEN         256
#define SC_RESOURCE_NUM_MAX_SHIFT       10          /* Number is 1 << 10 */

#define SC_RESOURCE_FLAG_NORMAL    (0x00000001UL)   /* Snooping inform Nginx to directly download */
#define SC_RESOURCE_FLAG_ORIGIN    (0x00000002UL)   /* Original URL needed to be parsed */
#define SC_RESOURCE_FLAG_STORED    (0x00000004UL)   /* Resource is stored in local file system */
#define SC_RESOURCE_FLAG_NOTIFY    (0x00000008UL)   /* Stored resource URL is notified to Snooping Module */
typedef struct sc_resource_info_s {
    unsigned long id;
    unsigned long flag;
    char url[SC_RESOURCE_URL_MAX_LEN];

    /* origin URL has */
    unsigned long cnt;                      /* Derivative's quantity */
    struct sc_resource_info_s *parsed;      /* Derivative of origin URL*/

    /* parsed type URL has */
    struct sc_resource_info_s *parent;      /* My origin URL */
    struct sc_resource_info_s *siblings;    /* Multi-parsed URL's siblings */
} sc_resource_info_t;

#define sc_res_set_stored(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RESOURCE_FLAG_STORED;  \
        }                                           \
    } while (0)

#define sc_res_set_notify(ri)                       \
    do {						                    \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RESOURCE_FLAG_NOTIFY;  \
        }                                           \
    } while (0)

#define sc_res_set_normal(ri)                       \
    do {                                            \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RESOURCE_FLAG_NORMAL;  \
        }                                           \
    } while (0)

#define sc_res_set_origin(ri)                       \
    do {                                            \
        if ((ri) != NULL) {                         \
            (ri)->flag |= SC_RESOURCE_FLAG_ORIGIN;  \
        }                                           \
    } while (0)

#define sc_res_is_stored(ri)    ((ri)->flag & SC_RESOURCE_FLAG_STORED)
#define sc_res_is_notify(ri)    ((ri)->flag & SC_RESOURCE_FLAG_NOTIFY)
#define sc_res_is_normal(ri)    ((ri)->flag & SC_RESOURCE_FLAG_NORMAL)
#define sc_res_is_origin(ri)    ((ri)->flag & SC_RESOURCE_FLAG_ORIGIN)

typedef struct sc_resource_list_s {
    int total;                              /* Equals to 1 << SC_RESOURCE_NUM_MAX_SHIFT */
    sc_resource_info_t *free;
    sc_resource_info_t res[];
} sc_resource_list_t;

int sc_ngx_download(char *ngx_ip, char *url);

void sc_snooping_serve(int sockfd);
int sc_snooping_do_add(sc_resource_info_t *ri);

extern sc_resource_list_t *sc_resource_info_list;
sc_resource_list_t *sc_res_list_alloc_and_init();
int sc_res_list_destroy_and_uninit();
void *sc_res_list_process_thread(void *arg);
int sc_res_info_add_normal(sc_resource_list_t *rl, const char *url, sc_resource_info_t **normal);
int sc_res_info_add_origin(sc_resource_list_t *rl, const char *url, sc_resource_info_t **origin);
int sc_res_info_add_parsed(sc_resource_list_t *rl,
                           sc_resource_info_t *origin_ri,
                           const char *url,
                           sc_resource_info_t **parsed);



int sc_get_yk_video(char *url, sc_resource_info_t *origin);
int sc_url_is_yk(char *url);

