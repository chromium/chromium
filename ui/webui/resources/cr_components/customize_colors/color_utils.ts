// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

export interface Color {
  background: SkColor;
  foreground: SkColor;
  base?: SkColor;
}

export const LIGHT_DEFAULT_COLOR: Color = {
  background: {value: 0xffffffff},
  foreground: {value: 0xffdee1e6},
  base: {value: 0},
};

export const LIGHT_BASELINE_GREY_COLOR: Color = {
  background: {value: 0xff0b57d0},
  foreground: {value: 0xffe3e3e3},
  base: {value: 0xffc7c7c7},
};
