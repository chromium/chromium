// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A collection of names for the custom events to be emitted by the page's
// components (in vue_components/).
const CUSTOM_EVENTS = {
  DETAILS_CHECK_NODE: 'details-check-node',
  DETAILS_UNCHECK_NODE: 'details-uncheck-node',
  DISPLAY_OPTION_CHANGED: 'display-option-changed',
  DISPLAY_PRESET_SELECTED: 'display-preset-selected',
  FILTER_DELIST: 'filter-delist',
  FILTER_DELIST_UNCHECKED: 'filter-delist-unchecked',
  FILTER_CHECK_ALL: 'filter-check-all',
  FILTER_UNCHECK_ALL: 'filter-uncheck-all',
  FILTER_SUBMITTED: 'filter-submitted',
  NODE_CLICKED: 'node-clicked',
  NODE_DOUBLE_CLICKED: 'node-double-clicked',
};

export {
  CUSTOM_EVENTS,
};
