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
        printf("Using IP address %s\n", ip_addr);
    } else if (argc == 1) {
        ip_addr = default_ip;
        printf("Using default IP address %s\n", ip_addr);
    } else {
        printf("Usage: %s [bind IP address]\n", argv[0]);
        return -1;
    }

    shmid = sc_res_list_alloc_and_init(&sc_res_info_list);
    if (shmid < 0) {
        hc_log_error("sc_res_list_alloc_and_init failed");
        return -1;
    }

    /* zhaoyao XXX: ��ʼ������������Դ */
    ret = sc_ld_init_and_load();
    if (ret != 0) {
        hc_log_error("Load stored local resources failed");
    }

    /* zhaoyao XXX: ��ʼ���ſ��� */
    ret = sc_yk_init_vf_adv();
    if (ret != 0) {
        hc_log_error("In Youku VF initial procedure");
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)HTTP_SP2C_PORT);
    sa.sin_addr.s_addr = inet_addr(ip_addr);
    salen = sizeof(struct sockaddr_in);

    sockfd = sock_init_server(SOCK_DGRAM, (struct sockaddr *)&sa, salen, 0);
    if (sockfd < 0) {
        hc_log_error("sock_init_server failed");
        err = -1;
        goto out1;
    }

    if (pthread_create(&tid, NULL, sc_res_list_process_thread, NULL) != 0) {
        hc_log_error("Create sc_res_list_process_thread failed: %s", strerror(errno));
        err = -1;
        goto out2;
    }

    if (pthread_detach(tid) != 0) {
        hc_log_error("Detach sc_res_list_process_thread failed: %s", strerror(errno));
        err = -1;
        goto out2;
    }

    /*
     * zhaoyao XXX TODO FIXME: ����snooping serve�̺߳���Դ�б������̶߳����res_info_list��������Ҫ
     *                         ʹ�û����������ٽ���Դ��Ŀǰ���������ӳ�snooping serve�ķ�����
     */
    sleep(5);

    sc_snooping_serve(sockfd);

out2:
    close(sockfd);
out1:
    sc_res_list_destroy_and_uninit(shmid);

    return err;
}
