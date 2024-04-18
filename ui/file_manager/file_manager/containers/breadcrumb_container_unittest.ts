// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

import {type CurrentDirectory, PropStatus} from '../state/state.js';
import {getEmptyState, getStore} from '../state/store.js';

import {BreadcrumbContainer} from './breadcrumb_container.js';


/** An instance of BreadcrumbContainer.  */
let breadcrumbContainer: BreadcrumbContainer|null = null;

export function setUp() {
  document.body.innerHTML =
      getTrustedHTML`<div id="breadcrumb-container"></div>`;
  breadcrumbContainer =
      new BreadcrumbContainer(document.querySelector('#breadcrumb-container')!);
}

/**
 * Tests that when current directory has "/" in the path, it will be escaped
 * before passing to the underlying breadcrumb.
 */
export function testPathWithSlash(done: () => void) {
  const store = getStore();
  store.init(getEmptyState());
  const currentDirectory: CurrentDirectory = {
    key: 'filesystem:chrome://file-manager/external/aaa/bbb',
    status: PropStatus.SUCCESS,
    rootType: undefined,
    pathComponents: [
      {name: 'Nexus/Pixel(MTP)', label: 'Nexus/Pixel(MTP)', key: ''},
      {name: 'DCIM', label: 'DCIM', key: ''},
    ],
    content: {
      keys: [],
      status: PropStatus.SUCCESS,
    },
    selection: {
      keys: [],
      dirCount: 0,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: 0,
      fileTasks: {
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        tasks: [],
        status: PropStatus.SUCCESS,
      },
    },
    hasDlpDisabledFiles: false,
  };

  // Simulate a state change from the store.
  breadcrumbContainer!.onStateChanged({...store.getState(), currentDirectory});

  // Check that breadcrumb's path has been escaped.
  const breadcrumb = document.querySelector('xf-breadcrumb');
  assertNotEquals(null, breadcrumb);
  assertEquals(
      breadcrumb!.path,
      'Nexus%2FPixel(MTP)/DCIM',
  );

  done();
}
