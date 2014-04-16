#include "common.h"
#include "sc_header.h"
#include "http-snooping.h"
#include "net_util.h"

int main(int argc, char *argv[])
{
    int err = 0, ret;
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;
    char *ip_addr, default_ip[MAX_HOST_NAME_LEN] = SC_CLIENT_DEFAULT_IP_ADDR;
    pthread_t tid;
    int shmid;

    if (argc == 2) {
        ip_addr = argv[1];
        fprintf(stdout, "Using IP address %s\n", ip_addr);
    } else if (argc == 1) {
        ip_addr = default_ip;
        fprintf(stdout, "Using default IP address %s\n", ip_addr);
    } else {
        fprintf(stderr, "Usage: %s [bind IP address]\n", argv[0]);
        return -1;
    }

    shmid = sc_res_list_alloc_and_init(&sc_res_info_list);
    if (shmid < 0) {
        fprintf(stderr, "sc_res_list_alloc_and_init failed\n");
        return -1;
    }

    /* zhaoyao XXX: 初始化本地已有资源 */
    ret = sc_ld_init_and_load();
    if (ret != 0) {
        fprintf(stderr, "%s ERROR: load stored local resources failed\n", __func__);
    }

    /* zhaoyao XXX: 初始化优酷广告 */
    ret = sc_yk_init_vf_adv();
    if (ret != 0) {
        fprintf(stderr, "%s ERROR: in Youku VF initial procedure\n", __func__);
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)HTTP_SP2C_PORT);
    sa.sin_addr.s_addr = inet_addr(ip_addr);
    salen = sizeof(struct sockaddr_in);

    sockfd = sock_init_server(SOCK_DGRAM, (struct sockaddr *)&sa, salen, 0);
    if (sockfd < 0) {
        fprintf(stderr, "sock_init_server failed\n");
        err = -1;
        goto out1;
    }

    if (pthread_create(&tid, NULL, sc_res_list_process_thread, NULL) != 0) {
        fprintf(stderr, "Create sc_res_list_process_thread failed: %s\n", strerror(errno));
        err = -1;
        goto out2;
    }

    if (pthread_detach(tid) != 0) {
        fprintf(stderr, "Detach sc_res_list_process_thread failed: %s\n", strerror(errno));
        err = -1;
        goto out2;
    }

    /*
     * zhaoyao XXX TODO FIXME: 由于snooping serve线程和资源列表处理线程都会对res_info_list操作，需要
     *                         使用互斥锁保护临界资源，目前仅仅采用延迟snooping serve的方法。
     */
    sleep(5);

    sc_snooping_serve(sockfd);

out2:
    close(sockfd);
out1:
    sc_res_list_destroy_and_uninit(shmid);

    return err;
}

