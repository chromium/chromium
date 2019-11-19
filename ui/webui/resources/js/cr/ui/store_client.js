// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {assertNotReached} from '../../assert.m.js';
// #import {Action, DeferredAction} from './store.m.js';

cr.define('cr.ui', function() {
  /**
   * StoreClient is a Polymer behavior which ties front-end elements to
   * back-end data from an associated Store. An element using this behavior
   * can use the watch method to tie one of its properties to a specific piece
   * of data from the Store.
   *
   * This StoreClient is generic, and needs to be combined with a
   * page-specific implementation, as in
   * chrome/browser/resources/bookmarks/store_client.js.
   * This implementation should override the watch, getState and getStore
   * methods.
   *
   * These methods need to be overridden to allow type-checking on them,
   * since they all use the page state type associated with the specific page,
   * and templates cannot be applied to Polymer behaviors. Polymer 2 Mixins
   * may solve these type-checking problems.
   *
   * @polymerBehavior
   */
  /* #export */ const StoreClient = {
    created: function() {
      /**
       * @type {!Array<{
       *   localProperty: string,
       *   valueGetter: function(!Object)
       * }>}
       */
      this.watches_ = [];
    },

    attached: function() {
      this.getStore().addObserver(this);
    },

    detached: function() {
      this.getStore().removeObserver(this);
    },

    /**
     * Watches a particular part of the state tree, updating |localProperty|
     * to the return value of |valueGetter| whenever the state changes. Eg, to
     * keep |this.item| updated with the value of a node:
     *   watch('item', (state) => state.nodes[this.itemId]);
     *
     * Note that object identity is used to determine if the value has changed
     * before updating the UI, rather than Polymer-style deep equality. If the
     * getter function returns |undefined|, no changes will propagate to the UI.
     *
     * @param {string} localProperty
     * @param {function(!Object)} valueGetter
     */
    watch_: function(localProperty, valueGetter) {
      this.watches_.push({
        localProperty: localProperty,
        valueGetter: valueGetter,
      });
    },

    /**
     * Helper to dispatch an action to the store, which will update the store
     * data and then (possibly) flow through to the UI.
     * @param {?cr.ui.Action} action
     */
    dispatch: function(action) {
      this.getStore().dispatch(action);
    },

    /**
     * Helper to dispatch a DeferredAction to the store, which will
     * asynchronously perform updates to the store data and UI.
     * @param {cr.ui.DeferredAction} action
     */
    dispatchAsync: function(action) {
      this.getStore().dispatchAsync(action);
    },

    /** @param {Object} newState */
    onStateChanged: function(newState) {
      this.watches_.forEach((watch) => {
        const oldValue = this[watch.localProperty];
        const newValue = watch.valueGetter(newState);

        // Avoid poking Polymer unless something has actually changed. Reducers
        // must return new objects rather than mutating existing objects, so
        // any real changes will pass through correctly.
        if (oldValue === newValue || newValue === undefined) {
          return;
        }

        this[watch.localProperty] = newValue;
      });
    },

    updateFromStore: function() {
      if (this.getStore().isInitialized()) {
        this.onStateChanged(this.getStore().data);
      }
    },

    /**
     * Should be overridden by a function which calls the private watch_
     * with the given arguments. Needs to be overridden to allow
     * type-checking on the valueGetter parameter.
     */
    watch: function(localProperty, valueGetter) {
      assertNotReached();
    },

    /**
     * Should be overridden by a function which returns the data from the
     * associated Store. Needs to be overridden to allow type-checking on the
     * return value, which will be a page state type specific to each page.
     */
    getState: function() {
      assertNotReached();
    },

    /**
     * Should be overridden by a function which returns the specific Store
     * instance associated with the StoreClient. Needs to be overridden to
     * allow type-checking on the return value, which will be a
     * page-specific subclass of Store.
     */
    getStore: function() {
      assertNotReached();
    },
  };

  // #cr_define_end
  return {
    StoreClient: StoreClient,
  };
});
