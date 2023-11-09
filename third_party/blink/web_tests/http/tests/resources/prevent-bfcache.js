// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// When the returned promised resolve, BFCache will not be used for the current
// page.
function preventBFCache() {
  return new Promise(resolve => {
    let webSocket = new WebSocket('ws://127.0.0.1:8880/echo');
    webSocket.onopen = () => { resolve(42); };
  });
}
