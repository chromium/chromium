// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../../cr_elements/cr_shared_style.css.js';
const template = document.createElement('template');
template.innerHTML = `
<dom-module id="network-shared" assetpath="chrome://resources/">{__html_template__}</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));
