#include "common.h"
#include "sc_header.h"
#include "os_util.h"

static sc_res_info_origin_t *youku_ctl_ld_origin = NULL;
static sc_res_info_origin_t *sohu_ctl_ld_origin = NULL;

sc_res_info_origin_t *sc_ld_obtain_ctl_ld_youku()
{
    if (youku_ctl_ld_origin == NULL) {
        fprintf(stderr, "%s ERROR: loaded controller of Youku is NULL\n", __func__);
    }

    return youku_ctl_ld_origin;
}

sc_res_info_origin_t *sc_ld_obtain_ctl_ld_sohu()
{
    if (sohu_ctl_ld_origin == NULL) {
        fprintf(stderr, "%s ERROR: loaded controller of Sohu is NULL\n", __func__);
    }

    return sohu_ctl_ld_origin;
}

static int sc_ld_is_duplicate_file(sc_res_info_origin_t *ctl_ld, char *fpath)
{
    char *vid, *p;
    int len;
    sc_res_info_active_t *loaded;

    if (ctl_ld == NULL || fpath == NULL) {
        return 0;
    }

    len = strlen(fpath);
    for (p = fpath + len - 1; *p != '/' && p >= fpath; p--) {
        ;
    }
    if (*p != '/') {
        return 0;
    }
    vid = p + 1;

    for (loaded = ctl_ld->child; loaded != NULL; loaded = loaded->siblings) {
        if (strstr(loaded->common.url, vid) != NULL) {
            return 1;
        }
    }

    return 0;
}

int sc_ld_file_process(char *fpath)
{
    int ret;
    sc_res_info_active_t *loaded;
    sc_res_info_origin_t *ctl_ld;
    char yk_std_fpath[SC_RES_LOCAL_PATH_MAX_LEN];

    if (sc_yk_is_local_path(fpath)) {
        ctl_ld = sc_ld_obtain_ctl_ld_youku();
    } else if (sc_yk_is_local_path_pure_vid(fpath)) {
        ctl_ld = sc_ld_obtain_ctl_ld_youku();
        bzero(yk_std_fpath, SC_RES_LOCAL_PATH_MAX_LEN);
        ret = sc_yk_trans_vid_to_std_path(fpath, yk_std_fpath, SC_RES_LOCAL_PATH_MAX_LEN);
        if (ret != 0) {
            fprintf(stderr, "%s ERROR: get youku standard path failed\n", __func__);
            /* zhaoyao XXX: 继续遍历 */
            return 0;
        }
        if (os_file_rename(fpath, yk_std_fpath) != 0) {
            fprintf(stderr, "%s ERROR: rename youku vid path to standard path failed\n", __func__);
            /* zhaoyao XXX: 继续遍历 */
            return 0;
        }
        fpath = yk_std_fpath;
    } else if (sc_sohu_is_local_path(fpath)) {
        ctl_ld = sc_ld_obtain_ctl_ld_sohu();
    } else {
        fprintf(stderr, "%s ERROR: unknown file %s\n", __func__, fpath);
        /* zhaoyao XXX: 继续遍历 */
        return 0;
    }

    if (sc_ld_is_duplicate_file(ctl_ld, fpath)) {
        ret = os_file_remove(fpath);
        if (ret != 0) {
            fprintf(stderr, "%s ERROR: remove duplicate file failed: %s\n", __func__, fpath);
        } else {
            fprintf(stdout, "%s: remove dup file: %s\n", __func__, fpath);
        }
        /* zhaoyao XXX: 继续遍历 */
        return 0;
    }

    ret = sc_res_info_add_loaded(sc_res_info_list, ctl_ld, fpath, &loaded);
    if (ret != 0) {
        fprintf(stderr, "%s ERROR: add loaded failed %s\n", __func__, fpath);
        /* zhaoyao XXX: 继续遍历 */
        return 0;
    }

    return 0;
}

int sc_ld_init_and_load()
{
    int ret;

    ret = sc_res_info_add_ctl_ld(sc_res_info_list, YOUKU_WEBSITE_URL, &youku_ctl_ld_origin);
    if (ret != 0) {
        fprintf(stderr, "%s ERROR: add loaded file controller of Youku failed\n", __func__);
        return -1;
    }

    ret = sc_res_info_add_ctl_ld(sc_res_info_list, SOHU_WEBSITE_URL, &sohu_ctl_ld_origin);
    if (ret != 0) {
        fprintf(stderr, "%s ERROR: add loaded file controller of Sohu failed\n", __func__);
        return -1;
    }

    ret = os_dir_walk(SC_NGX_ROOT_PATH);
    if (ret != 0) {
        fprintf(stderr, "%s ERROR: walk %s failed\n", __func__, SC_NGX_ROOT_PATH);
        return -1;
    }

    return 0;
}

