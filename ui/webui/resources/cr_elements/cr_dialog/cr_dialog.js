// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-dialog' is a component for showing a modal dialog. If the
 * dialog is closed via close(), a 'close' event is fired. If the dialog is
 * canceled via cancel(), a 'cancel' event is fired followed by a 'close' event.
 *
 * Additionally clients can get a reference to the internal native <dialog> via
 * calling getNative() and inspecting the |returnValue| property inside
 * the 'close' event listener to determine whether it was canceled or just
 * closed, where a truthy value means success, and a falsy value means it was
 * canceled.
 *
 * Note that <cr-dialog> wrapper itself always has 0x0 dimensions, and
 * specifying width/height on <cr-dialog> directly will have no effect on the
 * internal native <dialog>. Instead use the --cr-dialog-native mixin to specify
 * width/height (as well as other available mixins to style other parts of the
 * dialog contents).
 */
Polymer({
  is: 'cr-dialog',

  properties: {
    open: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * Alt-text for the dialog close button.
     */
    closeText: String,

    /**
     * True if the dialog should remain open on 'popstate' events. This is used
     * for navigable dialogs that have their separate navigation handling code.
     */
    ignorePopstate: {
      type: Boolean,
      value: false,
    },

    /**
     * True if the dialog should ignore 'Enter' keypresses.
     */
    ignoreEnterKey: {
      type: Boolean,
      value: false,
    },

    /**
     * True if the dialog should consume 'keydown' events. If ignoreEnterKey
     * is true, 'Enter' key won't be consumed.
     */
    consumeKeydownEvent: {
      type: Boolean,
      value: false,
    },

    /**
     * True if the dialog should not be able to be cancelled, which will prevent
     * 'Escape' key presses from closing the dialog.
     */
    noCancel: {
      type: Boolean,
      value: false,
    },

    // True if dialog should show the 'X' close button.
    showCloseButton: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'pointerdown': 'onPointerdown_',
  },

  /** @private {?IntersectionObserver} */
  intersectionObserver_: null,

  /** @private {?MutationObserver} */
  mutationObserver_: null,

  /** @private {?Function} */
  boundKeydown_: null,

  /** @override */
  ready: function() {
    // If the active history entry changes (i.e. user clicks back button),
    // all open dialogs should be cancelled.
    window.addEventListener('popstate', function() {
      if (!this.ignorePopstate && this.$.dialog.open)
        this.cancel();
    }.bind(this));

    if (!this.ignoreEnterKey)
      this.addEventListener('keypress', this.onKeypress_.bind(this));
  },

  /** @override */
  attached: function() {
    var mutationObserverCallback = function() {
      if (this.$.dialog.open) {
        this.addIntersectionObserver_();
        this.addKeydownListener_();
      } else {
        this.removeIntersectionObserver_();
        this.removeKeydownListener_();
      }
    }.bind(this);

    this.mutationObserver_ = new MutationObserver(mutationObserverCallback);

    this.mutationObserver_.observe(this.$.dialog, {
      attributes: true,
      attributeFilter: ['open'],
    });

    // In some cases dialog already has the 'open' attribute by this point.
    mutationObserverCallback();
  },

  /** @override */
  detached: function() {
    this.removeIntersectionObserver_();
    this.removeKeydownListener_();
    if (this.mutationObserver_) {
      this.mutationObserver_.disconnect();
      this.mutationObserver_ = null;
    }
  },

  /** @private */
  addIntersectionObserver_: function() {
    if (this.intersectionObserver_)
      return;

    var bodyContainer = this.$$('.body-container');

    var bottomMarker = this.$.bodyBottomMarker;
    var topMarker = this.$.bodyTopMarker;

    var callback = function(entries) {
      // In some rare cases, there could be more than one entry per observed
      // element, in which case the last entry's result stands.
      for (var i = 0; i < entries.length; i++) {
        var target = entries[i].target;
        assert(target == bottomMarker || target == topMarker);

        var classToToggle =
            target == bottomMarker ? 'bottom-scrollable' : 'top-scrollable';

        bodyContainer.classList.toggle(
            classToToggle, entries[i].intersectionRatio == 0);
      }
    };

    this.intersectionObserver_ = new IntersectionObserver(
        callback,
        /** @type {IntersectionObserverInit} */ ({
          root: bodyContainer,
          rootMargin: '1px 0px',
          threshold: 0,
        }));
    this.intersectionObserver_.observe(bottomMarker);
    this.intersectionObserver_.observe(topMarker);
  },

  /** @private */
  removeIntersectionObserver_: function() {
    if (this.intersectionObserver_) {
      this.intersectionObserver_.disconnect();
      this.intersectionObserver_ = null;
    }
  },

  /** @private */
  addKeydownListener_: function() {
    if (!this.consumeKeydownEvent)
      return;

    this.boundKeydown_ = this.boundKeydown_ || this.onKeydown_.bind(this);

    this.addEventListener('keydown', this.boundKeydown_);

    // Sometimes <body> is key event's target and in that case the event
    // will bypass cr-dialog. We should consume those events too in order to
    // behave modally. This prevents accidentally triggering keyboard commands.
    document.body.addEventListener('keydown', this.boundKeydown_);
  },

  /** @private */
  removeKeydownListener_: function() {
    if (!this.boundKeydown_)
      return;

    this.removeEventListener('keydown', this.boundKeydown_);
    document.body.removeEventListener('keydown', this.boundKeydown_);
    this.boundKeydown_ = null;
  },

  showModal: function() {
    this.$.dialog.showModal();
    assert(this.$.dialog.open);
    this.open = true;
    this.fire('cr-dialog-open');
  },

  cancel: function() {
    this.fire('cancel');
    this.$.dialog.close();
    assert(!this.$.dialog.open);
    this.open = false;
  },

  close: function() {
    this.$.dialog.close('success');
    assert(!this.$.dialog.open);
    this.open = false;
  },

  /**
   * @private
   * @param {Event} e
   */
  onCloseKeypress_: function(e) {
    // Because the dialog may have a default Enter key handler, prevent
    // keypress events from bubbling up from this element.
    e.stopPropagation();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onNativeDialogClose_: function(e) {
    // Ignore any 'close' events not fired directly by the <dialog> element.
    if (e.target !== this.getNative())
      return;

    // TODO(dpapad): This is necessary to make the code work both for Polymer 1
    // and Polymer 2. Remove once migration to Polymer 2 is completed.
    e.stopPropagation();

    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.fire('close');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onNativeDialogCancel_: function(e) {
    // Ignore any 'cancel' events not fired directly by the <dialog> element.
    if (e.target !== this.getNative())
      return;

    if (this.noCancel) {
      e.preventDefault();
      return;
    }

    // When the dialog is dismissed using the 'Esc' key, need to manually update
    // the |open| property (since close() is not called).
    this.open = false;

    // Catch and re-fire the native 'cancel' event such that it bubbles across
    // Shadow DOM v1.
    this.fire('cancel');
  },

  /**
   * Expose the inner native <dialog> for some rare cases where it needs to be
   * directly accessed (for example to programmatically setheight/width, which
   * would not work on the wrapper).
   * @return {!HTMLDialogElement}
   */
  getNative: function() {
    return this.$.dialog;
  },

  /** @return {!PaperIconButtonElement} */
  getCloseButton: function() {
    return this.$.close;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKeypress_: function(e) {
    if (e.key != 'Enter')
      return;

    // Accept Enter keys from either the dialog, or a child input element.
    if (e.target != this && e.target.tagName != 'CR-INPUT')
      return;

    var actionButton =
        this.querySelector('.action-button:not([disabled]):not([hidden])');
    if (actionButton) {
      actionButton.click();
      e.preventDefault();
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKeydown_: function(e) {
    assert(this.consumeKeydownEvent);

    if (!this.getNative().open)
      return;

    if (this.ignoreEnterKey && e.key == 'Enter')
      return;

    // Stop propagation to behave modally.
    e.stopPropagation();
  },

  /** @param {!PointerEvent} e */
  onPointerdown_: function(e) {
    // Only show pulse animation if user left-clicked outside of the dialog
    // contents.
    if (e.button != 0 || e.composedPath()[0].tagName !== 'DIALOG')
      return;

    this.$.dialog.animate(
        [
          {transform: 'scale(1)', offset: 0},
          {transform: 'scale(1.02)', offset: 0.4},
          {transform: 'scale(1.02)', offset: 0.6},
          {transform: 'scale(1)', offset: 1},
        ],
        /** @type {!KeyframeEffectOptions} */ ({
          duration: 180,
          easing: 'ease-in-out',
          iterations: 1,
        }));

    // Prevent any text from being selected within the dialog when clicking in
    // the backdrop area.
    e.preventDefault();
  },
});
