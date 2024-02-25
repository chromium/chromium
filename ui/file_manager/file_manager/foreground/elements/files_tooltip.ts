// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './files_tooltip.html.js';

export interface FilesTooltip {
  $: {label: HTMLDivElement, link: HTMLAnchorElement};
  showTimeout: number;
  hideTimeout: number;
}

/**
 * Files Tooltip.
 *
 * Adds target elements with addTarget or addTargets. Value of aria-label is
 * used as a label of the tooltip.
 *
 * Usage:
 * document.querySelector('files-tooltip').addTargets(
 *     document.querySelectorAll('[has-tooltip]'))
 */
export class FilesTooltip extends PolymerElement {
  static get is() {
    return 'files-tooltip';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Delay for showing the tooltip in milliseconds.
       */
      showTimeout: {
        type: Number,
        value: 500,  // ms
        readOnly: true,
      },

      /**
       * Delay for hiding the tooltip in milliseconds.
       */
      hideTimeout: {
        type: Number,
        value: 500,  // ms
        readOnly: true,
      },
    };
  }

  private visibleTooltipTarget_?: HTMLElement;
  private upcomingTooltipTarget_?: HTMLElement;
  private showTooltipTimerId_: number = 0;
  private hideTooltipTimerId_: number = 0;

  override connectedCallback() {
    super.connectedCallback();
    document.body.addEventListener('mousedown', this.onDocumentMouseDown_);
    window.addEventListener('resize', this.onDocumentMouseDown_);
    this.addEventListener('transitionend', this.onTransitionEnd_);
    this.addEventListener('mouseover', this.onMouseOver_);
    this.addEventListener('mouseout', this.onMouseOut_);
  }

  /**
   * Adds targets to tooltip.
   */
  addTargets(targets: NodeList|HTMLElement[]) {
    for (let i = 0; i < targets.length; i++) {
      this.addTarget(targets[i]!);
    }
  }

  /**
   * Adds a target to tooltip.
   */
  addTarget(target: HTMLElement|Node) {
    target.addEventListener('mouseover', this.onMouseOver_);
    target.addEventListener('mouseout', this.onMouseOut_);
    target.addEventListener('focus', this.onFocus_);
    target.addEventListener('blur', this.onBlur_);
  }

  /**
   * Hides currently visible tooltip if there is. In some cases, mouseout event
   * is not dispatched. This method is used to handle these cases manually.
   */
  hideTooltip() {
    if (this.showTooltipTimerId_) {
      clearTimeout(this.showTooltipTimerId_);
    }
    if (this.visibleTooltipTarget_) {
      this.initHidingTooltip_(this.visibleTooltipTarget_);
    }
  }

  /**
   * Update the tooltip text with the passed-in target.
   */
  updateTooltipText(target: HTMLElement) {
    this.initShowingTooltip_(target);
  }

  private initShowingTooltip_(target: HTMLElement) {
    // Some tooltip is already visible.
    if (this.visibleTooltipTarget_) {
      if (this.hideTooltipTimerId_) {
        clearTimeout(this.hideTooltipTimerId_);
        this.hideTooltipTimerId_ = 0;
      }
    }

    // Even the current target is the visible tooltip target, we still need to
    // check if the label is different from the existing tooltip text, because
    // if label text changes, we need to show the tooltip.
    if (this.visibleTooltipTarget_ === target &&
        target.hasAttribute('aria-label') &&
        this.$.label.textContent === target.getAttribute('aria-label')) {
      return;
    }

    this.upcomingTooltipTarget_ = target;
    if (this.showTooltipTimerId_) {
      clearTimeout(this.showTooltipTimerId_);
    }
    this.showTooltipTimerId_ = setTimeout(
        this.showTooltip_.bind(this, target),
        this.visibleTooltipTarget_ ? 0 : this.showTimeout);
  }

  private initHidingTooltip_(target: HTMLElement) {
    // The tooltip is not visible.
    if (this.visibleTooltipTarget_ !== target) {
      if (this.upcomingTooltipTarget_ === target) {
        clearTimeout(this.showTooltipTimerId_);
        this.showTooltipTimerId_ = 0;
      }
      return;
    }

    if (this.hideTooltipTimerId_) {
      clearTimeout(this.hideTooltipTimerId_);
    }
    this.hideTooltipTimerId_ =
        setTimeout(this.hideTooltip_.bind(this), this.hideTimeout);
  }

  private showTooltip_(target: HTMLElement) {
    if (this.showTooltipTimerId_) {
      clearTimeout(this.showTooltipTimerId_);
      this.showTooltipTimerId_ = 0;
    }

    this.visibleTooltipTarget_ = target;

    const useCardTooltip = target.hasAttribute('show-card-tooltip');
    const useLinkTooltip = target.dataset['tooltipLinkHref'] &&
        target.dataset['tooltipLinkAriaLabel'] &&
        target.dataset['tooltipLinkText'];

    const windowEdgePadding = 6;

    const label = target.getAttribute('aria-label');
    if (!label) {
      return;
    }

    this.$.label.textContent = label;
    if (useLinkTooltip) {
      this.classList.add('link-tooltip');
      this.$.link.setAttribute('href', target.dataset['tooltipLinkHref']!);
      this.$.link.setAttribute(
          'aria-label', target.dataset['tooltipLinkAriaLabel']!);
      this.$.link.textContent = target.dataset['tooltipLinkText']!;
      this.$.link.setAttribute('aria-hidden', 'false');
      this.$.link.classList.add('link-label');
    } else {
      this.cleanupLinkTooltip_();
    }

    const invert = 'invert-tooltip';
    this.$.label.toggleAttribute('invert', target.hasAttribute(invert));

    const rect = target.getBoundingClientRect();

    let top = rect.top + rect.height;
    if (!useCardTooltip) {
      top += 8;
    }

    if (top + this.offsetHeight > document.body.offsetHeight) {
      top = rect.top - this.offsetHeight;
    }

    this.style.top = `${Math.round(top)}px`;

    let left;

    if (useCardTooltip) {
      this.classList.add('card-tooltip');
      this.$.label.classList.add('card-label');

      // Push left to the body's left when tooltip is longer than viewport.
      if (this.offsetWidth > document.body.offsetWidth) {
        left = 0;
      } else if (document.dir === 'rtl') {
        // Calculate position for rtl mode to align to the right of target.
        const width = this.getBoundingClientRect().width;
        const minLeft = rect.right - width;

        // The tooltip remains inside viewport if right align push it outside.
        left = Math.max(minLeft, 0);
      } else {
        // The tooltip remains inside viewport if left align push it outside.
        let maxLeft = document.body.offsetWidth - this.offsetWidth;
        maxLeft = Math.max(0, maxLeft);

        // Stick to the body's right if it goes outside viewport from right.
        left = Math.min(rect.left, maxLeft);
      }
    } else {
      // Clearing out style in case card-tooltip displayed previously.
      this.cleanupCardTooltip_();

      left = rect.left + rect.width / 2 - this.offsetWidth / 2;
      if (left < windowEdgePadding) {
        left = windowEdgePadding;
      }

      const maxLeft =
          document.body.offsetWidth - this.offsetWidth - windowEdgePadding;
      if (left > maxLeft) {
        left = maxLeft;
      }
    }

    left = Math.round(left);
    this.style.left = `${left}px`;

    this.setAttribute('aria-hidden', 'false');
    this.setAttribute('visible', 'true');
  }

  private hideTooltip_() {
    if (this.hideTooltipTimerId_) {
      clearTimeout(this.hideTooltipTimerId_);
      this.hideTooltipTimerId_ = 0;
    }

    this.visibleTooltipTarget_ = undefined;
    this.removeAttribute('visible');
    this.setAttribute('aria-hidden', 'true');
  }

  private onMouseOver_ = (event: Event) => {
    const actualTarget = event?.currentTarget || this.visibleTooltipTarget_;
    if (actualTarget) {
      this.initShowingTooltip_(actualTarget as HTMLElement);
    }
  };

  private onMouseOut_ = (event: Event) => {
    const actualTarget = event?.currentTarget || this.visibleTooltipTarget_;
    if (actualTarget) {
      this.initHidingTooltip_(actualTarget as HTMLElement);
    }
  };

  private onFocus_ = (event: Event) => {
    this.initShowingTooltip_(event.currentTarget as HTMLElement);
  };

  private onBlur_ = (event: Event) => {
    this.initHidingTooltip_(event.currentTarget as HTMLElement);
  };

  private onDocumentMouseDown_ = () => {
    this.hideTooltip_();

    // Additionally prevent any scheduled tooltips from showing up.
    if (this.showTooltipTimerId_) {
      clearTimeout(this.showTooltipTimerId_);
      this.showTooltipTimerId_ = 0;
    }
  };

  private onTransitionEnd_ = () => {
    // Clear card and link tooltip.
    if (!this.hasAttribute('visible')) {
      this.cleanupLinkTooltip_();
      this.cleanupCardTooltip_();
    }
  };

  /**
   * Clear card tooltip styles to prevent overwriting normal tooltip rules.
   */
  private cleanupCardTooltip_() {
    this.classList.remove('card-tooltip');
    this.$.label.className = '';
  }

  /**
   * Clear link tooltip styles to prevent overwriting normal tooltip rules.
   */
  private cleanupLinkTooltip_() {
    this.$.link.setAttribute('href', '#');
    this.$.link.removeAttribute('aria-label');
    this.$.link.setAttribute('aria-hidden', 'true');
    this.$.link.textContent = '';
    this.$.link.classList.remove('link-label');
    this.classList.remove('link-tooltip');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'files-tooltip': FilesTooltip;
  }
}

customElements.define(FilesTooltip.is, FilesTooltip);

// #
// sourceURL=//ui/file_manager/file_manager/foreground/elements/files_tooltip.ts
