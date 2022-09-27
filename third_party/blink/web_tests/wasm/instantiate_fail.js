// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

test(function() {
       assert_false(try_instantiate());
     },
     "CSP disallows WebAssembly")
