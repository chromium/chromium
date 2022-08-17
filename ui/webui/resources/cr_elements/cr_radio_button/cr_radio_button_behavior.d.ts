// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PaperRippleBehavior} from 'chrome://resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';

export interface CrRadioButtonBehavior extends PaperRippleBehavior {
  checked: boolean;
  disabled: boolean;
  focusable: boolean;
  label: string;
  name: string;
}

declare const CrRadioButtonBehavior: object;
