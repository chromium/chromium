// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from 'chrome://resources/ash/common/util.js';

import {util} from '../../../common/js/util.js';

export class BaseDialog {
  constructor(parentNode) {
    this.parentNode_ = parentNode;
    this.document_ = parentNode.ownerDocument;

    // The DOM element from the dialog which should receive focus when the
    // dialog is first displayed.
    this.initialFocusElement_ = null;

    // The DOM element from the parent which had focus before we were displayed,
    // so we can restore it when we're hidden.
    this.previousActiveElement_ = null;

    /**
     * If set true, BaseDialog assumes that focus traversal of elements inside
     * the dialog due to 'Tab' key events is handled by its container (and the
     * practical example is this.parentNode_ is a modal <dialog> element).
     *
     * The default is false: BaseDialog handles focus traversal for the entire
     * DOM document. See findFocusableElements_(), also crbug.com/1078300.
     *
     * @protected {boolean}
     */
    this.hasModalContainer = false;

    /** @private{boolean} */
    this.showing_ = false;

    /** @protected {?Element} */
    this.container = null;

    /** @protected {?Element} */
    this.frame = null;

    /** @protected {?Element} */
    this.title = null;

    /** @protected {?Element} */
    this.text = null;

    /** @protected {?Element} */
    this.closeButton = null;

    /** @protected {?Element} */
    this.okButton = null;

    /** @protected {?Element} */
    this.cancelButton = null;

    /** @protected {?Element} */
    this.buttons = null;

    /** @private @type {Function|undefined} */
    this.onOk_ = null;

    /** @private @type {Function|undefined} */
    this.onCancel_ = null;

    /** @private @type {?Element} */
    this.shield_ = null;

    /** @private @type {?Array<Element>} */
    this.deactivatedNodes_ = null;

    /** @private @type {?Array<string>} */
    this.tabIndexes_ = null;

    this.initDom();
  }

  initDom() {
    const doc = this.document_;
    this.container = doc.createElement('div');
    this.container.className = 'cr-dialog-container';
    this.container.addEventListener(
        'keydown', this.onContainerKeyDown.bind(this));
    this.shield_ = doc.createElement('div');
    this.shield_.className = 'cr-dialog-shield';
    this.container.appendChild(this.shield_);
    this.container.addEventListener(
        'mousedown', this.onContainerMouseDown_.bind(this));

    this.frame = doc.createElement('div');
    this.frame.className = 'cr-dialog-frame';
    this.frame.setAttribute('role', 'dialog');
    // Elements that have negative tabIndex can be focused but are not traversed
    // by Tab key.
    this.frame.tabIndex = -1;
    this.container.appendChild(this.frame);

    this.title = doc.createElement('div');
    this.title.className = 'cr-dialog-title';
    this.frame.appendChild(this.title);

    this.closeButton = doc.createElement('div');
    this.closeButton.className = 'cr-dialog-close';
    this.closeButton.addEventListener('click', this.onCancelClick_.bind(this));
    this.frame.appendChild(this.closeButton);

    this.text = doc.createElement('div');
    this.text.className = 'cr-dialog-text';
    this.frame.appendChild(this.text);

    this.buttons = doc.createElement('div');
    this.buttons.className = 'cr-dialog-buttons';
    this.frame.appendChild(this.buttons);

    this.okButton = doc.createElement('button');
    this.okButton.setAttribute('tabindex', 0);
    this.okButton.className = 'cr-dialog-ok';
    this.okButton.textContent = BaseDialog.OK_LABEL;
    // Add hover/ripple layer for button in FilesRefresh.
    if (util.isJellyEnabled()) {
      const hoverLayer = doc.createElement('div');
      hoverLayer.className = 'hover-layer';
      this.okButton.appendChild(hoverLayer);
      this.okButton.appendChild(doc.createElement('paper-ripple'));
    }
    this.okButton.addEventListener('click', this.onOkClick_.bind(this));
    this.buttons.appendChild(this.okButton);

    this.cancelButton = doc.createElement('button');
    this.cancelButton.setAttribute('tabindex', 1);
    this.cancelButton.className = 'cr-dialog-cancel';
    this.cancelButton.textContent = BaseDialog.CANCEL_LABEL;
    // Add hover/ripple layer for button in FilesRefresh.
    if (util.isJellyEnabled()) {
      const hoverLayer = doc.createElement('div');
      hoverLayer.className = 'hover-layer';
      this.cancelButton.appendChild(hoverLayer);
      this.cancelButton.appendChild(doc.createElement('paper-ripple'));
    }
    this.cancelButton.addEventListener('click', this.onCancelClick_.bind(this));
    this.buttons.appendChild(this.cancelButton);

    this.initialFocusElement_ = this.okButton;
  }


  /** @protected */
  onContainerKeyDown(event) {
    // 0=cancel, 1=ok.
    const focus = i => [this.cancelButton, this.okButton][i].focus();

    if (event.key === 'Escape' && !this.cancelButton.disabled) {
      this.onCancelClick_(event);
    } else if (event.key === 'ArrowLeft') {
      focus(isRTL() ? 1 : 0);
    } else if (event.key === 'ArrowRight') {
      focus(isRTL() ? 0 : 1);
    } else {
      // Not handled, so return and allow event to propagate.
      return;
    }
    event.stopPropagation();
    event.preventDefault();
  }

  /** @private */
  onContainerMouseDown_(event) {
    if (event.target === this.container) {
      const classList = this.container.classList;
      // Start 'pulse' animation.
      classList.remove('pulse');
      setTimeout(classList.add.bind(classList, 'pulse'), 0);
      event.preventDefault();
    }
  }

  /** @private */
  onOkClick_(event) {
    this.hide();
    if (this.onOk_) {
      this.onOk_();
    }
  }

  /** @private */
  onCancelClick_(event) {
    this.hide();
    if (this.onCancel_) {
      this.onCancel_();
    }
  }

  /** @param {string} label */
  setOkLabel(label) {
    if (util.isJellyEnabled()) {
      // When Jelly is on, we have child elements inside the button, setting
      // textContent of the button will remove all children.
      this.okButton.childNodes[0].textContent = label;
    } else {
      this.okButton.textContent = label;
    }
  }

  /** @param {string} label */
  setCancelLabel(label) {
    if (util.isJellyEnabled()) {
      // When Jelly is on, we have child elements inside the button, setting
      // textContent of the button will remove all children.
      this.cancelButton.childNodes[0].textContent = label;
    } else {
      this.cancelButton.textContent = label;
    }
  }

  setInitialFocusOnCancel() {
    this.initialFocusElement_ = this.cancelButton;
  }

  /**
   * @param {string} message
   * @param {Function=} opt_onOk
   * @param {Function=} opt_onCancel
   * @param {Function=} opt_onShow
   */
  show(message, opt_onOk, opt_onCancel, opt_onShow) {
    this.showWithTitle('', message, opt_onOk, opt_onCancel, opt_onShow);
  }

  /**
   * @param {string} title
   * @param {string} message
   * @param {Function=} opt_onOk
   * @param {Function=} opt_onCancel
   * @param {Function=} opt_onShow
   */
  showHtml(title, message, opt_onOk, opt_onCancel, opt_onShow) {
    this.text.innerHTML = message;
    this.show_(title, opt_onOk, opt_onCancel, opt_onShow);
  }

  /** @private */
  findFocusableElements_(doc) {
    let elements =
        Array.prototype.filter.call(doc.querySelectorAll('*'), function(n) {
          return n.tabIndex >= 0;
        });

    const iframes = doc.querySelectorAll('iframe');
    for (let i = 0; i < iframes.length; i++) {
      // Some iframes have an undefined contentDocument for security reasons,
      // such as chrome://terms (which is used in the chromeos OOBE screens).
      const iframe = iframes[i];
      let contentDoc;
      try {
        contentDoc = iframe.contentDocument;
      } catch (e) {
      }  // ignore SecurityError
      if (contentDoc) {
        elements = elements.concat(this.findFocusableElements_(contentDoc));
      }
    }
    return elements;
  }

  /**
   * @param {string} title
   * @param {string} message
   * @param {Function=} opt_onOk
   * @param {Function=} opt_onCancel
   * @param {Function=} opt_onShow
   */
  showWithTitle(title, message, opt_onOk, opt_onCancel, opt_onShow) {
    this.text.textContent = message;
    this.show_(title, opt_onOk, opt_onCancel, opt_onShow);
  }

  /**
   * @param {string} title
   * @param {Function=} opt_onOk
   * @param {Function=} opt_onCancel
   * @param {Function=} opt_onShow
   * @private
   */
  show_(title, opt_onOk, opt_onCancel, opt_onShow) {
    this.showing_ = true;

    // Modal containers manage dialog focus traversal. Otherwise, the focus
    // is managed by |this| dialog, by making all outside nodes unfocusable
    // while the dialog is shown.
    if (!this.hasModalContainer) {
      this.deactivatedNodes_ = this.findFocusableElements_(this.document_);
      this.tabIndexes_ = this.deactivatedNodes_.map(function(n) {
        return n.getAttribute('tabindex');
      });
      this.deactivatedNodes_.forEach(function(n) {
        n.tabIndex = -1;
      });
    } else {
      this.deactivatedNodes_ = [];
    }

    this.previousActiveElement_ = this.document_.activeElement;
    this.parentNode_.appendChild(this.container);

    this.onOk_ = opt_onOk;
    this.onCancel_ = opt_onCancel;

    if (title) {
      this.title.textContent = title;
      this.title.hidden = false;
      this.frame.setAttribute('aria-label', title);
    } else {
      this.title.textContent = '';
      this.title.hidden = true;
      if (this.text.innerText) {
        this.frame.setAttribute('aria-label', this.text.innerText);
      } else {
        this.frame.removeAttribute('aria-label');
      }
    }

    const self = this;
    setTimeout(function() {
      // Check that hide() was not called in between.
      if (self.showing_) {
        self.container.classList.add('shown');
        self.initialFocusElement_.focus();
      }
      setTimeout(function() {
        if (opt_onShow) {
          opt_onShow();
        }
      }, ANIMATE_STABLE_DURATION);
    }, 0);
  }

  /** @param {Function=} opt_onHide */
  hide(opt_onHide) {
    this.showing_ = false;

    // Restore focusability for the non-modal container case.
    for (let i = 0; i < this.deactivatedNodes_.length; i++) {
      const node = this.deactivatedNodes_[i];
      if (this.tabIndexes_[i] === null) {
        node.removeAttribute('tabindex');
      } else {
        node.setAttribute('tabindex', this.tabIndexes_[i]);
      }
    }
    this.deactivatedNodes_ = null;
    this.tabIndexes_ = null;

    this.container.classList.remove('shown');
    this.container.classList.remove('pulse');

    if (this.previousActiveElement_) {
      this.previousActiveElement_.focus();
    } else {
      this.document_.body.focus();
    }

    const self = this;
    setTimeout(function() {
      // Wait until the transition is done before removing the dialog.
      // Check show() was not called in between.
      // It is also possible to show/hide/show/hide and have hide called twice
      // and container already removed from parentNode_.
      if (!self.showing_ && self.parentNode_ === self.container.parentNode) {
        self.parentNode_.removeChild(self.container);
      }
      if (opt_onHide) {
        opt_onHide();
      }
    }, ANIMATE_STABLE_DURATION);
  }
}

/**
 * Default text for Ok and Cancel buttons.
 *
 * Clients should override these with localized labels.
 */
BaseDialog.OK_LABEL = '[LOCALIZE ME] Ok';
BaseDialog.CANCEL_LABEL = '[LOCALIZE ME] Cancel';

/**
 * Number of milliseconds animation is expected to take, plus some margin for
 * error.
 */
const ANIMATE_STABLE_DURATION = 500;


/** AlertDialog contains just a message and an ok button. */
export class AlertDialog extends BaseDialog {
  constructor(parentNode) {
    super(parentNode);
    this.cancelButton.style.display = 'none';
  }

  /**
   * @param {Function=} opt_onOk
   * @param {Function=} opt_onShow
   * @override
   */
  show(message, opt_onOk, opt_onShow) {
    return super.show(message, opt_onOk, opt_onOk, opt_onShow);
  }
}

/** ConfirmDialog contains a message, an ok button, and a cancel button. */
export class ConfirmDialog extends BaseDialog {}
