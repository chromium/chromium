// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resourcces/js/custom_element.js';

import {getTemplate} from './foo_native.html.js';

export class CrFooElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }
}
