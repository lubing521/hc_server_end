/*
 * $id: $
 * Copyright (C)2001-2009 Èñ½ÝÍøÂç.  All rights reserved.
 *
 * http-snooping.h
 * Original Author:  tangyoucan@ruijie.com.cn, 2014-03-03
 *
 * HTTPÍ·ÎÄ¼þ
 *
 * History :
 *  v1.0    (tangyoucan@ruijie.com.cn)  2014-03-03  Initial version.
 */
#ifndef __HTTPSNOOPING_H__
#define __HTTPSNOOPING_H__

#define HTTP_SP_URL_LEN_MAX 512

#define HTTP_SP_NGX_SERVER_MAX (4)
#define HTTP_SP2C_PORT (9999)
#define HTTP_C2SP_PORT (10000)
#define HTTP_C2SP_ACTION_GET (1)
#define HTTP_C2SP_ACTION_GETNEXT (2)
#define HTTP_C2SP_ACTION_ADD (3)
#define HTTP_C2SP_ACTION_DELETE (4)
typedef struct http_c2sp_req_pkt_s {
    u32 session_id;
    u8 c2sp_action;
    u8 pad[3];
    u16 url_len;
    u8 usr_data[HTTP_SP_URL_LEN_MAX];
}http_c2sp_req_pkt_t;

typedef struct http_c2sp_res_pkt_s{
    u32 session_id;
    u8 status;
    u8 pad[3];
}http_c2sp_res_pkt_t;

#define HTTP_SP_STATUS_OK 0
#define HTTP_SP_STATUS_DEFAULT_ERROR 1

#define HTTP_SP2C_ACTION_PARSE 1
#define HTTP_SP2C_ACTION_DOWN 2
#define HTTP_SP2C_ACTION_GETNEXT 3
typedef struct http_sp2c_req_pkt_s
{
    u32 session_id;
    u8 sp2c_action;
    u8 pad[3];
    u16 url_len;
    u8 url_data[HTTP_SP_URL_LEN_MAX];
}http_sp2c_req_pkt_t;

typedef struct http_sp2c_res_pkt_s {
    u32 session_id;
    u8 status;
    u8 pad[3];
    u16 url_len;
    u8 url_data[HTTP_SP_URL_LEN_MAX];
}http_sp2c_res_pkt_t;

#endif /*__HTTPSNOOPING_H__*/

