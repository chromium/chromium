// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

import {SearchFileType, SearchLocation, SearchRecency} from '../externs/ts/state.js';

import {OptionKind, SEARCH_OPTIONS_CHANGED, XfSearchOptionsElement} from './xf_search_options.js';

/**
 * Creates new <xf-search-options> element for each test.
 */
export function setUp() {
  document.body.innerHTML = '<xf-search-options></xf-search-options>';
}

/**
 * Returns the <xf-search-options> element.
 */
function getSearchOptionsElement(): XfSearchOptionsElement {
  const element = document.querySelector('xf-search-options');
  return element!;
}

export async function testChangeLocation(done: () => void) {
  const element = getSearchOptionsElement();
  const locationSelector = element.getLocationSelector();
  locationSelector.options = [
    {value: SearchLocation.EVERYWHERE, text: 'Everywhere'},
    {value: SearchLocation.THIS_FOLDER, text: 'This folder', default: true},
  ];

  element.addEventListener(SEARCH_OPTIONS_CHANGED, (event) => {
    const want = {kind: OptionKind.LOCATION, value: SearchLocation.EVERYWHERE};
    assertDeepEquals(want, event.detail);
    done();
  });
  locationSelector.value = SearchLocation.EVERYWHERE;
}

export async function testChangeRecency(done: () => void) {
  const element = getSearchOptionsElement();
  const recencySelector = element.getRecencySelector();
  recencySelector.options = [
    {value: SearchRecency.ANYTIME, text: 'Any time'},
    {value: SearchRecency.YESTERDAY, text: 'Yesterday'},
  ];

  element.addEventListener(SEARCH_OPTIONS_CHANGED, (event) => {
    const want = {kind: OptionKind.RECENCY, value: SearchRecency.YESTERDAY};
    assertDeepEquals(want, event.detail);
    done();
  });
  recencySelector.value = SearchRecency.YESTERDAY;
}

export async function testChangeFileType(done: () => void) {
  const element = getSearchOptionsElement();
  const fileTypeSelector = element.getFileTypeSelector();
  fileTypeSelector.options = [
    {value: SearchFileType.ALL_TYPES, text: 'All types'},
    {value: SearchFileType.IMAGES, text: 'Images'},
  ];

  element.addEventListener(SEARCH_OPTIONS_CHANGED, (event) => {
    const want = {kind: OptionKind.FILE_TYPE, value: SearchFileType.IMAGES};
    assertDeepEquals(want, event.detail);
    done();
  });
  fileTypeSelector.value = SearchFileType.IMAGES;
}
