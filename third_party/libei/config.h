/*
 * Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef THIRD_PARTY_LIBEI_CONFIG_H_
#define THIRD_PARTY_LIBEI_CONFIG_H_

#define _GNU_SOURCE 1

#define EI_VERSION "1.4.0"
#define EIS_VERSION "1.4.0"

#define HAVE_MEMFD_CREATE 1

/* Prefix internal symbols to avoid collisions with libinput */
#define list_init ei_list_init
#define list_insert ei_list_insert
#define list_append ei_list_append
#define list_remove ei_list_remove
#define list_empty ei_list_empty
#define log_msg ei_util_log_msg
#define log_msg_va ei_util_log_msg_va
#define strv_from_string ei_strv_from_string
#define strv_join ei_strv_join

/* Other potential conflicts */
#define xsnprintf ei_xsnprintf
#define zalloc ei_zalloc
#define strendswith ei_strendswith

/* These are not needed for the core library */
#define HAVE_LIBEVDEV 0
#define HAVE_LIBXKBCOMMON 0

#endif  /* THIRD_PARTY_LIBEI_CONFIG_H_ */
