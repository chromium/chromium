// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox_input.css.js';
import {getHtml} from './composebox_input.html.js';

const ZERO_SPACE_STRING: string = '\u200b';

export interface ComposeboxInputElement {
  $: {
    cancelIcon: CrIconButtonElement,
    input: HTMLInputElement|HTMLTextAreaElement,
  };
}

export class ComposeboxInputElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'cr-composebox-input';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disableCaretColorAnimation: {type: Boolean, reflect: true},
      showDropdown: {type: Boolean},
      inputPlaceholder: {type: String},
      input: {type: String},
      smartComposeEnabled: {type: Boolean, reflect: true},
      smartComposeInlineHint: {type: String},
      isCollapsible: {type: Boolean, reflect: true},
      submitEnabled: {type: Boolean, reflect: true},
      entrypointName: {type: String, reflect: true},
      cancelButtonTitle: {type: String},
      isBackspacing_: {type: Boolean},
    };
  }

  accessor disableCaretColorAnimation: boolean = false;
  accessor showDropdown: boolean = false;
  accessor inputPlaceholder: string = '';
  accessor input: string = '';
  accessor smartComposeEnabled: boolean = false;
  accessor smartComposeInlineHint: string = '';
  accessor isCollapsible: boolean = false;
  accessor submitEnabled: boolean = false;
  accessor entrypointName: string = '';
  accessor cancelButtonTitle: string = '';
  accessor isBackspacing_: boolean = false;

  private resizeObserver_: ResizeObserver|null = null;
  private smartComposeResizeObserver_: ResizeObserver|null = null;
  private smartComposeHeightUpdateFrame_: number|null = null;
  private lastObservedInputWrapperWidth_: number = -1;
  private anchoredSpan_: HTMLElement|null = null;
  private measurementContext_: CanvasRenderingContext2D|null = null;
  private cachedPaddingLeft_: number = -1;
  private cachedPaddingRight_: number = -1;
  private cachedFont_: string = '';
  private cachedContentWidth_: number = -1;
  // Min height of the input wrapper so that it doesn't collapse when text is
  // deleted.
  private lockedMinHeight_: number = 0;
  private isRtl_: boolean = false;

  get inputElement(): HTMLInputElement|HTMLTextAreaElement {
    return this.$.input;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.updateDirection();
    this.setupResizeObservers_();
    this.smartComposeResizeObserver_ = new ResizeObserver(() => {
      this.scheduleSmartComposeHeightUpdate_();
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
    if (this.smartComposeResizeObserver_) {
      this.smartComposeResizeObserver_.disconnect();
      this.smartComposeResizeObserver_ = null;
    }
    if (this.smartComposeHeightUpdateFrame_ !== null) {
      cancelAnimationFrame(this.smartComposeHeightUpdateFrame_);
      this.smartComposeHeightUpdateFrame_ = null;
    }
    this.lastObservedInputWrapperWidth_ = -1;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('input') ||
        changedProperties.has('disableCaretColorAnimation')) {
      if (!this.disableCaretColorAnimation) {
        this.updateMirror_();
        this.updateCaret_();
      }
    }

    if (changedProperties.has('smartComposeInlineHint') ||
        changedProperties.has('isBackspacing_')) {
      if (this.showSmartComposeInlineHint_()) {
        const smartCompose =
            this.shadowRoot.querySelector<HTMLElement>('#smartCompose');
        if (smartCompose) {
          this.smartComposeResizeObserver_?.observe(smartCompose);
          this.scheduleSmartComposeHeightUpdate_();
        }
      } else {
        this.smartComposeResizeObserver_?.disconnect();
        if (this.smartComposeHeightUpdateFrame_ !== null) {
          cancelAnimationFrame(this.smartComposeHeightUpdateFrame_);
          this.smartComposeHeightUpdateFrame_ = null;
        }
        this.$.input.style.minHeight = '';
        if (this.smartComposeInlineHint) {
          this.dispatchEvent(new CustomEvent('clear-smart-compose'));
        }
      }
    }
  }

  private scheduleSmartComposeHeightUpdate_() {
    // Stale-callback guard at schedule-time: skip registration when the element
    // is detached or the hint already cleared. Avoids burning a frame slot for
    // work that the rAF callback would no-op on.
    if (!this.isConnected || !this.smartComposeInlineHint) {
      return;
    }
    if (this.smartComposeHeightUpdateFrame_ !== null) {
      return;
    }
    this.smartComposeHeightUpdateFrame_ = requestAnimationFrame(() => {
      this.smartComposeHeightUpdateFrame_ = null;
      // Stale-callback guard at run-time: state may have changed between
      // schedule and the rAF firing (e.g. hint cleared, element removed).
      // Re-check before touching DOM.
      if (!this.isConnected || !this.smartComposeInlineHint) {
        return;
      }
      this.updateSmartComposeHeight_();
    });
  }

  private updateSmartComposeHeight_() {
    const smartCompose =
        this.shadowRoot.querySelector<HTMLElement>('#smartCompose');
    if (!smartCompose) {
      return;
    }
    const input = this.$.input;

    // Convergence guard: short-circuit when the currently set inline
    // min-height already matches the rendered #smartCompose height.
    // browser_tests showed that writing #input.style.minHeight = feeds back
    // into the observed #smartCompose box, so repeated cross-frame
    // clear-and-set cycles can become observable churn event with the rAF
    // schedule. Skipping the clear/measure/write path when no change is needed
    // keeps the system at a fixed point.
    const currentSmartComposeHeight = smartCompose.scrollHeight;
    const desiredMinHeight = `${currentSmartComposeHeight}px`;
    if (input.style.minHeight === desiredMinHeight) {
      return;
    }

    const inputHeight = input.scrollHeight;
    const smartComposeHeight = smartCompose.scrollHeight;
    if (smartComposeHeight > inputHeight) {
      input.style.minHeight = `${smartComposeHeight}px`;
    }
  }

  protected onInputFocus_() {
    if (!this.disableCaretColorAnimation) {
      const caret = this.shadowRoot.getElementById('caret');
      if (caret) {
        caret.classList.add('caret-visible');
        this.updateCaret_();
      }
    }
  }

  protected onInputBlur_() {
    if (!this.disableCaretColorAnimation) {
      const caret = this.shadowRoot.getElementById('caret');
      if (caret) {
        caret.classList.remove('caret-visible');
      }
    }
  }

  protected onInputClick_(e: Event) {
    if (!this.disableCaretColorAnimation) {
      this.updateCaret_();
    }
    this.dispatchEvent(new CustomEvent('input-click', {detail: e}));
  }

  protected onInputKeydown_(e: KeyboardEvent) {
    if (e.key === 'Backspace') {
      this.isBackspacing_ = true;
    } else if (e.key.length === 1 || e.key === 'Enter') {
      this.isBackspacing_ = false;
    }
  }

  protected onInputKeyup_(e: KeyboardEvent) {
    if (!this.disableCaretColorAnimation) {
      this.updateCaret_();
    }
    this.dispatchEvent(new CustomEvent('input-keyup', {detail: e}));
  }

  protected onInputInput_(e: Event) {
    this.input = (e.target as HTMLInputElement).value;
    if (!this.disableCaretColorAnimation) {
      this.updateMirror_();
      this.updateCaret_();
    }
    this.dispatchEvent(new CustomEvent('input-input', {detail: e}));
  }

  protected onInputFocusin_(e: FocusEvent) {
    this.dispatchEvent(new CustomEvent('input-focusin', {detail: e}));
  }

  protected onCancelClick_(e: Event) {
    this.dispatchEvent(new CustomEvent('cancel-click', {detail: e}));
  }

  protected showSmartComposeInlineHint_(): boolean {
    // Don't show smart compose if there's no hint or the user is backspacing.
    if (!this.smartComposeInlineHint || this.isBackspacing_) {
      return false;
    }

    const cursorPosition = this.$.input.selectionEnd;
    // Don't show smart compose if the cursor is not at the end of the text.
    if (cursorPosition === null || cursorPosition !== this.input.length) {
      return false;
    }
    const charBefore = this.input.charAt(cursorPosition - 1);
    const isMidword =
        charBefore !== ' ' && !this.smartComposeInlineHint.startsWith(' ');
    // TODO(crbug.com/512817466): See if it is possible to return early at a
    // certain character length.
    if (isMidword) {
      if (!this.measurementContext_) {
        const canvas = document.createElement('canvas');
        this.measurementContext_ = canvas.getContext('2d');
      }
      const ctx = this.measurementContext_;
      if (ctx) {
        const input = this.$.input;
        // Cache the padding and width. Padding never changes and the width
        // don't change often. Calling calling `getComputedStyle` and
        // `clientWidth` can be expensive. This makes it so we are performing
        // purely mathematical calculations during keystrokes which is cheaper.
        // Width gets recomputed by resize observer below.
        if (this.cachedPaddingLeft_ === -1) {
          const style = window.getComputedStyle(input);
          this.cachedPaddingLeft_ = parseFloat(style.paddingLeft);
          this.cachedPaddingRight_ = parseFloat(style.paddingRight);
          this.cachedFont_ = style.font;
        }
        ctx.font = this.cachedFont_;

        if (this.cachedContentWidth_ === -1) {
          const inputWidth = input.clientWidth;
          this.cachedContentWidth_ =
              inputWidth - this.cachedPaddingLeft_ - this.cachedPaddingRight_;
        }
        const contentWidth = this.cachedContentWidth_;

        // Only care if the current word wraps onto the next line. If it does,
        // smart compose should be suppressed.
        const spaceIndex = this.smartComposeInlineHint.indexOf(' ');
        const currentHintWord = spaceIndex === -1 ?
            this.smartComposeInlineHint :
            this.smartComposeInlineHint.substring(0, spaceIndex);
        const fullTextWidth =
            ctx.measureText(this.input + currentHintWord).width;
        const inputTextWidth = ctx.measureText(this.input).width;

        if (contentWidth > 0) {
          const linesWithHint =
              Math.max(1, Math.ceil(fullTextWidth / contentWidth));
          const linesWithoutHint =
              Math.max(1, Math.ceil(inputTextWidth / contentWidth));

          if (linesWithHint > linesWithoutHint) {
            return false;
          }
        }
      }
    }
    return true;
  }

  // `widthResizeObserver_` is used to update the cached content width and the
  // caret position.
  // Recalculate the caret only when #inputWrapper's width changes.
  // The width guard skips height-only changes (e.g. field-sizing: content
  // growth Windows non-overlay scrollbar toggling) that would otherwise
  // feed back into a ResizeObserver loop.
  private setupResizeObservers_() {
    if (this.resizeObserver_) {
      return;
    }
    const inputWrapper = this.shadowRoot.getElementById('inputWrapper');
    if (!inputWrapper) {
      return;
    }

    this.lastObservedInputWrapperWidth_ = inputWrapper.clientWidth;
    this.resizeObserver_ = new ResizeObserver(() => {
      // Handle width changes.
      const currentWidth = inputWrapper.clientWidth;
      if (currentWidth !== this.lastObservedInputWrapperWidth_) {
        this.lastObservedInputWrapperWidth_ = currentWidth;
        this.cachedContentWidth_ = -1;  // Invalidate cache
        if (!this.disableCaretColorAnimation) {
          requestAnimationFrame(() => {
            this.updateCaret_();
          });
        }
      }

      // Handle height changes.
      const currentHeight = inputWrapper.clientHeight;
      if (currentHeight > this.lockedMinHeight_) {
        requestAnimationFrame(() => {
          if (currentHeight > this.lockedMinHeight_) {
            this.lockedMinHeight_ = currentHeight;
            inputWrapper.style.minHeight = `${currentHeight}px`;
          }
        });
      }
    });
    this.resizeObserver_.observe(inputWrapper);
  }

  // Update RTL status when the direction of the component changes.
  // Used mainly for testing to provide a way to update the RTL status.
  // The use of MutationObserver was considered but rejected since we do not
  // likely need responsiveness here.
  updateDirection() {
    this.isRtl_ = window.getComputedStyle(this).direction === 'rtl';
  }

  private updateMirror_() {
    const mirror = this.shadowRoot.getElementById('mirror');
    if (!mirror) {
      return;
    }

    mirror.textContent = '';
    const chars = this.input.split('');

    if (chars.length === 0) {
      const emptySpan = document.createElement('span');
      emptySpan.textContent = ZERO_SPACE_STRING;
      mirror.appendChild(emptySpan);
      return;
    }

    chars.forEach(char => {
      const span = document.createElement('span');
      if (char === ' ') {
        span.textContent = ' ';
      } else if (char === '\n') {
        span.textContent = `\n${ZERO_SPACE_STRING}`;
      } else {
        span.textContent = char;
      }
      mirror.appendChild(span);
    });
  }

  private updateCaret_() {
    const caret = this.shadowRoot.getElementById('caret');
    const input = this.$.input;
    const mirror = this.shadowRoot.getElementById('mirror');

    if (!caret || !input || !mirror) {
      return;
    }

    if (mirror.textContent?.length !== input.value.length) {
      this.updateMirror_();
    }

    // Restart the color-cycling animation on every update.
    caret.classList.remove('animating');
    void caret.offsetHeight;
    caret.classList.add('animating');

    // Clear anchor from the previously anchored span.
    if (this.anchoredSpan_) {
      this.anchoredSpan_.style.anchorName = '';
    }

    // Set anchor-name on the span at the cursor position.
    // CSS `position-anchor: --cursor-char` on #caret does the rest.
    const selectionEnd = (input as HTMLInputElement).selectionEnd;
    const atStart = selectionEnd === 0;
    const targetSpan =
        getTargetSpan(input as HTMLInputElement, mirror, this.isRtl_);

    if (targetSpan) {
      targetSpan.style.anchorName = '--cursor-char';
      this.anchoredSpan_ = targetSpan;
      caret.classList.toggle('at-start', atStart);
    }

  }

  resetCaret() {
    const caret = this.shadowRoot.getElementById('caret');
    const mirror = this.shadowRoot.getElementById('mirror');
    if (!caret || !mirror) {
      return;
    }

    this.updateMirror_();

    // Clear the previous anchor.
    if (this.anchoredSpan_) {
      this.anchoredSpan_.style.anchorName = '';
    }

    // Always anchor to the first span at the start position, regardless of the
    // current textarea selectionEnd. The parent calls this after clearing its
    // input property, but before the child's Lit render flushes,
    // so selectionEnd may still reflect the old cursor.
    const firstSpan = mirror.firstChild as HTMLElement;
    if (firstSpan) {
      firstSpan.style.anchorName = '--cursor-char';
      this.anchoredSpan_ = firstSpan;
      caret.classList.add('at-start');
    }
  }

  resetHeight() {
    this.lockedMinHeight_ = 0;
    this.$.input.style.minHeight = '';
    const inputWrapper = this.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    if (inputWrapper) {
      inputWrapper.style.minHeight = '';
    }
  }
}

function getTargetSpan(
    input: Readonly<HTMLInputElement|HTMLTextAreaElement>,
    mirror: Readonly<HTMLElement>, isRtl: boolean): HTMLElement|null {
  const selectionEnd = input.selectionEnd;
  const atStart = selectionEnd === 0;
  const atEnd = selectionEnd === input.value.length;

  if (atStart) {
    return mirror.firstChild as HTMLElement;
  }
  if (isRtl && atEnd) {
    let targetSpan: HTMLElement|null = null;
    let minLeft = Infinity;
    for (const child of mirror.children) {
      const element = child as HTMLElement;
      if (element.offsetLeft < minLeft) {
        minLeft = element.offsetLeft;
        targetSpan = element;
      }
    }
    return targetSpan;
  }
  return mirror.childNodes[selectionEnd! - 1] as HTMLElement;
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-input': ComposeboxInputElement;
  }
}

customElements.define(ComposeboxInputElement.is, ComposeboxInputElement);
