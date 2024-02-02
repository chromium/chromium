// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_shared_vars.css.js';

import {assert} from '//resources/js/assert.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {FocusRow} from '//resources/js/focus_row.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {isMac, isWindows} from '//resources/js/platform.js';
import {getDeepActiveElement} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_action_menu.css.js';
import {getHtml} from './cr_action_menu.html.js';

interface ShowAtConfig {
  top?: number;
  left?: number;
  width?: number;
  height?: number;
  anchorAlignmentX?: AnchorAlignment;
  anchorAlignmentY?: AnchorAlignment;
  minX?: number;
  minY?: number;
  maxX?: number;
  maxY?: number;
  noOffset?: boolean;
}

export interface ShowAtPositionConfig {
  top: number;
  left: number;
  width?: number;
  height?: number;
  anchorAlignmentX?: AnchorAlignment;
  anchorAlignmentY?: AnchorAlignment;
  minX?: number;
  minY?: number;
  maxX?: number;
  maxY?: number;
}

export enum AnchorAlignment {
  BEFORE_START = -2,
  AFTER_START = -1,
  CENTER = 0,
  BEFORE_END = 1,
  AFTER_END = 2,
}

const DROPDOWN_ITEM_CLASS: string = 'dropdown-item';

const SELECTABLE_DROPDOWN_ITEM_QUERY: string =
    `.${DROPDOWN_ITEM_CLASS}:not([hidden]):not([disabled])`;

const AFTER_END_OFFSET: number = 10;

/**
 * Returns the point to start along the X or Y axis given a start and end
 * point to anchor to, the length of the target and the direction to anchor
 * in. If honoring the anchor would force the menu outside of min/max, this
 * will ignore the anchor position and try to keep the menu within min/max.
 */
function getStartPointWithAnchor(
    start: number, end: number, menuLength: number,
    anchorAlignment: AnchorAlignment, min: number, max: number): number {
  let startPoint = 0;
  switch (anchorAlignment) {
    case AnchorAlignment.BEFORE_START:
      startPoint = start - menuLength;
      break;
    case AnchorAlignment.AFTER_START:
      startPoint = start;
      break;
    case AnchorAlignment.CENTER:
      startPoint = (start + end - menuLength) / 2;
      break;
    case AnchorAlignment.BEFORE_END:
      startPoint = end - menuLength;
      break;
    case AnchorAlignment.AFTER_END:
      startPoint = end;
      break;
  }

  if (startPoint + menuLength > max) {
    startPoint = end - menuLength;
  }
  if (startPoint < min) {
    startPoint = start;
  }

  startPoint = Math.max(min, Math.min(startPoint, max - menuLength));

  return startPoint;
}

function getDefaultShowConfig(): ShowAtPositionConfig {
  return {
    top: 0,
    left: 0,
    height: 0,
    width: 0,
    anchorAlignmentX: AnchorAlignment.AFTER_START,
    anchorAlignmentY: AnchorAlignment.AFTER_START,
    minX: 0,
    minY: 0,
    maxX: 0,
    maxY: 0,
  };
}

export interface CrActionMenuElement {
  $: {
    contentNode: HTMLSlotElement,
    dialog: HTMLDialogElement,
    wrapper: HTMLElement,
  };
}

export class CrActionMenuElement extends CrLitElement {
  static get is() {
    return 'cr-action-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // Accessibility text of the menu. Should be something along the lines of
      // "actions", or "more actions".
      accessibilityLabel: {type: String},

      // Setting this flag will make the menu listen for content size changes
      // and reposition to its anchor accordingly.
      autoReposition: {type: Boolean},

      open: {
        type: Boolean,
        notify: true,
      },

      // Descriptor of the menu. Should be something along the lines of "menu"
      roleDescription: {type: String},
    };
  }

  accessibilityLabel?: string;
  autoReposition: boolean = false;
  open: boolean = false;
  roleDescription?: string;

  private boundClose_: (() => void)|null = null;
  private resizeObserver_: ResizeObserver|null = null;
  private hasMousemoveListener_: boolean = false;
  private anchorElement_: HTMLElement|null = null;
  private lastConfig_: ShowAtPositionConfig|null = null;

  override firstUpdated() {
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.addEventListener('mouseover', this.onMouseover_);
    this.addEventListener('click', this.onClick_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.removeListeners_();
  }

  /**
   * Exposing internal <dialog> elements for tests.
   */
  getDialog(): HTMLDialogElement {
    return this.$.dialog;
  }

  private removeListeners_() {
    window.removeEventListener('resize', this.boundClose_!);
    window.removeEventListener('popstate', this.boundClose_!);

    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
  }

  protected onNativeDialogClose_(e: Event) {
    // Ignore any 'close' events not fired directly by the <dialog> element.
    if (e.target !== this.$.dialog) {
      return;
    }

    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.fire('close');
  }

  private onClick_(e: Event) {
    if (e.target === this) {
      this.close();
      e.stopPropagation();
    }
  }

  private onKeyDown_(e: KeyboardEvent) {
    e.stopPropagation();

    if (e.key === 'Tab' || e.key === 'Escape') {
      this.close();
      if (e.key === 'Tab') {
        this.fire('tabkeyclose', {shiftKey: e.shiftKey});
      }
      e.preventDefault();
      return;
    }

    if (e.key !== 'Enter' && e.key !== 'ArrowUp' && e.key !== 'ArrowDown') {
      return;
    }

    const options = Array.from(
        this.querySelectorAll<HTMLElement>(SELECTABLE_DROPDOWN_ITEM_QUERY));
    if (options.length === 0) {
      return;
    }

    const focused = getDeepActiveElement();
    const index = options.findIndex(
        option => FocusRow.getFocusableElement(option) === focused);

    if (e.key === 'Enter') {
      // If a menu item has focus, don't change focus or close menu on 'Enter'.
      if (index !== -1) {
        return;
      }

      if (isWindows || isMac) {
        this.close();
        e.preventDefault();
        return;
      }
    }

    e.preventDefault();
    this.updateFocus_(options, index, e.key !== 'ArrowUp');

    if (!this.hasMousemoveListener_) {
      this.hasMousemoveListener_ = true;
      this.addEventListener('mousemove', e => {
        this.onMouseover_(e);
        this.hasMousemoveListener_ = false;
      }, {once: true});
    }
  }

  private onMouseover_(e: Event) {
    const item =
        (e.composedPath() as HTMLElement[])
            .find(
                el => el.matches && el.matches(SELECTABLE_DROPDOWN_ITEM_QUERY));
    (item || this.$.wrapper).focus();
  }

  private updateFocus_(
      options: HTMLElement[], focusedIndex: number, next: boolean) {
    const numOptions = options.length;
    assert(numOptions > 0);
    let index;
    if (focusedIndex === -1) {
      index = next ? 0 : numOptions - 1;
    } else {
      const delta = next ? 1 : -1;
      index = (numOptions + focusedIndex + delta) % numOptions;
    }
    options[index]!.focus();
  }

  close() {
    if (!this.open) {
      return;
    }

    // Removing 'resize' and 'popstate' listeners when dialog is closed.
    this.removeListeners_();
    this.$.dialog.close();
    this.open = false;
    if (this.anchorElement_) {
      assert(this.anchorElement_);
      focusWithoutInk(this.anchorElement_);
      this.anchorElement_ = null;
    }
    if (this.lastConfig_) {
      this.lastConfig_ = null;
    }
  }

  /**
   * Shows the menu anchored to the given element.
   */
  showAt(anchorElement: HTMLElement, config?: ShowAtConfig) {
    this.anchorElement_ = anchorElement;
    // Scroll the anchor element into view so that the bounding rect will be
    // accurate for where the menu should be shown.
    this.anchorElement_.scrollIntoViewIfNeeded();

    const rect = this.anchorElement_!.getBoundingClientRect();

    let height = rect.height;
    if (config && !config.noOffset &&
        config.anchorAlignmentY === AnchorAlignment.AFTER_END) {
      // When an action menu is positioned after the end of an element, the
      // action menu can appear too far away from the anchor element, typically
      // because anchors tend to have padding. So we offset the height a bit
      // so the menu shows up slightly closer to the content of anchor.
      height -= AFTER_END_OFFSET;
    }

    this.showAtPosition(Object.assign(
        {
          top: rect.top,
          left: rect.left,
          height: height,
          width: rect.width,
          // Default to anchoring towards the left.
          anchorAlignmentX: AnchorAlignment.BEFORE_END,
        },
        config));
    this.$.wrapper.focus();
  }

  /**
   * Shows the menu anchored to the given box. The anchor alignment is
   * specified as an X and Y alignment which represents a point in the anchor
   * where the menu will align to, which can have the menu either before or
   * after the given point in each axis. Center alignment places the center of
   * the menu in line with the center of the anchor. Coordinates are relative to
   * the top-left of the viewport.
   *
   *            y-start
   *         _____________
   *         |           |
   *         |           |
   *         |   CENTER  |
   * x-start |     x     | x-end
   *         |           |
   *         |anchor box |
   *         |___________|
   *
   *             y-end
   *
   * For example, aligning the menu to the inside of the top-right edge of
   * the anchor, extending towards the bottom-left would use a alignment of
   * (BEFORE_END, AFTER_START), whereas centering the menu below the bottom
   * edge of the anchor would use (CENTER, AFTER_END).
   */
  showAtPosition(config: ShowAtPositionConfig) {
    // Save the scroll position of the viewport.
    const doc = document.scrollingElement!;
    const scrollLeft = doc.scrollLeft;
    const scrollTop = doc.scrollTop;

    // Reset position so that layout isn't affected by the previous position,
    // and so that the dialog is positioned at the top-start corner of the
    // document.
    this.resetStyle_();
    this.$.dialog.showModal();
    this.open = true;

    config.top += scrollTop;
    config.left += scrollLeft;

    this.positionDialog_(Object.assign(
        {
          minX: scrollLeft,
          minY: scrollTop,
          maxX: scrollLeft + doc.clientWidth,
          maxY: scrollTop + doc.clientHeight,
        },
        config));

    // Restore the scroll position.
    doc.scrollTop = scrollTop;
    doc.scrollLeft = scrollLeft;
    this.addListeners_();

    // Focus the first selectable item.
    const openedByKey = FocusOutlineManager.forDocument(document).visible;
    if (openedByKey) {
      const firstSelectableItem =
          this.querySelector<HTMLElement>(SELECTABLE_DROPDOWN_ITEM_QUERY);
      if (firstSelectableItem) {
        requestAnimationFrame(() => {
          // Wait for the next animation frame for the dialog to become visible.
          firstSelectableItem.focus();
        });
      }
    }
  }

  private resetStyle_() {
    this.$.dialog.style.left = '';
    this.$.dialog.style.right = '';
    this.$.dialog.style.top = '0';
  }

  /**
   * Position the dialog using the coordinates in config. Coordinates are
   * relative to the top-left of the viewport when scrolled to (0, 0).
   */
  private positionDialog_(config: ShowAtPositionConfig) {
    this.lastConfig_ = config;
    const c = Object.assign(getDefaultShowConfig(), config);

    const top = c.top;
    const left = c.left;
    const bottom = top + c.height!;
    const right = left + c.width!;

    // Flip the X anchor in RTL.
    const rtl = getComputedStyle(this).direction === 'rtl';
    if (rtl) {
      c.anchorAlignmentX! *= -1;
    }

    const offsetWidth = this.$.dialog.offsetWidth;
    const menuLeft = getStartPointWithAnchor(
        left, right, offsetWidth, c.anchorAlignmentX!, c.minX!, c.maxX!);

    if (rtl) {
      const menuRight =
          document.scrollingElement!.clientWidth - menuLeft - offsetWidth;
      this.$.dialog.style.right = menuRight + 'px';
    } else {
      this.$.dialog.style.left = menuLeft + 'px';
    }

    const menuTop = getStartPointWithAnchor(
        top, bottom, this.$.dialog.offsetHeight, c.anchorAlignmentY!, c.minY!,
        c.maxY!);
    this.$.dialog.style.top = menuTop + 'px';
  }

  protected onSlotchange_() {
    for (const node of this.$.contentNode.assignedElements({flatten: true})) {
      if (node.classList.contains(DROPDOWN_ITEM_CLASS) &&
          !node.getAttribute('role')) {
        node.setAttribute('role', 'menuitem');
      }
    }
  }

  private addListeners_() {
    this.boundClose_ = this.boundClose_ || (() => {
                         if (this.$.dialog.open) {
                           this.close();
                         }
                       });
    window.addEventListener('resize', this.boundClose_);
    window.addEventListener('popstate', this.boundClose_);

    if (this.autoReposition) {
      this.resizeObserver_ = new ResizeObserver(() => {
        if (this.lastConfig_) {
          this.positionDialog_(this.lastConfig_);
          this.fire('cr-action-menu-repositioned');  // For easier testing.
        }
      });

      this.resizeObserver_.observe(this.$.dialog);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-action-menu': CrActionMenuElement;
  }
}

customElements.define(CrActionMenuElement.is, CrActionMenuElement);
