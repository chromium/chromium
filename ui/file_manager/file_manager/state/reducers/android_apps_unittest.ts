// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {addAndroidApps as addAndroidAppsAction} from '../actions/android_apps.js';
import {getEmptyState} from '../store.js';

import {addAndroidApps} from './android_apps.js';

/** Tests that android apps can be added correctly to the store. */
export function testAddAndroidApps() {
  const currentState = getEmptyState();
  const androidApps: chrome.fileManagerPrivate.AndroidApp[] = [
    {
      name: 'App 1',
      packageName: 'com.test.app1',
      activityName: 'Activity1',
      iconSet: {icon16x16Url: 'url1', icon32x32Url: 'url2'},
    },
    {
      name: 'App 2',
      packageName: 'com.test.app2',
      activityName: 'Activity2',
      iconSet: {icon16x16Url: 'url3', icon32x32Url: 'url4'},
    },
  ];
  const newState =
      addAndroidApps(currentState, addAndroidAppsAction({apps: androidApps}));
  const keys = Object.keys(newState.androidApps);
  assertEquals(2, keys.length);
  assertEquals('com.test.app1', keys[0]);
  assertEquals('com.test.app2', keys[1]);
  assertEquals('App 1', newState.androidApps[keys[0]!].name);
  assertEquals('App 2', newState.androidApps[keys[1]!].name);
  assertEquals('Activity1', newState.androidApps[keys[0]!].activityName);
  assertEquals('url1', newState.androidApps[keys[0]!].iconSet.icon16x16Url);
  assertEquals('url4', newState.androidApps[keys[1]!].iconSet.icon32x32Url);
}
