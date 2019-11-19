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
      value: function() {
        return [];
      },
    },

    /**
     * The list of custom items to display after the list of networks.
     * @type {!Array<!NetworkList.CustomItemState>}
     */
    customItems: {
      type: Array,
      value: function() {
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
     * Contains |networks| + |customItems|.
     * @private {!Array<!NetworkList.NetworkListItemType>}
     */
    listItems_: {
      type: Array,
      value: function() {
        return [];
      },
    },
  },

  behaviors: [CrScrollableBehavior],

  observers: ['updateListItems_(networks, customItems)'],

  /** @private {boolean} */
  focusRequested_: false,

  focus: function() {
    this.focusRequested_ = true;
    this.focusFirstItem_();
  },

  /** @private */
  updateListItems_: function() {
    this.saveScroll(this.$.networkList);
    const beforeNetworks = this.customItems.filter(function(item) {
      return item.showBeforeNetworksList == true;
    });
    const afterNetworks = this.customItems.filter(function(item) {
      return item.showBeforeNetworksList == false;
    });
    this.listItems_ = beforeNetworks.concat(this.networks, afterNetworks);
    this.restoreScroll(this.$.networkList);
    this.updateScrollableContents();
    if (this.focusRequested_) {
      this.async(function() {
        this.focusFirstItem_();
      });
    }
  },

  /** @private */
  focusFirstItem_: function() {
    // Select the first network-list-item if there is one.
    const item = this.$$('network-list-item');
    if (!item) {
      return;
    }
    item.focus();
    this.focusRequested_ = false;
  },

  /**
   * Use iron-list selection (which is not the same as focus) to trigger
   * tap (requires selection-enabled) or keyboard selection.
   * @private
   */
  selectedItemChanged_: function() {
    if (this.selectedItem) {
      this.onItemAction_(this.selectedItem);
    }
  },

  /**
   * @param {!NetworkList.NetworkListItemType} item
   * @private
   */
  onItemAction_: function(item) {
    if (item.hasOwnProperty('customItemName')) {
      this.fire('custom-item-selected', item);
    } else {
      this.fire('selected', item);
      this.focusRequested_ = true;
    }
  },
});
