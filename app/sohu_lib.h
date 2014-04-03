
#ifndef __SOHU_LIB_H__
#define __SOHU_LIB_H__

#include "common.h"

int sohu_is_m3u8_url(char *sohu_url);
int sohu_http_session(char *url, char *response, unsigned long resp_len);
char *sohu_parse_m3u8_response(char *curr, char *real_url);
int sohu_parse_file_url_response(char *response, char *real_url);


#endif /* __SOHU_LIB_H__ */

