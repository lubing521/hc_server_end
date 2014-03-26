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

#include "sc_resource.h"

#define SC_NGX_DEFAULT_IP_ADDR          "127.0.0.1"
#define SC_NGX_DEFAULT_PORT             8089

#define SC_CLIENT_DEFAULT_IP_ADDR       "0.0.0.0"
#define SC_SNOOP_MOD_DEFAULT_IP_ADDR    "20.0.0.1"

int sc_ngx_download(char *ngx_ip, char *url);

void sc_snooping_serve(int sockfd);
int sc_snooping_do_add(sc_res_info_t *ri);

int sc_get_yk_video(char *url, sc_res_info_t *origin);
int sc_url_is_yk(char *url);

#endif /* __SC_HEADER_H__ */

