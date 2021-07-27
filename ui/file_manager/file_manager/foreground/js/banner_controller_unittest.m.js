// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MockChromeStorageAPI} from '../../common/js/mock_chrome.js';

import {BannerController} from './banner_controller.js';

/** @type {!BannerController} */
let controller;

export function setUp() {
  new MockChromeStorageAPI();
  controller = new BannerController();
}

export async function testSmoke(done) {
  await controller.initialize();
  done();
}
