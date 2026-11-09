// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './foo.js';

import {style} from '//resources/foo_style.css.js';

import sheet from './foo.css' with {type : 'css'};

console.info(style);
document.getElementById('foo').shadowRoot.adoptedStyleSheets = [sheet];
