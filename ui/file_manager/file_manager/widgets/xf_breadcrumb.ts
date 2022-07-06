// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {getTemplate} from './xf_breadcrumb.html.js';

/**
 * Breadcrumb displays the current directory path.
 *
 * It emits the `BREADCRUMB_CLICKED` event when any part of the breadcrumb is
 * clicked.
 */
export class XfBreadcrumb extends HTMLElement {
  /** Breadcrumb path parts.  */
  private parts_: string[];

  constructor() {
    super();

    // Create element content.
    const fragment = getTemplate().content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    this.parts_ = [];
  }

  /** DOM connected.  */
  connectedCallback() {
    this.addEventListener('click', this.onClicked_.bind(this));
    this.addEventListener('keydown', this.onKeydown_.bind(this));
    this.addEventListener('tabkeyclose', this.onTabkeyClose_.bind(this));
    this.addEventListener('close', this.closeMenu_.bind(this));
    this.addEventListener('blur', this.closeMenu_.bind(this));
  }

  /** Gets parts.  */
  get parts(): string[] {
    return this.parts_;
  }

  /** Gets path.  */
  get path(): string {
    return this.parts_.join('/');
  }

  /**
   * Sets the path: update parts from |path|. Emits a 'path-updated' _before_
   * updating the parts <button> element content to the new |path|.
   */
  set path(path: string) {
    this.parts_ = path ? path.split('/') : [];
    this.queueRender_();
  }

  private queueRender_() {
    window.requestAnimationFrame(this.renderParts_.bind(this));
  }

  /** Renders the path <button> parts. */
  private renderParts_() {
    const buttons: HTMLButtonElement[] =
        Array.from(this.shadowRoot!.querySelectorAll('button[id]'));
    const enabled: number[] = [];

    function setButton(i: number, text: string|null|undefined) {
      const button: HTMLButtonElement|undefined = buttons[i];
      if (!button) {
        console.error(`Invalid button index ${i}`);
        return;
      }
      const previousSibling =
          button.previousElementSibling as HTMLButtonElement;
      if (previousSibling.hasAttribute('caret')) {
        previousSibling.hidden = !text;
      }

      button.removeAttribute('has-tooltip');
      button.textContent = window.unescape(text || '');
      button.hidden = !text;
      button.disabled = false;
      !!text && enabled.push(i);
    }

    const parts = this.parts_;
    setButton(0, parts.length > 0 ? parts[0] : null);
    setButton(1, parts.length == 4 ? parts[parts.length - 3] : null);
    buttons[1]!.hidden = parts.length != 4;
    setButton(2, parts.length > 2 ? parts[parts.length - 2] : null);
    setButton(3, parts.length > 1 ? parts[parts.length - 1] : null);

    if (enabled.length) {  // Disable the "last" button.
      buttons[enabled.pop()!]!.disabled = true;
    }

    this.closeMenu_();
    this.renderElidedParts_();

    this.setAttribute('path', this.path);
  }

  /** Renders elided path parts in a drop-down menu.  */
  private renderElidedParts_() {
    const elider: HTMLButtonElement =
        this.shadowRoot!.querySelector('button[elider]')!;
    const parts = this.parts_;

    elider.hidden = parts.length <= 4;
    if (elider.hidden) {
      this.shadowRoot!.querySelector('cr-action-menu')!.innerHTML = '';
      (elider.previousElementSibling as HTMLButtonElement).hidden = true;
      return;
    }

    let elidedParts = '';
    for (let i = 1; i < parts.length - 2; ++i) {
      elidedParts += `<button class='dropdown-item'>${
          window.unescape(parts[i]!)}<paper-ripple></paper-ripple></button>`;
    }

    const menu = this.shadowRoot!.querySelector('cr-action-menu')!;
    menu.innerHTML = elidedParts;

    (elider.previousElementSibling as HTMLButtonElement).hidden = false;
    elider.hidden = false;
  }

  /**
   * Returns the breadcrumb buttons: they contain the current path ordered by
   * its parts, which are stored in the <button>.textContent.
   */
  private getBreadcrumbButtons_(): HTMLButtonElement[] {
    const parts = this.shadowRoot!.querySelectorAll<HTMLButtonElement>(
        'button[id]:not([hidden])')!;
    if (this.parts_.length <= 4) {
      return Array.from(parts);
    }

    const elided = this.shadowRoot!.querySelectorAll<HTMLButtonElement>(
        'cr-action-menu button')!;
    return [parts[0]].concat(Array.from(elided), Array.from(parts).slice(1)) as
        HTMLButtonElement[];
  }

  /**
   * Returns the visible buttons rendered CSS overflow: ellipsis that have no
   * 'has-tooltip' attribute.
   *
   * Note: call in a requestAnimationFrame() to avoid a style resolve.
   *
   * @return  buttons Callers can set the tool tip attribute on the returned
   *     buttons.
   */
  getEllipsisButtons(): HTMLButtonElement[] {
    return this.getBreadcrumbButtons_().filter(button => {
      if (!button.hasAttribute('has-tooltip') && button.offsetWidth) {
        return button.offsetWidth < button.scrollWidth;
      }
      return false;
    });
  }

  /**
   * Returns breadcrumb buttons that have a 'has-tooltip' attribute. Note the
   * elider button is excluded since it has an i18n aria-label.
   *
   * @return buttons Caller could remove the tool tip event listeners from the
   *     returned buttons.
   */
  getToolTipButtons(): HTMLButtonElement[] {
    const hasToolTip = 'button:not([elider])[has-tooltip]';
    return Array.from(this.shadowRoot!.querySelectorAll(hasToolTip));
  }

  /**
   * Handles 'click' events.
   *
   * Emits the `BREADCRUMB_CLICKED` event when a breadcumb button is clicked
   * with the index indicating the current path part that was clicked.
   */
  private onClicked_(event: MouseEvent|KeyboardEvent) {
    event.stopImmediatePropagation();
    event.preventDefault();

    if ((event as KeyboardEvent).repeat) {
      return;
    }

    const element = event.composedPath()[0] as HTMLElement;
    if (element.hasAttribute('elider')) {
      this.toggleMenu_();
      return;
    }

    if (element instanceof HTMLButtonElement) {
      const parts = this.getBreadcrumbButtons_();
      this.dispatchEvent(new CustomEvent(BREADCRUMB_CLICKED, {
        bubbles: true,
        composed: true,
        detail: {
          partIndex: parts.indexOf(element),
        },
      }));
    }
  }

  /** Handles keyboard events. */
  private onKeydown_(event: KeyboardEvent) {
    if (event.key === ' ' || event.key === 'Enter') {
      this.onClicked_(event);
    }
  }

  /**
   * Handles the custom 'tabkeyclose' event, that indicates a 'Tab' key event
   * has returned focus to button[elider] while closing its drop-down menu.
   *
   * Moves the focus to the left or right of the button[elider] based on that
   * 'Tab' key event's shiftKey state.  There is always a visible <button> to
   * the left or right of button[elider].
   */
  private onTabkeyClose_(event: Event) {
    const detail = (event as CustomEvent).detail;
    if (!detail.shiftKey) {
      (this.shadowRoot!.querySelector(':focus ~ button:not([hidden])') as
       HTMLElement)
          .focus();
    } else {  // button#first is left of the button[elider].
      (this.shadowRoot!.querySelector('#first') as HTMLElement).focus();
    }
  }

  /**
   * Toggles drop-down menu: opens if closed or closes if open via closeMenu_.
   */
  private toggleMenu_() {
    if (this.hasAttribute('checked')) {
      this.closeMenu_();
      return;
    }

    // Compute drop-down horizontal RTL/LTR position.
    let position;
    const elider: HTMLElement =
        this.shadowRoot!.querySelector('button[elider]')!;
    if (document.documentElement.getAttribute('dir') === 'rtl') {
      position = elider.offsetLeft + elider.offsetWidth;
      position = document.documentElement.offsetWidth - position;
    } else {
      position = elider.offsetLeft;
    }

    // Show drop-down below the elider button.
    const menu: CrActionMenuElement =
        this.shadowRoot!.querySelector('cr-action-menu')!;
    const top = elider.offsetTop + elider.offsetHeight + 8;
    menu.showAt(elider, {top: top});

    // Style drop-down and horizontal position.
    const dialog = menu.getDialog();
    dialog.style.left = position + 'px';
    dialog.style.right = position + 'px';
    dialog.style.overflow = 'hidden auto';
    dialog.style.maxHeight = '272px';

    // Update global <html> and |this| element state.
    document.documentElement.classList.add('breadcrumb-elider-expanded');
    elider.setAttribute('aria-expanded', 'true');
    this.setAttribute('checked', '');
  }

  /** Closes drop-down menu if needed.  */
  private closeMenu_() {
    // Update global <html> and |this| element state.
    document.documentElement.classList.remove('breadcrumb-elider-expanded');
    const elider = this.shadowRoot!.querySelector('button[elider]')!;
    elider.setAttribute('aria-expanded', 'false');
    this.removeAttribute('checked');

    // Close the drop-down <dialog> if needed.
    const menu: CrActionMenuElement =
        this.shadowRoot!.querySelector('cr-action-menu')!;
    if (menu.getDialog().hasAttribute('open')) {
      menu.close();
    }
  }
}

export const BREADCRUMB_CLICKED = 'breadcrumb_clicked';

/**
 * `partIndex` is the index of the breadcrumb path e.g.:
 * "/My files/Downloads/sub-folder" indexes:
 *   0        1         2
 */
export type BreadcrumbClickedEvent = CustomEvent<{partIndex: number}>;

declare global {
  interface HTMLElementEventMap {
    [BREADCRUMB_CLICKED]: BreadcrumbClickedEvent;
  }

  interface HTMLElementTagNameMap {
    'xf-breadcrumb': XfBreadcrumb;
  }
}

customElements.define('xf-breadcrumb', XfBreadcrumb);
