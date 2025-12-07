// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// background.js: test loading the [piexwasm] module in chrome extension.
//
window.onload = () => {
  console.info('[piexwasm] window.onload');

  const script = document.createElement('script');
  document.head.appendChild(script);

  script.onerror = (error) => {
    console.info('[piexwasm] failed loading script:', script.src);
  };

  script.onload = () => {
    console.info('[piexwasm] wrapper loaded');
    createPiexModule().then(module => {
      console.info('[piexwasm] module initialized', module);
    });
  };

  script.src = '/piex.js.wasm';
};
