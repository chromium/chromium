// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';

import {mouseEnterMaybeShowTooltip} from '../common/js/dom_utils.js';
import {str} from '../common/js/util.js';

import {customElement, property, query, state, css, html, XfBase, PropertyValues} from './xf_base.js';


/**
 * Breadcrumb displays the current directory path.
 */
@customElement('xf-breadcrumb')
export class XfBreadcrumb extends XfBase {
  /** A path is a "/" separated string. */
  @property({type: String, reflect: true}) path = '';

  static get events() {
    return {
      /** emits when any part of the breadcrumb is changed. */
      BREADCRUMB_CLICKED: 'breadcrumb_clicked',
    } as const;
  }

  /** Represents the parts extracted from the "path". */
  get parts(): string[] {
    return this.path.split('/');
  }

  @query('button[elider]') private $eliderButton_?: HTMLButtonElement;
  @query('cr-action-menu') private $actionMenu_?: CrActionMenuElement;
  @query('#first') private $firstButton_!: HTMLButtonElement;

  /** Indicates if the elider menu is open or not. */
  @state() private isMenuOpen_ = false;

  static override get styles() {
    return getCSS();
  }

  override render() {
    if (!this.path) {
      return html``;
    }
    const parts = this.path.split('/');
    const showElider = parts.length > 4;
    const partBeforeElider = parts[0];
    const eliderParts = showElider ? parts.slice(1, parts.length - 2) : [];
    const afterEliderIndex = showElider ? parts.length - 2 : 1;
    const partsAfterElider = parts.slice(afterEliderIndex);

    const ids = ['second', 'third', 'fourth'];

    return html`
      ${this.renderButton_(0, 'first', partBeforeElider)}
      ${showElider ? this.renderElider_(1, eliderParts) : ''}
      ${
        partsAfterElider.map(
            (part, index) => html`${
                this.renderButton_(
                    index + afterEliderIndex, ids[index]!, part)}`)}
    `;
  }

  /** Renders the path <button> parts. */
  private renderButton_(index: number, id: string, label: string|undefined) {
    const parts = this.path.split('/');
    const isLast = index === parts.length - 1;
    const caret = isLast ? '' : html`<span caret></span>`;
    return html`
      <button
        ?disabled=${isLast}
        id=${id}
        @click=${(event: MouseEvent) => this.onButtonClicked_(index, event)}
        @mouseenter=${this.onButtonMouseEntered_}
        @keydown=${
        (event: KeyboardEvent) => this.onButtonKeydown_(index, event)}
      >${window.unescape(label || '')}</button>
      ${caret}
    `;
  }

  /** Renders elided path parts in a drop-down menu.  */
  private renderElider_(startIndex: number, parts: string[]) {
    return html`
      <button
        elider
        aria-haspopup="menu"
        aria-expanded=${this.isMenuOpen_ ? 'true' : 'false'}
        aria-label=${str('LOCATION_BREADCRUMB_ELIDER_BUTTON_LABEL')}
        @click=${this.onEliderButtonClicked_}
        @keydown=${this.onEliderButtonKeydown_}
      ><span elider></span></button>
      <span caret></span>
      <cr-action-menu id="elider-menu">
        ${
        parts.map(
            (part, index) => html`
          <button
            class='dropdown-item'
            @click=${
                (event: MouseEvent) =>
                    this.onButtonClicked_(index + startIndex, event)}
            @mouseenter=${this.onButtonMouseEntered_}
            @keydown=${
                (event: KeyboardEvent) =>
                    this.onButtonKeydown_(index + startIndex, event)}
          >${window.unescape(part)}<paper-ripple></paper-ripple></button>
        `)}
      </cr-action-menu>
    `;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('tabkeyclose', this.onTabkeyClose_.bind(this));
    this.addEventListener('close', this.closeMenu_.bind(this));
    this.addEventListener('blur', this.closeMenu_.bind(this));
  }

  override willUpdate(changedProperties: PropertyValues) {
    // If path changes, we also need to update the state (isMenuOpen_) before
    // the next render.
    if (changedProperties.has('path') && this.isMenuOpen_) {
      this.closeMenu_();
    }
  }

  /**
   * Handles 'click' events for path button.
   *
   * Emits the `BREADCRUMB_CLICKED` event when a breadcrumb button is clicked
   * with the index indicating the current path part that was clicked.
   */
  private onButtonClicked_(index: number, event: MouseEvent|KeyboardEvent) {
    event.stopImmediatePropagation();
    event.preventDefault();

    if ((event as KeyboardEvent).repeat) {
      return;
    }

    const breadcrumbClickEvent =
        new CustomEvent(XfBreadcrumb.events.BREADCRUMB_CLICKED, {
          bubbles: true,
          composed: true,
          detail: {
            partIndex: index,
          },
        });
    this.dispatchEvent(breadcrumbClickEvent);
  }

  /** Handles mouseEnter event for the path button. */
  private onButtonMouseEntered_(event: MouseEvent) {
    mouseEnterMaybeShowTooltip(event);
  }

  /** Handles keyboard events for path button. */
  private onButtonKeydown_(index: number, event: KeyboardEvent) {
    if (event.key === ' ' || event.key === 'Enter') {
      this.onButtonClicked_(index, event);
    }
  }

  /**
   * Handles 'click' events for elider button.
   */
  private onEliderButtonClicked_(event: MouseEvent|KeyboardEvent) {
    event.stopImmediatePropagation();
    event.preventDefault();

    if ((event as KeyboardEvent).repeat) {
      return;
    }

    this.toggleMenu_();
  }

  /** Handles keyboard events for elider button. */
  private onEliderButtonKeydown_(event: KeyboardEvent) {
    if (event.key === ' ' || event.key === 'Enter') {
      this.onEliderButtonClicked_(event);
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
      (this.renderRoot.querySelector(':focus ~ button') as HTMLElement).focus();
    } else {  // button#first is left of the button[elider].
      this.$firstButton_.focus();
    }
  }

  /**
   * Toggles drop-down menu: opens if closed or closes if open via closeMenu_.
   */
  private toggleMenu_() {
    if (this.isMenuOpen_) {
      this.closeMenu_();
      return;
    }

    // Compute drop-down horizontal RTL/LTR position.
    let position: number;
    if (document.documentElement.getAttribute('dir') === 'rtl') {
      position =
          this.$eliderButton_!.offsetLeft + this.$eliderButton_!.offsetWidth;
      position = document.documentElement.offsetWidth - position;
    } else {
      position = this.$eliderButton_!.offsetLeft;
    }

    // Show drop-down below the elider button.
    const top =
        this.$eliderButton_!.offsetTop + this.$eliderButton_!.offsetHeight + 8;
    this.$actionMenu_!.showAt(this.$eliderButton_!, {top: top});

    // Style drop-down and horizontal position.
    const dialog = this.$actionMenu_!.getDialog();
    dialog.style.left = position + 'px';
    dialog.style.right = position + 'px';
    dialog.style.overflow = 'hidden auto';
    dialog.style.maxHeight = '272px';

    // Update global <html> and |this| element state.
    document.documentElement.classList.add('breadcrumb-elider-expanded');
    this.isMenuOpen_ = true;
  }

  /** Closes drop-down menu if needed.  */
  private closeMenu_() {
    // Update global <html> and |this| element state.
    document.documentElement.classList.remove('breadcrumb-elider-expanded');

    // Close the drop-down <dialog> if needed.
    if (this.$actionMenu_?.getDialog().hasAttribute('open')) {
      this.$actionMenu_.close();
    }
    this.isMenuOpen_ = false;
  }
}

function getCSS() {
  return css`
    :host([hidden]),
    [hidden] {
      display: none !important;
    }

    :host-context(html.col-resize) > * {
      cursor: unset !important;
    }

    :host {
      align-items: center;
      display: flex;
      font-family: 'Roboto Medium';
      font-size: 14px;
      outline: none;
      overflow: hidden;
      user-select: none;
      white-space: nowrap;
    }

    span[caret] {
      -webkit-mask-image: url(/foreground/images/files/ui/arrow_right.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: var(--cros-icon-color-secondary);
      display: inline-flex;
      height: 20px;
      min-width: 20px;
      padding: 8px 0;
      width: 20px;
    }

    :host-context(html[dir='rtl']) span[caret] {
      transform: scaleX(-1);
    }

    button {
      /* don't use browser's background-color. */
      background-color: unset;
      border: 2px solid transparent;
      border-radius: 4px;
      color: var(--cros-text-color-primary);
      cursor: pointer;
      display: inline-block;

      /* don't use browser's button font. */
      font: inherit;
      height: 32px;
      margin: 0;

      /* elide wide text */
      max-width: 200px;
      /* text rendering debounce: fix a minimum width. */
      min-width: calc(12px + 1em);
      outline: none;
      overflow: hidden;
      padding: 0 8px;

      /* text rendering debounce: center. */
      text-align: center;
      text-overflow: ellipsis;
    }

    button[disabled] {
      color: var(--cros-text-color-primary);
      cursor: default;
      font-weight: 500;
      margin-inline-end: 4px;
    }

    span[elider] {
      --tap-target-shift: -7px;
      -webkit-mask-image: url(/foreground/images/files/ui/menu_ng.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: var(--cros-icon-color-primary);
      height: 48px;
      margin-inline-start: var(--tap-target-shift);
      margin-top: var(--tap-target-shift);
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
      background-color: var(--cros-menu-item-background-hover);
    }

    button.dropdown-item > paper-ripple {
      --paper-ripple-opacity: 100%;
      color: var(--cros-menu-item-background-hover);
    }

    button:not([disabled]):not(:active):hover {
      background-color: var(--cros-ripple-color);
    }

    :host-context(:root.pointer-active) button:not(:active):hover {
      background-color: unset;
      cursor: default;
    }

    :host-context(:root.focus-outline-visible) > button:focus {
      background-color: unset;
      border: 2px solid var(--cros-icon-color-prominent);
    }

    :host-context(.breadcrumb-elider-expanded) button[elider] {
      background: var(--cros-icon-button-pressed-color);
    }

    button:active {
      background: var(--cros-icon-button-pressed-color);
    }

    #elider-menu button {
      border: unset;
      color: var(--cros-menu-label-color);
      display: block;
      font-family: 'Roboto';
      font-size: 13px;
      max-width: min(288px, 40vw);
      min-width: 192px;  /* menu width */
      padding: 0 16px;
      text-align: start;
    }

    :host-context(:root.focus-outline-visible) #elider-menu button:hover {
      background-color: unset;
    }

    :host-context(:root.focus-outline-visible) #elider-menu button:focus {
      background-color: var(--cros-menu-item-background-hover);
    }

    cr-action-menu {
      --cr-menu-background-color: var(--cros-bg-color-elevation-2);
      --cr-menu-background-sheen: none;
      --cr-menu-shadow: var(--cros-elevation-2-shadow);
    }
  `;
}

/**
 * `partIndex` is the index of the breadcrumb path e.g.:
 * "/My files/Downloads/sub-folder" indexes:
 *   0        1         2
 */
export type BreadcrumbClickedEvent = CustomEvent<{partIndex: number}>;

declare global {
  interface HTMLElementEventMap {
    [XfBreadcrumb.events.BREADCRUMB_CLICKED]: BreadcrumbClickedEvent;
  }

  interface HTMLElementTagNameMap {
    'xf-breadcrumb': XfBreadcrumb;
  }
}
