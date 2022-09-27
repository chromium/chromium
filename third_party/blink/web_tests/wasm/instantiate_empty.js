// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Attempts to instantiate a simple Wasm module. Returns true if it succeeds,
// false otherwise.
function try_instantiate() {
  // The smallest possible Wasm module. Just the header (0, "A", "S", "M"), and
  // the version (0x1).
  const bytes = new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0]);

  try {
    const module = new WebAssembly.Module(bytes);
    const instance = new WebAssembly.Instance(module);
    return true;
  } catch (e) {
    console.error(e);
    return false;
  }
}
