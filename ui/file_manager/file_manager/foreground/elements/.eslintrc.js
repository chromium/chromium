// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

module.exports = {
  'rules' : {
    // Turn off since there are many imports of 'Polymer'. Remove if/when
    // everything under this folder is migrated to PolymerElement.
    'no-restricted-imports': 'off',
  },
};
