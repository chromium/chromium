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
      smartComposeInlineHint: {type: String},
      isCollapsible: {type: Boolean, reflect: true},
      submitEnabled: {type: Boolean, reflect: true},
      entrypointName: {type: String, reflect: true},
      cancelButtonTitle: {type: String},
    };
  }

  accessor disableCaretColorAnimation: boolean = false;
  accessor showDropdown: boolean = false;
  accessor inputPlaceholder: string = '';
  accessor input: string = '';
  accessor smartComposeInlineHint: string = '';
  accessor isCollapsible: boolean = false;
  accessor submitEnabled: boolean = false;
  accessor entrypointName: string = '';
  accessor cancelButtonTitle: string = '';

  private caretResizeObserver_: ResizeObserver|null = null;
  private lastObservedInputWrapperWidth_: number = -1;
  private isRtl_: boolean = document.documentElement.dir === 'rtl';

  get inputElement(): HTMLInputElement|HTMLTextAreaElement {
    return this.$.input;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setupCaretResizeObserver_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.caretResizeObserver_) {
      this.caretResizeObserver_.disconnect();
      this.caretResizeObserver_ = null;
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

    if (changedProperties.has('smartComposeInlineHint')) {
      if (this.smartComposeInlineHint) {
        this.adjustInputForSmartCompose();
      } else {
        this.$.input.style.height = '';
        this.$.input.style.minHeight = '';
        const smartCompose = this.shadowRoot.getElementById('smartCompose');
        if (smartCompose) {
          smartCompose.style.minHeight = '';
        }
      }
    }
  }

  adjustInputForSmartCompose() {
    const smartCompose = this.shadowRoot.getElementById('smartCompose');
    if (!smartCompose) {
      return;
    }

    const ghostHeight = smartCompose.scrollHeight;
    if (ghostHeight > 48) {
      this.$.input.style.minHeight = `68px`;
      smartCompose.style.minHeight = `68px`;
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

  // Recalculate the caret only when #inputWrapper's width changes.
  // The width guard skips height-only changes (e.g. field-sizing: content
  // growth Windows non-overlay scrollbar toggling) that would otherwise
  // feed back into a ResizeObserver loop.
  private setupCaretResizeObserver_() {
    if (this.disableCaretColorAnimation) {
      return;
    }

    const inputWrapper = this.shadowRoot.getElementById('inputWrapper');
    if (!inputWrapper) {
      return;
    }

    this.lastObservedInputWrapperWidth_ = inputWrapper.clientWidth;
    this.caretResizeObserver_ = new ResizeObserver(() => {
      const currentWidth = inputWrapper.clientWidth;
      if (currentWidth === this.lastObservedInputWrapperWidth_) {
        return;
      }
      this.lastObservedInputWrapperWidth_ = currentWidth;
      requestAnimationFrame(() => {
        this.updateCaret_();
      });
    });
    this.caretResizeObserver_.observe(inputWrapper);
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

    if (chars.length === 0) {
      mirror.textContent = ZERO_SPACE_STRING;
    }
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

    caret.classList.remove('animating');
    void caret.offsetHeight;
    caret.classList.add('animating');

    const {selectionEnd} = input as HTMLInputElement;
    const wrapperRect = this.$.input.getBoundingClientRect();

    if (selectionEnd === 0) {
      const mirrorTextSpan = mirror.firstChild as HTMLElement;

      if (mirrorTextSpan) {
        const rect = mirrorTextSpan.getBoundingClientRect();
        const xOffset = this.isRtl_ ? rect.right : rect.left;
        const caretX = this.isRtl_ ? (xOffset - wrapperRect.right) :
                                     (xOffset - wrapperRect.left);
        const caretY = rect.top - wrapperRect.top;
        caret.style.transform = `translate(${caretX}px, ${caretY}px)`;
      } else {
        this.resetCaret();
      }
      return;
    }

    const charBeforeCursor =
        mirror.childNodes[selectionEnd! - 1] as HTMLElement;

    if (charBeforeCursor) {
      const rect = charBeforeCursor.getBoundingClientRect();
      const xOffset = this.isRtl_ ? rect.left : rect.right;
      const caretX = this.isRtl_ ? (xOffset - wrapperRect.right) :
                                   (xOffset - wrapperRect.left);
      const caretY = rect.top - wrapperRect.top;
      caret.style.transform = `translate(${caretX}px, ${caretY}px)`;
    }
  }

  resetCaret() {
    const caret = this.shadowRoot.getElementById('caret');
    if (!caret) {
      return;
    }
    const isRtl = document.documentElement.dir === 'rtl';
    const boxPaddingOffset = 12;
    let originX =
        this.entrypointName === 'ContextualTasks' ? 0 : boxPaddingOffset;
    if (isRtl) {
      originX = -originX;
    }
    const originY = boxPaddingOffset;
    caret.style.transform = `translate(${originX}px, ${originY}px)`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-input': ComposeboxInputElement;
  }
}

customElements.define(ComposeboxInputElement.is, ComposeboxInputElement);
