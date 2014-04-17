
#include "yk_lib.h"

/* 向服务器发起getflvpath的请求 */

/* getflvpath */
/*@num
 *@play_list
 *@seg_data
 *@use_jumptime
 *@jump_time
 *@out_url, 返回组装好的url
 
 */
bool yk_get_fileurl(int num, playlistdata_t *play_list, video_seg_data_t *seg_data, 
                    bool use_jumptime, int jump_time, char *out_url)
{
	bool isFDL = false; /* 含义未知，true时，访问k.youku.com */
    char _loc7_[3];
    char _loc8_[4];
    char int2str[32];   /* 临时存储int转换的字符串 */
    
	char host1[] = "k.youku.com/player/getFlvPath";
	char host2[] = "f.youku.com/player/getFlvPath";

    if (play_list == NULL || seg_data == NULL) {
        return false;
    }

	memset(_loc7_, 0, sizeof(_loc7_));
    sprintf(_loc7_, "%x", num);
    if (strlen(_loc7_) == 1) {
        _loc7_[1] = _loc7_[0];
        _loc7_[0] = '0';
    }

    if (isFDL) {
        strcat(out_url, host1);
    } else {
        strcat(out_url, host2);
    }

    strcat(out_url, "/sid/");
    strcat(out_url, play_list->sid);
    strcat(out_url, "_");
    strcat(out_url, _loc7_);
    memset(_loc8_, 0, sizeof(_loc8_));
    strcpy(_loc8_, play_list->fileType);
    if ((strcmp(play_list->fileType, "hd2") == 0) || 
        (strcmp(play_list->fileType, "hd3") == 0)) {
        memset(_loc8_, 0, sizeof(_loc8_));
        memcpy(_loc8_, VIDEO_FLV_SUFFIX, VIDEO_FLV_SUFFIX_LEN);
    }

    if(play_list->drm)
    {
        memset(_loc8_, 0, sizeof(_loc8_));
        memcpy(_loc8_, "f4v", 3);   
    }

    strcat(out_url, "/st/");
    strcat(out_url, _loc8_);
    strcat(out_url, "/fileid/");
    strcat(out_url, seg_data->fileId);
    if (use_jumptime) {
        if (play_list->drm) {
            strcat(out_url, "?K=");            
        } else {
            strcat(out_url, "?start=");
            sprintf(int2str, "%d", jump_time);
            strcat(out_url, "&K=");
        }
    } else {
        strcat(out_url, "?K=");        
    }

    if (seg_data->key[0] == '\0') {
        strcat(out_url, play_list->key2);
        strcat(out_url, play_list->key1);
    } else {
        strcat(out_url, seg_data->key); 
    }

    /* 以下是可选的参数 */
    #if 0

    if ((strcmp(play_list->fileType, SC_VIDEO_FLV_SUFFIX) == 0) || 
        (strcmp(play_list->fileType, "flvhd") == 0)) {
        strcat(out_url, "&hd=0"); 
    } else {
        if (strcmp(play_list->fileType, "mp4") == 0) {
            strcat(out_url, "&hd=1");
        } else {
            if (strcmp(play_list->fileType, "hd2") == 0) {
                strcat(out_url, "&hd=2");
            } else if (strcmp(play_list->fileType, "hd3") == 0) {
                strcat(out_url, "&hd=3");
            }
        }
    }  
    #endif

	return true;
}

