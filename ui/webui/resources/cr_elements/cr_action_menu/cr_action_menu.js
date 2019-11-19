// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   top: (number|undefined),
 *   left: (number|undefined),
 *   width: (number|undefined),
 *   height: (number|undefined),
 *   anchorAlignmentX: (number|undefined),
 *   anchorAlignmentY: (number|undefined),
 *   minX: (number|undefined),
 *   minY: (number|undefined),
 *   maxX: (number|undefined),
 *   maxY: (number|undefined),
 * }}
 */
let ShowAtConfig;

/**
 * @typedef {{
 *   top: number,
 *   left: number,
 *   width: (number|undefined),
 *   height: (number|undefined),
 *   anchorAlignmentX: (number|undefined),
 *   anchorAlignmentY: (number|undefined),
 *   minX: (number|undefined),
 *   minY: (number|undefined),
 *   maxX: (number|undefined),
 *   maxY: (number|undefined),
 * }}
 */
let ShowAtPositionConfig;

/**
 * @enum {number}
 * @const
 */
/* #export */ const AnchorAlignment = {
  BEFORE_START: -2,
  AFTER_START: -1,
  CENTER: 0,
  BEFORE_END: 1,
  AFTER_END: 2,
};

/** @const {string} */
const DROPDOWN_ITEM_CLASS = 'dropdown-item';

(function() {

/** @const {number} */
const AFTER_END_OFFSET = 10;

/**
 * Returns the point to start along the X or Y axis given a start and end
 * point to anchor to, the length of the target and the direction to anchor
 * in. If honoring the anchor would force the menu outside of min/max, this
 * will ignore the anchor position and try to keep the menu within min/max.
 * @private
 * @param {number} start
 * @param {number} end
 * @param {number} menuLength
 * @param {AnchorAlignment} anchorAlignment
 * @param {number} min
 * @param {number} max
 * @return {number}
 */
function getStartPointWithAnchor(
    start, end, menuLength, anchorAlignment, min, max) {
  let startPoint = 0;
  switch (anchorAlignment) {
    case AnchorAlignment.BEFORE_START:
      startPoint = -menuLength;
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

/**
 * @private
 * @return {!ShowAtPositionConfig}
 */
function getDefaultShowConfig() {
  const doc = document.scrollingElement;
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

Polymer({
  is: 'cr-action-menu',

  /**
   * The element which the action menu will be anchored to. Also the element
   * where focus will be returned after the menu is closed. Only populated if
   * menu is opened with showAt().
   * @private {?Element}
   */
  anchorElement_: null,

  /**
   * Bound reference to an event listener function such that it can be removed
   * on detach.
   * @private {?Function}
   */
  boundClose_: null,

  /** @private {boolean} */
  hasMousemoveListener_: false,

  /** @private {?PolymerDomApi.ObserveHandle} */
  contentObserver_: null,

  /** @private {?ResizeObserver} */
  resizeObserver_: null,

  /** @private {?ShowAtPositionConfig} */
  lastConfig_: null,

  properties: {
    // Setting this flag will make the menu listen for content size changes and
    // reposition to its anchor accordingly.
    autoReposition: {
      type: Boolean,
      value: false,
    },

    open: {
      type: Boolean,
      value: false,
    },

    /* Descriptor of the menu. Should be something along the lines of "menu" */
    roleDescription: String,
  },

  listeners: {
    'keydown': 'onKeyDown_',
    'mouseover': 'onMouseover_',
    'click': 'onClick_',
  },

  /** override */
  detached: function() {
    this.removeListeners_();
  },

  /**
   * Exposing internal <dialog> elements for tests.
   * @return {!HTMLDialogElement}
   */
  getDialog: function() {
    return /** @type {!HTMLDialogElement} */ (this.$.dialog);
  },

  /** @private */
  removeListeners_: function() {
    window.removeEventListener('resize', this.boundClose_);
    window.removeEventListener('popstate', this.boundClose_);
    if (this.contentObserver_) {
      Polymer.dom(this.$.contentNode).unobserveNodes(this.contentObserver_);
      this.contentObserver_ = null;
    }

    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onNativeDialogClose_: function(e) {
    // Ignore any 'close' events not fired directly by the <dialog> element.
    if (e.target !== this.$.dialog) {
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
  onClick_: function(e) {
    if (e.target == this) {
      this.close();
      e.stopPropagation();
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_: function(e) {
    e.stopPropagation();
    if (e.key == 'Tab' || e.key == 'Escape') {
      this.close();
      e.preventDefault();
      return;
    }

    if (e.key != 'Enter' && e.key != 'ArrowUp' && e.key != 'ArrowDown') {
      return;
    }

    const query = '.dropdown-item:not([disabled]):not([hidden])';
    const options = Array.from(this.querySelectorAll(query));
    if (options.length == 0) {
      return;
    }

    const focused = getDeepActiveElement();
    const index = options.findIndex(
        option => cr.ui.FocusRow.getFocusableElement(option) == focused);

    if (e.key == 'Enter') {
      // If a menu item has focus, don't change focus or close menu on 'Enter'.
      if (index != -1) {
        return;
      }

      if (cr.isWindows || cr.isMac) {
        this.close();
        e.preventDefault();
        return;
      }
    }

    e.preventDefault();
    this.updateFocus_(options, index, e.key != 'ArrowUp');

    if (!this.hasMousemoveListener_) {
      this.hasMousemoveListener_ = true;
      this.addEventListener('mousemove', e => {
        this.onMouseover_(e);
        this.hasMousemoveListener_ = false;
      }, {once: true});
    }
  },

  /**
   * @param {!Event} e
   * @private
   */
  onMouseover_: function(e) {
    const query = '.dropdown-item:not([disabled])';
    const item = e.composedPath().find(el => el.matches && el.matches(query));
    (item || this.$.wrapper).focus();
  },

  /**
   * @param {!Array<!HTMLElement>} options
   * @param {number} focusedIndex
   * @param {boolean} next
   * @private
   */
  updateFocus_: function(options, focusedIndex, next) {
    const numOptions = options.length;
    assert(numOptions > 0);
    let index;
    if (focusedIndex == -1) {
      index = next ? 0 : numOptions - 1;
    } else {
      const delta = next ? 1 : -1;
      index = (numOptions + focusedIndex + delta) % numOptions;
    }
    options[index].focus();
  },

  close: function() {
    // Removing 'resize' and 'popstate' listeners when dialog is closed.
    this.removeListeners_();
    this.$.dialog.close();
    this.open = false;
    if (this.anchorElement_) {
      cr.ui.focusWithoutInk(assert(this.anchorElement_));
      this.anchorElement_ = null;
    }
    if (this.lastConfig_) {
      this.lastConfig_ = null;
    }
  },

  /**
   * Shows the menu anchored to the given element.
   * @param {!Element} anchorElement
   * @param {ShowAtConfig=} opt_config
   */
  showAt: function(anchorElement, opt_config) {
    this.anchorElement_ = anchorElement;
    // Scroll the anchor element into view so that the bounding rect will be
    // accurate for where the menu should be shown.
    this.anchorElement_.scrollIntoViewIfNeeded();

    const rect = this.anchorElement_.getBoundingClientRect();

    let height = rect.height;
    if (opt_config &&
        opt_config.anchorAlignmentY == AnchorAlignment.AFTER_END) {
      // When an action menu is positioned after the end of an element, the
      // action menu can appear too far away from the anchor element, typically
      // because anchors tend to have padding. So we offset the height a bit
      // so the menu shows up slightly closer to the content of anchor.
      height -= AFTER_END_OFFSET;
    }

    this.showAtPosition(/** @type {ShowAtPositionConfig} */ (Object.assign(
        {
          top: rect.top,
          left: rect.left,
          height: height,
          width: rect.width,
          // Default to anchoring towards the left.
          anchorAlignmentX: AnchorAlignment.BEFORE_END,
        },
        opt_config)));
    this.$.wrapper.focus();
  },

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
   *
   * @param {!ShowAtPositionConfig} config
   */
  showAtPosition: function(config) {
    // Save the scroll position of the viewport.
    const doc = document.scrollingElement;
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

    this.positionDialog_(/** @type {ShowAtPositionConfig} */ (Object.assign(
        {
          minX: scrollLeft,
          minY: scrollTop,
          maxX: scrollLeft + doc.clientWidth,
          maxY: scrollTop + doc.clientHeight,
        },
        config)));

    // Restore the scroll position.
    doc.scrollTop = scrollTop;
    doc.scrollLeft = scrollLeft;
    this.addListeners_();
  },

  /** @private */
  resetStyle_: function() {
    this.$.dialog.style.left = '';
    this.$.dialog.style.right = '';
    this.$.dialog.style.top = '0';
  },

  /**
   * Position the dialog using the coordinates in config. Coordinates are
   * relative to the top-left of the viewport when scrolled to (0, 0).
   * @param {!ShowAtPositionConfig} config
   * @private
   */
  positionDialog_: function(config) {
    this.lastConfig_ = config;
    const c = Object.assign(getDefaultShowConfig(), config);

    const top = c.top;
    const left = c.left;
    const bottom = top + c.height;
    const right = left + c.width;

    // Flip the X anchor in RTL.
    const rtl = getComputedStyle(this).direction == 'rtl';
    if (rtl) {
      c.anchorAlignmentX *= -1;
    }

    const offsetWidth = this.$.dialog.offsetWidth;
    const menuLeft = getStartPointWithAnchor(
        left, right, offsetWidth, c.anchorAlignmentX, c.minX, c.maxX);

    if (rtl) {
      const menuRight =
          document.scrollingElement.clientWidth - menuLeft - offsetWidth;
      this.$.dialog.style.right = menuRight + 'px';
    } else {
      this.$.dialog.style.left = menuLeft + 'px';
    }

    const menuTop = getStartPointWithAnchor(
        top, bottom, this.$.dialog.offsetHeight, c.anchorAlignmentY, c.minY,
        c.maxY);
    this.$.dialog.style.top = menuTop + 'px';
  },

  /**
   * @private
   */
  addListeners_: function() {
    this.boundClose_ = this.boundClose_ || function() {
      if (this.$.dialog.open) {
        this.close();
      }
    }.bind(this);
    window.addEventListener('resize', this.boundClose_);
    window.addEventListener('popstate', this.boundClose_);

    this.contentObserver_ =
        Polymer.dom(this.$.contentNode).observeNodes((info) => {
          info.addedNodes.forEach((node) => {
            if (node.classList &&
                node.classList.contains(DROPDOWN_ITEM_CLASS) &&
                !node.getAttribute('role')) {
              node.setAttribute('role', 'menuitem');
            }
          });
        });

    if (this.autoReposition) {
      this.resizeObserver_ = new ResizeObserver(() => {
        if (this.lastConfig_) {
          this.positionDialog_(this.lastConfig_);
          this.fire('cr-action-menu-repositioned');  // For easier testing.
        }
      });

      this.resizeObserver_.observe(this.$.dialog);
    }
  },
});
})();
