// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-drawer',

  properties: {
    heading: String,

    /** @private */
    show_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** The alignment of the drawer on the screen ('ltr' or 'rtl'). */
    align: {
      type: String,
      value: 'ltr',
      reflectToAttribute: true,
    },

    /**
     * An iron-icon resource name, e.g. "cr20:menu". If null, no icon will
     * be shown.
     */
    iconName: {
      type: String,
      value: null,
    },

    /** Title attribute for the icon, if shown. */
    iconTitle: String,
  },

  /** @type {boolean} */
  get open() {
    return this.$.dialog.open;
  },

  set open(value) {
    assertNotReached('Cannot set |open|.');
  },

  /** Toggles the drawer open and close. */
  toggle: function() {
    if (this.open) {
      this.cancel();
    } else {
      this.openDrawer();
    }
  },

  /** Shows drawer and slides it into view. */
  openDrawer: function() {
    if (this.open) {
      return;
    }
    this.$.dialog.showModal();
    this.show_ = true;
    this.fire('cr-drawer-opening');
    listenOnce(this.$.dialog, 'transitionend', () => {
      this.fire('cr-drawer-opened');
    });
  },

  /**
   * Slides the drawer away, then closes it after the transition has ended. It
   * is up to the owner of this component to differentiate between close and
   * cancel.
   * @param {boolean} cancel
   */
  dismiss_: function(cancel) {
    if (!this.open) {
      return;
    }
    this.show_ = false;
    listenOnce(this.$.dialog, 'transitionend', () => {
      this.$.dialog.close(cancel ? 'canceled' : 'closed');
    });
  },

  cancel: function() {
    this.dismiss_(true);
  },

  close: function() {
    this.dismiss_(false);
  },

  /** @return {boolean} */
  wasCanceled: function() {
    return !this.open && this.$.dialog.returnValue == 'canceled';
  },

  /**
   * Handles a tap on the (optional) icon.
   * @param {!Event} event
   * @private
   */
  onIconTap_: function(event) {
    this.cancel();
  },

  /**
   * Stop propagation of a tap event inside the container. This will allow
   * |onDialogTap_| to only be called when clicked outside the container.
   * @param {!Event} event
   * @private
   */
  onContainerTap_: function(event) {
    event.stopPropagation();
  },

  /**
   * Close the dialog when tapped outside the container.
   * @private
   */
  onDialogTap_: function() {
    this.cancel();
  },

  /**
   * Overrides the default cancel machanism to allow for a close animation.
   * @param {!Event} event
   * @private
   */
  onDialogCancel_: function(event) {
    event.preventDefault();
    this.cancel();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onDialogClose_: function(event) {
    // TODO(dpapad): This is necessary to make the code work both for Polymer 1
    // and Polymer 2. Remove once migration to Polymer 2 is completed.
    event.stopPropagation();

    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.fire('close');
  },
});
