#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_LEN  512
#define MAX_SLEEP   32     /* TCP connect maximum retry time (sec) */

#define LF     ((unsigned char) 10)
#define CR     ((unsigned char) 13)


static char default_hostip[] = "192.168.46.89";

static unsigned char request_pattern[] = "GET /getfile?%s HTTP/1.1\r\n"
                                "Host: %s\r\n"
                                "Connection: close\r\n\r\n";

static int gf_conn_retry(int sockfd, const struct sockaddr *addr, socklen_t alen)
{
    int nsec;

    for (nsec = 1; nsec <= MAX_SLEEP; nsec <<= 1) {
        if (connect(sockfd, addr, alen) == 0) {
            /* Connect success */
            return 0;
        }
        perror("Connect failed");
        if (nsec <= MAX_SLEEP/2) {
            sleep(nsec);
        }
    }

    /* Connect failed */
    return -1;
}

static int gf_build_request(const char *ip, const unsigned char *uri, unsigned char *buf)
{
    if (buf == NULL) {
        return -1;
    }
    
    /* zhaoyao TODO: checking request line ,header and length */

    sprintf((char *)buf, (char *)request_pattern, uri, ip);

    return 0;
}

static void gf_usage(char *cmd)
{
    printf("%s [hostip] URI\n\n", cmd);
    printf("NOTE: Default host IP address is %s\n", default_hostip);
    printf("      URI should like this - 222.73.245.205/youku/6A6.flv\n");
}

static int gf_parse_status_line(unsigned char *buf, int *status)
{
    unsigned char ch;
    unsigned char *p;
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

    if (buf == NULL || status == NULL) {
        return -1;
    }

    state = sw_start;
    *status = 0;

    for (p = buf; p < buf + BUFFER_LEN; p++) {
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


int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in sa;
    socklen_t salen;
    char *hostip;
    unsigned char *uri;
    unsigned char buffer[BUFFER_LEN];
    int nsend, nrecv;
    int err = 0, status;

    if (argc == 2) {
        hostip = default_hostip;
        uri = (unsigned char *)argv[1];
    } else if (argc == 3) {
        hostip = argv[1];
        uri = (unsigned char *)argv[2];
    } else {
        gf_usage(argv[0]);
        exit(-1);
    }

    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)80);
    sa.sin_addr.s_addr = inet_addr(hostip);
    salen = sizeof(struct sockaddr_in);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        exit(-1);
    }

    if (gf_conn_retry(sockfd, (struct sockaddr *)&sa, salen) < 0) {
        err = -1;
        goto out;
    }

    memset(buffer, 0, BUFFER_LEN);
    if (gf_build_request(hostip, uri, buffer) < 0) {
        fprintf(stderr, "gf_build_request failed\n");
        err = -1;
        goto out;
    }

    nsend = send(sockfd, buffer, BUFFER_LEN, 0);
    if (nsend != BUFFER_LEN) {
        perror("Send failed");
        err = -1;
        goto out;
    }

    memset(buffer, 0, BUFFER_LEN);
    nrecv = recv(sockfd, buffer, BUFFER_LEN, MSG_WAITALL);
    if (nrecv <= 0) {
        perror("Recv failed or meet EOF");
        err = -1;
        goto out;
    }

    if (gf_parse_status_line(buffer, &status) < 0) {
        fprintf(stderr, "Parse status line failed:\n%s", buffer);
        err = -1;
        goto out;
    }

    if (status == 200) {
        fprintf(stdout, "Getfile success!!! Response status code %d\n", status);
    } else {
        fprintf(stderr, "Response status code %d:%s", status, buffer);
    }

out:
    close(sockfd);

    return err;
}

