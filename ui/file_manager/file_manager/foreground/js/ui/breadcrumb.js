// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @const {string} breadCrumbTemplate
 */
const breadCrumbTemplate = `
  <style>
    :host([hidden]), [hidden] {
      display: none !important;
    }

    :host-context(html.col-resize) > * {
      cursor: unset !important;
    }

    :host {
      display: flex;
      font-family: 'Roboto Medium';
      font-size: 14px;
      outline: none;
      overflow: hidden;
      user-select: none;
      white-space: nowrap;
      align-items: center;
    }

    span[caret] {
      -webkit-mask-image: url(../../images/files/ui/arrow_right.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: currentColor;
      display: inline-flex;
      height: 20px;
      padding: 8px 0;
      min-width: 20px;
      width: 20px;
    }

    :host-context(html[dir='rtl']) span[caret] {
      transform: scaleX(-1);
    }

    button {
      /* don't use browser's background-color. */
      background-color: unset;
      border: 1px solid transparent;
      border-radius: 4px;
      color: var(--google-grey-700);
      cursor: pointer;
      display: inline-block;

      /* don't use browser's button font. */
      font: inherit;
      height: 32px;
      margin: 0;

      /* text rendering debounce: fix a minimum width. */
      min-width: calc(12px + 1em);

      /* elide wide text */
      max-width: 200px;
      outline: none;
      overflow: hidden;
      padding: 0px 8px;

      /* text rendering debounce: center. */
      text-align: center;
      text-overflow: ellipsis;
    }

    button[disabled] {
      color: var(--google-grey-900);
      cursor: default;
      font-weight: 500;
      margin-inline-end: 4px;
    }

    span[elider] {
      --tap-target-shift: -7px;
      -webkit-mask-image: url(../../images/files/ui/menu_ng.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: currentColor;
      height: 48px;
      margin-top: var(--tap-target-shift);
      margin-inline-start: var(--tap-target-shift);
      min-width: 48px;
      position: relative;
      transform: rotate(90deg);
      width: 48px;
    }

    button[elider] {
      border-radius: 50%;
      box-sizing: border-box;
      display: inline-flex;
      height: 36px;
      min-width: 36px;
      padding: 0;
      width: 36px;
    }

    button.dropdown-item {
      position: relative;
    }

    :host-context(:root.pointer-active) button.dropdown-item:active {
      background-color: rgba(0, 0, 0, 4%);
    }

    :host-context(:root:not(.pointer-active)) button.dropdown-item > paper-ripple {
      display: none;
    }

    button.dropdown-item > paper-ripple {
      --paper-ripple-opacity: 8%;
      color: black;
    }

    button:not([disabled]):not(:active):hover {
      background-color: rgba(0, 0, 0, 4%);
    }

    :host-context(:root.pointer-active) button:not(:active):hover {
      background-color: unset;
      cursor: default;
    }

    :host-context(:root.focus-outline-visible) > button:focus {
      background-color: unset;
      border: 1px solid var(--google-blue-600);
    }

    :host([checked]) button[elider] {
      background-color: rgba(0, 0, 0, 12%);
    }

    button:active {
      background-color: rgba(0, 0, 0, 12%);
    }

    #elider-menu button {
      border: unset;
      color: rgb(51, 51, 51);
      display: block;
      font-family: 'Roboto';
      font-size: 13px;
      min-width: 192px;  /* menu width */
      max-width: min(288px, 40vw);
      padding: 0 16px;
      text-align: start;
    }

    :host-context(:root.focus-outline-visible) #elider-menu button:hover {
      background-color: unset;
    }

    :host-context(:root.focus-outline-visible) #elider-menu button:focus {
      background-color: rgba(0, 0, 0, 4%);
    }
  </style>

  <button id='first'></button>
  <span caret hidden></span>
  <button elider aria-haspopup='menu' aria-expanded='false'
      aria-label='$i18n{LOCATION_BREADCRUMB_ELIDER_BUTTON_LABEL}'>
    <span elider></span>
  </button>
  <span caret hidden></span>
  <button id='second'></button>
  <span caret hidden></span>
  <button id='third'></button>
  <span caret hidden></span>
  <button id='fourth'></button>
  <cr-action-menu id='elider-menu'></cr-action-menu>
`;

/**
 * Class BreadCrumb.
 */
class BreadCrumb extends HTMLElement {
  constructor() {
    /**
     * Create element content.
     */
    super().attachShadow({mode: 'open'}).innerHTML = breadCrumbTemplate;

    /**
     * User interaction signals callback.
     * @private @type {!function(*)}
     */
    this.signal_ = console.log;

    /**
     * BreadCrumb path parts.
     * @private @type {!Array<string>}
     */
    this.parts_ = [];
  }

  /**
   * Sets the user interaction signal callback.
   *
   * @param {?function(*)} signal
   */
  setSignalCallback(signal) {
    this.signal_ = signal || console.log;
  }

  /**
   * DOM connected.
   *
   * @private
   */
  connectedCallback() {
    this.onkeydown = this.onKeydown_.bind(this);
    this.onclick = this.onClicked_.bind(this);
    this.onblur = this.closeMenu_.bind(this);

    this.addEventListener('tabkeyclose', this.onTabkeyClose_.bind(this));
    this.addEventListener('close', this.onblur);
  }

  /**
   * Gets parts.
   * @return {!Array<string>}
   */
  get parts() {
    return this.parts_;
  }

  /**
   * Gets path.
   * @return {string} path
   */
  get path() {
    return this.parts_.join('/');
  }

  /**
   * Sets the path: update parts from |path|. Emits a 'path-updated' _before_
   * updating the parts <button> element content to the new |path|.
   *
   * @param {string} path
   */
  set path(path) {
    this.parts_ = path ? path.split('/') : [];
    this.signal_('path-updated');
    this.renderParts_();
  }

  /**
   * Renders the path <button> parts. Emits 'path-rendered' signal.
   *
   * @private
   */
  renderParts_() {
    const buttons = this.shadowRoot.querySelectorAll('button[id]');
    const enabled = [];

    function setButton(i, text) {
      const previousSibling = buttons[i].previousElementSibling;
      if (previousSibling.hasAttribute('caret')) {
        previousSibling.hidden = !text;
      }

      buttons[i].removeAttribute('has-tooltip');
      buttons[i].textContent = window.unescape(text);
      buttons[i].hidden = !text;
      buttons[i].disabled = false;
      !!text && enabled.push(i);
    }

    const parts = this.parts_;
    setButton(0, parts.length > 0 ? parts[0] : null);
    setButton(1, parts.length == 4 ? parts[parts.length - 3] : null);
    buttons[1].hidden = parts.length != 4;
    setButton(2, parts.length > 2 ? parts[parts.length - 2] : null);
    setButton(3, parts.length > 1 ? parts[parts.length - 1] : null);

    if (enabled.length) {  // Disable the "last" button.
      buttons[enabled.pop()].disabled = true;
    }

    this.closeMenu_();
    this.renderElidedParts_();

    this.setAttribute('path', this.path);
    this.signal_('path-rendered');
  }

  /**
   * Renders elided path parts in a drop-down menu.
   *
   * @private
   */
  renderElidedParts_() {
    const elider = this.shadowRoot.querySelector('button[elider]');
    const parts = this.parts_;

    elider.hidden = parts.length <= 4;
    if (elider.hidden) {
      this.shadowRoot.querySelector('cr-action-menu').innerHTML = '';
      elider.previousElementSibling.hidden = true;
      return;
    }

    let elidedParts = '';
    for (let i = 1; i < parts.length - 2; ++i) {
      elidedParts += `<button class='dropdown-item'>${
          window.unescape(parts[i])}<paper-ripple></paper-ripple></button>`;
    }

    const menu = this.shadowRoot.querySelector('cr-action-menu');
    menu.innerHTML = elidedParts;

    elider.previousElementSibling.hidden = false;
    elider.hidden = false;
  }

  /**
   * Returns the breadcrumb buttons: they contain the current path ordered by
   * its parts, which are stored in the <button>.textContent.
   *
   * @return {!Array<HTMLButtonElement>}
   * @private
   */
  getBreadcrumbButtons_() {
    const parts = this.shadowRoot.querySelectorAll('button[id]:not([hidden])');
    if (this.parts_.length <= 4) {
      return Array.from(parts);
    }

    const elided = this.shadowRoot.querySelectorAll('cr-action-menu button');
    return [parts[0]].concat(Array.from(elided), Array.from(parts).slice(1));
  }

  /**
   * Returns the visible buttons rendered CSS overflow: ellipsis that have no
   * 'has-tooltip' attribute.
   *
   * Note: call in a requestAnimationFrame() to avoid a style resolve.
   *
   * @return {!Array<HTMLButtonElement>} buttons Callers can set the tool tip
   *    attribute on the returned buttons.
   */
  getEllipsisButtons() {
    return this.getBreadcrumbButtons_().filter(button => {
      if (!button.hasAttribute('has-tooltip') && button.offsetWidth) {
        return button.offsetWidth < button.scrollWidth;
      }
    });
  }

  /**
   * Returns breadcrumb buttons that have a 'has-tooltip' attribute. Note the
   * elider button is excluded since it has an i18n aria-label.
   *
   * @return {!Array<HTMLButtonElement>} buttons Caller could remove the tool
   *    tip event listeners from the returned buttons.
   */
  getToolTipButtons() {
    const hasToolTip = 'button:not([elider])[has-tooltip]';
    return Array.from(this.shadowRoot.querySelectorAll(hasToolTip));
  }

  /**
   * Handles 'click' events.
   *
   * Emits an index signal on breadcumb button click: the index indicates the
   * current path part that was clicked.
   *
   * @param {Event} event
   * @private
   */
  onClicked_(event) {
    event.stopImmediatePropagation();
    event.preventDefault();

    if (event.repeat) {
      return;
    }

    const element = event.path[0];
    if (element.hasAttribute('elider')) {
      this.toggleMenu_();
      return;
    }

    if (element instanceof HTMLButtonElement) {
      const parts = this.getBreadcrumbButtons_();
      this.signal_(parts.indexOf(element));
    }
  }

  /**
   * Handles keyboard events.
   *
   * @param {Event} event
   * @private
   */
  onKeydown_(event) {
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
   *
   * @param {Event} event
   * @private
   */
  onTabkeyClose_(event) {
    if (!event.detail.shiftKey) {
      this.shadowRoot.querySelector(':focus ~ button:not([hidden])').focus();
    } else {  // button#first is left of the button[elider].
      this.shadowRoot.querySelector('#first').focus();
    }
  }

  /**
   * Toggles drop-down menu: opens if closed and emits 'path-rendered' signal
   * or closes if open via closeMenu_.
   *
   * @private
   */
  toggleMenu_() {
    if (this.hasAttribute('checked')) {
      this.closeMenu_();
      return;
    }

    // Compute drop-down horizontal RTL/LTR position.
    let position;
    const elider = this.shadowRoot.querySelector('button[elider]');
    if (document.documentElement.getAttribute('dir') === 'rtl') {
      position = elider.offsetLeft + elider.offsetWidth;
      position = document.documentElement.offsetWidth - position;
    } else {
      position = elider.offsetLeft;
    }

    // Show drop-down below the elider button.
    const menu = this.shadowRoot.querySelector('cr-action-menu');
    const top = elider.offsetTop + elider.offsetHeight + 8;
    !window.UNIT_TEST && menu.showAt(elider, {top: top});

    // Style drop-down and horizontal position.
    const dialog = !window.UNIT_TEST ? menu.getDialog() : {style: {}};
    dialog.style['left'] = position + 'px';
    dialog.style['right'] = position + 'px';
    dialog.style['overflow'] = 'hidden auto';
    dialog.style['max-height'] = '272px';

    // Update global <html> and |this| element state.
    document.documentElement.classList.add('breadcrumb-elider-expanded');
    elider.setAttribute('aria-expanded', 'true');
    this.setAttribute('checked', '');

    // Emit rendered signal.
    this.signal_('path-rendered');
  }

  /**
   * Closes drop-down menu if needed.
   *
   * @private
   */
  closeMenu_() {
    // Update global <html> and |this| element state.
    document.documentElement.classList.remove('breadcrumb-elider-expanded');
    const elider = this.shadowRoot.querySelector('button[elider]');
    elider.setAttribute('aria-expanded', 'false');
    this.removeAttribute('checked');

    // Close the drop-down <dialog> if needed.
    const menu = this.shadowRoot.querySelector('cr-action-menu');
    if (!window.UNIT_TEST && menu.getDialog().hasAttribute('open')) {
      menu.close();
    }
  }
}

customElements.define('bread-crumb', BreadCrumb);
