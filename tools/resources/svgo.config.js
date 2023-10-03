// Copyright 2022 The Chromium Authors
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

          // This plugin strips 'unused' IDs, however they may be used in
          // another file to embed an external SVG via <use>. This setting keeps
          // all IDs that start with "EXPORT_".
          cleanupIds: {
            preservePrefixes: ["EXPORT_"],
          },
        },
      },
    },
  ],
};
