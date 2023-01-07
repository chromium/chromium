// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createWasmModule() {
    // the file incrementer.wasm is copied from
    // //v8/test/mjsunit/wasm. This is because currently we cannot
    // reference files outside the LayoutTests folder. When wasm format
    // changes require that file to be updated, there is a test on the
    // v8 side (same folder), ensure-wasm-binaries-up-to-date.js, which
    // fails and will require incrementer.wasm to be updated on that side.
    return fetch('incrementer.wasm')
        .then(response => {
            if (!response.ok) throw new Error(response.statusText);
            return response.arrayBuffer();
        })
        .then(WebAssembly.compile);
}
