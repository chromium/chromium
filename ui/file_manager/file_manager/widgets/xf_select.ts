// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview xf-select element which is ChromeOS <select>..</select>.
 */

import type {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {getCrActionMenuTop} from '../common/js/dom_utils.js';

import {css, type CSSResultGroup, customElement, html, property, query, XfBase} from './xf_base.js';

/**
 * The data structure used to set the new options on the select element.
 * The value field should be independent of the user locale, while text should
 * be a string corresponding to the value in the user locale.
 */
export interface XfOption {
  // The locale independent value, e.g., 'home'.
  value: string;
  // The locale dependent string, e.g., 'hejmen' in Esperanto.
  text: string;
  // Whether this is the default option, shown prior to user actions.
  default?: boolean;
}

/**
 * The data structure used to inform SELECTION_CHANGED listeners about the
 * current selection. Posted in the event detail.
 */
export interface XfSelectedValue {
  index: number;
  value: string;
  text: string;
}

/**
 * Implements an element similar to HTML select, customized for ChromeOS files.
 *
 * It emits the `SELECTION_CHANGED` event when the selected element change. The
 * detail filed of the event carries index of the new element, its value and the
 * text visible to the user.
 *
 * const element = document.createElement('xf-select');
 * element.options = [
 *   {value: 'value-a', text: 'Text of value A'},
 *   ...
 * ];
 * element.icon = 'select-location';
 * element.addEventListener(
 *     SELECTION_CHANGED, (event) => {
 *       if (event.detail.value === 'value-a') {
 *         ... // React to value-a being selected.
 *       }
 *     });
 *
 */
@customElement('xf-select')
export class XfSelect extends XfBase {
  /**
   * The name of the icon to be used by the xf-select. This icon name must match
   * the name of an icon in the
   * //ui/file_manager/file_manager/foreground/images/files/ui/
   */
  @property({type: String, reflect: false}) icon = '';

  /**
   * The options available for selection.
   */
  @property({type: Array, reflect: false}) options: XfOption[] = [];

  /**
   * The current selected value.
   */
  @property({type: String, reflect: true}) value: string = '';

  /**
   * The alignment of items in the dropdown menu. Can be one of
   * 'start', 'center', 'end'.
   */
  @property({type: String, reflect: true}) menuAlignment: string = 'center';

  static get events() {
    return {
      /** emits when the currently selected option changed. */
      SELECTION_CHANGED: 'selection_changed',
    } as const;
  }

  /**
   * The button that toggles the options menu.
   */
  @query('cr-button#dropdown-toggle')
  private $toggleDropdownButton_?: CrButtonElement;

  /**
   * The options menu.
   */
  @query('cr-action-menu') private $optionsMenu_?: CrActionMenuElement;

  /**
   * The currently selected option.
   */
  private selectedOption_: XfSelectedValue = {
    index: -1,
    value: '',
    text: '',
  };

  static override get styles(): CSSResultGroup {
    return getCSS();
  }

  override render() {
    const selectedIndex = this.computeSelectedIndex_();
    return html`
        ${this.renderFilterChip_(selectedIndex)}
        ${this.renderDropdown_()}
    `;
  }

  override click() {
    if (this.$toggleDropdownButton_) {
      this.$toggleDropdownButton_.click();
    }
  }

  /**
   * Returns whether the component is expanded, with options visible, or
   * collapsed.
   */
  get expanded(): boolean {
    return this.$optionsMenu_ ? this.$optionsMenu_.open : false;
  }

  /**
   * Returns a template of the chip that shows the currently selected filter
   * value.
   */
  private renderFilterChip_(selectedIndex: number) {
    const buttonLabel =
        selectedIndex === -1 ? '' : this.options[selectedIndex]!.text;
    const iconPart = this.icon ?
        html`<span id="xf-select-icon" class="xf-select-icon ${
            this.icon}"></span>` :
        html``;
    const labelPart = html`<span id="selected-option">${buttonLabel}</span>`;

    return html`
      <cr-button id="dropdown-toggle"
              aria-haspopup="menu"
              aria-expanded=${this.expanded}
              @click=${this.onToggleOptions_}>
        ${iconPart}${labelPart}<span id="dropdown-icon"></span>
      </cr-button>`;
  }

  /**
   * Returns a template of the dropdown which shows available choices.
   */
  private renderDropdown_() {
    const alignment = this.menuAlignment || 'center';
    return html`<cr-action-menu>
        ${this.options.map((option, index) => {
      const checked = this.selectedOption_!.value === option.value;
      return html`
              <cr-button
                  class="dropdown-item dropdown-item-${alignment}"
                  role="menuitemcheckbox"
                  aria-label="${option.text}"
                  aria-checked="${checked}"
                  @click=${() => this.onOptionSelected_(index)}
                  ?selected=${checked}>
                ${option.text}
                <div class='dropdown-filler'></div>
                <div slot='suffix-icon' class='selected-icon'></div>
              </cr-button>`;
    })}
        </cr-action-menu>`;
  }

  override updated(changedProperties: Map<string, any>) {
    if (changedProperties.has('value')) {
      this.updateSelectedOption_(this.computeIndexForValue_(this.value));
    }
    if (changedProperties.has('options')) {
      this.updateSelectedOption_(this.computeSelectedIndex_());
    }
  }

  /**
   * Attempts to find the index of the value among options.
   */
  private computeIndexForValue_(value: string|null): number {
    let selectedIndex = -1;
    if (value) {
      selectedIndex = this.options.findIndex(e => e.value === this.value);
    }
    return selectedIndex;
  }

  /**
   * If the index is within range of option list, updates the selected value to
   * the one at the given index.
   */
  private updateSelectedOption_(index: number) {
    if (index !== this.selectedOption_.index) {
      if (index >= 0 && index < this.options.length) {
        this.selectedOption_ = {
          index: index,
          value: this.options[index]!.value,
          text: this.options[index]!.text,
        };
        this.dispatchSelectionChanged_();
      }
    }
  }

  /**
   * Attempts to establish the index of the selected item. The priority is given
   * the the value attribute. If set, it decides which option is selected. If
   * not set we pick either the first option, or the option with the default set
   * to true.
   */
  private computeSelectedIndex_(): number {
    let selectedIndex = this.computeIndexForValue_(this.value);
    // If we could not match the value, look for the default option.
    if (selectedIndex === -1) {
      for (let i = this.options.length - 1; i >= 0; --i) {
        if (this.options[i]!.default) {
          selectedIndex = i;
          break;
        }
      }
    }
    if (selectedIndex === -1 && this.options.length > 0) {
      selectedIndex = 0;
    }
    this.updateSelectedOption_(selectedIndex);
    return selectedIndex;
  }

  /**
   * Invoked when the toggle button is clicked. Toggles the visibility of the
   * dropdown options.
   */
  private onToggleOptions_(): void {
    if (this.expanded) {
      this.closeOptions_();
    } else {
      this.openOptions_();
    }
  }

  /**
   * Opens the dropdown options, providing they were closed.
   */
  private openOptions_() {
    if (!this.expanded) {
      const element: HTMLElement = this.$toggleDropdownButton_!;
      const top = getCrActionMenuTop(element, 8);
      this.$optionsMenu_!.showAt(
          element, {top: top, anchorAlignmentX: AnchorAlignment.AFTER_START});
    }
  }

  /**
   * Closes the dropdown options, providing they were open.
   */
  private closeOptions_() {
    if (this.expanded) {
      this.$optionsMenu_!.close();
    }
  }

  /**
   * React to one of the options being selected. If the selection changed the
   * currently selected option, it updates the value, which prompts
   * re-rendering. It also posts a selection change event. Finally it always
   * closes the option, regardless of change.
   */
  private onOptionSelected_(index: number) {
    if (index !== this.selectedOption_.index) {
      this.updateSelectedOption_(index);
      this.value = this.selectedOption_.value;
    }
    this.closeOptions_();
  }

  /**
   * Returns the currently selected option. If nothing is selected the index is
   * set to -1, and text and value are set to an empty string.
   */
  getSelectedOption(): XfSelectedValue {
    return this.selectedOption_;
  }

  /**
   * Dispatches SELECTION_CHANGED event with the current value of the selected
   * options.
   */
  private dispatchSelectionChanged_(): void {
    this.dispatchEvent(new CustomEvent(XfSelect.events.SELECTION_CHANGED, {
      bubbles: true,
      composed: true,
      detail: this.selectedOption_,
    }));
  }
}

/**
 * CSS used by the xf-select widget.
 */
function getCSS(): CSSResultGroup {
  return css`
    cr-button {
      --active-bg: none;
      --hover-bg-color: var(--cros-sys-hover_on_subtle);
      --hover-border-color: var(--cros-sys-separator);
      --ink-color: var(--cros-sys-ripple_neutral_on_subtle);
      --ripple-opacity: 100%;
      --text-color: var(--cros-sys-on_surface);
      box-shadow: none;
      font: var(--cros-button-1-font);
    }
    #dropdown-toggle {
      --border-color: var(--cros-sys-separator);
      --cr-button-height: 32px;
      border-radius: 8px;
      margin-inline: 4px;
      min-width: auto;
      padding-inline: 12px;
      white-space: nowrap;
    }
    :host(:first-of-type) #dropdown-toggle {
      margin-inline-start: 0;
    }
    :host-context(.focus-outline-visible) #dropdown-toggle:focus {
      outline: 2px solid var(--cros-sys-focus_ring);
      outline-offset: 2px;
    }
    .xf-select-icon {
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: var(--cros-sys-on_surface);
      height: 20px;
      width: 20px;
      margin-inline: 0 8px;
    }
    #xf-select-icon.select-location {
      -webkit-mask-image:
        url(/foreground/images/files/ui/select_location.svg);
    }
    #xf-select-icon.select-time {
      -webkit-mask-image:
        url(/foreground/images/files/ui/select_time.svg);
    }
    #xf-select-icon.select-filetype {
      -webkit-mask-image:
        url(/foreground/images/files/ui/select_filetype.svg);
    }
    #dropdown-icon {
      -webkit-mask-image:
        url(/foreground/images/files/ui/xf_select_dropdown.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: var(--cros-sys-on_surface);
      height: 20px;
      width: 20px;
      margin-inline: 8px 0;
    }
    cr-button.dropdown-item {
      --focus-shadow-color: none;
      font: var(--cros-button-2-font);
      height: 36px;
      padding: 0 16px;
    }
    cr-button.dropdown-item:hover {
      background-color: var(--cros-sys-hover_on_subtle);
    }
    cr-button.dropdown-item-center {
      justify-content: center;
    }
    cr-button.dropdown-item-start {
      justify-content: start;
    }
    cr-button.dropdown-item-end {
      justify-content: end;
    }
    div.dropdown-filler {
      flex-grow: 1;
    }
    div.selected-icon {
      -webkit-mask-image: url(/foreground/images/common/ic_selected.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: var(--cros-sys-primary);
      height: 20px;
      width: 20px;
      visibility: hidden;
    }
    cr-button[selected] div.selected-icon {
      visibility: visible;
    }
    :host-context(.focus-outline-visible)
        cr-action-menu cr-button:focus::after {
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
    :host-context(.focus-outline-visible) cr-action-menu cr-button:hover {
      background-color: unset;
    }
    cr-action-menu {
      --cr-menu-background-color: var(--cros-sys-base_elevated);
      --cr-menu-background-focus-color: none;
      --cr-menu-background-sheen: none;
      /* TODO(wenbojie): use elevation variable when it's ready.
      --cros-sys-elevation3 */
      --cr-menu-shadow: var(--cros-elevation-2-shadow);
    }
    cr-action-menu::part(dialog) {
      border-radius: 8px;
    }
  `;
}

/**
 * A custom event that informs the container which option kind change to what
 * value. It is up to the container to interpret these.
 */
export type SelectionChangedEvent = CustomEvent<XfSelectedValue>;

declare global {
  interface HTMLElementEventMap {
    [XfSelect.events.SELECTION_CHANGED]: SelectionChangedEvent;
  }

  interface HTMLElementTagNameMap {
    'xf-select': XfSelect;
  }
}
