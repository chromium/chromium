// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';

import {XfPathDisplayElement} from './xf_path_display.js';


/**
 * Creates new <xf-search-options> element for each test.
 */
export function setUp() {
  document.body.innerHTML =
      '<div style="width: 20ex;"><xf-path-display></xf-path-display></div>';
}

function getFolderText(element: XfPathDisplayElement): string {
  const divList = element.shadowRoot!.querySelectorAll<HTMLDivElement>(
      'div.folder,div.separator')!;
  const text: string[] = [];
  for (let i = 0; i < divList.length; ++i) {
    const divNode = divList[i];
    if (divNode) {
      text.push(divNode.innerText);
    }
  }
  return text.join('');
}

/**
 * Returns the <xf-path-display> element.
 */
function getPathDisplayElement(): XfPathDisplayElement {
  return document.querySelector<XfPathDisplayElement>('xf-path-display')!;
}

export function testElementCreated() {
  const element = getPathDisplayElement();
  assertEquals('XF-PATH-DISPLAY', element.tagName);
}

export async function testPathDisplay(done: () => void) {
  const element = getPathDisplayElement();
  const pathList = [
    'My files/a.txt',
    'My files/a/b/c.txt',
    'My files/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z.txt',
  ];
  for (const path of pathList) {
    element.path = path;
    await waitForElementUpdate(element);
    assertEquals(path.replaceAll('/', '>'), getFolderText(element));
  }
  done();
}
