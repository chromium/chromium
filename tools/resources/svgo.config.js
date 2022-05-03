// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  plugins: [
    {
      name: 'preset-default',
      params: {
        overrides: {
          // Removing viewBox is not always safe, since it assumes that
          // width/height are not overridden in all usages of an SVG file. Feel
          // free to remove viewBox manually from a certain SVG if you have
          // audited all its usages.
          removeViewBox: false,

          // https://github.com/svg/svgo/issues/1672
          minifyStyles: false,
        },
      },
    },
  ],
};
