/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * net_util.c
 * Original Author: zhaoyao@ruijie.com.cn, 2014-03-11
 *
 * Hot cache application's common network utilities codes file.
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#include "common.h"
#include "net_util.h"

int sock_conn_retry(int sockfd, const struct sockaddr *addr, socklen_t alen)
{
    int nsec;

    if (addr == NULL) {
        return -1;
    }

    for (nsec = 1; nsec <= TCP_CONN_MAX_RETRY_TIME; nsec <<= 1) {
        if (connect(sockfd, addr, alen) == 0) {
            /* Connect success */
            return 0;
        }
        perror("Connect failed");
        if (nsec <= TCP_CONN_MAX_RETRY_TIME / 2) {
            sleep(nsec);
        }
    }

    /* Connect failed */
    return -1;
}

int sock_init_server(int type, const struct sockaddr *addr, socklen_t alen, int qlen)
{
    int fd;
    int err = 0;

    if ((fd = socket(addr->sa_family, type, 0)) < 0) {
        perror("Socket");
        return -1;
    }

    if (bind(fd, addr, alen) < 0) {
        err = errno;
        perror("Bind");
        goto errout;
    }

    if (type == SOCK_STREAM || type == SOCK_SEQPACKET) {
        if (listen(fd, qlen) < 0) {
            err = errno;
            perror("Listen");
            goto errout;
        }
    }

    return fd;

errout:
    close(fd);
    errno = err;
    return -1;
}

#if 1

int http_host_connect(const char *host)
{
    int sockfd;
    struct addrinfo *ailist, *aip;
    struct addrinfo hint;

    if (host == NULL) {
        return -1;
    }

    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "http", &hint, &ailist) != 0) {
        perror("Getaddrinfo failed");
        return -1;
    }
    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
        if ((sockfd = socket(aip->ai_family, SOCK_STREAM, 0)) < 0) {
            perror("Socket failed, try again...");
            continue;
        }
        if (sock_conn_retry(sockfd, aip->ai_addr, aip->ai_addrlen) < 0) {
            close(sockfd);
            hc_log_error("Sock_conn_retry failed, try again...");
            continue;
        }
        break;
    }
    if (aip == NULL) {
        hc_log_error("Can not connect to %s: http", host);
        return -1;
    }

    return sockfd;
}

#else

int http_host_connect(const char *host)
{
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;

    if (host == NULL) {
        return -1;
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)80);
    if (strcmp(host, "v.youku.com") == 0) {
        /*
         * Address: 183.61.116.217
         * Address: 183.61.116.215
         * Address: 183.61.116.218
         * Address: 183.61.116.216
         */
        sa.sin_addr.s_addr = inet_addr("183.61.116.218");
    } else if (strcmp(host, "f.youku.com") == 0) {
        /*
         * Address: 183.61.116.54
         * Address: 183.61.116.52
         * Address: 183.61.116.55
         * Address: 183.61.116.53
         * Address: 183.61.116.56
         */
        sa.sin_addr.s_addr = inet_addr("183.61.116.52");
    } else {
        hc_log_error("Unknown host %s", host);
        return -1;
    }

    salen = sizeof(struct sockaddr_in);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        return -1;
    }

    if (sock_conn_retry(sockfd, (struct sockaddr *)&sa, salen) < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

#endif

int http_parse_status_line(char *buf, int len, int *status)
{
    char ch;
    char *p;
    int status_digits = 0;
    enum {
        sw_start = 0,
        sw_H,
        sw_HT,
        sw_HTT,
        sw_HTTP,
        sw_first_major_digit,
        sw_major_digit,
        sw_first_minor_digit,
        sw_minor_digit,
        sw_status,
        sw_space_after_status,
        sw_status_text,
        sw_almost_done
    } state;

    if (buf == NULL || status == NULL || len <= 0) {
        return -1;
    }

    state = sw_start;
    *status = 0;

    for (p = buf; p < buf + len; p++) {
        ch = *p;

        switch (state) {

        /* "HTTP/" */
        case sw_start:
            switch (ch) {
            case 'H':
                state = sw_H;
                break;
            default:
                return -1;
            }
            break;

        case sw_H:
            switch (ch) {
            case 'T':
                state = sw_HT;
                break;
            default:
                return -1;
            }
            break;

        case sw_HT:
            switch (ch) {
            case 'T':
                state = sw_HTT;
                break;
            default:
                return -1;
            }
            break;

        case sw_HTT:
            switch (ch) {
            case 'P':
                state = sw_HTTP;
                break;
            default:
                return -1;
            }
            break;

        case sw_HTTP:
            switch (ch) {
            case '/':
                state = sw_first_major_digit;
                break;
            default:
                return -1;
            }
            break;

        /* the first digit of major HTTP version */
        case sw_first_major_digit:
            if (ch < '1' || ch > '9') {
                return -1;
            }

            state = sw_major_digit;
            break;

        /* the major HTTP version or dot */
        case sw_major_digit:
            if (ch == '.') {
                state = sw_first_minor_digit;
                break;
            }

            if (ch < '0' || ch > '9') {
                return -1;
            }
            break;

        /* the first digit of minor HTTP version */
        case sw_first_minor_digit:
            if (ch < '0' || ch > '9') {
                return -1;
            }

            state = sw_minor_digit;
            break;

        /* the minor HTTP version or the end of the request line */
        case sw_minor_digit:
            if (ch == ' ') {
                state = sw_status;
                break;
            }

            if (ch < '0' || ch > '9') {
                return -1;
            }
            break;

        /* HTTP status code */
        case sw_status:
            if (ch == ' ') {
                break;
            }

            if (ch < '0' || ch > '9') {
                return -1;
            }

            *status = *status * 10 + ch - '0';
            
            if (++status_digits == 3) {
                state = sw_space_after_status;
            }
            break;

        /* space or end of line */
        case sw_space_after_status:
            switch (ch) {
            case ' ':
                state = sw_status_text;
                break;
            case '.':                    /* IIS may send 403.1, 403.2, etc */
                state = sw_status_text;
                break;
            case CR:
                state = sw_almost_done;
                break;
            case LF:
                goto done;
            default:
                return -1;
            }
            break;

        /* any text until end of line */
        case sw_status_text:
            switch (ch) {
            case CR:
                state = sw_almost_done;

                break;
            case LF:
                goto done;
            }
            break;

        /* end of status line */
        case sw_almost_done:
            switch (ch) {
            case LF:
                goto done;
            default:
                return -1;
            }
        }
    }

    return -1;

done:

    return 0;
}

/*
 * zhaoyao:将"http\u003a//k\u002eyouku\u002ecom/"的json格式转换为
 *           "http://k.youku.com/"的ascii格式
 */
int util_json_to_ascii_string(char *buf, int len)
{
    char *p, *q, *end;
    char temp[3];

    if (buf == NULL || len <= 0) {
        hc_log_error("Invalid input");
        return -1;
    }

    end = buf + len;
    for (p = buf, q = buf; *p != '\0' && p < end; p++, q++) {
        if (*p == '\\') {
            p = p + 4;
            bzero(temp, sizeof(temp));
            temp[0] = p[0];
            temp[1] = p[1];
            *q = (char)strtol(temp, NULL, 16);
            p++;
            continue;
        }
        *q = *p;
    }
    *q = '\0';

    return 0;
}

