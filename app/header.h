#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_LEN  512

#define MAX_NAME_LEN 32

#define MAX_SLEEP   32     /* TCP connect maximum retry time (sec) */

#define LF     ((unsigned char) 10)
#define CR     ((unsigned char) 13)

#define true 1
#define false 0
typedef int bool;

int sock_conn_retry(int sockfd, const struct sockaddr *addr, socklen_t alen);

int http_parse_status_line(unsigned char *buf, int *status);

bool yk_url_to_playlist(char *url, char *playlist);

