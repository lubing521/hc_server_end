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
        fprintf(stderr, "%s: unknown status code %u\n", __func__, status);
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
        fprintf(stderr, "%s: sendto failed, %s", __func__, strerror(errno));
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
    char *url;
    sc_res_info_origin_t *origin;
    
#if DEBUG
        fprintf(stdout, "%s DEBUG: req url:\n\t%s\n", __func__, req->url_data);
#endif

    url = (char *)req->url_data;

    /* zhaoyao XXX FIXME TODO: 如果参数不一样，url也将被视为不一样，如何配合snooping做好过滤? */
    origin = sc_res_info_find_origin(sc_res_info_list, url);
    if (origin != NULL) {
        if (!sc_res_is_origin(&origin->common)) {
            fprintf(stderr, "%s ERROR: url\n\t%s\ntype is conflicted\n", __func__, url);
        }
#if DEBUG
        fprintf(stderr, "%s WARNING: url\n\t%s\n is already exit\n", __func__, url);
#endif
        status = HTTP_SP_STATUS_OK;
        goto reply;
    }

    ret = sc_res_info_add_origin(sc_res_info_list, url, &origin);
    if (ret != 0) {
        fprintf(stderr, "%s: add %s in resources list failed\n", __func__, url);
        status = HTTP_SP_STATUS_DEFAULT_ERROR;
        goto reply;
    } else {
        /* zhaoyao XXX: return OK if add origin-type ri success anyway */
        fprintf(stdout, "%s: add origin url success:\n\t%s\n", __func__, origin->common.url);
        status = HTTP_SP_STATUS_OK;
    }

    /*
     * zhaoyao XXX TODO FIXME: if failure occured below, added origin ri should be deleted ???
     *                         parsing origin url maybe fail, we should make sure that all segments
     *                         are added in rl.
     */
    if (sc_url_is_yk(origin->common.url)) {
        ret = sc_get_yk_video(origin);
    } else if (sc_url_is_sohu(origin->common.url)) {
        ret = sc_get_sohu_video(origin);
    } else {
        fprintf(stderr, "%s: URL not support\n", __func__);
        goto reply;
    }


    if (ret < 0) {
        fprintf(stderr, "%s: parse or down %s failed\n", __func__, url);
    }

reply:
    ret = sc_snooping_resp_to_sp(sockfd, sa, salen, req, status);
    if (ret < 0) {
        fprintf(stderr, "%s: responsing(%u) Snooping module failed\n", __func__, status);
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
    sc_res_info_active_t *normal;
    char local_path[SC_RES_URL_MAX_LEN];

    normal = sc_res_info_find_active(sc_res_info_list, (const char *)req->url_data);
    if (normal != NULL) {
        if (!sc_res_is_normal(&normal->common)) {
            fprintf(stderr, "%s ERROR: url\n\t%s\ntype is conflicted\n", __func__, req->url_data);
        }
#if DEBUG
        fprintf(stderr, "%s WARNING: url\n\t%s\n is already exit\n", __func__, req->url_data);
#endif
        status = HTTP_SP_STATUS_OK;
        goto reply;
    }

    ret = sc_res_info_add_normal(sc_res_info_list, (char *)req->url_data, &normal);
    if (ret != 0) {
        fprintf(stderr, "%s: add url in resources list failed\n", __func__);
        status = HTTP_SP_STATUS_DEFAULT_ERROR;
        goto reply;
    }

    /* zhaoyao XXX: when add normal ri success, status set to OK, and calm down Snooping module */
    status = HTTP_SP_STATUS_OK;

    bzero(local_path, SC_RES_URL_MAX_LEN);
    ret = sc_res_get_local_path(&normal->common, local_path);
    if (ret != 0) {
        fprintf(stderr, "%s ERROR: sc_res_get_local_path failed, url %s\n", __func__, normal->common.url);
        goto reply;
    }

    ret = sc_ngx_download(normal->common.url, local_path);
    if (ret != 0) {
        fprintf(stderr, "%s: download %s failed\n", __func__, normal->common.url);
        /* zhaoyao XXX TODO FIXME: do we need sc_res_info_del_normal() now ??? */
        goto reply;
    }

    fprintf(stdout, "%s: inform Nginx to download %s success\n", __func__, normal->common.url);

reply:
    ret = sc_snooping_resp_to_sp(sockfd, sa, salen, req, status);
    if (ret < 0) {
        fprintf(stderr, "%s: responsing(%u) Snooping module failed\n", __func__, status);
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
        fprintf(stderr, "%s: invalid sockfd %d\n", __func__, sockfd);
        return;
    }

    while (1) {
        salen = sizeof(struct sockaddr);
        bzero(&sa, salen);
        bzero(buf, sizeof(buf));
        if ((nrecv = recvfrom(sockfd, buf, SC_SNOOPING_SND_RCV_BUF_LEN, 0, &sa, &salen)) < 0) {
            fprintf(stderr, "%s: recvfrom error: %s", __func__, strerror(errno));
            continue;
        }

        if (nrecv < sizeof(http_sp2c_req_pkt_t)) {
            fprintf(stderr, "%s: recvfrom invalid %d bytes\n", __func__, nrecv);
            continue;
        }
#if DEBUG
        in_sa = (struct sockaddr_in *)&sa;
        fprintf(stderr, "%s client: port %u, ip %s\n", __func__, ntohs(in_sa->sin_port),
                                inet_ntop(AF_INET, &in_sa->sin_addr, ip_addr, MAX_HOST_NAME_LEN));
#endif
        sp2c_req = (http_sp2c_req_pkt_t *)buf;
        sp2c_req->url_len = ntohs(sp2c_req->url_len);
        if (sp2c_req->url_len >= SC_RES_URL_MAX_LEN) {
            fprintf(stderr, "%s WARNING: sp2c_req's url (%u) is longer than %d\n",
                                __func__, sp2c_req->url_len, SC_RES_URL_MAX_LEN);
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
            fprintf(stderr, "%s: HTTP_SP2C_ACTION_GETNEXT %u not support now\n", __func__, sp2c_req->sp2c_action);
            break;
        default:
            fprintf(stderr, "%s: unknown sp2c_action %u\n", __func__, sp2c_req->sp2c_action);
        }
    }
}

int sc_snooping_do_add(sc_res_info_t *ri)
{
    int err = 0, nsend, nrecv;
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;
    char buf[SC_SNOOPING_SND_RCV_BUF_LEN];
    http_c2sp_req_pkt_t *req;
    http_c2sp_res_pkt_t *res;

    if (ri == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return -1;
    }

    if (!sc_res_is_stored(ri) || sc_res_is_notify(ri)) {
        fprintf(stderr, "%s ERROR: ri URL:\n\t%s\nflag:%lu can not be added\n",
                            __func__, ri->url, ri->flag);
        return -1;
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)HTTP_C2SP_PORT);
    sa.sin_addr.s_addr = inet_addr(SC_SNOOP_MOD_DEFAULT_IP_ADDR);
    salen = sizeof(struct sockaddr_in);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "%s Socket failed: %s\n", __func__, strerror(errno));
        return -1;
    }

    memset(buf, 0, sizeof(buf));
    if (SC_SNOOPING_SND_RCV_BUF_LEN < sizeof(http_c2sp_req_pkt_t)) {
        fprintf(stderr, "%s ERROR: small buf(%d) can not hold c2sp_req(%d)\n",
                            __func__, SC_SNOOPING_SND_RCV_BUF_LEN, sizeof(http_c2sp_req_pkt_t));
        return -1;
    }
    req = (http_c2sp_req_pkt_t *)buf;
    req->session_id = ri->id;
    req->c2sp_action = HTTP_C2SP_ACTION_ADD;
    req->url_len = htons(strlen(ri->url));
    sc_res_copy_url((char *)req->usr_data, ri->url, HTTP_SP_URL_LEN_MAX, 1);

    if ((nsend = sendto(sockfd, req, sizeof(http_c2sp_req_pkt_t), 0, (struct sockaddr *)&sa, salen)) < 0) {
        fprintf(stderr, "%s ERROR sendto failed: %s\n", __func__, strerror(errno));
        err = -1;
        goto out;
    }
    fprintf(stdout, "%s url: %s\n", __func__, req->usr_data);
//    fprintf(stdout, "%s sendto %d success\n", __func__, nsend);

    memset(buf, 0, sizeof(buf));
    nrecv = recvfrom(sockfd, buf, SC_SNOOPING_SND_RCV_BUF_LEN, 0, NULL, NULL);
//    fprintf(stdout, "%s recvfrom %d\n", __func__, nrecv);
    /* zhaoyao TODO XXX: timeout */
    if (nrecv < 0) {
        fprintf(stderr, "%s ERROR recvfrom %d, is not valid to %d: %s\n",
                            __func__, nrecv, sizeof(http_c2sp_res_pkt_t), strerror(errno));
        err = -1;
        goto out;
    }
    res = (http_c2sp_res_pkt_t *)buf;
    if (res->session_id != ri->id) {
        fprintf(stderr, "%s ERROR: send id %lu, not the same as response id %u\n",
                            __func__, ri->id, res->session_id);
        err = -1;
        goto out;
    }

    if (res->status == HTTP_SP_STATUS_OK) {
        fprintf(stdout, "%s OK\n", __func__);
        sc_res_set_notify(ri);
    } else {
        fprintf(stderr, "%s ERROR: response status is failed %u\n", __func__, res->status);
        err = -1;
        goto out;
    }

out:
    close(sockfd);
    return err;
}

