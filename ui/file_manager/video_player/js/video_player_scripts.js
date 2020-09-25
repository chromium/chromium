// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The include directives are put into Javascript-style comments to prevent
// parsing errors in non-flattened mode. The flattener still sees them.
// Note that this makes the flattener to comment out the first line of the
// included file but that's all right since any javascript file should start
// with a copyright comment anyway.

// <include src="error_util.js">

// <include src="../../file_manager/common/js/metrics_base.js">
// <include src="video_player_metrics.js">

// <include src="../../../webui/resources/js/cr.js">
// <include src="../../../webui/resources/js/util.js">
// <include src="../../../webui/resources/js/load_time_data.js">

// <include src="../../../webui/resources/js/event_tracker.js">

// <include src="../../../webui/resources/js/cr/ui.js">
// <include src="../../../webui/resources/js/cr/event_target.js">

// <include src="../../../webui/resources/js/cr/ui/array_data_model.js">
(function() {
'use strict';

// <include src="../../../webui/resources/js/load_time_data.js">
// <include src="../../../webui/resources/js/i18n_template_no_process.js">

// <include src="../../file_manager/common/js/async_util.js">
// <include src="../../file_manager/common/js/file_type.js">
// <include src="../../base/js/app_util.js">

/* TODO(tapted): Remove the util.js dependency */
// <include src="../../file_manager/common/js/util.js">

// <include src="../../base/js/volume_manager_types.js">
// <include src="../../base/js/filtered_volume_manager.js">

// <include src="video_player_native_controls.js">
// <include src="video_player.js">

window.unload = unload;

})();
