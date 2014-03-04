#include <ngx_core.h>
#include <ngx_http.h>

#define DEBUG_GETFILE   0   /* zhaoyao TODO: add debug switch */
#define BUFLEN          32
#define BUFLEN_L        512

#define NGX_HTTP_GETFILE_TEMP_PATH  "getfile_temp"

typedef struct {
    ngx_http_upstream_conf_t upstream;
} ngx_http_getfile_conf_t;

typedef struct {
    ngx_http_status_t status;
} ngx_http_getfile_ctx_t;

static ngx_path_init_t  ngx_http_getfile_temp_path = {
    ngx_string(NGX_HTTP_GETFILE_TEMP_PATH), { 1, 2, 0 }
};

static char * ngx_http_getfile(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_getfile_handler(ngx_http_request_t *r);
static ngx_int_t getfile_upstream_process_header(ngx_http_request_t *r);
static void *ngx_http_getfile_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_getfile_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t getfile_get_host_ip_from_uri(ngx_http_request_t *r, char ipaddr[]);

static ngx_http_module_t ngx_http_getfile_module_ctx = {
    NULL,
    NULL,

    NULL,
    NULL,

    NULL,
    NULL,

    ngx_http_getfile_create_loc_conf,
    ngx_http_getfile_merge_loc_conf,
};

static ngx_command_t ngx_http_getfile_commands[] = {
    { ngx_string("getfile"),
      NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
      ngx_http_getfile,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command,
};

ngx_module_t ngx_http_getfile_module = {
    NGX_MODULE_V1,
    &ngx_http_getfile_module_ctx,
    ngx_http_getfile_commands,
    NGX_HTTP_MODULE,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING,
};

static char * ngx_http_getfile(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_getfile_handler;

    return NGX_CONF_OK;
}

static void *ngx_http_getfile_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_getfile_conf_t *mycf;

    mycf = (ngx_http_getfile_conf_t *)ngx_pcalloc(cf->pool, sizeof(ngx_http_getfile_conf_t));
    if (mycf == NULL) {
        return NULL;
    }

    mycf->upstream.connect_timeout = 60000;
    mycf->upstream.send_timeout = 60000;
    mycf->upstream.read_timeout = 60000;
    mycf->upstream.store_access = 0600;
    
    mycf->upstream.buffering = 1;       /* zhaoyao XXX: using temp file for buffering */
    mycf->upstream.store = 1;           /* zhaoyao XXX: store temp file at local directory */
    mycf->upstream.bufs.num = 8;
    mycf->upstream.bufs.size = ngx_pagesize;
    mycf->upstream.buffer_size = ngx_pagesize;
    mycf->upstream.busy_buffers_size = 2 * ngx_pagesize;
    mycf->upstream.temp_file_write_size = 2 * ngx_pagesize;
    mycf->upstream.max_temp_file_size = 1024 * 1024 * 1024;

    mycf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
    mycf->upstream.pass_headers = NGX_CONF_UNSET_PTR;

    return mycf;
}

static ngx_str_t  ngx_http_getfile_hide_headers[] = {
    ngx_string("Date"),
    ngx_string("Server"),
    ngx_string("X-Pad"),
    ngx_string("X-Accel-Expires"),
    ngx_string("X-Accel-Redirect"),
    ngx_string("X-Accel-Limit-Rate"),
    ngx_string("X-Accel-Buffering"),
    ngx_string("X-Accel-Charset"),
    ngx_null_string
};

static char *ngx_http_getfile_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_getfile_conf_t *prev = (ngx_http_getfile_conf_t *)parent;
    ngx_http_getfile_conf_t *conf = (ngx_http_getfile_conf_t *)child;
    ngx_hash_init_t hash;

    hash.max_size = 100;
    hash.bucket_size = 1024;
    hash.name = "getfile_headers_hash";
    if (ngx_http_upstream_hide_headers_hash(cf, &conf->upstream,
            &prev->upstream, ngx_http_getfile_hide_headers, &hash) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    if (ngx_conf_merge_path_value(cf, &conf->upstream.temp_path, NULL, &ngx_http_getfile_temp_path)
        != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

static ngx_int_t getfile_get_real_uri_from_uri(ngx_http_request_t *r, char ru[])
{
    u_char *cp;
    size_t i, len;

    for (cp = r->args.data, len = 0; *cp != '/' && len < r->args.len; cp++, len++) {
        ;
    }

    for (i = 0; len < r->args.len && i < BUFLEN_L; cp++, i++, len++) {
        ru[i] = *cp;
    }
    ru[i] = '\0';

    return 0;
}

/*
 * http://192.168.46.89/getfile?192.68.196.38/Videos/CrewEarthObservationsVideos/bangkokpacific_iss_20140130/bangkokpacific_iss_20140130.mov
 */
static ngx_int_t getfile_upstream_create_request(ngx_http_request_t *r)
{
    static ngx_str_t backendQueryLine =
            ngx_string("GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/32.0.1700.107 Safari/537.36\r\nConnection: close\r\n\r\n");
    ngx_int_t queryLineLen;
    ngx_buf_t *b;
    char host_ip[BUFLEN];
    char real_uri[BUFLEN_L];

    getfile_get_host_ip_from_uri(r, host_ip);
    getfile_get_real_uri_from_uri(r, real_uri);
    ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s real_uri %s", __func__, real_uri);

    queryLineLen = backendQueryLine.len + strlen(real_uri) - 2 + strlen(host_ip) - 2;
    b = ngx_create_temp_buf(r->pool, queryLineLen);
    if (b == NULL) {
        return NGX_ERROR;
    }
    b->last = b->pos + queryLineLen;

    ngx_snprintf(b->pos, queryLineLen, (char *)backendQueryLine.data, real_uri, host_ip);

    r->upstream->request_bufs = ngx_alloc_chain_link(r->pool);
    if (r->upstream->request_bufs == NULL) {
        return NGX_ERROR;
    }

    r->upstream->request_bufs->buf = b;
    r->upstream->request_bufs->next = NULL;

    r->upstream->request_sent = 0;
    r->upstream->header_sent = 0;

    r->header_hash = 1;

    return NGX_OK;
}

static void getfile_debug_buffer(ngx_buf_t *b)
{
#if DEBUG_GETFILE
    static u_char str[BUFLEN_L];
    int i;

    if (b == NULL) {
        return;
    }

    ngx_memzero(str, BUFLEN_L);

    for (i = 0; i < (b->last - b->pos) && i < (BUFLEN_L - 1); i++) {
        str[i] = b->pos[i];
    }
    ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s: \n"
                           "Buffer size: %p\n"
                           "Buffer data length: %p\n"
                           "%s", __func__, (b->end - b->start), (b->last - b->pos), str);
#endif
}

static ngx_int_t getfile_process_status_line(ngx_http_request_t *r)
{
    size_t len;
    ngx_int_t rc;
    ngx_http_upstream_t *u;

    ngx_http_getfile_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_getfile_module);
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    u = r->upstream;

    getfile_debug_buffer(&u->buffer);
    rc = ngx_http_parse_status_line(r, &u->buffer, &ctx->status);
    if (rc == NGX_AGAIN) {
        return rc;
    }
    if (rc == NGX_ERROR) {
        ngx_log_stderr(NGX_OK, "%s upstream sent no valid HTTP/1.0 header", __func__);

        r->http_version = NGX_HTTP_VERSION_9;
        u->state->status = NGX_HTTP_OK;

        return NGX_OK;
    }

    if (u->state) {
        u->state->status = ctx->status.code;
    }

    u->headers_in.status_n = ctx->status.code;

    len = ctx->status.end - ctx->status.start;
    u->headers_in.status_line.len = len;

    u->headers_in.status_line.data = ngx_pnalloc(r->pool, len);
    if (u->headers_in.status_line.data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(u->headers_in.status_line.data, ctx->status.start, len);
    ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s status line: %V", __func__, &u->headers_in.status_line);

    u->process_header = getfile_upstream_process_header;

    return getfile_upstream_process_header(r);
}

static void getfile_debug_upstream_headers(ngx_http_request_t *r)
{
#if DEBUG_GETFILE
    ngx_table_elt_t *h;
    h = r->upstream->headers_in.content_type;
    if (h != NULL) {
        ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s: %V[%V]", __func__, &h->key, &h->value);
    }
    h = r->upstream->headers_in.content_length;
    if (h != NULL) {
        ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s: %V[%V]", __func__, &h->key, &h->value);
    }
    h = r->upstream->headers_in.accept_ranges;
    if (h != NULL) {
        ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s: %V[%V]", __func__, &h->key, &h->value);
    }
    h = r->upstream->headers_in.connection;
    if (h != NULL) {
        ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s: %V[%V]", __func__, &h->key, &h->value);
    }
    h = r->upstream->headers_in.location;
    if (h != NULL) {
        ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s: %V[%V]", __func__, &h->key, &h->value);
    }
    h = r->upstream->headers_in.server;
    if (h != NULL) {
        ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s: %V[%V]", __func__, &h->key, &h->value);
    }
    h = r->upstream->headers_in.date;
    if (h != NULL) {
        ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s: %V[%V]", __func__, &h->key, &h->value);
    }
#endif
}

static ngx_int_t getfile_upstream_process_header(ngx_http_request_t *r)
{
    ngx_int_t rc;
    ngx_table_elt_t *h;
    ngx_http_upstream_header_t *hh;
    ngx_http_upstream_main_conf_t *umcf;

    umcf = ngx_http_get_module_main_conf(r, ngx_http_upstream_module);

    for ( ;; ) {
        rc = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);
        if (rc == NGX_OK) {
            h = ngx_list_push(&r->upstream->headers_in.headers);
            if (h == NULL) {
                return NGX_ERROR;
            }
            h->hash = r->header_hash;

            h->key.len = r->header_name_end - r->header_name_start;
            h->value.len = r->header_end - r->header_start;
            h->key.data = ngx_pnalloc(r->pool, h->key.len + 1 + h->value.len + 1 + h->key.len);
            if (h->key.data == NULL) {
                return NGX_ERROR;
            }

            h->value.data = h->key.data + h->key.len + 1;
            h->lowcase_key = h->key.data + h->key.len + 1 + h->value.len + 1;

            ngx_memcpy(h->key.data, r->header_name_start, h->key.len);
            h->key.data[h->key.len] = '\0';
            ngx_memcpy(h->value.data, r->header_start, h->value.len);
            h->value.data[h->value.len] = '\0';

            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
            } else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }

            hh = ngx_hash_find(&umcf->headers_in_hash, h->hash, h->lowcase_key, h->key.len);

            if (hh && hh->handler(r, h, hh->offset) != NGX_OK) {
                return NGX_ERROR;
            }

            continue;
        }

        if (rc == NGX_HTTP_PARSE_HEADER_DONE) {
            if (r->upstream->headers_in.server == NULL) {
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }

                h->hash = ngx_hash(ngx_hash(ngx_hash(ngx_hash(ngx_hash('s', 'e'), 'r'), 'v'), 'e'), 'r');

                ngx_str_set(&h->key, "Server");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char *) "server";
            }

            if (r->upstream->headers_in.date == NULL) {
                h = ngx_list_push(&r->upstream->headers_in.headers);
                if (h == NULL) {
                    return NGX_ERROR;
                }

                h->hash = ngx_hash(ngx_hash(ngx_hash('d', 'a'), 't'), 'e');

                ngx_str_set(&h->key, "Date");
                ngx_str_null(&h->value);
                h->lowcase_key = (u_char *) "date";
            }

            getfile_debug_upstream_headers(r);

            return NGX_OK;
        }

        if (rc == NGX_AGAIN) {
            return NGX_AGAIN;
        }

        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "upstream sent invalid header");

        return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }
}

static void getfile_upstream_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
#if DEBUG_GETFILE
    ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s:\n"
                           "\targs: %V",
                           __func__, &r->args);
#endif
}

static ngx_int_t getfile_get_host_ip_from_uri(ngx_http_request_t *r, char ipaddr[])
{
    u_char *cp;
    size_t i;

    for (cp = r->args.data, i = 0; *cp != '/' && i < BUFLEN; cp++, i++) {
        ipaddr[i] = *cp;
    }
    ipaddr[i] = '\0';

    return 0;
}

static ngx_int_t ngx_http_getfile_handler(ngx_http_request_t *r)
{
    ngx_http_getfile_ctx_t *myctx;
    ngx_http_getfile_conf_t *mycf;
    ngx_http_upstream_t *u;
    static struct sockaddr_in backendSockAddr;
    char svrIP[BUFLEN];
    ngx_str_t str;

    myctx = ngx_http_get_module_ctx(r, ngx_http_getfile_module);
    if (myctx == NULL) {
        myctx = ngx_palloc(r->pool, sizeof(ngx_http_getfile_ctx_t));
        if (myctx == NULL) {
            return NGX_ERROR;
        }
        ngx_memzero(myctx, sizeof(ngx_http_getfile_ctx_t));
        
        ngx_http_set_ctx(r, myctx, ngx_http_getfile_module);
    }
#if DEBUG_GETFILE
    ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s arguments: %V", __func__, &r->args);
#endif
    if (r->args.len <= 21) {
        ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s arguments invalid, less than 21", __func__);
        return NGX_ERROR;
    }

    /* zhaoyao XXX TODO: take care only flv, mp4, mov, gif, png, jpg */
    if (strncmp((char *)(r->args.data + r->args.len - 3), "flv", 3) && 
        strncmp((char *)(r->args.data + r->args.len - 3), "mp4", 3) &&
        strncmp((char *)(r->args.data + r->args.len - 3), "gif", 3) &&
        strncmp((char *)(r->args.data + r->args.len - 3), "png", 3) &&
        strncmp((char *)(r->args.data + r->args.len - 3), "jpg", 3) &&
        strncmp((char *)(r->args.data + r->args.len - 3), "mov", 3)) {
        str.data = r->args.data + r->args.len - 3;
        str.len = 3;
        ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s not support xxx.%V", __func__, &str);
        return NGX_ERROR;
    }

    if (ngx_http_upstream_create(r) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "ngx_http_upstream_create() failed");
        return NGX_ERROR;
    }

    mycf = (ngx_http_getfile_conf_t *)ngx_http_get_module_loc_conf(r, ngx_http_getfile_module);
    u = r->upstream;
    u->conf = &mycf->upstream;
    u->buffering = mycf->upstream.buffering;
    u->store = mycf->upstream.store;

    r->header_only = 1; /* zhaoyao XXX: do not send response body to downstream */
    r->getfile = 1; /* zhaoyao XXX TODO: stored file's name is from uri->args */
    
    u->pipe = ngx_pcalloc(r->pool, sizeof(ngx_event_pipe_t));
    if (u->pipe == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    u->pipe->input_filter = ngx_event_pipe_copy_input_filter;
    u->pipe->input_ctx = r;

    u->resolved = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_resolved_t));
    if (u->resolved == NULL) {
        ngx_log_stderr(NGX_OK, "%s ngx_pcalloc resolved error: %s.", __func__, strerror(errno));
        return NGX_ERROR;
    }
    
    getfile_get_host_ip_from_uri(r, svrIP);
    ngx_log_stderr(NGX_OK, "*ZHAOYAO* %s: svrIP is %s", __func__, svrIP);
    backendSockAddr.sin_family = AF_INET;
    backendSockAddr.sin_port = htons((in_port_t) 80);
    backendSockAddr.sin_addr.s_addr = inet_addr(svrIP);

    u->resolved->sockaddr = (struct sockaddr *)&backendSockAddr;
    u->resolved->socklen = sizeof(struct sockaddr_in);
    u->resolved->naddrs = 1;

    u->create_request = getfile_upstream_create_request;
    u->process_header = getfile_process_status_line;
    u->finalize_request = getfile_upstream_finalize_request;

    r->main->count++;
    ngx_http_upstream_init(r);

    return NGX_DONE;
}
