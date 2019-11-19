// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// background.js: test loading the [piexwasm] module in chrome extension.
//
window.onload = () => {
  console.log('[piexwasm] window.onload');

  let script = document.createElement('script');
  document.head.appendChild(script);

  script.onerror = (error) => {
    console.log('[piexwasm] failed loading script:', script.src);
  };

  window.Module = {
    onRuntimeInitialized: () => {
      console.log('[piexwasm] runtime loaded');
    },
  };

  script.src = '/piex.js.wasm';
};
