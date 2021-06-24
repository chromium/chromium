// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './shared_vars_css.m.js';

const $_documentContainer = document.createElement('template');
$_documentContainer.innerHTML = `{__html_template__}`;
document.head.appendChild($_documentContainer.content);
