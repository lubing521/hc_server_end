/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * header.h
 * Original Author: yaoshangping@ruijie.com.cn,
 *                  zhaoyao@ruijie.com.cn,      2014-03-11
 *
 * Youku video common header file.
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#include "header.h"

#define YK_FILEID_LEN           66   /* 视频文件名解析后的长度 */
#define YK_MAX_NAME_LEN         32

#define RESP_BUF_LEN            (0x1 << 14)         /* 16KB */

#define SEED_STRING_LEN         8

#define STREAM_TYPE_TOTAL       4   /* flv, mp4, hd2, hd3 */
#define STREAM_TYPE_LEN         4
#define STREAM_FILE_IDS_LEN     256
#define STREAM_SEGS_MAX         32

#define SEG_INFO_LEN            256
#define SEGMENT_K_LEN           32
#define SEGMENT_K2_LEN          32

/* url中所需的基础数据结构 */
typedef struct _rootData {
	char video_id[YK_MAX_NAME_LEN];	
	char fid[YK_MAX_NAME_LEN];	
	char passwords[YK_MAX_NAME_LEN];	
	char partnerId[YK_MAX_NAME_LEN];
	char extstring[YK_MAX_NAME_LEN];
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
	char client_id[YK_MAX_NAME_LEN];
	char extstring[YK_MAX_NAME_LEN];

} url_var_t;

typedef struct PlayerConfig {
    bool isFDL;     /* true: 和k.youku.com 有关系，具体含义未知 */ 
    bool isTudouPlayer;
} player_cfg_t;

typedef struct PlayerConstant {
    char ctype[10]; /* "standard", "story" , 具体含义未知 */
    int ev;         /* 具体含义未知 */             
} player_cons_t;

typedef struct IPlayListData {
    char sid[25];
    char fileType[6];       /* flv, mp4, hd2, hd3, flvhd */
    bool drm;               /* 含义未知 */
    char key1[25];
    char key2[25];
} playlistdata_t;

typedef struct VideoSegmentData {
    char fileId[YK_FILEID_LEN + 3];
    char key[25];
    int seconds;
} video_seg_data_t;

typedef struct yk_stream_info_s yk_stream_info_t;

typedef struct yk_segment_info_s {
    yk_stream_info_t *stream;       /* Where I am belonging to. */
    int no;                         /* number */
    int size;
    int seconds;
    char k[SEGMENT_K_LEN];
    char k2[SEGMENT_K2_LEN];
} yk_segment_info_t;

struct yk_stream_info_s {
    char type[STREAM_TYPE_LEN];     /* flv, mp4, hd2, hd3 */
    char streamfileids[STREAM_FILE_IDS_LEN];
    int  streamsizes;
    yk_segment_info_t *segs[STREAM_SEGS_MAX];
};

bool yk_url_to_playlist(char *url, char *playlist);
bool yk_get_fileid(char *streamfileids, int video_num, int seed, char *fileids);
bool yk_get_fileurl(int num, playlistdata_t *play_list, video_seg_data_t *seg_data, 
                    bool use_jumptime, int jump_time, char *out_url);

int yk_parse_playlist(char *data, yk_stream_info_t *stream[], int *seed);
void yk_destroy_streams_all(yk_stream_info_t *stream[]);
void yk_debug_streams_all(yk_stream_info_t *streams[]);

