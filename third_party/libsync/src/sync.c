/*
 *  sync.c
 *
 *   Copyright 2012 Google, Inc
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <android/sync.h>

/* Prototypes for deprecated functions that used to be declared in the legacy
 * android/sync.h. They've been moved here to make sure new code does not use
 * them, but the functions are still defined to avoid breaking existing
 * binaries. Eventually they can be removed altogether.
 */
struct sync_fence_info_data {
    uint32_t len;
    char name[32];
    int32_t status;
    uint8_t pt_info[0];
};
struct sync_pt_info {
    uint32_t len;
    char obj_name[32];
    char driver_name[32];
    int32_t status;
    uint64_t timestamp_ns;
    uint8_t driver_data[0];
};
struct sync_fence_info_data* sync_fence_info(int fd);
struct sync_pt_info* sync_pt_info(struct sync_fence_info_data* info, struct sync_pt_info* itr);
void sync_fence_info_free(struct sync_fence_info_data* info);

/* Legacy Sync API */

struct sync_legacy_merge_data {
 int32_t fd2;
 char name[32];
 int32_t fence;
};

/**
 * DOC: SYNC_IOC_MERGE - merge two fences
 *
 * Takes a struct sync_merge_data.  Creates a new fence containing copies of
 * the sync_pts in both the calling fd and sync_merge_data.fd2.  Returns the
 * new fence's fd in sync_merge_data.fence
 *
 * This is the legacy version of the Sync API before the de-stage that happened
 * on Linux kernel 4.7.
 */
#define SYNC_IOC_LEGACY_MERGE   _IOWR(SYNC_IOC_MAGIC, 1, \
    struct sync_legacy_merge_data)

/**
 * DOC: SYNC_IOC_LEGACY_FENCE_INFO - get detailed information on a fence
 *
 * Takes a struct sync_fence_info_data with extra space allocated for pt_info.
 * Caller should write the size of the buffer into len.  On return, len is
 * updated to reflect the total size of the sync_fence_info_data including
 * pt_info.
 *
 * pt_info is a buffer containing sync_pt_infos for every sync_pt in the fence.
 * To iterate over the sync_pt_infos, use the sync_pt_info.len field.
 *
 * This is the legacy version of the Sync API before the de-stage that happened
 * on Linux kernel 4.7.
 */
#define SYNC_IOC_LEGACY_FENCE_INFO  _IOWR(SYNC_IOC_MAGIC, 2,\
    struct sync_fence_info_data)

/* SW Sync API */

struct sw_sync_create_fence_data {
  __u32 value;
  char name[32];
  __s32 fence;
};

#define SW_SYNC_IOC_MAGIC 'W'
#define SW_SYNC_IOC_CREATE_FENCE _IOWR(SW_SYNC_IOC_MAGIC, 0, struct sw_sync_create_fence_data)
#define SW_SYNC_IOC_INC _IOW(SW_SYNC_IOC_MAGIC, 1, __u32)

// ---------------------------------------------------------------------------
// Support for caching the sync uapi version.
//
// This library supports both legacy (android/staging) uapi and modern
// (mainline) sync uapi. Library calls first try one uapi, and if that fails,
// try the other. Since any given kernel only supports one uapi version, after
// the first successful syscall we know what the kernel supports and can skip
// trying the other.

enum uapi_version {
    UAPI_UNKNOWN,
    UAPI_MODERN,
    UAPI_LEGACY
};
static atomic_int g_uapi_version = ATOMIC_VAR_INIT(UAPI_UNKNOWN);

// ---------------------------------------------------------------------------

int sync_wait(int fd, int timeout)
{
    struct pollfd fds;
    int ret;

    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }

    fds.fd = fd;
    fds.events = POLLIN;

    do {
        ret = poll(&fds, 1, timeout);
        if (ret > 0) {
            if (fds.revents & (POLLERR | POLLNVAL)) {
                errno = EINVAL;
                return -1;
            }
            return 0;
        } else if (ret == 0) {
            errno = ETIME;
            return -1;
        }
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

    return ret;
}

static int legacy_sync_merge(const char *name, int fd1, int fd2)
{
    struct sync_legacy_merge_data data;
    int ret;

    data.fd2 = fd2;
    strlcpy(data.name, name, sizeof(data.name));
    ret = ioctl(fd1, SYNC_IOC_LEGACY_MERGE, &data);
    if (ret < 0)
        return ret;
    return data.fence;
}

static int modern_sync_merge(const char *name, int fd1, int fd2)
{
    struct sync_merge_data data;
    int ret;

    data.fd2 = fd2;
    strlcpy(data.name, name, sizeof(data.name));
    data.flags = 0;
    data.pad = 0;

    ret = ioctl(fd1, SYNC_IOC_MERGE, &data);
    if (ret < 0)
        return ret;
    return data.fence;
}

int sync_merge(const char *name, int fd1, int fd2)
{
    int uapi;
    int ret;

    uapi = atomic_load_explicit(&g_uapi_version, memory_order_acquire);

    if (uapi == UAPI_MODERN || uapi == UAPI_UNKNOWN) {
        ret = modern_sync_merge(name, fd1, fd2);
        if (ret >= 0 || errno != ENOTTY) {
            if (ret >= 0 && uapi == UAPI_UNKNOWN) {
                atomic_store_explicit(&g_uapi_version, UAPI_MODERN,
                                      memory_order_release);
            }
            return ret;
        }
    }

    ret = legacy_sync_merge(name, fd1, fd2);
    if (ret >= 0 && uapi == UAPI_UNKNOWN) {
        atomic_store_explicit(&g_uapi_version, UAPI_LEGACY,
                              memory_order_release);
    }
    return ret;
}

static struct sync_fence_info_data *legacy_sync_fence_info(int fd)
{
    struct sync_fence_info_data *legacy_info;
    struct sync_pt_info *legacy_pt_info;
    int err;

    legacy_info = malloc(4096);
    if (legacy_info == NULL)
        return NULL;

    legacy_info->len = 4096;
    err = ioctl(fd, SYNC_IOC_LEGACY_FENCE_INFO, legacy_info);
    if (err < 0) {
        free(legacy_info);
        return NULL;
    }
    return legacy_info;
}

static struct sync_file_info *modern_sync_file_info(int fd)
{
    struct sync_file_info local_info;
    struct sync_file_info *info;
    int err;

    memset(&local_info, 0, sizeof(local_info));
    err = ioctl(fd, SYNC_IOC_FILE_INFO, &local_info);
    if (err < 0)
        return NULL;

    info = calloc(1, sizeof(struct sync_file_info) +
                  local_info.num_fences * sizeof(struct sync_fence_info));
    if (!info)
        return NULL;

    info->num_fences = local_info.num_fences;
    info->sync_fence_info = (__u64)(uintptr_t)(info + 1);

    err = ioctl(fd, SYNC_IOC_FILE_INFO, info);
    if (err < 0) {
        free(info);
        return NULL;
    }

    return info;
}

static struct sync_fence_info_data *sync_file_info_to_legacy_fence_info(
    const struct sync_file_info *info)
{
    struct sync_fence_info_data *legacy_info;
    struct sync_pt_info *legacy_pt_info;
    const struct sync_fence_info *fence_info = sync_get_fence_info(info);
    const uint32_t num_fences = info->num_fences;

    legacy_info = malloc(4096);
    if (legacy_info == NULL)
        return NULL;
    legacy_info->len = sizeof(*legacy_info) +
                        num_fences * sizeof(struct sync_pt_info);
    strlcpy(legacy_info->name, info->name, sizeof(legacy_info->name));
    legacy_info->status = info->status;

    legacy_pt_info = (struct sync_pt_info *)legacy_info->pt_info;
    for (uint32_t i = 0; i < num_fences; i++) {
        legacy_pt_info[i].len = sizeof(*legacy_pt_info);
        strlcpy(legacy_pt_info[i].obj_name, fence_info[i].obj_name,
                sizeof(legacy_pt_info->obj_name));
        strlcpy(legacy_pt_info[i].driver_name, fence_info[i].driver_name,
                sizeof(legacy_pt_info->driver_name));
        legacy_pt_info[i].status = fence_info[i].status;
        legacy_pt_info[i].timestamp_ns = fence_info[i].timestamp_ns;
    }

    return legacy_info;
}

static struct sync_file_info* legacy_fence_info_to_sync_file_info(
                                    struct sync_fence_info_data *legacy_info)
{
    struct sync_file_info *info;
    struct sync_pt_info *pt;
    struct sync_fence_info *fence;
    size_t num_fences;
    int err;

    pt = NULL;
    num_fences = 0;
    while ((pt = sync_pt_info(legacy_info, pt)) != NULL)
        num_fences++;

    info = calloc(1, sizeof(struct sync_file_info) +
                     num_fences * sizeof(struct sync_fence_info));
    if (!info) {
        return NULL;
    }
    info->sync_fence_info = (__u64)(uintptr_t)(info + 1);

    strlcpy(info->name, legacy_info->name, sizeof(info->name));
    info->status = legacy_info->status;
    info->num_fences = num_fences;

    pt = NULL;
    fence = sync_get_fence_info(info);
    while ((pt = sync_pt_info(legacy_info, pt)) != NULL) {
        strlcpy(fence->obj_name, pt->obj_name, sizeof(fence->obj_name));
        strlcpy(fence->driver_name, pt->driver_name,
                sizeof(fence->driver_name));
        fence->status = pt->status;
        fence->timestamp_ns = pt->timestamp_ns;
        fence++;
    }

    return info;
}

struct sync_fence_info_data *sync_fence_info(int fd)
{
    struct sync_fence_info_data *legacy_info;
    int uapi;

    uapi = atomic_load_explicit(&g_uapi_version, memory_order_acquire);

    if (uapi == UAPI_LEGACY || uapi == UAPI_UNKNOWN) {
        legacy_info = legacy_sync_fence_info(fd);
        if (legacy_info || errno != ENOTTY) {
            if (legacy_info && uapi == UAPI_UNKNOWN) {
                atomic_store_explicit(&g_uapi_version, UAPI_LEGACY,
                                      memory_order_release);
            }
            return legacy_info;
        }
    }

    struct sync_file_info* file_info;
    file_info = modern_sync_file_info(fd);
    if (!file_info)
        return NULL;
    if (uapi == UAPI_UNKNOWN) {
        atomic_store_explicit(&g_uapi_version, UAPI_MODERN,
                              memory_order_release);
    }
    legacy_info = sync_file_info_to_legacy_fence_info(file_info);
    sync_file_info_free(file_info);
    return legacy_info;
}

struct sync_file_info* sync_file_info(int32_t fd)
{
    struct sync_file_info *info;
    int uapi;

    uapi = atomic_load_explicit(&g_uapi_version, memory_order_acquire);

    if (uapi == UAPI_MODERN || uapi == UAPI_UNKNOWN) {
        info = modern_sync_file_info(fd);
        if (info || errno != ENOTTY) {
            if (info && uapi == UAPI_UNKNOWN) {
                atomic_store_explicit(&g_uapi_version, UAPI_MODERN,
                                      memory_order_release);
            }
            return info;
        }
    }

    struct sync_fence_info_data *legacy_info;
    legacy_info = legacy_sync_fence_info(fd);
    if (!legacy_info)
        return NULL;
    if (uapi == UAPI_UNKNOWN) {
        atomic_store_explicit(&g_uapi_version, UAPI_LEGACY,
                              memory_order_release);
    }
    info = legacy_fence_info_to_sync_file_info(legacy_info);
    sync_fence_info_free(legacy_info);
    return info;
}

struct sync_pt_info *sync_pt_info(struct sync_fence_info_data *info,
                                  struct sync_pt_info *itr)
{
    if (itr == NULL)
        itr = (struct sync_pt_info *) info->pt_info;
    else
        itr = (struct sync_pt_info *) ((__u8 *)itr + itr->len);

    if ((__u8 *)itr - (__u8 *)info >= (int)info->len)
        return NULL;

    return itr;
}

void sync_fence_info_free(struct sync_fence_info_data *info)
{
    free(info);
}

void sync_file_info_free(struct sync_file_info *info)
{
    free(info);
}


int sw_sync_timeline_create(void)
{
    int ret;

    ret = open("/sys/kernel/debug/sync/sw_sync", O_RDWR);
    if (ret < 0)
        ret = open("/dev/sw_sync", O_RDWR);

    return ret;
}

int sw_sync_timeline_inc(int fd, unsigned count)
{
    __u32 arg = count;

    return ioctl(fd, SW_SYNC_IOC_INC, &arg);
}

int sw_sync_fence_create(int fd, const char *name, unsigned value)
{
    struct sw_sync_create_fence_data data;
    int err;

    data.value = value;
    strlcpy(data.name, name, sizeof(data.name));

    err = ioctl(fd, SW_SYNC_IOC_CREATE_FENCE, &data);
    if (err < 0)
        return err;

    return data.fence;
}
