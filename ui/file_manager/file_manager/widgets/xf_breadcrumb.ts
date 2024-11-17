// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';

import type {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';

import {getCrActionMenuTop, mouseEnterMaybeShowTooltip} from '../common/js/dom_utils.js';
import {str} from '../common/js/translations.js';

import {css, customElement, html, property, type PropertyValues, query, state, XfBase} from './xf_base.js';


/**
 * Breadcrumb displays the current directory path.
 */
@customElement('xf-breadcrumb')
export class XfBreadcrumb extends XfBase {
  /** A path is a "/" separated string. */
  @property({type: String, reflect: true}) path = '';

  /** The maximum number of path elements shown. */
  @property({type: Number, reflect: true}) maxPathParts = 4;

  /* Should breadcrumb be rendered in small size (for file path display bar in
   * search) */
  @property({type: Boolean, reflect: true}) small = false;

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
    const showElider = parts.length > this.maxPathParts;
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
      >${window.unescape(label || '')}<paper-ripple></paper-ripple></button>
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
      ><span elider></span><paper-ripple></paper-ripple></button>
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
      (this.renderRoot.querySelector<HTMLElement>(':focus ~ button')!).focus();
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
    const top = getCrActionMenuTop(this.$eliderButton_!, 8);
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
      outline: none;
      overflow: hidden;
      padding-inline-start: 8px;
      user-select: none;
      white-space: nowrap;
    }

    span[caret] {
      -webkit-mask-image: url(/foreground/images/files/ui/arrow_right.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: var(--cros-sys-on_surface_variant);
      display: inline-flex;
      height: 20px;
      min-width: 20px;
      width: 20px;
    }

    :host([small]) span[caret] {
      height: 16px;
      min-width: 16px;
      width: 16px;
    }

    :host-context(html[dir='rtl']) span[caret] {
      transform: scaleX(-1);
    }

    button {
      /* don't use browser's background-color. */
      background-color: unset;
      border: none;
      color: var(--cros-sys-on_surface_variant);
      cursor: pointer;
      display: inline-block;
      position: relative;

      font: var(--cros-title-1-font);
      margin: 0;

      /* elide wide text */
      max-width: 200px;
      /* text rendering debounce: fix a minimum width. */
      min-width: calc(12px + 1em);
      outline: none;
      overflow: hidden;

      /* text rendering debounce: center. */
      text-align: center;
      text-overflow: ellipsis;
    }

    /* no eliding text in small layout */
    :host([small]) button {
      max-width: none;
    }

    :host([small]) button {
      font: var(--cros-button-1-font);
    }

    button[disabled] {
      cursor: default;
      margin-inline-end: 4px;
      pointer-events: none;
    }

    span[elider] {
      --tap-target-shift: -6px;
      -webkit-mask-image: url(/foreground/images/files/ui/menu_ng.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      -webkit-mask-size: 20px;
      background-color: currentColor;
      height: 48px;
      margin-inline-start: var(--tap-target-shift);
      margin-top: var(--tap-target-shift);
      min-width: 48px;
      position: relative;
      transform: rotate(90deg);
      width: 48px;
    }

    :host([small]) span[elider] {
      --tap-target-shift: 0;
      -webkit-mask-size: 16px;
      height: 32px;
      min-width: 32px;
      width: 32px;
    }

    button[elider] {
      border-radius: 50%;
      display: inline-flex;
      height: 36px;
      min-width: 36px;
      padding: 0;
      width: 36px;
    }

    :host([small]) button[elider] {
      height: 32px;
      min-width: 32px;
      width: 32px;
    }

    :host > button:not([elider]) {
      border-radius: 18px;
      height: 36px;
      margin: 6px 2px;
      padding: 0 12px;
    }

    :host([small]) > button:not([elider]) {
      border-radius: 16px;
      height: 32px;
      margin: 8px 0;
      padding: 0 8px;
    }

    :host > button:first-child {
      margin-inline-start: 0;
    }

    button[disabled] {
      color: var(--cros-sys-on_surface);
    }

    button:not(:active):hover {
      background-color: var(--cros-sys-hover_on_subtle);
    }

    :host-context(.pointer-active) button:not(:active):hover {
      background-color: unset;
      cursor: default;
    }

    paper-ripple {
      --paper-ripple-opacity: 100%;
      color: var(--cros-sys-ripple_neutral_on_subtle);
    }

    :host > button:focus-visible {
      outline: 2px solid var(--cros-sys-focus_ring);
    }

    button:active {
      background-color: var(--cros-sys-hover_on_subtle);
    }

    button[elider][aria-expanded="true"] {
      background-color: var(--cros-sys-pressed_on_subtle);
    }

    #elider-menu button {
      color: var(--cros-sys-on_surface);
      display: block;
      font: var(--cros-button-2-font);
      height: 36px;
      max-width: min(288px, 40vw);
      min-width: 192px;  /* menu width */
      padding: 0 16px;
      position: relative;
      text-align: start;
    }

    :host-context(.focus-outline-visible) #elider-menu button:focus::after {
      border: 2px solid var(--cros-sys-focus_ring);
      border-radius: 8px;
      content: '';
      height: 32px; /* option height - 2 x border width */
      left: 0;
      position: absolute;
      top: 0;
      width: calc(100% - 4px); /* 2 x border width */
    }

    /** Reset the hover color when using keyboard to navigate the menu items. */
    :host-context(.focus-outline-visible) #elider-menu button:hover {
      background-color: unset;
    }

    cr-action-menu {
      --cr-menu-background-color: var(--cros-sys-base_elevated);
      --cr-menu-background-sheen: none;
      /* TODO(wenbojie): use elevation variable when it's ready.
      --cros-sys-elevation3 */
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
