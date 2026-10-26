/*
 *  Copyright 2017 The Android Open Source Project
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

/**
 * @addtogroup Sync
 * @{
 */

/**
 * @file sync.h
 */

#ifndef ANDROID_SYNC_H
#define ANDROID_SYNC_H

#include <stdint.h>
#include <sys/cdefs.h>

#include <linux/sync_file.h>

__BEGIN_DECLS

/* Fences indicate the status of an asynchronous task. They are initially
 * in unsignaled state (0), and make a one-time transition to either signaled
 * (1) or error (< 0) state. A sync file is a collection of one or more fences;
 * the sync file's status is error if any of its fences are in error state,
 * signaled if all of the child fences are signaled, or unsignaled otherwise.
 *
 * Sync files are created by various device APIs in response to submitting
 * tasks to the device. Standard file descriptor lifetime syscalls like dup()
 * and close() are used to manage sync file lifetime.
 *
 * The poll(), ppoll(), or select() syscalls can be used to wait for the sync
 * file to change status, or (with a timeout of zero) to check its status.
 *
 * The functions below provide a few additional sync-specific operations.
 */

/**
 * Merge two sync files.
 *
 * This produces a new sync file with the given name which has the union of the
 * two original sync file's fences; redundant fences may be removed.
 *
 * If one of the input sync files is signaled or invalid, then this function
 * may behave like dup(): the new file descriptor refers to the valid/unsignaled
 * sync file with its original name, rather than a new sync file.
 *
 * The original fences remain valid, and the caller is responsible for closing
 * them.
 *
 * Available since API level 26.
 */
int32_t sync_merge(const char* name, int32_t fd1, int32_t fd2) __INTRODUCED_IN(26);

/**
 * Retrieve detailed information about a sync file and its fences.
 *
 * The returned sync_file_info must be freed by calling sync_file_info_free().
 *
 * Available since API level 26.
 */
struct sync_file_info* sync_file_info(int32_t fd) __INTRODUCED_IN(26);

/**
 * Get the array of fence infos from the sync file's info.
 *
 * The returned array is owned by the parent sync file info, and has
 * info->num_fences entries.
 *
 * Available since API level 26.
 */
static inline struct sync_fence_info* sync_get_fence_info(const struct sync_file_info* info) {
// This header should compile in C, but some C++ projects enable
// warnings-as-error for C-style casts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    return (struct sync_fence_info *)(uintptr_t)(info->sync_fence_info);
#pragma GCC diagnostic pop
}

/**
 * Free a struct sync_file_info structure
 *
 * Available since API level 26.
 */
void sync_file_info_free(struct sync_file_info* info) __INTRODUCED_IN(26);

__END_DECLS

#endif /* ANDROID_SYNC_H */

/** @} */
