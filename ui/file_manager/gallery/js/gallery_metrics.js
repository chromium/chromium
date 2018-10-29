// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var metrics = metricsBase;

/**
 * @private
 */
metrics.convertName_ = function(name) {
  // Historically, Gallery incorrectly uses the Files App prefix. Keep doing
  // that to preserve continuity.
  return 'FileBrowser.' + name;
};
