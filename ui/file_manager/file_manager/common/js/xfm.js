// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {notifications} from './notifications_browser_proxy.js';
import {power} from './power.js';
import {storage} from './storage_adapter.js';

// namespace
export const xfm = {
  notifications,
  power,
  storage,
};
