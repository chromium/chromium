// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (internals.runtimeFlags.webAssemblyCSPEnabled) {
  test(function() {
    assert_true(try_instantiate());
  }, 'CSP allows WebAssembly');
} else {
  test(function() {
    assert_false(try_instantiate());
  }, 'CSP disallows WebAssembly');
}
