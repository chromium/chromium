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
 * internal native <dialog>. Instead use cr-dialog::part(dialog) to specify
 * width/height (as well as other available mixins to style other parts of the
 * dialog contents).
 */
Polymer({
  is: 'cr-dialog',

  behaviors: [CrContainerShadowBehavior],

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

    showOnAttach: {
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
  ready() {
    // If the active history entry changes (i.e. user clicks back button),
    // all open dialogs should be cancelled.
    window.addEventListener('popstate', function() {
      if (!this.ignorePopstate && this.$.dialog.open) {
        this.cancel();
      }
    }.bind(this));

    if (!this.ignoreEnterKey) {
      this.addEventListener('keypress', this.onKeypress_.bind(this));
    }
  },

  /** @override */
  attached() {
    const mutationObserverCallback = function() {
      if (this.$.dialog.open) {
        this.enableShadowBehavior(true);
        this.addKeydownListener_();
      } else {
        this.enableShadowBehavior(false);
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
    if (this.showOnAttach) {
      this.showModal();
    }
  },

  /** @override */
  detached() {
    this.removeKeydownListener_();
    if (this.mutationObserver_) {
      this.mutationObserver_.disconnect();
      this.mutationObserver_ = null;
    }
  },

  /** @private */
  addKeydownListener_() {
    if (!this.consumeKeydownEvent) {
      return;
    }

    this.boundKeydown_ = this.boundKeydown_ || this.onKeydown_.bind(this);

    this.addEventListener('keydown', this.boundKeydown_);

    // Sometimes <body> is key event's target and in that case the event
    // will bypass cr-dialog. We should consume those events too in order to
    // behave modally. This prevents accidentally triggering keyboard commands.
    document.body.addEventListener('keydown', this.boundKeydown_);
  },

  /** @private */
  removeKeydownListener_() {
    if (!this.boundKeydown_) {
      return;
    }

    this.removeEventListener('keydown', this.boundKeydown_);
    document.body.removeEventListener('keydown', this.boundKeydown_);
    this.boundKeydown_ = null;
  },

  showModal() {
    this.$.dialog.showModal();
    assert(this.$.dialog.open);
    this.open = true;
    this.fire('cr-dialog-open');
  },

  cancel() {
    this.fire('cancel');
    this.$.dialog.close();
    assert(!this.$.dialog.open);
    this.open = false;
  },

  close() {
    this.$.dialog.close('success');
    assert(!this.$.dialog.open);
    this.open = false;
  },

  /**
   * Set the title of the dialog for a11y reader.
   * @param {string} title Title of the dialog.
   */
  setTitleAriaLabel(title) {
    this.$.dialog.removeAttribute('aria-labelledby');
    this.$.dialog.setAttribute('aria-label', title);
  },

  /**
   * @private
   * @param {Event} e
   */
  onCloseKeypress_(e) {
    // Because the dialog may have a default Enter key handler, prevent
    // keypress events from bubbling up from this element.
    e.stopPropagation();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onNativeDialogClose_(e) {
    // Ignore any 'close' events not fired directly by the <dialog> element.
    if (e.target !== this.getNative()) {
      return;
    }

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
  onNativeDialogCancel_(e) {
    // Ignore any 'cancel' events not fired directly by the <dialog> element.
    if (e.target !== this.getNative()) {
      return;
    }

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
  getNative() {
    return /** @type {!HTMLDialogElement} */ (this.$.dialog);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKeypress_(e) {
    if (e.key !== 'Enter') {
      return;
    }

    // Accept Enter keys from either the dialog itself, or a child cr-input,
    // considering that the event may have been retargeted, for example if the
    // cr-input is nested inside another element. Also exclude inputs of type
    // 'search', since hitting 'Enter' on a search field most likely intends to
    // trigger searching.
    const accept = e.target === this ||
        e.composedPath().some(
            el => el.tagName === 'CR-INPUT' && el.type !== 'search');
    if (!accept) {
      return;
    }
    const actionButton =
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
  onKeydown_(e) {
    assert(this.consumeKeydownEvent);

    if (!this.getNative().open) {
      return;
    }

    if (this.ignoreEnterKey && e.key === 'Enter') {
      return;
    }

    // Stop propagation to behave modally.
    e.stopPropagation();
  },

  /** @param {!PointerEvent} e */
  onPointerdown_(e) {
    // Only show pulse animation if user left-clicked outside of the dialog
    // contents.
    if (e.button !== 0 || e.composedPath()[0].tagName !== 'DIALOG') {
      return;
    }

    this.$.dialog.animate(
        [
          {transform: 'scale(1)', offset: 0},
          {transform: 'scale(1.02)', offset: 0.4},
          {transform: 'scale(1.02)', offset: 0.6},
          {transform: 'scale(1)', offset: 1},
        ],
        /** @type {!KeyframeAnimationOptions} */ ({
          duration: 180,
          easing: 'ease-in-out',
          iterations: 1,
        }));

    // Prevent any text from being selected within the dialog when clicking in
    // the backdrop area.
    e.preventDefault();
  },

  focus() {
    this.$$('.title-container').focus();
  },
});
/* #ignore */ console.warn('crbug/1173575, non-JS module files deprecated.');
