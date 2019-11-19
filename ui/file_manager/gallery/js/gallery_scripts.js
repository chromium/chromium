// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The include directives are put into Javascript-style comments to prevent
// parsing errors in non-flattened mode. The flattener still sees them.
// Note that this makes the flattener to comment out the first line of the
// included file but that's all right since any javascript file should start
// with a copyright comment anyway.

// <include src="../../file_manager/common/js/metrics_base.js">
// <include src="gallery_metrics.js">
// <include src="../../file_manager/foreground/js/metrics_start.js">

// <include src="../../file_manager/common/js/lru_cache.js">
// <include src="../../image_loader/load_image_request.js">
// <include src="../../image_loader/image_loader_client.js">

// <include src="../../../webui/resources/js/cr.js">
// <include src="../../../webui/resources/js/util.js">
// <include src="../../../webui/resources/js/event_tracker.js">
// <include src="../../../webui/resources/js/load_time_data.js">
// <include src="../../../webui/resources/js/i18n_template_no_process.js">

// <include src="../../../webui/resources/js/cr/ui.js">
// <include src="../../../webui/resources/js/cr/event_target.js">
// <include src="../../../webui/resources/js/cr/ui/array_data_model.js">
// <include src="../../../webui/resources/js/cr/ui/dialogs.js">
// <include src="../../../webui/resources/js/cr/ui/list_item.js">
// <include src="../../../webui/resources/js/cr/ui/list_selection_model.js">
// <include src="../../../webui/resources/js/cr/ui/list_single_selection_model.js">
// <include src="../../../webui/resources/js/cr/ui/list_selection_controller.js">
// <include src="../../../webui/resources/js/cr/ui/list.js">
// <include src="../../../webui/resources/js/cr/ui/grid.js">

(function() {
// 'strict mode' is invoked for this scope.

// Base classes.
// <include src="../../file_manager/foreground/js/metadata/metadata_cache_set.js">
// <include src="../../file_manager/foreground/js/metadata/metadata_provider.js">
// <include src="../../file_manager/foreground/js/metadata/metadata_request.js">

// <include src="../../file_manager/common/js/async_util.js">
// <include src="../../file_manager/common/js/file_type.js">
// <include src="../../base/js/app_util.js">

/* TODO(tapted): Remove the util.js dependency */
// <include src="../../file_manager/common/js/util.js">

// <include src="../../base/js/volume_manager_types.js">
// <include
// src="../../file_manager/foreground/js/metadata/content_metadata_provider.js">
// <include src="../../file_manager/foreground/js/metadata/exif_constants.js">
// <include
// src="../../file_manager/foreground/js/metadata/external_metadata_provider.js">
// <include
// src="../../file_manager/foreground/js/metadata/file_system_metadata_provider.js">
// <include
// src="../../file_manager/foreground/js/metadata/metadata_cache_item.js">
// <include src="../../file_manager/foreground/js/metadata/metadata_item.js">
// <include src="../../file_manager/foreground/js/metadata/metadata_model.js">
// <include
// src="../../file_manager/foreground/js/metadata/multi_metadata_provider.js">
// <include src="../../file_manager/foreground/js/metadata/thumbnail_model.js">
// <include src="../../file_manager/foreground/js/thumbnail_loader.js">
// <include
// src="../../file_manager/foreground/js/ui/file_manager_dialog_base.js">
// <include src="../../file_manager/foreground/js/ui/files_alert_dialog.js">
// <include src="../../file_manager/foreground/js/ui/files_confirm_dialog.js">
// <include src="../../base/js/filtered_volume_manager.js">

// <include src="image_editor/image_util.js">
// <include src="image_editor/viewport.js">
// <include src="image_editor/image_buffer.js">
// <include src="image_editor/image_loader.js">
// <include src="image_editor/image_view.js">
// <include src="image_editor/commands.js">
// <include src="image_editor/image_editor_prompt.js">
// <include src="image_editor/image_editor_mode.js">
// <include src="image_editor/image_editor_toolbar.js">
// <include src="image_editor/image_editor.js">
// <include src="image_editor/image_transform.js">
// <include src="image_editor/image_adjust.js">
// <include src="image_editor/image_resize.js">
// <include src="image_editor/filter.js">
// <include src="image_editor/image_encoder.js">
// <include src="image_editor/exif_encoder.js">

// <include src="dimmable_ui_controller.js">
// <include src="entry_list_watcher.js">
// <include src="error_banner.js">
// <include src="gallery_constants.js">
// <include src="gallery_data_model.js">
// <include src="gallery_item.js">
// <include src="gallery_util.js">
// <include src="ribbon.js">
// <include src="slide_mode.js">
// <include src="thumbnail_mode.js">
// <include src="gallery.js">

// Exports
window.ImageUtil = ImageUtil;
window.metrics = metrics;
window.Gallery = Gallery;

})();
