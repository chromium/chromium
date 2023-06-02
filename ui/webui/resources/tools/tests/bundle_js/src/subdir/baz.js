// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://resources/foo_untrusted.js';

import {bar} from '//resources/bar_resource.js';

alert('Hello from src/subdir/baz.js');
