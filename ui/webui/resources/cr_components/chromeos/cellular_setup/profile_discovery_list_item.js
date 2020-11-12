// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in the profile-discovery-list-page list displaying details of an eSIM
 * profile.
 */

Polymer({
  is: 'profile-discovery-list-item',

  behaviors: [I18nBehavior],

  properties: {
    // TODO(crbug.com/1093185) Add type annotation when the real Profile struct
    // is available.
    profile: {
      type: Object,
      value: null,
    },

    selected: {
      type: Boolean,
      reflectToAttribute: true,
    },
  },
});
