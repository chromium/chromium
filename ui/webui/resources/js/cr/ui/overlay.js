// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides dialog-like behaviors for the tracing UI.
 */
cr.define('cr.ui.overlay', function() {
  /**
   * Gets the top, visible overlay. It makes the assumption that if multiple
   * overlays are visible, the last in the byte order is topmost.
   * TODO(estade): rely on aria-visibility instead?
   * @return {HTMLElement} The overlay.
   */
  function getTopOverlay() {
    const overlays = /** @type !NodeList<!HTMLElement> */ (
        document.querySelectorAll('.overlay:not([hidden])'));
    return overlays[overlays.length - 1];
  }

  /**
   * Returns a visible default button of the overlay, if it has one. If the
   * overlay has more than one, the first one will be returned.
   *
   * @param {HTMLElement} overlay The .overlay.
   * @return {HTMLElement} The default button.
   */
  function getDefaultButton(overlay) {
    function isHidden(node) {
      return node.hidden;
    }
    const defaultButtons = /** @type !NodeList<!HTMLElement> */ (
        overlay.querySelectorAll('.page .button-strip > .default-button'));
    for (let i = 0; i < defaultButtons.length; i++) {
      if (!findAncestor(defaultButtons[i], isHidden)) {
        return defaultButtons[i];
      }
    }
    return null;
  }

  /** @type {boolean} */
  let globallyInitialized = false;

  /**
   * Makes initializations which must hook at the document level.
   */
  function globalInitialization() {
    if (!globallyInitialized) {
      document.addEventListener('keydown', function(e) {
        const overlay = getTopOverlay();
        if (!overlay) {
          return;
        }

        // Close the overlay on escape.
        if (e.key == 'Escape') {
          cr.dispatchSimpleEvent(overlay, 'cancelOverlay');
        }

        // Execute the overlay's default button on enter, unless focus is on an
        // element that has standard behavior for the enter key.
        const forbiddenTagNames = /^(A|BUTTON|SELECT|TEXTAREA)$/;
        if (e.key == 'Enter' &&
            !forbiddenTagNames.test(document.activeElement.tagName)) {
          const button = getDefaultButton(overlay);
          if (button) {
            button.click();
            // Executing the default button may result in focus moving to a
            // different button. Calling preventDefault is necessary to not have
            // that button execute as well.
            e.preventDefault();
          }
        }
      });

      window.addEventListener('resize', setMaxHeightAllPages);
      globallyInitialized = true;
    }

    setMaxHeightAllPages();
  }

  /**
   * Sets the max-height of all pages in all overlays, based on the window
   * height.
   */
  function setMaxHeightAllPages() {
    const pages =
        document.querySelectorAll('.overlay .page:not(.not-resizable)');

    const maxHeight = Math.min(0.9 * window.innerHeight, 640) + 'px';
    for (let i = 0; i < pages.length; i++) {
      pages[i].style.maxHeight = maxHeight;
    }
  }

  /**
   * Adds behavioral hooks for the given overlay.
   * @param {HTMLElement} overlay The .overlay.
   *
   * TODO(crbug.com/425829): This function makes use of deprecated getter or
   * setter functions.
   * @suppress {deprecated}
   */
  function setupOverlay(overlay) {
    // Close the overlay on clicking any of the pages' close buttons.
    const closeButtons = overlay.querySelectorAll('.page > .close-button');
    for (let i = 0; i < closeButtons.length; i++) {
      closeButtons[i].addEventListener('click', function(e) {
        if (cr.ui.FocusOutlineManager) {
          cr.ui.FocusOutlineManager.forDocument(document).updateVisibility();
        }
        cr.dispatchSimpleEvent(overlay, 'cancelOverlay');
      });
    }

    // TODO(crbug.com/425829): Remove above suppression once we no longer use
    // deprecated functions defineSetter, and defineGetter.
    // Remove the 'pulse' animation any time the overlay is hidden or shown.
    // eslint-disable-next-line no-restricted-properties
    overlay.__defineSetter__('hidden', function(value) {
      this.classList.remove('pulse');
      if (value) {
        this.setAttribute('hidden', true);
      } else {
        this.removeAttribute('hidden');
      }
    });
    // eslint-disable-next-line no-restricted-properties
    overlay.__defineGetter__('hidden', function() {
      return this.hasAttribute('hidden');
    });

    // Shake when the user clicks away.
    overlay.addEventListener('click', function(e) {
      // Only pulse if the overlay was the target of the click.
      if (this != e.target) {
        return;
      }

      // This may be null while the overlay is closing.
      const overlayPage = this.querySelector('.page:not([hidden])');
      if (overlayPage) {
        overlayPage.classList.add('pulse');
      }
    });
    overlay.addEventListener('animationend', function(e) {
      e.target.classList.remove('pulse');
    });
  }

  return {
    getDefaultButton: getDefaultButton,
    globalInitialization: globalInitialization,
    setupOverlay: setupOverlay,
  };
});
