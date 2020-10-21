// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a collapsable list of networks.
 */

/**
 * Polymer class definition for 'network-list'.
 */
Polymer({
  is: 'network-list',

  properties: {
    /**
     * The list of network state properties for the items to display.
     * @type {!Array<!OncMojo.NetworkStateProperties>}
     */
    networks: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * The list of custom items to display after the list of networks.
     * @type {!Array<!NetworkList.CustomItemState>}
     */
    customItems: {
      type: Array,
      value() {
        return [];
      },
    },

    /** True if action buttons should be shown for the itmes. */
    showButtons: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** Whether to show technology badges on mobile network icons. */
    showTechnologyBadge: {type: Boolean, value: true},

    /**
     * Reflects the iron-list selecteditem property.
     * @type {!NetworkList.NetworkListItemType}
     */
    selectedItem: {
      type: Object,
      observer: 'selectedItemChanged_',
    },

    /** Whether cellular activation is unavailable in the current context. */
    activationUnavailable: Boolean,

    /**
     * DeviceState associated with the type of |networks| listed, or undefined
     * if none was provided.
     * @private {!OncMojo.DeviceStateProperties|undefined} deviceState
     */
    deviceState: Object,

    /**
     * Contains |networks| + |customItems|.
     * @private {!Array<!NetworkList.NetworkListItemType>}
     */
    listItems_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * Used by FocusRowBehavior to track the last focused element on a row.
     * @private
     */
    lastFocused_: Object,

    /**
     * Used by FocusRowBehavior to track if the list has been blurred.
     * @private
     */
    listBlurred_: Boolean,
  },

  behaviors: [CrScrollableBehavior, ListPropertyUpdateBehavior],

  observers: ['updateListItems_(networks, customItems)'],

  /** @type {ResizeObserver} used to observer size changes to this element */
  resizeObserver_: null,

  /** @override */
  attached() {
    // This is a required work around to get the iron-list to display on first
    // view. Currently iron-list won't generate item elements on attach if the
    // element is not visible. Because there are some instances where this
    // component might not be visible when the items are bound, we listen for
    // resize events and manually call notifyResize on the iron-list
    this.resizeObserver_ = new ResizeObserver(entries => {
      const networkList =
          /** @type {IronListElement} */ (this.$$('#networkList'));
      if (networkList) {
        networkList.notifyResize();
      }
    });
    this.resizeObserver_.observe(this);
  },

  /** @override */
  detached() {
    this.resizeObserver_.disconnect();
  },

  /** @private {boolean} */
  focusRequested_: false,

  focus() {
    this.focusRequested_ = true;
    this.focusFirstItem_();
  },

  /** @private */
  updateListItems_: function() {
    const beforeNetworks =
        this.customItems.filter(n => n.showBeforeNetworksList === true);
    const afterNetworks =
        this.customItems.filter(n => n.showBeforeNetworksList === false);
    const newList = beforeNetworks.concat(this.networks, afterNetworks);

    this.updateList('listItems_', item => item.guid, newList);

    this.updateScrollableContents();
    if (this.focusRequested_) {
      this.async(function() {
        this.focusFirstItem_();
      });
    }
  },

  /** @private */
  focusFirstItem_() {
    // Select the first network-list-item if there is one.
    const item = this.$$('network-list-item');
    if (!item) {
      return;
    }
    item.focus();
    this.focusRequested_ = false;
  },
});
