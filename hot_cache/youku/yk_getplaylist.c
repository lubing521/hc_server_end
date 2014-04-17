// yk_lib.c : �������̨Ӧ�ó������ڵ㡣
//
/* ������Ƶ��ҳ����ȡ��Ƶ�ļ��� */
/* author: yaoshangping */
/* data :20140307 */

#include <time.h>

#include "yk_lib.h"

/************************************************ 
* ���ַ�������URL���롣 
* ���룺 
* str: Ҫ������ַ��� 
* strSize: �ַ����ĳ��ȡ�����str�п����Ƕ��������� 
* result: ����������ĵ�ַ 
* resultSize:�����ַ�Ļ�������С(���str�����ַ������룬��ֵΪstrSize*3) 
* ����ֵ�� 
* >0: result��ʵ����Ч���ַ����ȣ� 
* 0: ����ʧ�ܣ�ԭ���ǽ��������result�ĳ���̫С 
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
* ���ַ�������URL���롣 
* ���룺 
* str: Ҫ������ַ��� 
* strSize: �ַ����ĳ��ȡ� 
* result: ����������ĵ�ַ 
* resultSize:�����ַ�Ļ�������С������<=strSize 
* ����ֵ�� 
* >0: result��ʵ����Ч���ַ����ȣ� 
* 0: ����ʧ�ܣ�ԭ���ǽ��������result�ĳ���̫С 
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

/* ʶ���ſ���Ƶ��ҳ��url����ȷ����true����֮����false */
/* ���:url */
bool identify_yk_video_url(char *purl)
{
    char *obj_url="v.youku.com/v_show";
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

/* ���ſ���Ƶ��ҳ��html�л�ȡ��Ƶ��name��folderId, urlһ��������������ʽ */
/* v.youku.com/v_show/id_XNjY2OTkyNjk2.html */
/* v.youku.com/v_show/id_XNjcxNTUzNjIw.html?f=21894121&ev=4 */
/* v.youku.com/v_show/id_XNjcxNTUzNjIw.html?f=21894122 */
int get_yk_video_name(char *purl, char name[], char folderId[])
{

    char *purl_pre = "v.youku.com/v_show/id_";
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
        for (name_idx = name_idx + url_pre_len;*name_idx != '.' && i < YK_MAX_NAME_LEN; name_idx++) {
            name[i++] = *name_idx;
        }

        name[i] = '\0';
		if ((url_pre_len + i + 5) == strlen(purl)) {
			/* purl���治��?f= */
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

/* getPlayList�������� */
bool request_playlist(rootdata_t *rootdata, url_var_t *url_var, 
                      player_cfg_t *player_cfg, player_cons_t *player_cons,
                      char *url)
{
	char *time_zone = "+08";
	char *server_domain = "v.youku.com";
	int is_url_encode_ok;
    char int2str[32];   /* ��ʱ�洢intת�����ַ��� */

	if (rootdata == NULL || url_var == NULL
        || player_cfg == NULL || player_cons == NULL
        || url == NULL) {
		return false;
	}	

	strcat(url, server_domain);//ƴ���ַ���
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

/* ���: ��Ƶ��ҳ��url */
/* �������ɵ�request playlist��url */
bool yk_url_to_playlist(char *url, char *playlist)
{
	bool video_url;
	char video_name[YK_MAX_NAME_LEN];
	char folderId[YK_MAX_NAME_LEN];
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
            /* ����rootdata */
        	memcpy(rootdata.ob, ob, sizeof(ob));
        	memcpy(rootdata.pt, pt, sizeof(pt));
        	memcpy(rootdata.passwords, passwords, sizeof(passwords));	
        	memcpy(rootdata.version, version, sizeof(version));
        	memcpy(rootdata.source, source, sizeof(source));
            if (ret_get_vname == 2) {
                memcpy(rootdata.fid, folderId, strlen(folderId));
                memcpy(rootdata.type, type, sizeof(type));
            }

            /* ����getplaylist ������ */
            player_cfg.isTudouPlayer = false;
            player_cfg.isFDL = true;
            player_cons.ev = 1;
			memcpy(player_cons.ctype, ctype, sizeof(ctype));
            /* ����url�Ŀ�ѡ���� */
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

