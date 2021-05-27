// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface CrRadioButtonBehaviorInterface {
  checked: boolean;
  disabled: boolean;
  focusable: boolean;
  label: string;
  name: string;
}

export {CrRadioButtonBehavior};

interface CrRadioButtonBehavior extends CrRadioButtonBehaviorInterface {}

declare const CrRadioButtonBehavior: object;
