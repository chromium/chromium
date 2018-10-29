// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'setup-failed-page',

  properties: {
    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'tryAgain',
    },

    /** Overridden from UiPageContainerBehavior. */
    cancelButtonTextId: {
      type: String,
      value: 'cancel',
    },

    /** Overridden from UiPageContainerBehavior. */
    backwardButtonTextId: {
      type: String,
      value: 'back',
    },

    /** Overridden from UiPageContainerBehavior. */
    headerId: {
      type: String,
      value: 'setupFailedPageHeader',
    },

    /** Overridden from UiPageContainerBehavior. */
    messageId: {
      type: String,
      value: 'setupFailedPageMessage',
    },
  },

  behaviors: [
    UiPageContainerBehavior,
  ],
});
