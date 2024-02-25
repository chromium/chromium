// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// When the returned promised resolves, BFCache will not be used for the current
// page. The function should be async even if it doesn't seem necessary, so that
// if we need to change how it blocks in the future to something async, we will
// not need to update all callers.
async function preventBFCache() {
  await new Promise(resolve => {
    // Use a random UUID as the (highly likely) unique lock name.
    navigator.locks.request(Math.random(), () => {
      resolve();
    });
  });
}
