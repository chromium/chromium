// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof Polymer === 'undefined') {
  Polymer = {
    dom: 'shadow',
    lazyRegister: true,
    legacyOptimizations: true,
    preserveStyleIncludes: true,  // Only matters when using polymer-css-build.
    suppressBindingNotifications: true,
    suppressTemplateNotifications: true,
    useNativeCSSProperties: true,
  };
} else {
  console.error('Polymer is already defined.');
}
