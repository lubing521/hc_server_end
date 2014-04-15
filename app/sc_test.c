#include "common.h"
#include "sc_header.h"
#include "http-snooping.h"

#if 0
int main(int argc, char *argv[])
{
    int ret;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s url\n", argv[0]);
        return -1;
    }

    ret = sc_snooping_do_add(-1, argv[1]);
    if (ret != 0) {
        fprintf(stderr, "Add failed\n");
        return -1;
    }

    return 0;
}
#endif

#if 0
int main(int argc, char *argv[])
{
    int ret;
    char *vf_url = "valf.atm.youku.com/vf?vip=0&site=1&rt=MHwxMzk3NDQ0MzA3ODI1fFhOamd5TURrNU1qTXk=&p=1&vl=5272&fu=0&ct=c&cs=2034&paid=0&s=177092&td=0&sid=739744430145310e7d997&v=170524808&wintype=interior&u=97454045&vs=1.0&rst=flv&partnerid=null&dq=mp4&k=%u6536%u8D39&os=Windows%207&d=0&ti=%E5%A4%A7%E8%AF%9D%E5%A4%A9%E4%BB%99";
    char *referer = "http://v.youku.com/v_show/id_XNjgyMDk5MjMy.html";

    ret = sc_yk_get_vf(vf_url, referer);
    if (ret != 0) {
        fprintf(stderr, "Get Youku vf response failed\n");
        return -1;
    }

    return 0;
}
#endif
