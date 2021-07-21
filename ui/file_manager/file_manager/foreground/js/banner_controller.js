// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

/**
 * The central component to the Banners Framework. The controller maintains the
 * core logic that dictates which banner should be shown as well as what events
 * require a reconciliation of the banners to ensure the right banner is shown
 * at the right time.
 */
export class BannerController extends EventTarget {
  constructor() {
    super();

    this.warningBanners_ = [];
    this.educationalBanners_ = [];
  }
}
