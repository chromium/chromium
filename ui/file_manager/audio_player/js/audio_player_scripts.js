// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="../../../webui/resources/js/cr.js">
// <include src="../../../webui/resources/js/load_time_data.js">
// <include src="../../../webui/resources/js/cr/event_target.js">
// <include src="../../../webui/resources/js/cr/ui/array_data_model.js">

// Hack for polymer, notifying that CSP is enabled here.
// TODO(yoshiki): Find a way to remove the hack.
if (!('securityPolicy' in document)) {
  document['securityPolicy'] = {};
}
if (!('allowsEval' in document.securityPolicy)) {
  document.securityPolicy['allowsEval'] = false;
}

(function() {

// 'strict mode' is invoked for this scope.
'use strict';

// Base classes.
// <include src="../../file_manager/foreground/js/metadata/metadata_cache_set.js">
// <include src="../../file_manager/foreground/js/metadata/metadata_provider.js">

// <include src="../../file_manager/common/js/async_util.js">
// <include src="../../file_manager/common/js/file_type.js">
// <include src="../../file_manager/common/js/util.js">
// <include src="../../base/js/mediasession_types.js">
// <include src="../../base/js/app_util.js">
// <include src="../../base/js/volume_manager_types.js">
// <include src="../../base/js/filtered_volume_manager.js">

// <include src="../../file_manager/foreground/js/metadata/content_metadata_provider.js">
// <include src="../../file_manager/foreground/js/metadata/external_metadata_provider.js">
// <include src="../../file_manager/foreground/js/metadata/file_system_metadata_provider.js">
// <include src="../../file_manager/foreground/js/metadata/metadata_cache_item.js">
// <include src="../../file_manager/foreground/js/metadata/metadata_item.js">
// <include src="../../file_manager/foreground/js/metadata/metadata_model.js">
// <include src="../../file_manager/foreground/js/metadata/metadata_request.js">
// <include src="../../file_manager/foreground/js/metadata/multi_metadata_provider.js">
// <include src="../../file_manager/foreground/js/metadata/thumbnail_model.js">

// <include src="audio_player.js">

window.reload = reload;
window.unload = unload;
window.AudioPlayer = AudioPlayer;

})();
