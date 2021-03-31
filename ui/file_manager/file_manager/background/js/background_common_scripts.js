// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Scripts which are commonly used as parts of background scripts
 * in Files app, Gallery app, Video Player app, and Audio Player app.
 * The purpose of this file is to share common files between Files app and its
 * companion apps to save the size.
 * Note that adding a script in this file results in being loaded in each app.
 */

// <include src="../../common/js/async_util.js">
// <include src="../../common/js/file_type.js">
// <include src="../../common/js/metrics_base.js">
// <include src="../../common/js/files_app_entry_types.js">
// <include src="../../common/js/power.js">
// <include src="../../common/js/storage_adapter.js">
// <include src="../../common/js/xfm.js">
// <include src="../../../file_manager/common/js/app_util.js">

/* TODO(tapted): Remove this when it is specific to the files app */
// <include src="../../common/js/util.js">

// <include src="../../../file_manager/common/js/volume_manager_types.js">
// <include src="app_window_wrapper.js">
// <include src="app_windows.js">
// <include src="background_base.js">
// <include src="entry_location_impl.js">
// <include src="test_util_base.js">
// <include src="volume_info_impl.js">
// <include src="volume_info_list_impl.js">
// <include src="volume_manager_factory.js">
// <include src="volume_manager_impl.js">
// <include src="volume_manager_util.js">
