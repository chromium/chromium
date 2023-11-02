// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-foo',

  _template: html`
    
      <button on-click="onClick_">I should be included in HTML</button>
    
    /*grit-removed-lines:2*/
  `,

  onClick_() {
    // /*grit-removed-lines:3*/
    // 
    console.log('I should be included in JS');
    // 
  }
});
