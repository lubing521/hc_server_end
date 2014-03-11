// yk_lib.c : 定义控制台应用程序的入口点。
//
/* 根据视频首页，获取视频文件名 */
/* author: yaoshangping */
/* data :20140307 */

#include <time.h>
#include <ctype.h>

#include "header.h"
#include "yk_lib.h"

/************************************************ 
* 把字符串进行URL编码。 
* 输入： 
* str: 要编码的字符串 
* strSize: 字符串的长度。这样str中可以是二进制数据 
* result: 结果缓冲区的地址 
* resultSize:结果地址的缓冲区大小(如果str所有字符都编码，该值为strSize*3) 
* 返回值： 
* >0: result中实际有效的字符长度， 
* 0: 编码失败，原因是结果缓冲区result的长度太小 
************************************************/ 
int url_encode(const char* str, const int strSize, char* result, const int resultSize) 
{ 
    int i; 
    int j = 0; /* for result index */ 
    char ch; 

    if ((str == NULL) || (result == NULL) || (strSize <= 0) || (resultSize <= 0)) { 
        return 0; 
    } 

    for (i=0; (i<strSize) && (j<resultSize); i++) { 
        ch = str[i]; 
        if ((ch >= 'A') && (ch <= 'Z')) { 
            result[j++] = ch; 
        } else if ((ch >= 'a') && (ch <= 'z')) { 
            result[j++] = ch; 
        } else if ((ch >= '0') && (ch <= '9')) { 
            result[j++] = ch; 
        } else if(ch == ' '){ 
            result[j++] = '+'; 
        } else { 
            if (j + 3 < resultSize) { 
                sprintf(result+j, "%%%02X", (unsigned char)ch); 
                j += 3; 
            } else { 
                return 0; 
            } 
        } 
    } 

    result[j] = '\0'; 
    return j; 
} 

/************************************************ 
* 把字符串进行URL解码。 
* 输入： 
* str: 要解码的字符串 
* strSize: 字符串的长度。 
* result: 结果缓冲区的地址 
* resultSize:结果地址的缓冲区大小，可以<=strSize 
* 返回值： 
* >0: result中实际有效的字符长度， 
* 0: 解码失败，原因是结果缓冲区result的长度太小 
************************************************/ 
int url_decode(const char* str, const int strSize, char* result, const int resultSize) 
{ 
    char ch, ch1, ch2; 
    int i; 
    int j = 0; /* for result index */ 

    if ((str == NULL) || (result == NULL) || (strSize <= 0) || (resultSize <= 0)) { 
        return 0; 
    } 

    for (i=0; (i<strSize) && (j<resultSize); i++) { 
        ch = str[i]; 
        switch (ch) { 
        case '+': 
            result[j++] = ' '; 
            break; 

        case '%': 
            if (i+2 < strSize) { 
                ch1 = (str[i+1])- '0'; 
                ch2 = (str[i+2]) - '0'; 
                if ((ch1 != '0') && (ch2 != '0')) { 
                result[j++] = (char)((ch1<<4) | ch2); 
                i += 2; 
                break; 
            } 
        } 

        /* goto default */ 
        default: 
            result[j++] = ch; 
            break; 
        } 
    } 

    result[j] = '\0'; 
    return j; 
} 

/* 识别优酷视频首页的url，正确返回true，反之返回false */
/* 入参:url */
bool identify_yk_video_url(char *purl)
{
    char *obj_url="http://v.youku.com/v_show";
    char *presult = NULL;
    if (purl == NULL) {
        return false;
    }
    
    presult=strstr(purl, obj_url);
    if (presult== purl) {
        return true;
    } else {
        return false;
    }
}

/* 从优酷视频首页的html中获取视频的name和folderId, url一共有如下三种形式 */
/* http://v.youku.com/v_show/id_XNjY2OTkyNjk2.html */
/* http://v.youku.com/v_show/id_XNjcxNTUzNjIw.html?f=21894121&ev=4 */
/* http://v.youku.com/v_show/id_XNjcxNTUzNjIw.html?f=21894122 */
int get_yk_video_name(char *purl, char name[], char folderId[])
{

    char *purl_pre = "http://v.youku.com/v_show/id_";
    char *name_idx;
	char *folder_idx;
    int i,url_pre_len;

    if (purl == NULL) {
        return 0;
    }

    name_idx = strstr(purl, purl_pre);    
    if (name_idx == NULL) {
        return 0;
    } else {        
        i = 0;
		url_pre_len =strlen(purl_pre);
        for (name_idx = name_idx + url_pre_len;*name_idx != '.' && i < MAX_NAME_LEN; name_idx++) {
            name[i++] = *name_idx;
        }

        name[i] = '\0';
		if ((url_pre_len + i + 5) == strlen(purl)) {
			/* purl里面不带?f= */
			return 1;
		} else {
			/* .html?f= */
			folder_idx = name_idx + 8;
			i = 0;
			for ( ; *folder_idx != '\0' && *folder_idx != '&'; folder_idx++) {
				folderId[i++] = *folder_idx;
			}
			folderId[i] = '\0';
			return 2;
		}
    }
}

/* url中所需的基础数据结构 */
typedef struct _rootData {
	char video_id[MAX_NAME_LEN];	
	char fid[MAX_NAME_LEN];	
	char passwords[MAX_NAME_LEN];	
	char partnerId[MAX_NAME_LEN];
	char extstring[MAX_NAME_LEN];
	char source[10];
	char version[10];
	char type[10];
	char pt[10];
	char ob[10];
	char pf[10];
	bool isRedirect;
	
	/* 内容可扩展 */
} rootdata_t;

/* url中的可选参数 */
typedef struct URLVariables {
	int n;
	char ctype[10];
	unsigned int ran;
	int ev;
	bool isRedirect;
	char passwords[128];
	char client_id[MAX_NAME_LEN];
	char extstring[MAX_NAME_LEN];

} url_var_t;

typedef struct PlayerConfig {
    bool isFDL;     /* true: 和k.youku.com 有关系，具体含义未知 */ 
    bool isTudouPlayer;
} player_cfg_t;

typedef struct PlayerConstant {
    char ctype[10]; /* "standard", "story" , 具体含义未知 */
    int ev;         /* 具体含义未知 */             
} player_cons_t;
    

/* getPlayList的请求报文 */
bool request_playlist(rootdata_t *rootdata, url_var_t *url_var, 
                      player_cfg_t *player_cfg, player_cons_t *player_cons,
                      char *url)
{
	char *time_zone = "+08";
	char *server_domain = "v.youku.com";
	int is_url_encode_ok;
    char int2str[32];   /* 临时存储int转换的字符串 */

	if (rootdata == NULL || url_var == NULL
        || player_cfg == NULL || player_cons == NULL
        || url == NULL) {
		return false;
	}	

	strcat(url, "http://");
	strcat(url, server_domain);//拼接字符串
	strcat(url, "/player/getPlayList");
	strcat(url, "/VideoIDS/");
	strcat(url, rootdata->video_id);
	strcat(url, "/timezone/");
	strcat(url, time_zone);
	strcat(url, "/version/");
	strcat(url, rootdata->version);
	strcat(url, "/source/");
	strcat(url, rootdata->source);
	if (rootdata->type[0]!= '\0') {
		strcat(url, "/Type/");
		strcat(url, rootdata->type);
	}

	if (strcmp(rootdata->type, "Folder") == 0) {
		strcat(url, "/Fid/");
		strcat(url, rootdata->fid);
		strcat(url, "/Pt/");
		strcat(url, rootdata->pt);
		if (rootdata->ob[0]!= '\0') {
			strcat(url, "/Ob/");
			strcat(url, rootdata->ob);
		}
	}

	if (rootdata->pf[0] != '\0') {
		strcat(url, "/Pf/");
		strcat(url, rootdata->pf);
	}

	if (player_cfg->isTudouPlayer) {
		strcat(url, "/Sc/");
		strcat(url, "2");
	}

	strcat(url, "?password=");
	if (rootdata->passwords[0]!= '\0') {
		is_url_encode_ok = url_encode(rootdata->passwords, strlen(rootdata->passwords), 
										url_var->passwords, sizeof(url_var->passwords));
		if (is_url_encode_ok) {
			strcat(url, url_var->passwords);
		}
	}

    strcat(url, "&n=");
    sprintf(int2str, "%d", url_var->n);
    strcat(url, int2str);
    strcat(url, "&ran=");
    sprintf(int2str, "%d", url_var->ran);
    strcat(url, int2str);

    if (player_cfg->isFDL) {
        strcat(url, "&ctype=");
        strcat(url, player_cons->ctype);
        strcat(url, "&ev=");
        sprintf(int2str, "%d", player_cons->ev);
        strcat(url, int2str);        
    }

	return true;
}

/* 入参: 视频首页的url */
/* 返回生成的request playlist的url */
bool yk_url_to_playlist(char *url, char *playlist)
{
	bool video_url;
	char video_name[MAX_NAME_LEN];
	char folderId[MAX_NAME_LEN];
	int  ret_get_vname;

    rootdata_t rootdata;
	url_var_t url_variables;
    player_cfg_t player_cfg;
    player_cons_t player_cons;    
    
	char passwords[] = "";
	char version[] = "5";
	char source[] = "video";
	char type[] = "Folder";
	char ob[] = "1";
	char pt[] = "0";
	char ctype[] = "10";   

    if (url == NULL || playlist == NULL) {
        return false;
    }

	video_url = identify_yk_video_url(url);
	if (video_url== true) {
		memset(video_name, 0, sizeof(video_name));
		memset(folderId, 0, sizeof(folderId));
        memset(&rootdata, 0, sizeof(rootdata));
        memset(&url_variables, 0, sizeof(url_variables));
        memset(&player_cfg, 0, sizeof(player_cfg));
        memset(&player_cons, 0, sizeof(player_cons));
        
		ret_get_vname = get_yk_video_name(url, video_name, folderId);
        if (ret_get_vname) {
            memcpy(rootdata.video_id, video_name, strlen(video_name));                
            /* 构造rootdata */
        	memcpy(rootdata.ob, ob, sizeof(ob));
        	memcpy(rootdata.pt, pt, sizeof(pt));
        	memcpy(rootdata.passwords, passwords, sizeof(passwords));	
        	memcpy(rootdata.version, version, sizeof(version));
        	memcpy(rootdata.source, source, sizeof(source));
            if (ret_get_vname == 2) {
                memcpy(rootdata.fid, folderId, strlen(folderId));
                memcpy(rootdata.type, type, sizeof(type));
            }

            /* 生成getplaylist 的请求 */
            player_cfg.isTudouPlayer = false;
            player_cfg.isFDL = true;
            player_cons.ev = 1;
			memcpy(player_cons.ctype, ctype, sizeof(ctype));
            /* 构造url的可选参数 */
        	url_variables.n = 3;
        	srand((int)time(NULL));
        	url_variables.ran = rand() % 10000;	
            return request_playlist(&rootdata, &url_variables, &player_cfg, &player_cons, playlist);
        	
        } else {
            return false;
        }
	}	else {
	    return false;
	}
}

static yk_stream_info_t *yk_create_stream_info(const char *type)
{
    yk_stream_info_t *p;

    p = malloc(sizeof(yk_stream_info_t));
    if (p == NULL) {
        fprintf(stderr, "%s memory allocate %d bytes failed\n", __func__, sizeof(yk_stream_info_t));
    } else {
        memset(p, 0, sizeof(yk_stream_info_t));
        memcpy(p->type, type, STREAM_TYPE_LEN - 1);
    }

    return p;
}

#define VALUE_LEN 16
static yk_segment_info_t *yk_create_segment_info(char *info)
{
    yk_segment_info_t *p;
    char *cur;
    char val[VALUE_LEN];
    int i;

    p = malloc(sizeof(yk_segment_info_t));
    if (p == NULL) {
        fprintf(stderr, "%s memory allocate %d bytes failed\n", __func__, sizeof(yk_segment_info_t));
        return NULL;
    }
    
    memset(p, 0, sizeof(yk_segment_info_t));
    cur = info;
    
    cur = strstr(cur, "no");
    memset(val, 0, VALUE_LEN);
    for (cur = cur + 5, i = 0; *cur != '"' && i < VALUE_LEN; cur++, i++) {
        val[i] = (*cur);
    }
    p->no = atoi(val);

    cur = strstr(cur, "size");
    memset(val, 0, VALUE_LEN);
    for (cur = cur + 7, i = 0; *cur != '"' && i < VALUE_LEN; cur++, i++) {
        val[i] = (*cur);
    }
    p->size = atoi(val);

    /*
     * zhaoyao XXX ATTENTION: format may like "seconds":"196" or "seconds":411
     */
    cur = strstr(cur, "seconds");
    memset(val, 0, VALUE_LEN);
    cur = cur + 9;
    if (*cur == '"') {
        cur++;
    }
    for (i = 0; *cur != '"' && i < VALUE_LEN; cur++, i++) {
        val[i] = (*cur);
    }
    p->seconds = atoi(val);

    cur = strchr(cur, 'k');
    for (cur = cur + 4, i = 0; *cur != '"' && i < SEGMENT_K_LEN; cur++, i++) {
        p->k[i] = (*cur);
    }

    cur = strstr(cur, "k2");
    for (cur = cur + 5, i = 0; *cur != '"' && i < SEGMENT_K2_LEN; cur++, i++) {
        p->k2[i] = (*cur);
    }

    return p;
}

static yk_stream_info_t *yk_find_stream_info(yk_stream_info_t *streams[], const char *type)
{
    int i;

    for (i = 0; i < STREAM_TYPE_TOTAL; i++) {
        if (strcmp(streams[i]->type, type) == 0) {
            return streams[i];
        }
    }

    return NULL;
}

static char *yk_parse_pl_seed(const char *cur, char *seed)
{
    char *ret;

    ret = strstr(cur, "seed");
    if (ret == NULL) {
        return ret;
    }

    for (ret = ret + 6; *ret != ',' && isdigit(*ret); ret++, seed++) {
        *seed = *ret;
    }
    *seed = '\0';

    return ret;
}

void yk_destroy_streams_all(yk_stream_info_t *streams[])
{
    int i, j;

    if (streams == NULL) {
        return;
    }

    for (i = 0; i < STREAM_TYPE_TOTAL; i++) {
        if (streams[i] != NULL) {
            if (streams[i]->segs != NULL) {
                for (j = 0; j < STREAM_SEGS_MAX; j++) {
                    if (streams[i]->segs[j] != NULL) {
                        free(streams[i]->segs[j]);
                    }
                }
            }
            free(streams[i]);
        }
    }
}

void yk_debug_streams_all(yk_stream_info_t *streams[])
{
    int i, j;
    yk_stream_info_t *p;
    yk_segment_info_t *q;

    if (streams == NULL) {
        return;
    }

    for (i = 0; streams[i] != NULL && i < STREAM_TYPE_TOTAL; i++) {
        p = streams[i];
        printf("Stream type: %s\n", p->type);
        printf("       streamfileids: %s\n", p->streamfileids);
        printf("       streamsizes: %d\n", p->streamsizes);
        if (p->segs != NULL) {
            for (j = 0; p->segs[j] != NULL && j < STREAM_SEGS_MAX; j++) {
                q = p->segs[j];
                printf("       segs [%2d]:\n", q->no);
                printf("            size: %d\n", q->size);
                printf("            seconds: %d\n", q->seconds);
                printf("            k: %s\n", q->k);
                printf("            k2: %s\n", q->k2);
            }
        }
        printf("---------------------------------------End\n\n");
    }
}

static char *yk_parse_pl_streamfileids(const char *cur, yk_stream_info_t *streams[])
{
    char *ret, *v;
    int i = 0;
    yk_stream_info_t *p;

    ret = strstr(cur, "streamfileids");
    if (ret == NULL) {
        return ret;
    }

    ret = ret + strlen("streamfileids\":{\"");
    while (1) {
        if (memcmp(ret, "hd2", 3) == 0) {
            p = yk_create_stream_info("hd2");
            if (p == NULL) {
                goto error;
            }
            for (ret = ret + 6, v = p->streamfileids; *ret != '"'; ret++, v++) {
                *v = *ret;
            }
            *v = '\0';
            ret = ret + 3;
            streams[i] = p;
            i++;
        } else if (memcmp(ret, "mp4", 3) == 0) {
            p = yk_create_stream_info("mp4");
            if (p == NULL) {
                goto error;
            }
            for (ret = ret + 6, v = p->streamfileids; *ret != '"'; ret++, v++) {
                *v = *ret;
            }
            *v = '\0';
            ret = ret + 3;
            streams[i] = p;
            i++;
        } else if (memcmp(ret, "hd3", 3) == 0) {
            p = yk_create_stream_info("hd3");
            if (p == NULL) {
                goto error;
            }
            for (ret = ret + 6, v = p->streamfileids; *ret != '"'; ret++, v++) {
                *v = *ret;
            }
            *v = '\0';
            ret = ret + 3;
            streams[i] = p;
            i++;
        } else if (memcmp(ret, "flv", 3) == 0) {
            p = yk_create_stream_info("flv");
            if (p == NULL) {
                goto error;
            }
            for (ret = ret + 6, v = p->streamfileids; *ret != '"'; ret++, v++) {
                *v = *ret;
            }
            *v = '\0';
            ret = ret + 3;
            streams[i] = p;
            i++;
        } else if (memcmp(ret, "\"se", 3) == 0) {   /* End of streamfileids */
            ret = ret + 1;
            return ret;
        } else {
            goto error;
        }
    }

error:
    fprintf(stderr, "%s failed\n", __func__);
    yk_destroy_streams_all(streams);

    return NULL;
}

#define SEGS_INFO_LEN   256
static char *yk_parse_pl_segs_do(char *cur, yk_stream_info_t *stream)
{
    char *ret, *end, info[SEGS_INFO_LEN];
    yk_segment_info_t *seg;
    int i, seg_cnt = 0;

    ret = cur + 6;
    end = strchr(ret, ']');
    while (ret < end) {
        memset(info, 0, SEGS_INFO_LEN);
        for (i = 0; *ret != '}' && i < SEGS_INFO_LEN; ret++, i++) {
            info[i] = (*ret);
        }
        seg = yk_create_segment_info(info);
        if (seg == NULL) {
            return NULL;
        }
        seg->stream = stream;
        stream->segs[seg_cnt] = seg;
        seg_cnt++;
        if (seg_cnt > STREAM_SEGS_MAX) {
            return NULL;
        }
        ret = ret + 2;
    }
    ret = ret + 2;

    return ret;
}

static char *yk_parse_pl_segs(const char *cur, yk_stream_info_t *streams[])
{
    char *ret;
    yk_stream_info_t *p;

    ret = strstr(cur, "segs");
    if (ret == NULL) {
        return ret;
    }
    ret = ret + 8;
    while (1) {
        if (memcmp(ret, "hd2", 3) == 0) {
            p = yk_find_stream_info(streams, "hd2");
            if (p == NULL) {
                goto error;
            }
            ret = yk_parse_pl_segs_do(ret, p);
            if (ret == NULL) {
                goto error;
            }
        } else if (memcmp(ret, "mp4", 3) == 0) {
            p = yk_find_stream_info(streams, "mp4");
            if (p == NULL) {
                goto error;
            }
            ret = yk_parse_pl_segs_do(ret, p);
            if (ret == NULL) {
                goto error;
            }
        } else if (memcmp(ret, "hd3", 3) == 0) {
            p = yk_find_stream_info(streams, "hd3");
            if (p == NULL) {
                goto error;
            }
            ret = yk_parse_pl_segs_do(ret, p);
            if (ret == NULL) {
                goto error;
            }
        } else if (memcmp(ret, "flv", 3) == 0) {
            p = yk_find_stream_info(streams, "flv");
            if (p == NULL) {
                goto error;
            }
            ret = yk_parse_pl_segs_do(ret, p);
            if (ret == NULL) {
                goto error;
            }
        } else if (memcmp(ret, "\"st", 3) == 0) {   /* End of segs */
            ret = ret + 1;
            return ret;
        } else {
            goto error;
        }
    }

error:
    fprintf(stderr, "%s failed\n", __func__);
    yk_destroy_streams_all(streams);

    return NULL;
}

int yk_parse_playlist(char *data, yk_stream_info_t *streams[], char *seed)
{
    int i;
    char *cur = data;

    if (data == NULL || streams == NULL || seed == NULL) {
        return -1;
    }
    for (i = 0; i < STREAM_TYPE_TOTAL; i++) {
        if (streams[i] != NULL) {
            return -1;
        }
    }

    /* Get seed */
    cur = yk_parse_pl_seed(cur, seed);
    if (cur == NULL) {
        return -1;
    }

    /* Get streamfileids */
    cur = yk_parse_pl_streamfileids(cur, streams);   /* zhaoyao XXX: it will allocate stream_info */
    if (cur == NULL) {
        return -1;
    }

    /* Get segs */
    cur = yk_parse_pl_segs(cur, streams);            /* zhaoyao XXX: it will allocate segment_info */
    if (cur == NULL) {
        return -1;
    }

    /* zhaoyao TODO: Get streamsizes */

    return 0;
}
