// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {isRTL} from 'chrome://resources/ash/common/util.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

export class BaseDialog {
  /**
   * Default text for Ok and Cancel buttons.
   *
   * Clients should override these with localized labels.
   */
  static okLabel: string = '[LOCALIZE ME] Ok';
  static cancelLabel: string = '[LOCALIZE ME] Cancel';


  // The DOM element from the dialog which should receive focus when the
  // dialog is first displayed.
  protected initialFocusElement_: HTMLElement;

  // The DOM element from the parent which had focus before we were displayed,
  // so we can restore it when we're hidden.
  private previousActiveElement_: HTMLElement|null = null;

  protected document_: Document;

  /**
   * If set true, BaseDialog assumes that focus traversal of elements inside
   * the dialog due to 'Tab' key events is handled by its container (and the
   * practical example is this.parentNode_ is a modal <dialog> element).
   *
   * The default is false: BaseDialog handles focus traversal for the entire
   * DOM document. See findFocusableElements_(), also crbug.com/1078300.
   *
   */
  protected hasModalContainer: boolean = false;

  private showing_: boolean = false;

  protected container: HTMLElement;

  protected frame: HTMLElement;

  protected title: HTMLElement;

  protected text: HTMLElement;

  protected closeButton: CrButtonElement;

  protected okButton: HTMLButtonElement;

  protected cancelButton: HTMLButtonElement;

  protected buttons: HTMLElement;

  private onOk_: VoidCallback|undefined = undefined;

  private onCancel_: VoidCallback|undefined = undefined;

  private shield_: HTMLElement;

  private deactivatedNodes_: HTMLElement[]|null = null;

  private tabIndexes_: string[]|null = null;

  constructor(protected parentNode_: HTMLElement) {
    const doc = this.parentNode_.ownerDocument;
    this.document_ = doc;
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

    // Use cr-button as close button.
    this.closeButton = doc.createElement('cr-button');
    const icon = doc.createElement('div');
    icon.className = 'icon';
    this.closeButton.appendChild(icon);
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
    this.okButton.setAttribute('tabindex', '0');
    this.okButton.className = 'cr-dialog-ok';
    this.okButton.textContent = BaseDialog.okLabel;
    // Add hover/ripple layer for button.
    const hoverLayerForOk = doc.createElement('div');
    hoverLayerForOk.className = 'hover-layer';
    this.okButton.appendChild(hoverLayerForOk);
    this.okButton.appendChild(doc.createElement('paper-ripple'));
    this.okButton.addEventListener('click', this.onOkClick_.bind(this));
    this.buttons.appendChild(this.okButton);

    this.cancelButton = doc.createElement('button');
    this.cancelButton.setAttribute('tabindex', '1');
    this.cancelButton.className = 'cr-dialog-cancel';
    this.cancelButton.textContent = BaseDialog.cancelLabel;
    // Add hover/ripple layer for button.
    const hoverLayerForCancel = doc.createElement('div');
    hoverLayerForCancel.className = 'hover-layer';
    this.cancelButton.appendChild(hoverLayerForCancel);
    this.cancelButton.appendChild(doc.createElement('paper-ripple'));
    this.cancelButton.addEventListener('click', this.onCancelClick_.bind(this));
    this.buttons.appendChild(this.cancelButton);

    this.initialFocusElement_ = this.okButton;
    this.initDom();
  }

  /**
   * Hook method for extending classes. Empty at this level.
   */
  initDom() {}


  protected onContainerKeyDown(event: KeyboardEvent) {
    // 0=cancel, 1=ok.
    const focus = (i: number) =>
        (i === 0 ? this.cancelButton : this.okButton).focus();

    if (event.key === 'Escape' && !this.cancelButton.disabled) {
      this.onCancelClick_();
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

  private onContainerMouseDown_(event: MouseEvent) {
    if (event.target === this.container) {
      const classList = this.container.classList;
      // Start 'pulse' animation.
      classList.remove('pulse');
      setTimeout(classList.add.bind(classList, 'pulse'), 0);
      event.preventDefault();
    }
  }

  private onOkClick_() {
    this.hide();
    if (this.onOk_) {
      this.onOk_();
    }
  }

  private onCancelClick_() {
    this.hide();
    if (this.onCancel_) {
      this.onCancel_();
    }
  }

  setOkLabel(label: string) {
    // We have child elements (hover/ripple) inside the button, setting
    // textContent of the button will remove all children
    this.okButton.childNodes[0]!.textContent = label;
  }

  setCancelLabel(label: string) {
    // We have child elements inside the button, setting
    // textContent of the button will remove all children.
    this.cancelButton.childNodes[0]!.textContent = label;
  }

  setInitialFocusOnCancel() {
    this.initialFocusElement_ = this.cancelButton;
  }

  show(
      message: string, onOk?: VoidCallback, onCancel?: VoidCallback,
      onShow?: VoidCallback) {
    this.showWithTitle('', message, onOk, onCancel, onShow);
  }

  showHtml(
      title: string, messageHtml: string, onOk?: VoidCallback,
      onCancel?: VoidCallback, onShow?: VoidCallback) {
    this.text.innerHTML = sanitizeInnerHtml(messageHtml);
    this.show_(title, onOk, onCancel, onShow);
  }

  private findFocusableElements_(doc: Document) {
    let elements =
        Array.prototype.filter.call(doc.querySelectorAll('*'), (n) => {
          return n.tabIndex >= 0;
        });

    const iframes = doc.querySelectorAll('iframe');
    for (let i = 0; i < iframes.length; i++) {
      // Some iframes have an undefined contentDocument for security reasons,
      // such as chrome://terms (which is used in the chromeos OOBE screens).
      const iframe = iframes[i]!;
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

  showWithTitle(
      title: string, message: string, onOk?: VoidCallback,
      onCancel?: VoidCallback, onShow?: VoidCallback) {
    this.text.textContent = message;
    this.show_(title, onOk, onCancel, onShow);
  }

  protected show_(
      title: string, onOk?: VoidCallback, onCancel?: VoidCallback,
      onShow?: VoidCallback) {
    this.showing_ = true;

    // Modal containers manage dialog focus traversal. Otherwise, the focus
    // is managed by |this| dialog, by making all outside nodes unfocusable
    // while the dialog is shown.
    if (!this.hasModalContainer) {
      this.deactivatedNodes_ = this.findFocusableElements_(this.document_);
      this.tabIndexes_ = this.deactivatedNodes_.map((n: HTMLElement) => {
        return n.getAttribute('tabindex') || '';
      });
      this.deactivatedNodes_.forEach((n: HTMLElement) => {
        n.tabIndex = -1;
      });
    } else {
      this.deactivatedNodes_ = [];
    }

    this.previousActiveElement_ = this.document_.activeElement as HTMLElement;
    this.parentNode_.appendChild(this.container);

    this.onOk_ = onOk;
    this.onCancel_ = onCancel;

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
    setTimeout(() => {
      // Check that hide() was not called in between.
      if (self.showing_) {
        self.container.classList.add('shown');
        self.initialFocusElement_.focus();
      }
      setTimeout(() => {
        if (onShow) {
          onShow();
        }
      }, ANIMATE_STABLE_DURATION);
    }, 0);
  }

  hide(onHide?: VoidCallback) {
    this.showing_ = false;

    // Restore focusability for the non-modal container case.
    if (this.deactivatedNodes_ && this.tabIndexes_) {
      for (let i = 0; i < this.deactivatedNodes_.length; i++) {
        const node = this.deactivatedNodes_[i]!;
        if (this.tabIndexes_[i] === null) {
          node.removeAttribute('tabindex');
        } else {
          node.setAttribute('tabindex', this.tabIndexes_[i]!);
        }
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
    setTimeout(() => {
      // Wait until the transition is done before removing the dialog.
      // Check show() was not called in between.
      // It is also possible to show/hide/show/hide and have hide called twice
      // and container already removed from parentNode_.
      if (!self.showing_ && self.parentNode_ === self.container.parentNode) {
        self.parentNode_.removeChild(self.container);
      }
      if (onHide) {
        onHide();
      }
    }, ANIMATE_STABLE_DURATION);
  }
}

/**
 * Number of milliseconds animation is expected to take, plus some margin for
 * error.
 */
const ANIMATE_STABLE_DURATION = 500;


/** AlertDialog contains just a message and an ok button. */
export class AlertDialog extends BaseDialog {
  constructor(parentNode: HTMLElement) {
    super(parentNode);
    this.cancelButton.style.display = 'none';
  }

  override show(message: string, onOk?: VoidCallback, onShow?: VoidCallback) {
    return super.show(message, onOk, onOk, onShow);
  }
}

/** ConfirmDialog contains a message, an ok button, and a cancel button. */
export class ConfirmDialog extends BaseDialog {}
