// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_icon_button/cr_icon_button.js';
import '../cr_shared_vars.css.js';
import '../icons.html.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrIconButtonElement} from '../cr_icon_button/cr_icon_button.js';

import {getTemplate} from './cr_feedback_buttons.html.js';

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

export class CrFeedbackButtonsElement extends PolymerElement {
  static get is() {
    return 'cr-feedback-buttons';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedOption: {
        type: String,
        value: CrFeedbackOption.UNSPECIFIED,
      },
      thumbsDownLabel_: {
        type: String,
        value: () => loadTimeData.getString('thumbsDown'),
      },
      thumbsUpLabel_: {
        type: String,
        value: () => loadTimeData.getString('thumbsUp'),
      },
    };
  }

  selectedOption: CrFeedbackOption;
  private thumbsDownLabel_: string;
  private thumbsUpLabel_: string;

  private getThumbsDownAriaPressed_(): boolean {
    return this.selectedOption === CrFeedbackOption.THUMBS_DOWN;
  }

  private getThumbsDownIcon_(): string {
    return this.selectedOption === CrFeedbackOption.THUMBS_DOWN ?
        'cr:thumbs-down-filled' :
        'cr:thumbs-down';
  }

  private getThumbsUpAriaPressed_(): boolean {
    return this.selectedOption === CrFeedbackOption.THUMBS_UP;
  }

  private getThumbsUpIcon_(): string {
    return this.selectedOption === CrFeedbackOption.THUMBS_UP ?
        'cr:thumbs-up-filled' :
        'cr:thumbs-up';
  }

  private notifySelectedOptionChanged_() {
    this.dispatchEvent(new CustomEvent('selected-option-changed', {
      bubbles: true,
      composed: true,
      detail: {value: this.selectedOption},
    }));
  }

  private onThumbsDownClick_() {
    this.selectedOption = this.selectedOption === CrFeedbackOption.THUMBS_DOWN ?
        CrFeedbackOption.UNSPECIFIED :
        CrFeedbackOption.THUMBS_DOWN;
    this.notifySelectedOptionChanged_();
  }

  private onThumbsUpClick_() {
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
