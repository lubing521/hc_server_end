/* 计算频文件名 */
/* author: yaoshangping */
/* data :20140308 */
#include "yk_lib.h"

/* 返回所在时区的本地时间中的年份 */
static int randomB() 
{
    bool _loc9_  = false;
    bool _loc10_ = true;
    int _loc7_ = 0;
    int _loc8_ = 0;
    int _loc1_[256];
    int _loc2_ = 1;
    int _loc3_ = 0;
    #if 0
	time_t t;
	struct tm *area;
    #endif

	while(_loc3_ < 256)
	{
		_loc7_ = _loc3_;
		_loc8_ = 8;
		while(--_loc8_ >= 0)
		{
		   if((_loc7_ & 1) != 0)
		   {
			  _loc7_ = 3988292384 ^ _loc7_ >> 1;
		   }
		   else
		   {
			  _loc7_ = _loc7_ >> 1;
		   }
		}
		_loc2_ = _loc7_ >> 1 & 10;
        _loc1_[_loc3_] = _loc7_;
		_loc2_ = 0;
		_loc3_++;
	}

#if 0
	tzset(); /*tzset()*/
	t = time32();
	/* 所在时区的本地时间 */
	localtime_r(&t, area);
	_loc2_ = area->tm_year + 1900;  /* TODO */
#endif
    _loc2_ = 2014;
	return _loc2_;
}

/* 该函数的返回值为2 */
static int randomFT()
{
    bool _loc9_  = false;
    bool _loc10_ = true;
    int _loc7_ = 0;
    int _loc8_ = 0;
    int _loc1_[256];
    int _loc2_ = 1;
    int _loc3_ = 0;
    #if 0
	time_t t;
	struct tm *area;
    #endif

	while(_loc3_ < 256)
	{
		_loc7_ = _loc3_;
		_loc8_ = 8;
		while(--_loc8_ >= 0)
		{
		   if((_loc7_ & 1) != 0)
		   {
			  _loc7_ = 3988292384 ^ _loc7_ >> 1;
		   }
		   else
		   {
			  _loc7_ = _loc7_ >> 1;
		   }
		}
		_loc2_ = _loc7_ >> 1 & 10;
        _loc1_[_loc3_] = _loc7_;
		_loc3_++;
	}
    #if 0
	tzset(); /*tzset()*/
	t = time32();
	/* 所在时区的本地时间 */
	localtime_r(&t, area);	    /* TODO */
    #endif
    
	return _loc2_;
}

/* 计算dt,具体含义不知道 */
static double zf(int *dt)
{
    bool _loc1_ = true;
    bool _loc2_ = false;
	double ret;
    *dt = (*dt * 211 + 30031) % 65536;
	ret = *dt;
	ret = ret / 65536;
    return ret;
}

/* @streamfileids: 服务器返回的streamfileids, "40*32*...*48*54*", 用于定位字符的位置 
 * @processed_str : 经过ranom8T加工过的字符串 */
/* @output_str: 返回提取的字符串, 注意还不是最终的fileids */
static bool cg_fun(char *streamfileids, char *processed_str, char *output_str) 
{    
    bool _loc5_ = true;
    bool _loc6_ = false;
    int _loc2_[YK_FILEID_LEN];
    int _loc4_;
    char *delim = "*";
    char *pstr;
    int i;

    if (streamfileids == NULL || 
        processed_str == NULL || 
        output_str == NULL) {
        return false;
    }
    
    pstr = strtok(streamfileids, delim);
    i = 0;
    /* 将下标转换成数字 */
    _loc2_[i++] = atoi(pstr);
    while ((pstr = strtok(NULL, delim))) {
        _loc2_[i++] = atoi(pstr);
    }

    _loc4_ = 0;
    while (_loc4_ < YK_FILEID_LEN) {
        output_str[_loc4_] = *(processed_str + _loc2_[_loc4_]);
        _loc4_++;
    }

    output_str[_loc4_] = '\0';

    return true;
}

/* 删除字符串中，指定的字符
 * @input, 原始字符串
 * @key, 要删除的字符
 */
static void del_key_in_str(char *input, char key) {
    int i;

    if (input == NULL) {
    }
    
    i = 0;
    while (input[i] != key && input[i] != '\0') {
        i++;
    }

    while (input[i] != '\0') {
        input[i] = input[i+1];
        i++;
    }

    input[i] = '\0';    
    return;
}

/* 返回 output_str */
static bool ranom8T(char *output_str, int seed)
{
    bool _loc8_ = false;
    bool _loc9_ = true;
    char _loc2_[YK_FILEID_LEN + 3];
    int _loc1_;
    int _loc4_;
    int _loc5_;
    int _loc6_;
    int _loc7_;
    char *alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char *symbol_number = "/\\:._-1234567890";
    char *tp_str;

    if (output_str == NULL) {
        return false;
    }
   
    _loc1_ = randomFT(); //返回值: 2    
    _loc1_ = _loc1_ + randomB(); //返回年: 2014
    memset(_loc2_, 0, sizeof(_loc2_));
    strcat(_loc2_, alphabet);
    strcat(_loc2_, symbol_number);         
    /* abc...xyzABC...XYZ/\:._-123...890 */
    _loc4_ = strlen(_loc2_); 
    _loc5_ = 0;
    _loc7_ = 0;
    while(_loc5_ < _loc4_) {
        _loc7_ = (int)(zf(&seed) * strlen(_loc2_));		
        output_str[_loc5_] = _loc2_[_loc7_];
        del_key_in_str(_loc2_, _loc2_[_loc7_]);		
        _loc5_++;
    }
    
    _loc6_ = randomFT(); //返回值: 2
    _loc1_ = _loc6_;
    return true;
}

/* 计算视频文件名
 * @streamfileids, 各类视频的streamfileids
 * @video_num, 该视频的分段号
 * @seed, 服务器给出的随机数
 * @fileids, 返回视频文件名,一共66个字符
 */
/* 在此，提供一组测试数据:
 * "40*32*40*40*40*12*40*17*40*40*44*12*31*2*64*49*44*44*45*2*40*45*40*40*64*45*1*18*32*12*48*12*40*12*32*2*18*12*39*48*44*44*49*39*44*31*64*48*39*54*48*32*54*39*1*2*2*49*2*35*44*31*64*1*48*54*"
 * video_num = 0
 * seed = 1599
 * fileids: 030002010052F96855C90C006CBE32420239E2-4558-5F64-A43A-B998975F6B4A
 */
bool yk_get_fileid(char *streamfileids, int video_num, int seed, char *fileids) 
{
	char vnum_str[3]; 
	char processed_str[YK_FILEID_LEN + 2];
	char output_str[YK_FILEID_LEN + 1];
	bool ret = false;

	if (streamfileids == NULL || fileids == NULL) {
		return false;
	}
	
	memset(processed_str, 0, sizeof(processed_str));
	ret = ranom8T(processed_str, seed);
	if (ret == false) {
		return false;
	}
	
	memset(output_str, 0, sizeof(output_str));
	ret = cg_fun(streamfileids, processed_str, output_str) ;
	if (ret == false) {
		return false;
	} else {
		/* 将分段号转换为16进制 */
		sprintf(vnum_str, "%x", video_num);
		if (strlen(vnum_str) == 1) {
			vnum_str[1] = vnum_str[0];
			vnum_str[0] = '0';
		}
		
		/* output_str的第9、10个字符替换掉 */
		output_str[8] = vnum_str[0];
		output_str[9] = vnum_str[1];
		memcpy(fileids, output_str, strlen(output_str));
		return true;
	}
}

