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

#ifndef __SC_HEADER_H__
#define __SC_HEADER_H__

#define SC_NGX_ROOT_PATH                "/usr/local/hot-cache/"
#define SC_NGX_ROOT_PATH_LEN            (sizeof(SC_NGX_ROOT_PATH) - 1)
#define SC_NGX_DEFAULT_IP_ADDR          "127.0.0.1"
#define SC_NGX_DEFAULT_PORT             8089

#define SC_CLIENT_DEFAULT_IP_ADDR       "0.0.0.0"
#define SC_SNOOP_MOD_DEFAULT_IP_ADDR    "20.0.0.1"
#define SC_SNOOPING_SND_RCV_BUF_LEN     1024

#define SC_REDIRECT_KEY_WORD            "Location: http://20.0.0.99:8080"

#include "sc_resource.h"

int sc_ngx_download(char *url, char *local_path);

void sc_snooping_serve(int sockfd);
int sc_snooping_do_add(u32 sid, char *url);
int sc_snooping_do_del(u32 sid, char *url);

int sc_ld_file_process(char *fpath);
int sc_ld_init_and_load();
sc_res_info_mgmt_t *sc_ld_obtain_ctl_ld_youku();
sc_res_info_mgmt_t *sc_ld_obtain_ctl_ld_sohu();

int sc_get_yk_video(sc_res_info_mgmt_t *origin);
int sc_url_is_yk(char *url);
int sc_yk_url_to_local_path(char *url, char *local_path, int len);
int sc_yk_gen_origin_url(char *req_url, char *origin_url);
int sc_youku_download(sc_res_info_ctnt_t *parsed);
int sc_yk_add_ctnt_url(sc_res_info_ctnt_t *ctnt);
int sc_yk_get_vf(char *vf_url, char *referer);
int sc_yk_init_vf_adv();
int sc_yk_is_local_path(char *local_path);
int sc_yk_is_local_path_pure_vid(char *local_path);
int sc_yk_trans_vid_to_std_path(char *vid_path, char *std_path, unsigned int path_len);

int sc_get_sohu_video(sc_res_info_mgmt_t *origin);
int sc_url_is_sohu(char * url);
int sc_url_is_sohu_file_url(char *url);
int sc_sohu_file_url_to_local_path(char *file_url, char *local_path, int len);
int sc_sohu_gen_origin_url(char *req_url, char *origin_url);
int sc_sohu_download(sc_res_info_ctnt_t *parsed);
int sc_sohu_is_local_path(char *local_path);

#endif /* __SC_HEADER_H__ */

