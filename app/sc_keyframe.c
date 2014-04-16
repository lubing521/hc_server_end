#include "common.h"
#include "sc_header.h"

#if 0   /* May use in RGOS 10.x */
#define BigEndian 1  
#define LittleEndian 0 

#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))  
#define Swap32(l) (((l) >> 24) | \
(((l) & 0x00ff0000) >> 8)  | \
(((l) & 0x0000ff00) << 8)  | \
((l) << 24))  
#define Swap64(ll) (((ll) >> 56) | \
(((ll) & 0x00ff000000000000) >> 40) |\
(((ll) & 0x0000ff0000000000) >> 24) |\
(((ll) & 0x000000ff00000000) >> 8)    |\
(((ll) & 0x00000000ff000000) << 8)    |\
(((ll) & 0x0000000000ff0000) << 24) |\
(((ll) & 0x000000000000ff00) << 40) |\
(((ll) << 56))) 

#define BigEndian_16(s) is_BigEndian() ? s : Swap16(s)  
#define LittleEndian_16(s) is_BigEndian() ? Swap16(s) : s  
#define BigEndian_32(l) is_BigEndian() ? l : Swap32(l)  
#define LittleEndian_32(l) is_BigEndian() ? Swap32(l) : l  
#define BigEndian_64(ll) is_BigEndian() ? ll : Swap64(ll)  
#define LittleEndian_64(ll) is_BigEndian() ? Swap64(ll) : ll 


/* 判断本机系统的大小端 */
static bool is_BigEndian()  
{  
    /*定义一个2个字节长度的数据，并赋值为1,则n的16进制表示为0x0001 
    如果系统以“大端”存放数据，也即是以MSB方式存放，那么低字节存放的必定是0x00，高字节存放的必定是0x01 
    如果系统以“小端”存放数据，也即是以LSB方式存放，那么低字节存放的必定是0x01，高字节存放的必定是0x00 
    所谓MSB，就是将最重要的位存入低位，而LSB则是将最不重要的位存入低位 
    我们可以通过检测低位的数值就可以知道系统的字节序 
    */  
    const int16_t n = 1;  
    if(*(char *)&n)  
    {  
        return LittleEndian;  
    }  
    return BigEndian;  
}

/* 将IEEE754格式的double转换成int行 */
/* 仅支持大端序 */
unsigned int double_to_int(unsigned char *double_data)
{
    /* 
     * IEEE754格式的double格式(对应数值6)如下:
     * s      E               M
     * 0   10000000001 1000000000000000000000000000000000000000000000000000
     * 其中s占一位,e占11位,M占52位
     * 计算公式如下:
     * F=1.M(二进制)
     * V=(-1)^s*2^(E-1023)*F
     */

//     unsigned char ch;
//     bool s;
     unsigned short int E;
     unsigned int val,M, M_H4, M_L12;

     /* 取符号位 */
//     ch = double_data[0];
//     s = (ch & 0x80);

     /* 取指数e,即 E - 1023 */
     E = *((unsigned short int *)&double_data[0]);
#if 0
     /* 取消符号位 */
     E = (E & 0x7FFF);
     printf("e:%u, %s - %d\n", E, __FILE__, __LINE__);
#endif
     /* 取11位E位 */
     E = E >> 4;
     E = E - 1023;
     
     M_H4 = *((unsigned int *)&double_data[0]);

     /* 取M的高4位 */
     M_H4 = M_H4 << 12;
     /* 取M的低12位 */
    M_L12 = *((unsigned int *)&double_data[3]);
    M_L12 =  M_L12 >> 4;

    M  =  (M_H4 | M_L12);
     
     /* 1.M */
     val = M >> 1;
     val = (val | 0x80000000);
     val = (val >> (32 - E - 1));
     return val;
}

bool is_keyframe(int tag_data) {
    bool ret = false;
    if (((tag_data >> 4) & 0x0f) == 1) {
        ret = true;
    }

    return ret;
}
#endif

static bool sc_kf_flv_is_audio_tag(char tag_type)
{
	bool ret = false;
	if (tag_type == 0x08) {
		ret = true;
	}

	return ret;
}

static bool sc_kf_flv_is_script_tag(char tag_type)
{
	bool ret = false;
	if (tag_type == 0x12) {
		ret = true;
	}

	return ret;
}

static bool sc_kf_flv_is_video_tag(char tag_type)
{
	bool ret = false;
	if (tag_type == 0x09) {
		ret = true;
	}

	return ret;
}

/* 查找flv视频中，tag的位置 
 * tag_index, tag在ptr中的位置
 * tag_size, tag的大小, 含11字节的头部长度
 * 返回false，查找失败 */
static bool sc_kf_flv_find_tag_pos(char *ptr, int buf_size, int *tag_size, int *tag_index) {
	int i;
	char *buffer;
	int ptr_len;
    int tag_start_pos, tag_end_pos;
    bool ret = false;

    if (ptr == NULL) {
        return false;
    }
    
	ptr_len = buf_size;
	buffer = ptr;
	i = 0 ;
	while (i < ptr_len) {
		if (sc_kf_flv_is_script_tag(buffer[i]) || 
            sc_kf_flv_is_audio_tag(buffer[i]) || 
            sc_kf_flv_is_video_tag(buffer[i])) {	
			if ((int)(buffer[i+8]) == 0x00 && 
                (int)(buffer[i+9]) == 0x00 && 
                (int)(buffer[i+10]) == 0x00) {//判断流ID是否为0
                tag_start_pos = (unsigned char)(buffer[i+1])*16*16*16*16 + 
                                (unsigned char)(buffer[i+2])*16*16 + (unsigned char)(buffer[i+3]);

				if ((i + tag_start_pos) <= ptr_len && tag_start_pos > 0 ) {
                    tag_end_pos = (unsigned char)(buffer[i+tag_start_pos+1+11])*16*16*16*16 + 
                                  (unsigned char)(buffer[i+tag_start_pos+2+11])*16*16 + 
                                  (unsigned char)(buffer[i+tag_start_pos+3+11]) - 11; 

                    if (tag_start_pos == tag_end_pos) {
                        *tag_index = i; //找到了视频tag的位置
                        *tag_size = tag_start_pos + 11; /* 该tag的大小 */
                        ret = true;
                        break;
                    }
                }
			}
		}

		i++;
	}
	
	return ret;
}

/* 查找keyframes字符串的位置 
 * buf是script的数据
 */
static int sc_kf_flv_find_kf_word(unsigned char *buf, int tag_size)
{
    int i = 0;
    if (buf == NULL) {
        return 0;
    }

    /* keyframes字符串 */
    while (i < tag_size) {
        if (buf[i + 0] == 0x6B && buf[i + 1] == 0x65 && buf[i + 2] == 0x79 &&
            buf[i + 3] == 0x66 && buf[i + 4] == 0x72 && buf[i + 5] == 0x61 &&
            buf[i + 6] == 0x6D && buf[i + 7] == 0x65 && buf[i + 8] == 0x73) {
            break;
        }
        
        i++;
    }

    return i;    
}

/* 生成关键帧列表
 *@fp, 文件句柄
 *@key_info, 生成的关键帧列表, 按时间序递增 
 *@key_num, 关键帧的个数
 * script tag, video tag, audio tag
 */

#define SC_KF_FLV_CREATE_INFO_TEMP_MAX_NUM  256
static bool sc_kf_flv_create_info_do(FILE *fp, sc_kf_flv_info_t *key_info, u32 *key_num)
{
	int tag_pos;
	char buf[SC_KF_FLV_READ_MAX];
	int read_cnt;
    int tag_size;
    bool is_tag;
	unsigned int keyframe_num;
    int index;
    int nonius; /* buf下标 */
    unsigned char switch_buf[8];
    int kf, j;
    uint64_t temp64;
    double dd;

	if (fp == NULL) {
		printf("error: the input fp is NULL\r\n");
        return false;
    } 

    if (key_info == NULL) {
		printf("error: the input key_info is NULL\r\n");
        return false;
    }

    if (key_num == NULL) {
		printf("error: the input key_num is NULL\r\n");
        return false;
    }

	fseek(fp, 0, SEEK_SET);  /* 定位到文件开头 */

	memset(buf, 0, sizeof(buf));
    /* 找第一个tag, 即是script tag */
    read_cnt = fread((void *)buf, sizeof(unsigned char), SC_KF_FLV_READ_MAX, fp);
    if (read_cnt) {
        is_tag = sc_kf_flv_find_tag_pos(buf, read_cnt, &tag_size, &tag_pos);
        if (!is_tag) {
            printf("error: find_tag_pos failure, 你应该扩大第一次查找的范围\n");
            return false;
        }
    } else {
        return false;
    }

    if (!sc_kf_flv_is_script_tag(buf[tag_pos])) {
        return false;       
    } 

    index = sc_kf_flv_find_kf_word((unsigned char *)(buf + tag_pos) , tag_size);
    /* 03 00 0D + filepositions + 0A + 4bytes的长度 */
    memset(switch_buf, 0, sizeof(switch_buf));
    nonius = tag_pos + index + 26;
    /* 读取出来的数据是大端序 */
    switch_buf[0] = buf[nonius];
    switch_buf[1] = buf[nonius + 1];
    switch_buf[2] = buf[nonius + 2];
    switch_buf[3] = buf[nonius + 3];
    /* 读取keyframe 的个数 */
    keyframe_num = *((unsigned int *)(&switch_buf));
    keyframe_num = be32toh(keyframe_num);
    *key_num = keyframe_num;
    if (keyframe_num > SC_KF_FLV_CREATE_INFO_TEMP_MAX_NUM) {
        fprintf(stderr, "%s ERROR: keyframe is too many(%u) than temp num(%u)\n",
                            __func__, keyframe_num, SC_KF_FLV_CREATE_INFO_TEMP_MAX_NUM);
        return false;
    }
    nonius = nonius + 4;
    /* filepositions信息 */
    for (kf = 0; kf < keyframe_num; kf++) {
        nonius++;   /* 类型1bytes */
        memset(switch_buf, 0, sizeof(switch_buf));
        /* 8bytes */
        for (j = 0; j < 8; j++) {
            switch_buf[j] = buf[nonius++]; 
        }
        temp64 = be64toh(*(uint64_t *)&switch_buf);
        dd = *(double *)&temp64;
        key_info[kf].file_pos = (int)dd; 
    }

    /* times信息 */
    nonius = nonius + 12;
    for (kf = 0; kf < keyframe_num; kf++) {
        nonius++;   /* 类型1bytes */
        memset(switch_buf, 0, sizeof(switch_buf));
        /* 8bytes */
        for (j = 0; j < 8; j++) {
            switch_buf[j] = buf[nonius++];
        }

        temp64 = be64toh(*(uint64_t *)&switch_buf);
        dd = *(double *)&temp64;
        key_info[kf].time= (int)(dd); 
    } 
    
	//app_fclose(fp);	
	return true;
}

static int sc_kf_flv_create_info_limited(FILE *fp, u32 limit, sc_kf_flv_info_t *key_info, u32 *key_num)
{
    u32 temp_ki_num;
    sc_kf_flv_info_t *temp_ki;
    int mem_size;
    void *p;

    int delta, i, drop = 0;
    u32 div;

    if (fp == NULL || key_info == NULL || key_num == NULL) {
        return -1;
    }

    if (limit < SC_KF_FLV_LIMITED_NUM_MIN) {
        fprintf(stderr, "%s ERROR: limit %u is too small\n", __func__, limit);
        return -1;
    }

    mem_size = sizeof(sc_kf_flv_info_t) * SC_KF_FLV_CREATE_INFO_TEMP_MAX_NUM;
    p = malloc(mem_size);
    if (p == NULL) {
        fprintf(stderr, "%s ERROR: allocate %d bytes memory failed\n", __func__, mem_size);
        return -1;
    }

    temp_ki = (sc_kf_flv_info_t *)p;
    if (sc_kf_flv_create_info_do(fp, temp_ki, &temp_ki_num) != true) {
        return -1;
    }

    if (temp_ki_num <= limit) {
        for (i = 0; i < temp_ki_num; i++) {
            key_info[i].file_pos = temp_ki[i].file_pos;
            key_info[i].time = temp_ki[i].time;
        }
        *key_num = temp_ki_num;
        return 0;
    }

    *key_num = 0;
    delta = (int)(temp_ki_num - limit);
    div = temp_ki_num / limit;
    for (i = 0; i < 2; i++) {   /* zhaoyao: first two key frames are needed */
        key_info[i].file_pos = temp_ki[i].file_pos;
        key_info[i].time = temp_ki[i].time;
        (*key_num)++;
    }
    if (div == 1) {
        for ( ; i < temp_ki_num; i++) {
            if (delta > 0 && drop) {
                delta--;
                drop = !drop;
                continue;
            }
            key_info[(*key_num)].file_pos = temp_ki[i].file_pos;
            key_info[(*key_num)].time = temp_ki[i].time;
            (*key_num)++;
            drop = !drop;
        }
    } else {
        div = div + 1;
        for ( ; i < temp_ki_num; i++) {
            if ((i % div) == 0) {
                key_info[(*key_num)].file_pos = temp_ki[i].file_pos;
                key_info[(*key_num)].time = temp_ki[i].time;
                (*key_num)++;
                if ((*key_num) >= limit) {
                    break;
                }
            }
        }
    }

    fprintf(stdout, "%s: true kf_num %u, limit to %u, compressed to %u\n",
                        __func__, temp_ki_num, limit, *key_num);

    if ((*key_num) > limit) {
        fprintf(stderr, "%s FATAL ERROR: generate key_frame(%u) is more than limit(%u)\n",
                            __func__, (*key_num), limit);
        return -1;
    }

    return 0;
}

int sc_kf_flv_create_info(sc_res_info_active_t *active)
{
    FILE *fp;
    int err = 0;
    char fpath[BUFFER_LEN];
    sc_res_info_t *ri;

    if (active == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return -1;
    }

    ri = &active->common;
    fprintf(stdout, "%s:  %120s\n", __func__, ri->url);

    if (!sc_res_is_normal(ri) && !sc_res_is_parsed(ri) && !sc_res_is_loaded(ri)) {
        fprintf(stderr, "%s ERROR: only normal or parsed (active) can create flv keyframe info\n",
                            __func__);
        return -1;
    }

    if (!sc_res_is_stored(ri) || sc_res_is_kf_crt(ri)) {
        fprintf(stderr, "%s ERROR: ri URL:\n\t%s\nflag:%lu can not create flv key frame\n",
                            __func__, ri->url, ri->flag);
        return -1;
    }

    bzero(fpath, BUFFER_LEN);
    if (sc_res_map_to_file_path(active, fpath, BUFFER_LEN) != 0) {
        fprintf(stderr, "%s ERROR: map %s to file path failed\n", __func__, active->localpath);
        return -1;
    }

    fp = fopen(fpath, "r");
    if (fp == NULL) {
        fprintf(stderr, "%s ERROR: fopen %s failed\n", __func__, fpath);
        return -1;
    }

    memset(active->kf_info, 0, sizeof(active->kf_info));
    active->kf_num = 0;
    if (sc_kf_flv_create_info_limited(fp, SC_KF_FLV_MAX_NUM, active->kf_info, (u32 *)&active->kf_num) != 0) {
        fprintf(stderr, "%s ERROR: create %s key frame info failed\n", __func__, fpath);
        err = -1;
        goto out;
    }

    sc_res_set_kf_crt(ri);

out:
    fclose(fp);

    return err;
}
