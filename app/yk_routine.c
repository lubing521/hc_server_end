/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * yk_routine.c
 * Original Author: zhaoyao@ruijie.com.cn, 2014-03-11
 *
 * Parse Youku video protocol data routines.
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#include <ctype.h>

#include "header.h"
#include "yk_lib.h"

static yk_stream_info_t *yk_create_stream_info(const char *type)
{
    yk_stream_info_t *p;

    p = malloc(sizeof(yk_stream_info_t));
    if (p == NULL) {
        fprintf(stderr, "%s memory allocate %d bytes failed\n", __func__, sizeof(yk_stream_info_t));
    } else {
        memset(p, 0, sizeof(yk_stream_info_t));
        memcpy(p->type, type, STREAM_TYPE_LEN - 1);
    }

    return p;
}

#define VALUE_LEN 16
static yk_segment_info_t *yk_create_segment_info(char *info)
{
    yk_segment_info_t *p;
    char *cur;
    char val[VALUE_LEN];
    int i;

    p = malloc(sizeof(yk_segment_info_t));
    if (p == NULL) {
        fprintf(stderr, "%s memory allocate %d bytes failed\n", __func__, sizeof(yk_segment_info_t));
        return NULL;
    }
    
    memset(p, 0, sizeof(yk_segment_info_t));
    cur = info;
    
    cur = strstr(cur, "no");
    memset(val, 0, VALUE_LEN);
    for (cur = cur + 5, i = 0; *cur != '"' && i < VALUE_LEN; cur++, i++) {
        val[i] = (*cur);
    }
    p->no = atoi(val);

    cur = strstr(cur, "size");
    memset(val, 0, VALUE_LEN);
    for (cur = cur + 7, i = 0; *cur != '"' && i < VALUE_LEN; cur++, i++) {
        val[i] = (*cur);
    }
    p->size = atoi(val);

    /*
     * zhaoyao XXX ATTENTION: format may like "seconds":"196" or "seconds":411
     */
    cur = strstr(cur, "seconds");
    memset(val, 0, VALUE_LEN);
    cur = cur + 9;
    if (*cur == '"') {
        cur++;
    }
    for (i = 0; *cur != '"' && i < VALUE_LEN; cur++, i++) {
        val[i] = (*cur);
    }
    p->seconds = atoi(val);

    cur = strchr(cur, 'k');
    for (cur = cur + 4, i = 0; *cur != '"' && i < SEGMENT_K_LEN; cur++, i++) {
        p->k[i] = (*cur);
    }

    cur = strstr(cur, "k2");
    for (cur = cur + 5, i = 0; *cur != '"' && i < SEGMENT_K2_LEN; cur++, i++) {
        p->k2[i] = (*cur);
    }

    return p;
}
#undef VALUE_LEN

static yk_stream_info_t *yk_find_stream_info(yk_stream_info_t *streams[], const char *type)
{
    int i;

    if (streams == NULL || type == NULL) {
        return NULL;
    }

    for (i = 0; i < STREAM_TYPE_TOTAL; i++) {
        if (streams[i] == NULL) {
            continue;
        }
        if (strcmp(streams[i]->type, type) == 0) {
            return streams[i];
        }
    }

    return NULL;
}

static char *yk_parse_pl_seed(const char *cur, char *seed)
{
    char *ret;

    ret = strstr(cur, "seed");
    if (ret == NULL) {
        return ret;
    }

    for (ret = ret + 6; *ret != ',' && isdigit(*ret); ret++, seed++) {
        *seed = *ret;
    }
    *seed = '\0';

    return ret;
}

void yk_destroy_streams_all(yk_stream_info_t *streams[])
{
    int i, j;

    if (streams == NULL) {
        return;
    }

    for (i = 0; i < STREAM_TYPE_TOTAL; i++) {
        if (streams[i] != NULL) {
            if (streams[i]->segs != NULL) {
                for (j = 0; j < STREAM_SEGS_MAX; j++) {
                    if (streams[i]->segs[j] != NULL) {
                        free(streams[i]->segs[j]);
                    }
                }
            }
            free(streams[i]);
        }
    }
}

void yk_debug_streams_all(yk_stream_info_t *streams[])
{
    int i, j;
    yk_stream_info_t *p;
    yk_segment_info_t *q;

    if (streams == NULL) {
        return;
    }

    for (i = 0; streams[i] != NULL && i < STREAM_TYPE_TOTAL; i++) {
        p = streams[i];
        printf("Stream type: %s\n", p->type);
        printf("       streamfileids: %s\n", p->streamfileids);
        printf("       seed: %d\n", p->seed);
        printf("       streamsizes: %d\n", p->streamsizes);
        if (p->segs != NULL) {
            for (j = 0; p->segs[j] != NULL && j < STREAM_SEGS_MAX; j++) {
                q = p->segs[j];
                printf("       segs [%2d]:\n", q->no);
                printf("            size: %d\n", q->size);
                printf("            seconds: %d\n", q->seconds);
                printf("            k: %s\n", q->k);
                printf("            k2: %s\n", q->k2);
            }
        }
        printf("------------------------------------End\n\n");
    }
}

static char *yk_parse_pl_streamfileids(const char *cur, yk_stream_info_t *streams[])
{
    char *ret, *v;
    int i = 0;
    yk_stream_info_t *p;

    ret = strstr(cur, "streamfileids");
    if (ret == NULL) {
        return ret;
    }

    ret = ret + strlen("streamfileids\":{\"");
    while (1) {
        if (memcmp(ret, "hd2", 3) == 0) {
            p = yk_create_stream_info("hd2");
            if (p == NULL) {
                goto error;
            }
            for (ret = ret + 6, v = p->streamfileids; *ret != '"'; ret++, v++) {
                *v = *ret;
            }
            *v = '\0';
            ret = ret + 3;
            streams[i] = p;
            i++;
        } else if (memcmp(ret, "mp4", 3) == 0) {
            p = yk_create_stream_info("mp4");
            if (p == NULL) {
                goto error;
            }
            for (ret = ret + 6, v = p->streamfileids; *ret != '"'; ret++, v++) {
                *v = *ret;
            }
            *v = '\0';
            ret = ret + 3;
            streams[i] = p;
            i++;
        } else if (memcmp(ret, "hd3", 3) == 0) {
            p = yk_create_stream_info("hd3");
            if (p == NULL) {
                goto error;
            }
            for (ret = ret + 6, v = p->streamfileids; *ret != '"'; ret++, v++) {
                *v = *ret;
            }
            *v = '\0';
            ret = ret + 3;
            streams[i] = p;
            i++;
        } else if (memcmp(ret, "flv", 3) == 0) {
            p = yk_create_stream_info("flv");
            if (p == NULL) {
                goto error;
            }
            for (ret = ret + 6, v = p->streamfileids; *ret != '"'; ret++, v++) {
                *v = *ret;
            }
            *v = '\0';
            ret = ret + 3;
            streams[i] = p;
            i++;
        } else if (memcmp(ret, "\"se", 3) == 0) {   /* End of streamfileids */
            ret = ret + 1;
            return ret;
        } else {
            goto error;
        }
    }

error:
    fprintf(stderr, "%s failed\n", __func__);
    yk_destroy_streams_all(streams);

    return NULL;
}

static char *yk_parse_pl_segs_do(char *cur, yk_stream_info_t *stream)
{
    char *ret, *end, info[SEG_INFO_LEN];
    yk_segment_info_t *seg;
    int i, seg_cnt = 0;

    ret = cur + 6;
    end = strchr(ret, ']');
    while (ret < end) {
        memset(info, 0, SEG_INFO_LEN);
        for (i = 0; *ret != '}' && i < SEG_INFO_LEN; ret++, i++) {
            info[i] = (*ret);
        }
        seg = yk_create_segment_info(info);
        if (seg == NULL) {
            return NULL;
        }
        seg->stream = stream;
        stream->segs[seg_cnt] = seg;
        seg_cnt++;
        if (seg_cnt > STREAM_SEGS_MAX) {
            return NULL;
        }
        ret = ret + 2;
    }
    ret = ret + 2;

    return ret;
}

static char *yk_parse_pl_segs(const char *cur, yk_stream_info_t *streams[])
{
    char *ret;
    yk_stream_info_t *p;

    ret = strstr(cur, "segs");
    if (ret == NULL) {
        return ret;
    }
    ret = ret + 8;
    while (1) {
        if (memcmp(ret, "hd2", 3) == 0) {
            p = yk_find_stream_info(streams, "hd2");
            if (p == NULL) {
                goto error;
            }
            ret = yk_parse_pl_segs_do(ret, p);
            if (ret == NULL) {
                goto error;
            }
        } else if (memcmp(ret, "mp4", 3) == 0) {
            p = yk_find_stream_info(streams, "mp4");
            if (p == NULL) {
                goto error;
            }
            ret = yk_parse_pl_segs_do(ret, p);
            if (ret == NULL) {
                goto error;
            }
        } else if (memcmp(ret, "hd3", 3) == 0) {
            p = yk_find_stream_info(streams, "hd3");
            if (p == NULL) {
                goto error;
            }
            ret = yk_parse_pl_segs_do(ret, p);
            if (ret == NULL) {
                goto error;
            }
        } else if (memcmp(ret, "flv", 3) == 0) {
            p = yk_find_stream_info(streams, "flv");
            if (p == NULL) {
                goto error;
            }
            ret = yk_parse_pl_segs_do(ret, p);
            if (ret == NULL) {
                goto error;
            }
        } else if (memcmp(ret, "\"st", 3) == 0) {   /* End of segs */
            ret = ret + 1;
            return ret;
        } else {
            goto error;
        }
    }

error:
    fprintf(stderr, "%s failed\n", __func__);
    yk_destroy_streams_all(streams);

    return NULL;
}

int yk_parse_playlist(char *data, yk_stream_info_t *streams[])
{
    int i, seed;
    char *cur = data;
    char ss[SEED_STRING_LEN];

    if (data == NULL || streams == NULL) {
        return -1;
    }
    for (i = 0; i < STREAM_TYPE_TOTAL; i++) {
        if (streams[i] != NULL) {
            return -1;
        }
    }

    /* Get seed */
    cur = yk_parse_pl_seed(cur, ss);
    seed = atoi(ss);
    if (cur == NULL) {
        return -1;
    }

    /* Get streamfileids */
    cur = yk_parse_pl_streamfileids(cur, streams);   /* zhaoyao XXX: it will allocate stream_info */
    if (cur == NULL) {
        return -1;
    }
    for (i = 0; streams[i] != NULL && i < STREAM_TYPE_TOTAL; i++) {
        streams[i]->seed = seed;                     /* zhaoyao XXX: set seed for convenience */
    }

    /* Get segs */
    cur = yk_parse_pl_segs(cur, streams);            /* zhaoyao XXX: it will allocate segment_info */
    if (cur == NULL) {
        return -1;
    }

    /* zhaoyao TODO: Get streamsizes */

    return 0;
}

int yk_parse_flvpath(char *data, char *real_url)
{
    char *tag1 = "Location: ";
    char *tag2 = "server\":\"";
    char *p, *q;

    if (data == NULL || real_url == NULL) {
        return -1;
    }

    memset(real_url, 0, BUFFER_LEN);

    p = strstr(data, tag1);
    if (p != NULL) {
        for (p = p + strlen(tag1), q = real_url; *p != '\r' && *p != '\n' && *p != '\0'; p++, q++) {
            *q = *p;
        }
        goto end;
    }

    p = strstr(data, tag2);
    if (p != NULL) {
        for (p = p + strlen(tag2), q = real_url; *p != '"' && *p != ',' && *p != '\0'; p++, q++) {
            *q = *p;
        }
        goto end;
    }

    if (p == NULL) {
        fprintf(stderr, "%s error, do not find %s or %s\n", __func__, tag1, tag2);
        return -1;
    }

end:
    if (q >= real_url + BUFFER_LEN) {
        fprintf(stderr, "WARNING: real_url is exceeding buffer length %d\n", BUFFER_LEN);
        return -1;
    }

    return 0;
}

