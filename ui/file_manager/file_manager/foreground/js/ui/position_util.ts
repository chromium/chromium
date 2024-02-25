// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides utility functions for position popups.
 */

/**
 * Enum for defining how to anchor a popup to an anchor element.
 */
export enum AnchorType {
  /**
   * The popup's right edge is aligned with the left edge of the anchor.
   * The popup's top edge is aligned with the top edge of the anchor.
   */
  BEFORE = 1,  // p: right, a: left, p: top, a: top

  /**
   * The popop's left edge is aligned with the right edge of the anchor.
   * The popup's top edge is aligned with the top edge of the anchor.
   */
  AFTER = 2,  // p: left, a: right, p: top, a: top

  /**
   * The popop's bottom edge is aligned with the top edge of the anchor.
   * The popup's left edge is aligned with the left edge of the anchor.
   */
  ABOVE = 3,  // p: bottom, a: top, p: left, a: left

  /**
   * The popop's top edge is aligned with the bottom edge of the anchor.
   * The popup's left edge is aligned with the left edge of the anchor.
   */
  BELOW = 4,  // p: top, a: bottom, p: left, a: left
}

/**
 * Helper function for positionPopupAroundElement and positionPopupAroundRect.
 * @param anchorRect The rect for the anchor.
 * @param popupElement The element used for the popup.
 * @param type The type of anchoring to do.
 * @param invertLeftRight [Optional] Whether to invert the right/left
 *     alignment.
 */
function positionPopupAroundRect(
    anchorRect: DOMRect, popupElement: HTMLElement, type: AnchorType,
    invertLeftRight?: boolean) {
  const popupRect = popupElement.getBoundingClientRect();
  let availRect;
  const ownerDoc = popupElement.ownerDocument;
  const cs = ownerDoc.defaultView!.getComputedStyle(popupElement);
  const docElement = ownerDoc.documentElement;

  if (cs.position === 'fixed') {
    // For 'fixed' positioned popups, the available rectangle should be based
    // on the viewport rather than the document.
    availRect = {
      height: docElement.clientHeight,
      width: docElement.clientWidth,
      top: 0,
      bottom: docElement.clientHeight,
      left: 0,
      right: docElement.clientWidth,
    };
  } else {
    availRect = popupElement.offsetParent!.getBoundingClientRect();
  }

  if (cs.direction === 'rtl') {
    invertLeftRight = !invertLeftRight;
  }

  // Flip BEFORE, AFTER based on alignment.
  if (invertLeftRight) {
    if (type === AnchorType.BEFORE) {
      type = AnchorType.AFTER;
    } else if (type === AnchorType.AFTER) {
      type = AnchorType.BEFORE;
    }
  }

  // Flip type based on available size
  switch (type) {
    case AnchorType.BELOW:
      if (anchorRect.bottom + popupRect.height > availRect.height &&
          popupRect.height <= anchorRect.top) {
        type = AnchorType.ABOVE;
      }
      break;
    case AnchorType.ABOVE:
      if (popupRect.height > anchorRect.top &&
          anchorRect.bottom + popupRect.height <= availRect.height) {
        type = AnchorType.BELOW;
      }
      break;
    case AnchorType.AFTER:
      if (anchorRect.right + popupRect.width > availRect.width &&
          popupRect.width <= anchorRect.left) {
        type = AnchorType.BEFORE;
      }
      break;
    case AnchorType.BEFORE:
      if (popupRect.width > anchorRect.left &&
          anchorRect.right + popupRect.width <= availRect.width) {
        type = AnchorType.AFTER;
      }
      break;
  }
  // flipping done

  const style = popupElement.style;
  // Reset all directions.
  style.left = style.right = style.top = style.bottom = 'auto';

  // Primary direction
  switch (type) {
    case AnchorType.BELOW:
      if (anchorRect.bottom + popupRect.height <= availRect.height) {
        style.top = anchorRect.bottom + 'px';
      } else {
        style.bottom = '0';
      }
      break;
    case AnchorType.ABOVE:
      if (availRect.height - anchorRect.top >= 0) {
        style.bottom = availRect.height - anchorRect.top + 'px';
      } else {
        style.top = '0';
      }
      break;
    case AnchorType.AFTER:
      if (anchorRect.right + popupRect.width <= availRect.width) {
        style.left = anchorRect.right + 'px';
      } else {
        style.right = '0';
      }
      break;
    case AnchorType.BEFORE:
      if (availRect.width - anchorRect.left >= 0) {
        style.right = availRect.width - anchorRect.left + 'px';
      } else {
        style.left = '0';
      }
      break;
  }

  // Secondary direction
  switch (type) {
    case AnchorType.BELOW:
    case AnchorType.ABOVE:
      if (invertLeftRight) {
        // align right edges
        if (anchorRect.right - popupRect.width >= 0) {
          style.right = availRect.width - anchorRect.right + 'px';

          // align left edges
        } else if (anchorRect.left + popupRect.width <= availRect.width) {
          style.left = anchorRect.left + 'px';

          // not enough room on either side
        } else {
          style.right = '0';
        }
      } else {
        // align left edges
        if (anchorRect.left + popupRect.width <= availRect.width) {
          style.left = anchorRect.left + 'px';

          // align right edges
        } else if (anchorRect.right - popupRect.width >= 0) {
          style.right = availRect.width - anchorRect.right + 'px';

          // not enough room on either side
        } else {
          style.left = '0';
        }
      }
      break;

    case AnchorType.AFTER:
    case AnchorType.BEFORE:
      // align top edges
      if (anchorRect.top + popupRect.height <= availRect.height) {
        style.top = anchorRect.top + 'px';

        // align bottom edges
      } else if (anchorRect.bottom - popupRect.height >= 0) {
        style.bottom = availRect.height - anchorRect.bottom + 'px';

        // not enough room on either side
      } else {
        style.top = '0';
      }
      break;
  }
}

/**
 * Positions a popup element relative to an anchor element. The popup element
 * should have position set to absolute and it should be a child of the body
 * element.
 * @param anchorElement The element that the popup is anchored
 *     to.
 * @param popupElement The popup element we are positioning.
 * @param type The type of anchoring we want.
 * @param invertLeftRight [Optional] Whether to invert the right/left
 *     alignment.
 */
export function positionPopupAroundElement(
    anchorElement: HTMLElement, popupElement: HTMLElement, type: AnchorType,
    invertLeftRight?: boolean) {
  const anchorRect = anchorElement.getBoundingClientRect();
  positionPopupAroundRect(anchorRect, popupElement, type, !!invertLeftRight);
}

/**
 * Positions a popup around a point.
 * @param x The client x position.
 * @param y The client y position.
 * @param popupElement The popup element we are positioning.
 * @param anchorType [Optional] The type of anchoring we want.
 */
export function positionPopupAtPoint(
    x: number, y: number, popupElement: HTMLElement,
    anchorType: AnchorType = AnchorType.BELOW) {
  positionPopupAroundRect(new DOMRect(x, y, 0, 0), popupElement, anchorType);
}
