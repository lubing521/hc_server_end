
#ifndef __SOHU_LIB_H__
#define __SOHU_LIB_H__

#include "common.h"

int sohu_is_m3u8_url(char *sohu_url);
int sohu_http_session(char *url, char *response);
char *sohu_parse_m3u8_response(char *curr, char *real_url);


#endif /* __SOHU_LIB_H__ */

