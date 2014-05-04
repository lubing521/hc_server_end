/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * sc_snooping.c
 * Original Author: zhaoyao@ruijie.com.cn, 2014-04-28
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#include "common.h"
#include "sc_header.h"
#include "http-snooping.h"

static int sc_snooping_resp_to_sp(int sockfd,
                                  struct sockaddr *sa, 
                                  socklen_t salen,
                                  http_sp2c_req_pkt_t *req,
                                  const u8 status)
{
    http_sp2c_res_pkt_t *resp;
    u8 old;
    int nsend;

    if (status != HTTP_SP_STATUS_OK && status != HTTP_SP_STATUS_DEFAULT_ERROR) {
        hc_log_error("unknown status code %u", status);
        return -1;
    }

    /* zhaoyao XXX FIXME: http_sp2c_req_pkt_t and http_sp2c_res_pkt_t almost the same, reuse req */
    resp = (http_sp2c_res_pkt_t *)req;
    old = req->sp2c_action; /* zhaoyao XXX FIXME: change req's value directly maybe dangerous */
    resp->status = status;
    resp->url_len = htons(resp->url_len);
    nsend = sendto(sockfd, (void *)resp, sizeof(http_sp2c_res_pkt_t), 0, sa, salen);

    req->url_len = ntohs(req->url_len);
    req->sp2c_action = old; /* zhaoyao XXX: keep it untouched */

    if (nsend < 0) {
        hc_log_error("sendto failed, %s", strerror(errno));
        return -1;
    }

    return 0;
}

static void sc_snooping_do_parse(int sockfd,
                                 struct sockaddr *sa, 
                                 socklen_t salen,
                                 http_sp2c_req_pkt_t *req)
{
    int ret;
    u8 status;
    char *req_url, origin_url[HTTP_URL_MAX_LEN];
    sc_res_info_mgmt_t *origin;
    
#if 1
    hc_log_debug("req url: %s", req->url_data);
#endif

    req_url = (char *)req->url_data;

    /* zhaoyao XXX: 使用固定的pattern生成原始url */
    bzero(origin_url, HTTP_URL_MAX_LEN);
    ret = sc_res_gen_origin_url(req_url, origin_url);
    if (ret != 0) {
        hc_log_error("generate origin url failed, req_url: %s...", req_url);
        status = HTTP_SP_STATUS_OK;
        goto reply;
    }

    origin = sc_res_info_find_mgmt(sc_res_info_list, origin_url);
    if (origin != NULL) {
        if (!sc_res_gen_is_origin(&origin->common)) {
            hc_log_error("url\n\t%s\ntype is conflicted", origin->common.url);
        }
#if DEBUG
        hc_log_debug("url\n\t%s\n is already exit", origin->common.url);
#endif
        status = HTTP_SP_STATUS_OK;
        goto reply;
    }

    ret = sc_res_info_add_origin(sc_res_info_list, origin_url, &origin);
    if (ret != 0) {
        hc_log_error("add %s in resources list failed", origin_url);
        status = HTTP_SP_STATUS_DEFAULT_ERROR;
        goto reply;
    } else {
        /* zhaoyao XXX: return OK if add origin-type ri success anyway */
        hc_log_info("add origin url: %s", origin->common.url);
        status = HTTP_SP_STATUS_OK;
    }

    /*
     * zhaoyao XXX TODO FIXME: if failure occured below, added origin ri should be deleted ???
     *                         parsing origin url maybe fail, we should make sure that all segments
     *                         are added in rl.
     *                         sc_get_xxx_video的行为需要明确的定义，才能知道在出错时如何处理origin.
     *                         错误最可能是添加parsed失败、同服务器通信失败，这都需要对origin进行容错
     *                         处理。
     */
    if (sc_res_site_is_youku(&origin->common)) {
        ret = sc_get_yk_video(origin);
    } else if (sc_res_site_is_sohu(&origin->common)) {
        ret = sc_get_sohu_video(origin);
    } else {
        hc_log_error("URL not support");
        goto reply;
    }

    if (ret < 0) {
        hc_log_error("parse or down %s failed", origin->common.url);
    }

reply:
    ret = sc_snooping_resp_to_sp(sockfd, sa, salen, req, status);
    if (ret < 0) {
        hc_log_error("responsing(%u) Snooping module failed", status);
    }

    return;
}

static void sc_snooping_do_down(int sockfd,
                                struct sockaddr *sa, 
                                socklen_t salen,
                                http_sp2c_req_pkt_t *req)
{
    int ret;
    u8 status;
    sc_res_info_mgmt_t *normal;
    sc_res_info_ctnt_t *ctnt;

    normal = sc_res_info_find_mgmt(sc_res_info_list, (const char *)req->url_data);
    if (normal != NULL) {
        if (!sc_res_gen_is_normal(&normal->common)) {
            hc_log_error("url\n\t%s\ntype is conflicted", req->url_data);
        }
#if DEBUG
        hc_log_debug("url\n\t%s\n is already exit", req->url_data);
#endif
        status = HTTP_SP_STATUS_OK;
        goto reply;
    }

    ret = sc_res_info_add_normal(sc_res_info_list, (char *)req->url_data, &normal);
    if (ret != 0) {
        hc_log_error("add normal ri in resources list failed");
        status = HTTP_SP_STATUS_DEFAULT_ERROR;
        goto reply;
    }

    /* zhaoyao XXX: 对normal而言，其content的url与它的url相同。 */
    ret = sc_res_info_add_ctnt(sc_res_info_list, normal, normal->common.url, &ctnt);
    if (ret != 0) {
        hc_log_error("add content ri in resources list failed");
        status = HTTP_SP_STATUS_DEFAULT_ERROR;
        goto reply;
    }

    /* zhaoyao XXX: when add content ri success, status set to OK, and calm down Snooping module */
    status = HTTP_SP_STATUS_OK;

    ret = sc_ngx_download(ctnt->common.url, ctnt->localpath);
    if (ret != 0) {
        hc_log_error("download %s failed", ctnt->common.url);
        /*
         * zhaoyao XXX: 同parsed，当ri添加成功，但download失败时，尝试重复下载。
         */
        sc_res_flag_set_i_fail(&ctnt->common);
        goto reply;
    }

    hc_log_info("inform Nginx to download %s success", ctnt->common.url);

reply:
    ret = sc_snooping_resp_to_sp(sockfd, sa, salen, req, status);
    if (ret < 0) {
        hc_log_error("responsing(%u) Snooping module failed\n", status);
    }

    return;
}

void sc_snooping_serve(int sockfd)
{
    int nrecv;
    struct sockaddr sa;
    socklen_t salen;
    char buf[SC_SNOOPING_SND_RCV_BUF_LEN];
    http_sp2c_req_pkt_t *sp2c_req;
#if DEBUG
    struct sockaddr_in *in_sa;
    char ip_addr[MAX_HOST_NAME_LEN];
#endif

    if (sockfd < 0) {
        hc_log_error("invalid sockfd %d", sockfd);
        return;
    }

    while (1) {
        salen = sizeof(struct sockaddr);
        bzero(&sa, salen);
        bzero(buf, sizeof(buf));
        if ((nrecv = recvfrom(sockfd, buf, SC_SNOOPING_SND_RCV_BUF_LEN, 0, &sa, &salen)) < 0) {
            hc_log_error("recvfrom error: %s", strerror(errno));
            continue;
        }

        if (nrecv < sizeof(http_sp2c_req_pkt_t)) {
            hc_log_error("recvfrom invalid %d bytes", nrecv);
            continue;
        }
#if DEBUG
        in_sa = (struct sockaddr_in *)&sa;
        hc_log_debug("client: port %u, ip %s", ntohs(in_sa->sin_port),
                                inet_ntop(AF_INET, &in_sa->sin_addr, ip_addr, MAX_HOST_NAME_LEN));
#endif
        sp2c_req = (http_sp2c_req_pkt_t *)buf;
        sp2c_req->url_len = ntohs(sp2c_req->url_len);
        if (sp2c_req->url_len >= HTTP_URL_MAX_LEN) {
            hc_log_error("sp2c_req's url (%u) is longer than %d", sp2c_req->url_len, HTTP_URL_MAX_LEN);
            continue;
        }

        switch (sp2c_req->sp2c_action) {
        case HTTP_SP2C_ACTION_PARSE:
            sc_snooping_do_parse(sockfd, &sa, salen, sp2c_req);
            break;
        case HTTP_SP2C_ACTION_DOWN:
            sc_snooping_do_down(sockfd, &sa, salen, sp2c_req);
            break;
        case HTTP_SP2C_ACTION_GETNEXT:
            hc_log_error("HTTP_SP2C_ACTION_GETNEXT %u not support now", sp2c_req->sp2c_action);
            break;
        default:
            hc_log_error("unknown sp2c_action %u", sp2c_req->sp2c_action);
        }
    }
}

static int sc_snooping_initiate_action(u8 act, u32 sid, char *url)
{
    int err = 0, nsend, nrecv;
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;
    char buf[SC_SNOOPING_SND_RCV_BUF_LEN];
    http_c2sp_req_pkt_t *req;
    http_c2sp_res_pkt_t *res;

    if (url == NULL) {
        hc_log_error("invalid input");
        return -1;
    }

    if (act != HTTP_C2SP_ACTION_ADD && act != HTTP_C2SP_ACTION_DELETE) {
        hc_log_error("non-support action %u", act);
        return -1;
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)HTTP_C2SP_PORT);
    sa.sin_addr.s_addr = inet_addr(SC_SNOOP_MOD_DEFAULT_IP_ADDR);
    salen = sizeof(struct sockaddr_in);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        hc_log_error("Socket failed: %s", strerror(errno));
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    if (SC_SNOOPING_SND_RCV_BUF_LEN < sizeof(http_c2sp_req_pkt_t)) {
        hc_log_error("small buf(%d) can not hold c2sp_req(%d)", SC_SNOOPING_SND_RCV_BUF_LEN, sizeof(http_c2sp_req_pkt_t));
        return -1;
    }
    req = (http_c2sp_req_pkt_t *)buf;
    req->session_id = sid;
    req->c2sp_action = act;
    req->url_len = htons(strlen(url));
    sc_res_copy_url((char *)req->usr_data, url, HTTP_SP_URL_LEN_MAX, 1);

    if ((nsend = sendto(sockfd, req, sizeof(http_c2sp_req_pkt_t), 0, (struct sockaddr *)&sa, salen)) < 0) {
        hc_log_error("sendto failed: %s", strerror(errno));
        err = -1;
        goto out;
    }

    memset(buf, 0, sizeof(buf));
    nrecv = recvfrom(sockfd, buf, SC_SNOOPING_SND_RCV_BUF_LEN, 0, NULL, NULL);
    if (nrecv < 0) {
        hc_log_error("recvfrom %d, is not valid to %d: %s",
                            nrecv, sizeof(http_c2sp_res_pkt_t), strerror(errno));
        err = -1;
        goto out;
    }
    res = (http_c2sp_res_pkt_t *)buf;
    if (res->session_id != sid) {
        hc_log_error("send id %u, not the same as response id %u", sid, res->session_id);
        err = -1;
        goto out;
    }

    if (res->status == HTTP_SP_STATUS_OK) {
        ;
    } else {
        hc_log_error("response status is failed %u", res->status);
        err = -1;
        goto out;
    }

out:
    close(sockfd);
    return err;
}

int sc_snooping_do_add(u32 sid, char *url)
{
    int ret;

    ret = sc_snooping_initiate_action(HTTP_C2SP_ACTION_ADD, sid, url);
    hc_log_info("%120s", url);

    return ret;
}

int sc_snooping_do_del(u32 sid, char *url)
{
    int ret;

    ret = sc_snooping_initiate_action(HTTP_C2SP_ACTION_DELETE, sid, url);
    hc_log_info("%120s", url);

    return ret;
}


