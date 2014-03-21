#include "common.h"
#include "sc_header.h"

int main(int argc, char *argv[])
{
    char url[BUFFER_LEN];

    if (argc != 2) {
        fprintf(stderr, "Invalid input\n");
        return -1;
    }

    if (strlen(argv[1]) >= BUFFER_LEN) {
        fprintf(stderr, "Invalid URL, too long\n");
        return -1;
    }

    bzero(url, BUFFER_LEN);
    strcpy(url, argv[1]);

    if (sc_url_is_yk(url)) {
        return sc_get_yk_video(url);
    } else {
        fprintf(stderr, "Unknown URL\n");
        return -1;
    }
}

