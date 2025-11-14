// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox_voice_search.css.js';
import {getHtml} from './composebox_voice_search.html.js';

// TODO(crbug.com/40449919): Remove when bug is fixed.
declare global {
  interface Window {
    webkitSpeechRecognition: typeof SpeechRecognition;
  }
}

export interface ComposeboxVoiceSearchElement {
  $: {
    input: HTMLInputElement,
  };
}

export class ComposeboxVoiceSearchElement extends CrLitElement {
  static get is() {
    return 'cr-composebox-voice-search';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      transcript_: {type: String},
      listeningPlaceholder_: {type: String},
    };
  }

  protected accessor transcript_: string = '';
  protected accessor listeningPlaceholder_: string =
      loadTimeData.getString('listening');
  private voiceRecognition_: SpeechRecognition;

  constructor() {
    super();
    this.voiceRecognition_ = new window.webkitSpeechRecognition();
    this.voiceRecognition_.continuous = false;
    this.voiceRecognition_.interimResults = true;
    this.voiceRecognition_.lang = window.navigator.language;
    this.voiceRecognition_.onresult = this.onResult_.bind(this);
    this.voiceRecognition_.onend = this.onEnd_.bind(this);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.voiceRecognition_.abort();
  }


  start() {
    this.voiceRecognition_.start();
  }

  stop() {
    this.voiceRecognition_.stop();
  }

  private onResult_(e: SpeechRecognitionEvent) {
    const results = e.results;
    if (results.length === 0) {
      return;
    }

    let transcript = '';
    for (let i = 0; i < results.length; ++i) {
      transcript += results[i]![0]!.transcript;
    }
    this.transcript_ = transcript;
  }

  private onEnd_() {
    // TODO(crbug.com/455878144): Handle error, should not always submit if
    // voice search was cancelled.
    this.fire('on-voice-search-final-result', this.transcript_);
  }

  protected onCloseClick_() {
    this.voiceRecognition_.abort();
    this.fire('on-voice-search-cancel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-voice-search': ComposeboxVoiceSearchElement;
  }
}

customElements.define(
    ComposeboxVoiceSearchElement.is, ComposeboxVoiceSearchElement);
