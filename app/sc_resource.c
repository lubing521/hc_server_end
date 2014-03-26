#include "common.h"
#include "sc_header.h"
#include "sc_resource.h"

volatile unsigned long sc_res_info_session_id_curr = 0;
sc_res_list_t *sc_res_info_list = NULL;
int sc_res_share_mem_shmid = -1;

sc_res_list_t *sc_res_list_alloc_and_init()
{
    sc_res_list_t *rl;
    int mem_size, i;
    void *shmptr;
    int shmid;

    mem_size = SC_RES_SHARE_MEM_SIZE;
    if ((shmid = shmget(SC_RES_SHARE_MEM_ID, mem_size, SC_RES_SHARE_MEM_MODE | IPC_CREAT)) < 0) {
        fprintf(stderr, "%s shmget failed, memory size %d: %s", __func__, mem_size, strerror(errno));
        return NULL;
    }
    sc_res_share_mem_shmid = shmid;

    if ((shmptr = shmat(shmid, 0, 0)) == (void *)-1) {
        fprintf(stderr, "%s shmat failed: %s", __func__, strerror(errno));
        sc_res_list_destroy_and_uninit();
        return NULL;
    }
    memset(shmptr, 0, mem_size);

    rl = (sc_res_list_t *)shmptr;
    rl->total = 0x1 << SC_RES_NUM_MAX_SHIFT;
    rl->res[0].id = 0;
    for (i = 1; i < rl->total; i++) {
        rl->res[i].id = (unsigned long)(&(rl->res[i - 1]));
    }
    rl->free = (&(rl->res[i - 1]));

    return rl;
}

int sc_res_list_destroy_and_uninit()
{
    if (shmctl(sc_res_share_mem_shmid, IPC_RMID, NULL) < 0) {
        fprintf(stderr, "%s shmctl failed: %s", __func__, strerror(errno));
        return -1;
    }

    sc_res_share_mem_shmid = -1;
    sc_res_info_list = NULL;

    return 0;
}

static sc_res_info_t *sc_res_info_get(sc_res_list_t *rl)
{
    sc_res_info_t *ri;

    if (rl == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return NULL;
    }

    if (rl->free == NULL) {
        fprintf(stderr, "%s ERROR: %d resource info totally ran out\n", __func__, rl->total);
        return NULL;
    }

    ri = rl->free;
    rl->free = (sc_res_info_t *)(ri->id);

    memset(ri, 0, sizeof(sc_res_info_t));
    ri->id = sc_res_info_session_id_curr++;    /* Mark a global context id */

    return ri;
}

static void sc_res_info_put(sc_res_list_t *rl, sc_res_info_t *ri)
{
    if (rl == NULL || ri == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return;
    }

    memset(ri, 0, sizeof(sc_res_info_t));
    ri->id = (unsigned long)(rl->free);
    rl->free = ri;
}

int sc_res_info_add_normal(sc_res_list_t *rl, const char *url, sc_res_info_t **normal)
{
    int len;
    sc_res_info_t *ri;

    if (rl == NULL || url == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return -1;
    }
    len = strlen(url);
    if (len >= SC_RES_URL_MAX_LEN) {
        fprintf(stderr, "%s ERROR: url is longer than MAX_LEN %d\n", __func__, SC_RES_URL_MAX_LEN);
        return -1;
    }

    ri = sc_res_info_get(rl);
    if (ri == NULL) {
        fprintf(stderr, "%s ERROR: get free res_info failed\n", __func__);
        return -1;
    }

    sc_res_set_normal(ri);
    strcpy(ri->url, url);

    if (normal != NULL) {
        *normal = ri;
    }

    return 0;
}

void sc_res_info_del_normal(sc_res_list_t *rl, sc_res_info_t *ri)
{
    if (rl == NULL || ri == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return;
    }

    if (!sc_res_is_normal(ri)) {
        fprintf(stderr, "%s ERROR: can not delete 0x%lx flag res_info\n", __func__, ri->flag);
        return;
    }

    if (sc_res_is_stored(ri)) {
        fprintf(stderr, "%s WARNING: \n%s\n\tstored local file is not deleted\n", __func__, ri->url);
    }

    sc_res_info_put(rl, ri);
}

int sc_res_info_add_origin(sc_res_list_t *rl, const char *url, sc_res_info_t **origin)
{
    int len;
    sc_res_info_t *ri;

    if (rl == NULL || url == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return -1;
    }
    len = strlen(url);
    if (len >= SC_RES_URL_MAX_LEN) {
        fprintf(stderr, "%s ERROR: url is longer than MAX_LEN %d\n", __func__, SC_RES_URL_MAX_LEN);
        return -1;
    }

    ri = sc_res_info_get(rl);
    if (ri == NULL) {
        fprintf(stderr, "%s ERROR: get free res_info failed\n", __func__);
        return -1;
    }

    sc_res_set_origin(ri);
    strcpy(ri->url, url);

    if (origin != NULL) {
        *origin = ri;
    }

    return 0;
}

void sc_res_info_del_origin(sc_res_list_t *rl, sc_res_info_t *ri)
{
    if (rl == NULL || ri == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return;
    }

    if (!sc_res_is_origin(ri)) {
        fprintf(stderr, "%s ERROR: can not delete 0x%lx flag resource_info\n", __func__, ri->flag);
        return;
    }

    if (sc_res_is_stored(ri)) {
        fprintf(stderr, "%s WARNING: \n%s\n\tstored local file is not deleted\n", __func__, ri->url);
    }

    /* zhaoyao XXX TODO FIXME: huge parsed URL stuff need to be done before put it */

    sc_res_info_put(rl, ri);
}

int sc_res_info_add_parsed(sc_res_list_t *rl,
                           sc_res_info_t *origin_ri,
                           const char *url,
                           sc_res_info_t **parsed)
{
    int len;
    sc_res_info_t *ri;

    if (rl == NULL || origin_ri == NULL || url == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return -1;
    }
    len = strlen(url);
    if (len >= SC_RES_URL_MAX_LEN) {
        fprintf(stderr, "%s ERROR: url is longer than MAX_LEN %d\n", __func__, SC_RES_URL_MAX_LEN);
        return -1;
    }

    ri = sc_res_info_get(rl);
    if (ri == NULL) {
        fprintf(stderr, "%s ERROR: get free res_info failed\n", __func__);
        return -1;
    }

    strcpy(ri->url, url);

    if (origin_ri->cnt == 0) {  /* First derivative is come */
        origin_ri->parsed = ri;
        ri->parent = origin_ri;
    } else {
        ri->siblings = origin_ri->parsed;
        origin_ri->parsed = ri;
        ri->parent = origin_ri;
    }
    origin_ri->cnt++;

    if (parsed != NULL) {
        *parsed = ri;
    }

    return 0;
}

void sc_res_info_del_parsed(sc_res_list_t *rl,
                            sc_res_info_t *origin_ri,
                            sc_res_info_t *ri)
{
    if (rl == NULL || origin_ri == NULL || ri == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return;
    }

    if (sc_res_is_origin(ri) || sc_res_is_normal(ri)) {
        fprintf(stderr, "%s ERROR: can not delete 0x%lx flag res_info\n", __func__, ri->flag);
        return;
    }

    if (sc_res_is_stored(ri)) {
        fprintf(stderr, "%s WARNING: \n%s\n\tstored local file is not deleted\n", __func__, ri->url);
    }

    /* zhaoyao XXX TODO FIXME: huge origin URL stuff need to be done before put it */

    sc_res_info_put(rl, ri);
}

sc_res_info_t *sc_res_info_find(sc_res_list_t *rl, const char *url)
{
    sc_res_info_t *curr;
    int i;

    if (rl == NULL || url == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return NULL;
    }

    for (i = 0; i < rl->total; i++) {
        curr = &rl->res[i];
        if (strcmp(url, curr->url) == 0) {
            return curr;
        }
    }

    return NULL;
}

int sc_res_list_process_func(sc_res_list_t *rl)
{
    sc_res_info_t *curr;
    int i, err = 0, ret;

    if (rl == NULL) {
        fprintf(stderr, "%s ERROR: invalid input\n", __func__);
        return -1;
    }

    for (i = 0; i < rl->total; i++) {
        curr = &rl->res[i];

        if (curr->url[0] == '\0') {
            continue;
        }

        if (sc_res_is_origin(curr)) {
            continue;
        }

#if 0
        if (!sc_res_is_stored(curr)) {
            /* zhaoyao XXX TODO: timeout, and re-download */
        }
#endif

        if (sc_res_is_stored(curr) && !sc_res_is_notify(curr)) {
            fprintf(stderr, "%s use sc_snooping_do_add %s\n", __func__, curr->url);
            ret = sc_snooping_do_add(curr);
            if (ret != 0) {
                fprintf(stderr, "%s inform Snooping Module add URL failed\n", __func__);
                err++;
            }
            continue;
        }

        if (!sc_res_is_stored(curr) && sc_res_is_notify(curr)) {
            fprintf(stderr, "%s INTERNAL ERROR, not stored but notified...\n", __func__);
            err++;
            continue;
        }
    }

    return err;
}

void *sc_res_list_process_thread(void *arg)
{
    int ret;

    while (1) {
        ret = sc_res_list_process_func(sc_res_info_list);
        if (ret < 0) {
            fprintf(stderr, "%s exit...\n", __func__);
            break;
        } else if (ret > 0) {
            fprintf(stderr, "%s problem occured...\n", __func__);
        }

        sleep(3);
    }

    return ((void *)0);
}
