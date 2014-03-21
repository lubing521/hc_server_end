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

#define SC_NGX_DEFAULT_IP_ADDR  "127.0.0.1"
#define SC_NGX_DEFAULT_PORT     8089

int sc_ngx_download(char *ngx_ip, char *url);

int sc_get_yk_video(char *url);
int sc_url_is_yk(char *url);

