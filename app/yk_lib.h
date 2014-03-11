#define SEED_STRING_LEN         8

#define STREAM_TYPE_TOTAL       4   /* flv, mp4, hd2, hd3 */
#define STREAM_TYPE_LEN         4
#define STREAM_FILE_IDS_LEN     256
#define STREAM_SEGS_MAX         32

#define SEGMENT_K_LEN           32
#define SEGMENT_K2_LEN          32

typedef struct yk_stream_info_s yk_stream_info_t;

typedef struct yk_segment_info_s {
    yk_stream_info_t *stream;       /* Where I am belonging to. */
    int no;                         /* number */
    int size;
    int seconds;
    char k[SEGMENT_K_LEN];
    char k2[SEGMENT_K2_LEN];
} yk_segment_info_t;

struct yk_stream_info_s {
    char type[STREAM_TYPE_LEN];     /* flv, mp4, hd2, hd3 */
    char streamfileids[STREAM_FILE_IDS_LEN];
    int  streamsizes;
    yk_segment_info_t *segs[STREAM_SEGS_MAX];
};

bool yk_url_to_playlist(char *url, char *playlist);

int yk_parse_playlist(char *data, yk_stream_info_t *stream[], char *seed);
void yk_destroy_streams_all(yk_stream_info_t *stream[]);
void yk_debug_streams_all(yk_stream_info_t *streams[]);

