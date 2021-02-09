// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @return {boolean} Whether a test module was loaded. */
export function loadTestModule() {
  const params = new URLSearchParams(window.location.search);
  const module = params.get('module');
  if (!module) {
    return false;
  }
  const script = document.createElement('script');
  script.type = 'module';
  script.src = `chrome://test/${module}`;
  document.body.appendChild(script);
  return true;
}
