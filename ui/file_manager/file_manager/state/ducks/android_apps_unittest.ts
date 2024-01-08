// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ICON_TYPES} from '../../foreground/js/constants.js';
import type {State} from '../../state/state.js';
import {setupStore, waitDeepEquals} from '../for_tests.js';

import {addAndroidApps} from './android_apps.js';

/** Tests that android apps can be added correctly to the store. */
export async function testAddAndroidApps(done: () => void) {
  const app1 = {
    name: 'App 1',
    packageName: 'com.test.app1',
    activityName: 'Activity1',
    iconSet: {icon16x16Url: 'url1', icon32x32Url: 'url2'},
  };
  const app2 = {
    name: 'App 2',
    packageName: 'com.test.app2',
    activityName: 'Activity2',
    iconSet: {icon16x16Url: '', icon32x32Url: ''},
  };
  const androidApps: chrome.fileManagerPrivate.AndroidApp[] = [app1, app2];

  // Dispatch an action to add android apps.
  const store = setupStore();
  store.dispatch(addAndroidApps({apps: androidApps}));

  // Expect both android apps are existed in the store.
  const want: State['androidApps'] = {
    'com.test.app1': {
      ...app1,
      icon: app1.iconSet,
    },
    'com.test.app2': {
      ...app2,
      icon: ICON_TYPES.GENERIC,
    },
  };
  await waitDeepEquals(store, want, (state) => state.androidApps);

  done();
}
