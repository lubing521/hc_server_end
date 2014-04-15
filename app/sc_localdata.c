#include "common.h"
#include "sc_header.h"
#include "os_util.h"

static sc_res_info_origin_t *youku_ctl_ld_origin = NULL;
static sc_res_info_origin_t *sohu_ctl_ld_origin = NULL;

int sc_ld_file_process(char *fpath)
{
    int ret;
    sc_res_info_active_t *loaded;
    sc_res_info_origin_t *ctl_ld;

    if (sc_yk_is_local_path(fpath)) {
        ctl_ld = youku_ctl_ld_origin;
    } else if (sc_sohu_is_local_path(fpath)) {
        ctl_ld = sohu_ctl_ld_origin;
    } else {
        fprintf(stderr, "%s ERROR: unknown file %s\n", __func__, fpath);
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

