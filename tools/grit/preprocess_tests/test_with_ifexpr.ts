// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function clickHandler() {
  // <if expr="orange">
  console.log('I should be excluded from TS');
  console.log('I also should be excluded');
  // </if>
  // <if expr="foo">
  console.log('I should be included in TS');
  // </if>
}
