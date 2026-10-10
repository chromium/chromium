// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox_input.css.js';
import {getHtml} from './composebox_input.html.js';

const ZERO_SPACE_STRING: string = '\u200b';
export const NON_BREAKING_SPACE = '\u00A0';

export const CHIP_CLASS = 'aim-chip';
export const CHIP_LABEL_CLASS = 'chip-label';

export const CHIP_DATASET_ID = 'chipId';
export const CHIP_DATASET_TEXT = 'chipText';
export const CHIP_DATASET_EMOJI = 'chipEmoji';
export const CHIP_DATASET_ICON_URL = 'chipIconUrl';

let chipPolicy: Pick<TrustedTypePolicy, 'createHTML'>|undefined;

export const chipSanitizer = new Sanitizer({
  elements: [
    {
      name: 'span',
      attributes: [
        'class',
        'contenteditable',
        'tabindex',
        'title',
        'data-chip-id',
        'data-chip-text',
        'data-chip-emoji',
        'data-chip-icon-url',
      ],
    },
    {
      name: 'img',
      attributes: ['src', 'class', 'alt'],
    },
  ],
});

function ensureChipPolicy(): void {
  if (window.trustedTypes && !chipPolicy) {
    chipPolicy = window.trustedTypes.createPolicy('composebox-input', {
      createHTML: (untrustedHtml: string) => {
        const tempContainer = document.createElement('div');
        tempContainer.setHTML(untrustedHtml, {sanitizer: chipSanitizer});
        return tempContainer.innerHTML;
      },
    });
  }
}

export function getChipPolicyForTesting():
    Pick<TrustedTypePolicy, 'createHTML'>|undefined {
  ensureChipPolicy();
  return chipPolicy;
}

export interface ComposeboxChip {
  id?: string;
  text: string;
  description?: string;
  emoji?: string;
  iconUrl?: string;
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
      composeboxSkillsEnabled: {type: Boolean, reflect: true},
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
      hideCancel: {type: Boolean},
      isBackspacing_: {type: Boolean},
    };
  }

  accessor composeboxSkillsEnabled: boolean = false;
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
  accessor hideCancel: boolean = false;
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
  private heightLockGeneration_: number = 0;
  private isRtl_: boolean = false;

  get inputElement(): HTMLElement {
    return this.shadowRoot.querySelector<HTMLElement>('#input')!;
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
    this.heightLockGeneration_++;
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

    const inputEl = this.shadowRoot.querySelector<HTMLElement>('#input')!;

    if (changedProperties.has('input')) {
      if (this.composeboxSkillsEnabled) {
        const text = (this.input || '').replaceAll(NON_BREAKING_SPACE, ' ');
        const plainText = getPlainText(inputEl);
        if (text !== plainText && text.trim() !== plainText.trim()) {
          if (text === '') {
            inputEl.replaceChildren();
          } else {
            inputEl.innerText = text;
          }
          if (this.shadowRoot?.activeElement === inputEl) {
            setCaretToEnd(inputEl, this.shadowRoot);
          }
        }
      }
    }

    if (changedProperties.has('input') ||
        changedProperties.has('disableCaretColorAnimation')) {
      if (!this.input) {
        this.resetHeight();
      }
      this.updateMirrorAndCaret_();
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
        inputEl.style.minHeight = '';
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
    const input = this.shadowRoot.querySelector<HTMLElement>('#input')!;

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
    if (this.composeboxSkillsEnabled) {
      this.input =
          getPlainText(this.shadowRoot.querySelector<HTMLElement>('#input')!);
    } else {
      this.input = (e.target as HTMLTextAreaElement).value;
    }

    this.updateMirrorAndCaret_();
    this.dispatchEvent(new CustomEvent('input-input', {detail: e}));
  }

  protected onInputFocusin_(e: FocusEvent) {
    this.dispatchEvent(new CustomEvent('input-focusin', {detail: e}));
  }

  protected onInputCopy_(e: ClipboardEvent) {
    if (!this.composeboxSkillsEnabled || !e.clipboardData) {
      return;
    }
    const sel = this.shadowRoot.getSelection();
    if (!sel || sel.rangeCount === 0 || sel.isCollapsed) {
      return;
    }
    const range = sel.getRangeAt(0).cloneRange();

    // Expand range boundaries if they fall inside a chip element so the full
    // chip is copied.
    const startElement = range.startContainer instanceof Element ?
        range.startContainer :
        range.startContainer.parentElement;
    const startChip = startElement?.closest(`.${CHIP_CLASS}`);
    if (startChip) {
      range.setStartBefore(startChip);
    }
    const endElement = range.endContainer instanceof Element ?
        range.endContainer :
        range.endContainer.parentElement;
    const endChip = endElement?.closest(`.${CHIP_CLASS}`);
    if (endChip) {
      range.setEndAfter(endChip);
    }

    const fragment = range.cloneContents();
    const tempContainer = document.createElement('div');
    tempContainer.appendChild(fragment);

    const plainText = `${getPlainText(tempContainer).trimEnd()} `;
    const html =
        `${tempContainer.innerHTML.replace(/(&nbsp;|\s)+$/, '')}&nbsp;`;
    e.clipboardData.setData('text/plain', plainText);
    e.clipboardData.setData('text/html', html);
    e.preventDefault();
  }

  protected onInputCut_(e: ClipboardEvent) {
    this.onInputCopy_(e);
    if (e.defaultPrevented) {
      document.execCommand('delete');
    }
  }

  protected onInputPaste_(e: ClipboardEvent) {
    if (!this.composeboxSkillsEnabled || !e.clipboardData ||
        e.clipboardData.files.length > 0) {
      return;
    }
    const html = e.clipboardData.getData('text/html');
    // Only intercept paste when the clipboard HTML contains a chip; otherwise
    // let `contenteditable="plaintext-only"` handle plain text and external
    // rich text natively.
    if (!html || !html.includes(CHIP_CLASS)) {
      return;
    }

    e.preventDefault();
    this.insertHtmlWithChips_(html);
  }

  protected onCancelClick_(e: Event) {
    this.dispatchEvent(new CustomEvent('cancel-click', {detail: e}));
  }

  protected showSmartComposeInlineHint_(): boolean {
    // Don't show smart compose if there's no hint or the user is backspacing.
    if (!this.smartComposeInlineHint || this.isBackspacing_) {
      return false;
    }

    const cursorPosition = this.getSelectionEnd();
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
        const input = this.shadowRoot.querySelector<HTMLElement>('#input')!;
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
        const generation = this.heightLockGeneration_;
        requestAnimationFrame(() => {
          if (generation !== this.heightLockGeneration_) {
            return;
          }
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

  insertSkillChip(chip: ComposeboxChip) {
    const chipElement = createChipElement(chip);
    const rawHtml = `${chipElement.outerHTML}&nbsp;`;
    this.insertHtmlWithChips_(rawHtml);
  }

  private focusAndEnsureSelection_() {
    const input = this.shadowRoot.querySelector<HTMLElement>('#input')!;
    input.focus();
    const sel = this.shadowRoot.getSelection();
    const isSelectionInside =
        !!sel && sel.rangeCount > 0 && input.contains(sel.anchorNode);
    if (!isSelectionInside) {
      setCaretToEnd(input, this.shadowRoot);
    }
  }

  private insertHtmlWithChips_(rawHtml: string) {
    let trustedHtml: TrustedHTML|string = rawHtml;
    if (window.trustedTypes) {
      ensureChipPolicy();
      trustedHtml = chipPolicy!.createHTML(rawHtml);
    }
    this.focusAndEnsureSelection_();
    const input = this.shadowRoot.querySelector<HTMLElement>('#input')!;
    // Temporarily switch to 'true' so that Blink does not strip HTML tags
    // (such as .aim-chip spans) during execCommand('insertHTML'), while still
    // preserving the native undo/redo stack. Revert immediately to
    // 'plaintext-only' to keep user input and paste operations unformatted.
    input.contentEditable = 'true';
    document.execCommand('insertHTML', false, trustedHtml);
    input.contentEditable = 'plaintext-only';
  }

  private updateMirrorAndCaret_() {
    if (!this.disableCaretColorAnimation) {
      this.updateMirror_();
      this.updateCaret_();
    }
  }

  /**
   * Reconstructs the `#mirror` element's DOM spans to mirror each character and
   * chip in `#input` for caret anchor positioning.
   */
  private updateMirror_() {
    const mirror = this.shadowRoot.getElementById('mirror');
    if (!mirror) {
      return;
    }

    if (this.composeboxSkillsEnabled) {
      mirror.replaceChildren();

      if (this.input.length === 0) {
        appendCharSpan(ZERO_SPACE_STRING, mirror);
        return;
      }

      const input = this.shadowRoot.querySelector<HTMLElement>('#input')!;
      for (const node of input.childNodes) {
        if (node.nodeType === Node.TEXT_NODE) {
          const text = node.textContent || '';
          for (const char of text) {
            appendCharSpan(char, mirror);
          }
        } else if (
            node instanceof HTMLElement &&
            node.classList.contains(CHIP_CLASS)) {
          const chipText = node.dataset[CHIP_DATASET_TEXT] || '';
          const chipContainer = document.createElement('span');
          chipContainer.classList.add(CHIP_CLASS);
          for (const char of chipText) {
            appendCharSpan(char, chipContainer);
          }
          mirror.appendChild(chipContainer);
        }
      }
    } else {
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
  }

  private updateCaret_() {
    const caret = this.shadowRoot.getElementById('caret');
    const input = this.shadowRoot.querySelector<HTMLElement>('#input')!;
    const mirror = this.shadowRoot.getElementById('mirror');

    if (!caret || !input || !mirror) {
      return;
    }

    if (mirror.textContent?.length !== this.input.length) {
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
    const selectionEnd = this.getSelectionEnd();
    const atStart = selectionEnd === 0;
    const targetSpan = getTargetSpan(
        selectionEnd, this.input.length, mirror, this.isRtl_,
        this.composeboxSkillsEnabled);

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
    const firstSpan = this.composeboxSkillsEnabled ?
        mirror.querySelector<HTMLElement>(`span:not(.${CHIP_CLASS})`) :
        (mirror.firstChild as HTMLElement);
    if (firstSpan) {
      firstSpan.style.anchorName = '--cursor-char';
      this.anchoredSpan_ = firstSpan;
      caret.classList.add('at-start');
    }
  }

  resetHeight() {
    this.heightLockGeneration_++;
    this.lockedMinHeight_ = 0;
    this.shadowRoot.querySelector<HTMLElement>('#input')!.style.minHeight = '';
    const inputWrapper = this.shadowRoot.querySelector<HTMLElement>('#inputWrapper');
    if (inputWrapper) {
      inputWrapper.style.minHeight = '';
    }
  }

  getSelectionEnd(): number {
    const input = this.shadowRoot.querySelector<HTMLElement>('#input')!;
    const isFocused = this.shadowRoot?.activeElement === input;
    if (!isFocused) {
      return this.input ? this.input.length : 0;
    }
    if (this.composeboxSkillsEnabled) {
      return getCaretCharacterOffsetWithin(input, this.shadowRoot);
    }
    return (input as HTMLTextAreaElement).selectionEnd ??
        (this.input ? this.input.length : 0);
  }

  selectAll() {
    const input = this.shadowRoot.querySelector<HTMLElement>('#input')!;
    if (this.composeboxSkillsEnabled) {
      selectContents(input, this.shadowRoot);
    } else {
      (input as HTMLTextAreaElement).select();
    }
    if (!this.disableCaretColorAnimation) {
      this.updateCaret_();
    }
  }
}

function getCaretCharacterOffsetWithin(
    element: HTMLElement, shadowRoot?: ShadowRoot|null): number {
  let caretOffset = 0;
  const sel = shadowRoot?.getSelection() ?? window.getSelection();
  if (sel && sel.rangeCount > 0) {
    const range = sel.getRangeAt(0);
    try {
      const preCaretRange = range.cloneRange();
      preCaretRange.selectNodeContents(element);
      preCaretRange.setEnd(range.endContainer, range.endOffset);
      caretOffset = preCaretRange.toString().length;
    } catch {
      return 0;
    }
  }
  return caretOffset;
}

/**
 * Selects the contents of `element`, or places the caret at the end of it if
 * `collapseToEnd` is true.
 */
function selectContents(
    element: HTMLElement, shadowRoot?: ShadowRoot|null,
    collapseToEnd: boolean = false) {
  const sel = shadowRoot?.getSelection() ?? window.getSelection();
  if (sel) {
    const range = document.createRange();
    range.selectNodeContents(element);
    if (collapseToEnd) {
      range.collapse(false);
    }
    sel.removeAllRanges();
    sel.addRange(range);
  }
}

function setCaretToEnd(element: HTMLElement, shadowRoot?: ShadowRoot|null) {
  selectContents(element, shadowRoot, /* collapseToEnd= */ true);
}

function getTargetSpan(
    selectionEnd: number, textLength: number, mirror: Readonly<HTMLElement>,
    isRtl: boolean, composeboxSkillsEnabled: boolean): HTMLElement|null {
  const atStart = selectionEnd === 0;
  const atEnd = selectionEnd === textLength;

  if (composeboxSkillsEnabled) {
    if (atStart) {
      return mirror.querySelector<HTMLElement>(`span:not(.${CHIP_CLASS})`);
    }
    const allSpans =
        mirror.querySelectorAll<HTMLElement>(`span:not(.${CHIP_CLASS})`);
    if (isRtl && atEnd) {
      let targetSpan: HTMLElement|null = null;
      let minLeft = Infinity;
      for (const element of allSpans) {
        if (element.offsetLeft < minLeft) {
          minLeft = element.offsetLeft;
          targetSpan = element;
        }
      }
      return targetSpan;
    }
    if (allSpans.length > 0) {
      if (selectionEnd - 1 < allSpans.length) {
        return allSpans[selectionEnd - 1] as HTMLElement;
      }
      return allSpans[allSpans.length - 1] as HTMLElement;
    }
    return null;
  } else {
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
    return mirror.childNodes[selectionEnd - 1] as HTMLElement;
  }
}

/**
 * Creates a span mirroring a single character and appends it to `container`.
 * Newlines are followed by a zero-width space so that the line break is
 * measurable for caret positioning. Spaces are mirrored as-is.
 */
function appendCharSpan(char: string, container: Node) {
  const span = document.createElement('span');
  span.textContent = char === '\n' ? `\n${ZERO_SPACE_STRING}` : char;
  container.appendChild(span);
}

/**
 * Creates and returns a non-editable HTML span element representing a chip,
 * along with associated data attributes.
 */
export function createChipElement(chip: ComposeboxChip): HTMLElement {
  const chipEl = document.createElement('span');
  chipEl.classList.add(CHIP_CLASS);
  chipEl.setAttribute('contenteditable', 'false');
  chipEl.setAttribute('tabindex', '-1');
  chipEl.title = chip.description || chip.text;
  if (chip.id) {
    chipEl.dataset[CHIP_DATASET_ID] = chip.id;
  }
  chipEl.dataset[CHIP_DATASET_TEXT] = chip.text;
  if (chip.emoji) {
    chipEl.dataset[CHIP_DATASET_EMOJI] = chip.emoji;
  }
  if (chip.iconUrl) {
    chipEl.dataset[CHIP_DATASET_ICON_URL] = chip.iconUrl;
  }

  const textSpan = document.createElement('span');
  textSpan.classList.add(CHIP_LABEL_CLASS);
  textSpan.textContent = chip.text;
  chipEl.appendChild(textSpan);

  return chipEl;
}

/**
 * Traverses the DOM tree inside `container` recursively to extract plaintext,
 * converting `.aim-chip` elements to their plain text representation.
 */
function getPlainText(container: Node): string {
  let result = '';
  function traverse(node: Node) {
    if (node.nodeType === Node.TEXT_NODE) {
      result += node.textContent || '';
    } else if (
        node instanceof HTMLElement && node.classList.contains(CHIP_CLASS)) {
      result += node.dataset[CHIP_DATASET_TEXT] || '';
    } else {
      for (const child of node.childNodes) {
        traverse(child);
      }
    }
  }
  traverse(container);
  return result.replaceAll(NON_BREAKING_SPACE, ' ');
}

declare global {
  interface Document {
    execCommand(
        commandId: string, showUI?: boolean,
        value?: string|TrustedHTML): boolean;
  }

  interface SetHtmlOptions {
    sanitizer?: Sanitizer|SanitizerConfig;
  }

  interface Element {
    setHTML(html: string, options?: SetHtmlOptions): void;
  }

  interface HTMLElementTagNameMap {
    'cr-composebox-input': ComposeboxInputElement;
  }
}

customElements.define(ComposeboxInputElement.is, ComposeboxInputElement);
