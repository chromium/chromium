// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrIconButtonElement} from '../cr_icon_button/cr_icon_button.js';

import {getCss} from './cr_feedback_buttons.css.js';
import {getHtml} from './cr_feedback_buttons.html.js';

export enum CrFeedbackOption {
  THUMBS_DOWN = 0,
  THUMBS_UP = 1,
  UNSPECIFIED = 2,
}

export interface CrFeedbackButtonsElement {
  $: {
    thumbsDown: CrIconButtonElement,
    thumbsUp: CrIconButtonElement,
  };
}

export class CrFeedbackButtonsElement extends CrLitElement {
  static get is() {
    return 'cr-feedback-buttons';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedOption: {type: Number},
      thumbsDownLabel_: {type: String},
      thumbsUpLabel_: {type: String},
      disabled: {type: Boolean},
    };
  }

  selectedOption: CrFeedbackOption = CrFeedbackOption.UNSPECIFIED;
  protected thumbsDownLabel_: string = loadTimeData.getString('thumbsDown');
  protected thumbsUpLabel_: string = loadTimeData.getString('thumbsUp');
  disabled: boolean = false;

  protected getThumbsDownAriaPressed_(): boolean {
    return this.selectedOption === CrFeedbackOption.THUMBS_DOWN;
  }

  protected getThumbsDownIcon_(): string {
    return this.selectedOption === CrFeedbackOption.THUMBS_DOWN ?
        'cr:thumbs-down-filled' :
        'cr:thumbs-down';
  }

  protected getThumbsUpAriaPressed_(): boolean {
    return this.selectedOption === CrFeedbackOption.THUMBS_UP;
  }

  protected getThumbsUpIcon_(): string {
    return this.selectedOption === CrFeedbackOption.THUMBS_UP ?
        'cr:thumbs-up-filled' :
        'cr:thumbs-up';
  }

  private async notifySelectedOptionChanged_() {
    // Wait for the element's DOM to be updated before dispatching
    // selected-option-changed event.
    await this.updateComplete;
    this.dispatchEvent(new CustomEvent('selected-option-changed', {
      bubbles: true,
      composed: true,
      detail: {value: this.selectedOption},
    }));
  }

  protected onThumbsDownClick_() {
    this.selectedOption = this.selectedOption === CrFeedbackOption.THUMBS_DOWN ?
        CrFeedbackOption.UNSPECIFIED :
        CrFeedbackOption.THUMBS_DOWN;
    this.notifySelectedOptionChanged_();
  }

  protected onThumbsUpClick_() {
    this.selectedOption = this.selectedOption === CrFeedbackOption.THUMBS_UP ?
        CrFeedbackOption.UNSPECIFIED :
        CrFeedbackOption.THUMBS_UP;
    this.notifySelectedOptionChanged_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-feedback-buttons': CrFeedbackButtonsElement;
  }
}

customElements.define(CrFeedbackButtonsElement.is, CrFeedbackButtonsElement);
