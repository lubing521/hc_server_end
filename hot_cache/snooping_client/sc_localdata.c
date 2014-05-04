/*
 * Copyright(C) 2014 Ruijie Network. All rights reserved.
 */
/*
 * sc_localdata.c
 * Original Author: zhaoyao@ruijie.com.cn, 2014-04-28
 *
 * ATTENTION:
 *     1. xxx
 *
 * History
 */

#include "common.h"
#include "sc_header.h"
#include "os_util.h"

static sc_res_info_mgmt_t *youku_controller_of_loaded = NULL;
static sc_res_info_mgmt_t *sohu_controller_of_loaded = NULL;

sc_res_info_mgmt_t *sc_ld_obtain_ctrl_ld_youku()
{
    if (youku_controller_of_loaded == NULL) {
        hc_log_error("loadeds' controller of Youku is NULL");
    }

    return youku_controller_of_loaded;
}

sc_res_info_mgmt_t *sc_ld_obtain_ctrl_ld_sohu()
{
    if (sohu_controller_of_loaded == NULL) {
        hc_log_error("loaded controller of Sohu is NULL");
    }

    return sohu_controller_of_loaded;
}

static int sc_ld_is_duplicate_file(sc_res_info_mgmt_t *ctrl_ld, char *fpath)
{
    char *vid, *p;
    int len;
    sc_res_info_ctnt_t *loaded;

    if (ctrl_ld == NULL || fpath == NULL) {
        return 0;
    }

    /* zhaoyao XXX: 目前只有local path文件才被检验是否重复 */
    if (!sc_yk_is_local_path(fpath) &&
        !sc_sohu_is_local_path(fpath) &&
        !sc_yk_is_local_path_pure_vid(fpath)) {
        return 0;
    }

    len = strlen(fpath);
    for (p = fpath + len - 1; *p != '_' && p >= fpath; p--) {
        ;
    }
    if (*p != '_') {
        return 0;
    }
    vid = p + 1;

    for (loaded = ctrl_ld->child; loaded != NULL; loaded = loaded->siblings) {
        if (strstr(loaded->common.url, vid) != NULL) {
            return 1;
        }
    }

    return 0;
}

int sc_ld_file_process(char *fpath)
{
    int ret;
    sc_res_info_ctnt_t *loaded;
    sc_res_info_mgmt_t *ctrl_ld;
    char yk_std_fpath[SC_RES_LOCAL_PATH_MAX_LEN];
    char url[HTTP_URL_MAX_LEN], *local_path;

    if (sc_yk_is_local_path(fpath)) {
        ctrl_ld = sc_ld_obtain_ctrl_ld_youku();
    } else if (sc_yk_is_local_path_pure_vid(fpath)) {
        ctrl_ld = sc_ld_obtain_ctrl_ld_youku();
        bzero(yk_std_fpath, SC_RES_LOCAL_PATH_MAX_LEN);
        ret = sc_yk_trans_vid_to_std_path(fpath, yk_std_fpath, SC_RES_LOCAL_PATH_MAX_LEN);
        if (ret != 0) {
            hc_log_error("Obtain youku standard path failed");
            /* zhaoyao XXX: 继续遍历 */
            return 0;
        }
        if (os_file_rename(fpath, yk_std_fpath) != 0) {
            hc_log_error("Rename youku vid path to standard path failed");
            /* zhaoyao XXX: 继续遍历 */
            return 0;
        }
        fpath = yk_std_fpath;
    } else if (sc_sohu_is_local_path(fpath)) {
        ctrl_ld = sc_ld_obtain_ctrl_ld_sohu();
    } else {
        hc_log_error("Unknown file %s", fpath);
        /* zhaoyao XXX: 继续遍历 */
        return 0;
    }

    if (sc_ld_is_duplicate_file(ctrl_ld, fpath)) {
        ret = os_file_remove(fpath);
        if (ret != 0) {
            hc_log_error("Remove duplicate file failed: %s", fpath);
        } else {
            hc_log_error("Remove dup file: %s", fpath);
        }
        /* zhaoyao XXX: 继续遍历 */
        return 0;
    }

    bzero(url, HTTP_URL_MAX_LEN);
    local_path = fpath;
    if (memcmp(local_path, SC_NGX_ROOT_PATH, SC_NGX_ROOT_PATH_LEN) == 0) {
        local_path = local_path + SC_NGX_ROOT_PATH_LEN;
    }
    ret = sc_res_recover_url_from_local_path(local_path, url);
    if (ret != 0) {
        hc_log_error("recover url from local path failed: %s", local_path);
        return 0;
    }

    ret = sc_res_info_add_ctnt(sc_res_info_list, ctrl_ld, url, &loaded);
    if (ret != 0) {
        hc_log_error("Add loaded failed");
        /* zhaoyao XXX: 继续遍历 */
        return 0;
    }

    return 0;
}

int sc_ld_init_and_load()
{
    int ret;

    ret = sc_res_info_add_ctrl_ld(sc_res_info_list, YOUKU_WEBSITE_URL, &youku_controller_of_loaded);
    if (ret != 0) {
        hc_log_error("Add loaded file controller of Youku failed");
        return -1;
    }

    ret = sc_res_info_add_ctrl_ld(sc_res_info_list, SOHU_WEBSITE_URL, &sohu_controller_of_loaded);
    if (ret != 0) {
        hc_log_error("Add loaded file controller of Sohu failed");
        return -1;
    }

    ret = os_dir_walk(SC_NGX_ROOT_PATH);
    if (ret != 0) {
        hc_log_error("Walk %s failed", SC_NGX_ROOT_PATH);
        return -1;
    }

    return 0;
}

