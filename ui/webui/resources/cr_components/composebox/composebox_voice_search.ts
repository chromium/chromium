// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox_voice_search.css.js';
import {getHtml} from './composebox_voice_search.html.js';
import {WindowProxy} from './window_proxy.js';

/**
 * Threshold for considering an interim speech transcript result as "confident
 * enough". The more confident the API is about a transcript, the higher the
 * confidence (number between 0 and 1).
 */
const RECOGNITION_CONFIDENCE_THRESHOLD: number = 0.5;

/**
 * Time in milliseconds to wait before closing the UI if no interaction has
 * occurred.
 */
const IDLE_TIMEOUT_MS: number = 8000;

// The set of controller states.
enum State {
  // Initial state before voice recognition has been set up.
  UNINITIALIZED = -1,
  // Indicates that speech recognition has started, but no audio has yet
  // been captured.
  STARTED = 0,
  // Indicates that audio is being captured by the Web Speech API, but no
  // speech has yet been recognized. UI indicates that audio is being captured.
  AUDIO_RECEIVED = 1,
  // Indicates that speech has been recognized by the Web Speech API, but no
  // resulting transcripts have yet been received back. UI indicates that audio
  // is being captured and is pulsating audio button.
  SPEECH_RECEIVED = 2,
  // Indicates speech has been successfully recognized and text transcripts have
  // been reported back. UI indicates that audio is being captured and is
  // displaying transcripts received so far.
  RESULT_RECEIVED = 3,
  // Indicates that speech recognition has failed due to an error (or a no match
  // error) being received from the Web Speech API. A timeout may have occurred
  // as well. UI displays the error message.
  ERROR_RECEIVED = 4,
  // Indicates speech recognition has received a final search query but the UI
  // has not yet redirected. The UI is displaying the final query.
  RESULT_FINAL = 5,
}

// The set of controller errors
enum Error {
  // Error given when voice search permission enabled.
  NOT_ALLOWED = 0,
}

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

const ComposeboxVoiceSearchElementBase = I18nMixinLit(CrLitElement);

export class ComposeboxVoiceSearchElement extends
    ComposeboxVoiceSearchElementBase {
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
      state_: {type: Number},
      finalResult_: {type: String},
      interimResult_: {type: String},
      errorMessage_: {type: String},
      error_: {type: Number},
    };
  }

  private accessor state_: State = State.UNINITIALIZED;
  protected accessor transcript_: string = '';
  protected accessor listeningPlaceholder_: string =
      loadTimeData.getString('listening');
  private voiceRecognition_: SpeechRecognition;
  protected accessor finalResult_: string = '';
  protected accessor interimResult_: string = '';
  private timerId_: number|null = null;
  protected accessor error_: Error|null = null;
  protected accessor errorMessage_: string = '';
  protected detailsUrl_: string =
      `https://support.google.com/chrome/?p=ui_voice_search&hl=${
          window.navigator.language}`;

  protected get showErrorScrim_(): boolean {
    return !!this.errorMessage_;
  }

  constructor() {
    super();
    this.voiceRecognition_ = new window.webkitSpeechRecognition();
    this.voiceRecognition_.continuous = false;
    this.voiceRecognition_.interimResults = true;
    this.voiceRecognition_.lang = window.navigator.language;
    this.voiceRecognition_.onresult = this.onResult_.bind(this);
    this.voiceRecognition_.onend = this.onEnd_.bind(this);
    this.voiceRecognition_.onaudiostart = this.onAudioStart_.bind(this);
    this.voiceRecognition_.onspeechstart = this.onSpeechStart_.bind(this);
    this.voiceRecognition_.onerror = (e) => {
      this.onError_(e.error);
    };
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.voiceRecognition_.abort();
  }


  start() {
    this.errorMessage_ = '';
    this.voiceRecognition_.start();
    this.state_ = State.STARTED;
    this.resetIdleTimer_();
  }

  stop() {
    this.voiceRecognition_.stop();
  }

  private resetIdleTimer_() {
    WindowProxy.getInstance().clearTimeout(this.timerId_);
    this.timerId_ = WindowProxy.getInstance().setTimeout(
        this.onIdleTimeout_.bind(this), IDLE_TIMEOUT_MS);
  }

  private onIdleTimeout_() {
    if (this.state_ === State.RESULT_FINAL) {
      // Waiting for query redirect.
      return;
    }
    if (this.finalResult_) {
      // Query what we recognized so far.
      this.onFinalResult_();
      return;
    }
    this.voiceRecognition_.abort();
  }

  private onAudioStart_() {
    this.resetIdleTimer_();
    this.state_ = State.AUDIO_RECEIVED;
  }

  private onSpeechStart_() {
    this.resetIdleTimer_();
    this.state_ = State.SPEECH_RECEIVED;
    this.fire('speech-received');
  }


  private onResult_(e: SpeechRecognitionEvent) {
    this.resetIdleTimer_();
    const results = e.results;
    if (results.length === 0) {
      return;
    }

    this.state_ = State.RESULT_RECEIVED;
    this.interimResult_ = '';
    this.finalResult_ = '';

    const speechResult = results[e.resultIndex];
    assert(speechResult);
    // Process final results if is fully final.
    if (!!speechResult && speechResult.isFinal) {
      this.finalResult_ = speechResult[0]!.transcript;
      this.onFinalResult_();
      return;
    }

    // Process interim results based on confidence.
    for (let j = 0; j < results.length; j++) {
      const resultList = results[j]!;
      const result = resultList[0];  // best guess
      assert(result);

      if (result.confidence > RECOGNITION_CONFIDENCE_THRESHOLD) {
        this.finalResult_ += result.transcript;  // Displayed
      } else {
        this.interimResult_ += result.transcript;
      }
    }
    this.transcript_ = this.finalResult_ + this.interimResult_;
    this.fire('transcript-update', this.transcript_);
  }

  private onEnd_() {
    // TODO(crbug.com/455878144): Log specific errors for each state.
    switch (this.state_) {
      // If voiceRecognition calls `onEnd_` with the state being anything other
      // than RESULT_FINAL, close out voice search since there was an error.
      case State.STARTED:
      case State.AUDIO_RECEIVED:
      case State.SPEECH_RECEIVED:
      case State.RESULT_RECEIVED:
        this.fire('voice-search-cancel');
        return;
      case State.ERROR_RECEIVED:
        // All other errors should close voice search.
        if (this.error_ !== Error.NOT_ALLOWED) {
          this.resetState_();
          this.fire('voice-search-cancel');
        }
        return;
      case State.RESULT_FINAL:
        return;
      default:
        return;
    }
  }

  private onError_(webkitError: string) {
    this.state_ = State.ERROR_RECEIVED;
    switch (webkitError) {
      case 'not-allowed':
        this.error_ = Error.NOT_ALLOWED;
        this.errorMessage_ = loadTimeData.getString('permissionError');
        return;
      default:
        return;
    }
  }

  private onFinalResult_() {
    if (!this.finalResult_) {
      return;
    }
    this.state_ = State.RESULT_FINAL;
    this.fire('voice-search-final-result', this.finalResult_);
  }

  protected onCloseClick_() {
    this.voiceRecognition_.abort();
    this.resetState_();
    this.fire('voice-search-cancel');
  }

  private resetState_() {
    this.state_ = State.UNINITIALIZED;
    this.finalResult_ = '';
    this.interimResult_ = '';
    this.error_ = null;
    this.errorMessage_ = '';
  }

  protected onLinkClick_() {
    this.fire('voice-search-cancel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-voice-search': ComposeboxVoiceSearchElement;
  }
}

customElements.define(
    ComposeboxVoiceSearchElement.is, ComposeboxVoiceSearchElement);
