// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-foo',

  _template: html`
    <if expr="bar">
      <button on-click="onClick_">I should be included in HTML</button>
    </if>
    <if expr="apple">
      <div>I should be excluded from HTML</div>
    </if>
  `,

  onClick_() {
    // <if expr="orange">
    console.log('I should be excluded from JS');
    console.log('I also should be excluded');
    // </if>
    // <if expr="foo">
    console.log('I should be included in JS');
    // </if>
  }
});
