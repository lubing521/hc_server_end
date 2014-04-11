#include "common.h"
#include "sc_header.h"
#include "http-snooping.h"

int main(int argc, char *argv[])
{
    int err = 0, nsend, nrecv;
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;
    char buf[SC_SNOOPING_SND_RCV_BUF_LEN];
    http_c2sp_req_pkt_t *req;
    http_c2sp_res_pkt_t *res;
    char *url;

    if (argc != 2) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return -1;
    }

    url = argv[1];
    if (strncmp(url, HTTP_URL_PREFIX, HTTP_URL_PRE_LEN) == 0) {
        url += HTTP_URL_PRE_LEN;
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
    req->session_id = 0;
    req->c2sp_action = HTTP_C2SP_ACTION_ADD;
    req->url_len = htons(strlen(url));
    strcpy((char *)req->usr_data, url);

    if ((nsend = sendto(sockfd, req, sizeof(http_c2sp_req_pkt_t), 0, (struct sockaddr *)&sa, salen)) < 0) {
        fprintf(stderr, "%s ERROR sendto failed: %s\n", __func__, strerror(errno));
        err = -1;
        goto out;
    }
    fprintf(stdout, "%s url: %s\n", __func__, req->usr_data);

    memset(buf, 0, sizeof(buf));
    nrecv = recvfrom(sockfd, buf, SC_SNOOPING_SND_RCV_BUF_LEN, 0, NULL, NULL);
    if (nrecv < 0) {
        fprintf(stderr, "%s ERROR recvfrom %d, is not valid to %d: %s\n",
                            __func__, nrecv, sizeof(http_c2sp_res_pkt_t), strerror(errno));
        err = -1;
        goto out;
    }
    res = (http_c2sp_res_pkt_t *)buf;
    if (res->session_id != 0) {
        fprintf(stderr, "%s ERROR: send id %u, not the same as response id %u\n",
                            __func__, 0, res->session_id);
        err = -1;
        goto out;
    }

    if (res->status == HTTP_SP_STATUS_OK) {
        fprintf(stdout, "%s OK\n", __func__);
    } else {
        fprintf(stderr, "%s ERROR: response status is failed %u\n", __func__, res->status);
        err = -1;
        goto out;
    }

out:
    close(sockfd);
    return err;
}

