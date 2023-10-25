// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from 'chrome://resources/ash/common/util.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

import {isJellyEnabled} from '../../../common/js/flags.js';

export class BaseDialog {
  // @ts-ignore: error TS7006: Parameter 'parentNode' implicitly has an 'any'
  // type.
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
     * @protected @type {boolean}
     */
    this.hasModalContainer = false;

    /** @private{boolean} */
    this.showing_ = false;

    /** @protected @type {?Element} */
    this.container = null;

    /** @protected @type {?Element} */
    this.frame = null;

    /** @protected @type {?Element} */
    this.title = null;

    /** @protected @type {?Element} */
    this.text = null;

    /** @protected @type {?Element} */
    this.closeButton = null;

    /** @protected @type {?Element} */
    this.okButton = null;

    /** @protected @type {?Element} */
    this.cancelButton = null;

    /** @protected @type {?Element} */
    this.buttons = null;

    /** @private @type {Function|undefined} */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type 'Function
    // | undefined'.
    this.onOk_ = null;

    /** @private @type {Function|undefined} */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type 'Function
    // | undefined'.
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
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.container.className = 'cr-dialog-container';
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.container.addEventListener(
        'keydown', this.onContainerKeyDown.bind(this));
    this.shield_ = doc.createElement('div');
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.shield_.className = 'cr-dialog-shield';
    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Node'.
    this.container.appendChild(this.shield_);
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.container.addEventListener(
        'mousedown', this.onContainerMouseDown_.bind(this));

    this.frame = doc.createElement('div');
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.className = 'cr-dialog-frame';
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.frame.setAttribute('role', 'dialog');
    // Elements that have negative tabIndex can be focused but are not traversed
    // by Tab key.
    // @ts-ignore: error TS2339: Property 'tabIndex' does not exist on type
    // 'Element'.
    this.frame.tabIndex = -1;
    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Node'.
    this.container.appendChild(this.frame);

    this.title = doc.createElement('div');
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.title.className = 'cr-dialog-title';
    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Node'.
    this.frame.appendChild(this.title);

    // Use cr-button as close button for refresh23 style.
    if (isJellyEnabled()) {
      this.closeButton = doc.createElement('cr-button');
      const icon = doc.createElement('div');
      icon.className = 'icon';
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.closeButton.appendChild(icon);
    } else {
      this.closeButton = doc.createElement('div');
    }
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.closeButton.className = 'cr-dialog-close';
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.closeButton.addEventListener('click', this.onCancelClick_.bind(this));
    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Node'.
    this.frame.appendChild(this.closeButton);

    this.text = doc.createElement('div');
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.text.className = 'cr-dialog-text';
    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Node'.
    this.frame.appendChild(this.text);

    this.buttons = doc.createElement('div');
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.buttons.className = 'cr-dialog-buttons';
    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Node'.
    this.frame.appendChild(this.buttons);

    this.okButton = doc.createElement('button');
    // @ts-ignore: error TS2345: Argument of type 'number' is not assignable to
    // parameter of type 'string'.
    this.okButton.setAttribute('tabindex', 0);
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.okButton.className = 'cr-dialog-ok';
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.okButton.textContent = BaseDialog.OK_LABEL;
    // Add hover/ripple layer for button in FilesRefresh.
    if (isJellyEnabled()) {
      const hoverLayer = doc.createElement('div');
      hoverLayer.className = 'hover-layer';
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.okButton.appendChild(hoverLayer);
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.okButton.appendChild(doc.createElement('paper-ripple'));
    }
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.okButton.addEventListener('click', this.onOkClick_.bind(this));
    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Node'.
    this.buttons.appendChild(this.okButton);

    this.cancelButton = doc.createElement('button');
    // @ts-ignore: error TS2345: Argument of type 'number' is not assignable to
    // parameter of type 'string'.
    this.cancelButton.setAttribute('tabindex', 1);
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.cancelButton.className = 'cr-dialog-cancel';
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.cancelButton.textContent = BaseDialog.CANCEL_LABEL;
    // Add hover/ripple layer for button in FilesRefresh.
    if (isJellyEnabled()) {
      const hoverLayer = doc.createElement('div');
      hoverLayer.className = 'hover-layer';
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.cancelButton.appendChild(hoverLayer);
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.cancelButton.appendChild(doc.createElement('paper-ripple'));
    }
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.cancelButton.addEventListener('click', this.onCancelClick_.bind(this));
    // @ts-ignore: error TS2345: Argument of type 'Element | null' is not
    // assignable to parameter of type 'Node'.
    this.buttons.appendChild(this.cancelButton);

    this.initialFocusElement_ = this.okButton;
  }


  /** @protected */
  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  onContainerKeyDown(event) {
    // 0=cancel, 1=ok.
    // @ts-ignore: error TS7006: Parameter 'i' implicitly has an 'any' type.
    const focus = i => [this.cancelButton, this.okButton][i].focus();

    // @ts-ignore: error TS2339: Property 'disabled' does not exist on type
    // 'Element'.
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
  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  onContainerMouseDown_(event) {
    if (event.target === this.container) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      const classList = this.container.classList;
      // Start 'pulse' animation.
      classList.remove('pulse');
      setTimeout(classList.add.bind(classList, 'pulse'), 0);
      event.preventDefault();
    }
  }

  /** @private */
  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  onOkClick_(event) {
    this.hide();
    if (this.onOk_) {
      this.onOk_();
    }
  }

  /** @private */
  // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any' type.
  onCancelClick_(event) {
    this.hide();
    if (this.onCancel_) {
      this.onCancel_();
    }
  }

  /** @param {string} label */
  setOkLabel(label) {
    if (isJellyEnabled()) {
      // When Jelly is on, we have child elements inside the button, setting
      // textContent of the button will remove all children.
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      this.okButton.childNodes[0].textContent = label;
    } else {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.okButton.textContent = label;
    }
  }

  /** @param {string} label */
  setCancelLabel(label) {
    if (isJellyEnabled()) {
      // When Jelly is on, we have child elements inside the button, setting
      // textContent of the button will remove all children.
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      this.cancelButton.childNodes[0].textContent = label;
    } else {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
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
   * @param {string} messageHtml a message that may contain HTML tags.
   * @param {Function=} opt_onOk
   * @param {Function=} opt_onCancel
   * @param {Function=} opt_onShow
   */
  showHtml(title, messageHtml, opt_onOk, opt_onCancel, opt_onShow) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.text.innerHTML = sanitizeInnerHtml(messageHtml);
    this.show_(title, opt_onOk, opt_onCancel, opt_onShow);
  }

  /** @private */
  // @ts-ignore: error TS7006: Parameter 'doc' implicitly has an 'any' type.
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
    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
      // @ts-ignore: error TS2322: Type '(string | null)[]' is not assignable to
      // type 'string[]'.
      this.tabIndexes_ = this.deactivatedNodes_.map(function(n) {
        return n.getAttribute('tabindex');
      });
      this.deactivatedNodes_.forEach(function(n) {
        // @ts-ignore: error TS2339: Property 'tabIndex' does not exist on type
        // 'Element'.
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
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.title.textContent = title;
      // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
      // 'Element'.
      this.title.hidden = false;
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.frame.setAttribute('aria-label', title);
    } else {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.title.textContent = '';
      // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
      // 'Element'.
      this.title.hidden = true;
      // @ts-ignore: error TS2339: Property 'innerText' does not exist on type
      // 'Element'.
      if (this.text.innerText) {
        // @ts-ignore: error TS2339: Property 'innerText' does not exist on type
        // 'Element'.
        this.frame.setAttribute('aria-label', this.text.innerText);
      } else {
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.frame.removeAttribute('aria-label');
      }
    }

    const self = this;
    setTimeout(function() {
      // Check that hide() was not called in between.
      if (self.showing_) {
        // @ts-ignore: error TS18047: 'self.container' is possibly 'null'.
        self.container.classList.add('shown');
        // @ts-ignore: error TS2339: Property 'focus' does not exist on type
        // 'Element'.
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
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    for (let i = 0; i < this.deactivatedNodes_.length; i++) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      const node = this.deactivatedNodes_[i];
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      if (this.tabIndexes_[i] === null) {
        // @ts-ignore: error TS18048: 'node' is possibly 'undefined'.
        node.removeAttribute('tabindex');
      } else {
        // @ts-ignore: error TS2345: Argument of type 'string | undefined' is
        // not assignable to parameter of type 'string'.
        node.setAttribute('tabindex', this.tabIndexes_[i]);
      }
    }
    this.deactivatedNodes_ = null;
    this.tabIndexes_ = null;

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.container.classList.remove('shown');
    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
      // @ts-ignore: error TS18047: 'self.container' is possibly 'null'.
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
  // @ts-ignore: error TS7006: Parameter 'parentNode' implicitly has an 'any'
  // type.
  constructor(parentNode) {
    super(parentNode);
    // @ts-ignore: error TS2339: Property 'style' does not exist on type
    // 'Element'.
    this.cancelButton.style.display = 'none';
  }

  /**
   * @param {Function=} opt_onOk
   * @param {Function=} opt_onShow
   * @override
   */
  // @ts-ignore: error TS7006: Parameter 'message' implicitly has an 'any' type.
  show(message, opt_onOk, opt_onShow) {
    return super.show(message, opt_onOk, opt_onOk, opt_onShow);
  }
}

/** ConfirmDialog contains a message, an ok button, and a cancel button. */
export class ConfirmDialog extends BaseDialog {}
