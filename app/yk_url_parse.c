
#include "header.h"
#include "yk_lib.h"

#define RESP_BUF_LEN        (0x1 << 14)         /* 16KB */
#define HTTP_URL_PRE_LEN    7                   /* strlen("http://") */

static unsigned char yk_request_pattern[] = 
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Connection: close\r\n"
    "User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) "
                "Chrome/33.0.1750.117 Safari/537.36\r\n"
    "Accept: */*\r\n"
    "Referer: %s\r\n"
    "Accept-Encoding: gzip,deflate,sdch\r\n"
    "Accept-Language: en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4\r\n\r\n";

static int yk_build_request(const char *host,
                            const unsigned char *uri,
                            const unsigned char *referer,
                            unsigned char *buf)
{
    if (buf == NULL) {
        return -1;
    }
    
    /* zhaoyao TODO: checking request line ,header and length */

    sprintf((char *)buf, (char *)yk_request_pattern, uri, host, referer);

    return 0;
}

static void yk_print_usage(char *cmd)
{
    printf("%s youku_video_url\n", cmd);
    printf("\tURL's format is http://v.youku.com/v_show/id_XNjgzMjc0MjY4.html\n\n");
}

/* zhaoyao TODO: url's validity */
static int yk_is_valid_url(const unsigned char *url)
{
    return 1;
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo *ailist, *aip;
    struct addrinfo hint;
    char host[MAX_NAME_LEN];
    unsigned char *url, uri[BUFFER_LEN], *real_start;
    unsigned char buffer[BUFFER_LEN];
    unsigned char *response = NULL;
    int nsend, nrecv, len, i;
    int err = 0, status;
    yk_stream_info_t *streams[STREAM_TYPE_TOTAL];
    char seed[SEED_STRING_LEN];

    if (argc == 2) {
        url = (unsigned char *)argv[1];
    } else {
        yk_print_usage(argv[0]);
        exit(-1);
    }

    if (!yk_is_valid_url(url)) {
        fprintf(stderr, "Invalid URL of Youku video: %s\n", url);
        exit(-1);
    }

    memset(uri, 0, BUFFER_LEN);
    /* zhaoyao FIXME TODO: yk_url_to_playlist has no string length control, overflow may occur */
    if (yk_url_to_playlist((char *)url, (char *)uri) != true) {
        fprintf(stderr, "yk_url_to_playlist failed, url is:\n%s\n", url);
        exit(-1);
    }

    memset(host, 0, MAX_NAME_LEN);
    for (i = 0; (*(uri + HTTP_URL_PRE_LEN + i) != '/') && (*(uri + HTTP_URL_PRE_LEN + i) != '\0'); i++) {
        host[i] = *(uri + HTTP_URL_PRE_LEN + i);
    }
    if (*(uri + HTTP_URL_PRE_LEN + i) == '\0') {
        fprintf(stderr, "yk_url_to_playlist uri is invalid: %s\n", uri);
        exit(-1);
    }
    host[i] = '\0';
    real_start = uri + HTTP_URL_PRE_LEN + i;
    
    printf("getPlaylist's\nHost:%s\nURI:%s\n", host, real_start);

    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "http", &hint, &ailist) != 0) {
        perror("Getaddrinfo failed");
        exit(-1);
    }
    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
        if ((sockfd = socket(aip->ai_family, SOCK_STREAM, 0)) < 0) {
            perror("Socket failed, try again...");
            continue;
        }
        if (sock_conn_retry(sockfd, aip->ai_addr, aip->ai_addrlen) < 0) {
            close(sockfd);
            fprintf(stderr, "Sock_conn_retry failed, try again...\n");
            continue;
        }
        break;
    }
    if (aip == NULL) {
        fprintf(stderr, "Can not connect to %s: http\n", host);
        exit(-1);
    }

    memset(buffer, 0, BUFFER_LEN);
    if (yk_build_request(host, real_start, url, buffer) < 0) {
        fprintf(stderr, "yk_build_request failed\n");
        err = -1;
        goto out;
    }

    len = strlen((char *)buffer);
    if (len > BUFFER_LEN) {
        fprintf(stderr, "Request is too long, error!!!\n");
        err = -1;
        goto out;
    }
    printf("************** Request ***************\n%s**************************************\n", buffer);
    nsend = send(sockfd, buffer, len, 0);
    if (nsend != len) {
        perror("Send failed");
        err = -1;
        goto out;
    }

    response = malloc(RESP_BUF_LEN);
    if (response == NULL) {
        perror("Malloc failed");
        err = -1;
        goto out;
    }
    memset(response, 0, RESP_BUF_LEN);
    nrecv = recv(sockfd, response, RESP_BUF_LEN, MSG_WAITALL);
    if (nrecv <= 0) {
        perror("Recv failed or meet EOF");
        err = -1;
        goto out;
    }

    if (http_parse_status_line(response, &status) < 0) {
        fprintf(stderr, "Parse status line failed:\n%s", response);
        err = -1;
        goto out;
    }

    if (status == 200) {
        fprintf(stdout, "Get response success!!! Response status code %d\n%s\n", status, response);
    } else {
        fprintf(stderr, "Response status code %d:%s", status, response);
    }

    memset(streams, 0, sizeof(streams));
    memset(seed, 0, SEED_STRING_LEN);
    if (yk_parse_playlist((char *)response, streams, seed) != 0) {
        fprintf(stderr, "Parse getPlaylist response failed\n");
    } else {
        printf("Parse getPlaylist response success, seed is %s\n", seed);
        yk_debug_streams_all(streams);
    }

out:
    free(response);
    close(sockfd);
    yk_destroy_streams_all(streams);

    return err;
}

