// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars_css.m.js';

const styleMod = document.createElement('dom-module');
styleMod.innerHTML = `{__html_template__}`;
styleMod.register('search-highlight-style');
