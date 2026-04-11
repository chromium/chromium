// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {VoiceSearchState} from '//resources/cr_components/search/constants.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import {getCss} from './composebox_voice_search.css.js';
import {getHtml} from './composebox_voice_search.html.js';
import {WindowProxy} from './window_proxy.js';

/**
 * Threshold for considering an interim speech transcript result as "confident
 * enough". The more confident the API is about a transcript, the higher the
 * confidence (number between 0 and 1).
 */
const RECOGNITION_CONFIDENCE_THRESHOLD: number = 0.7;

/**
 * Time in milliseconds to wait before closing the UI if no interaction
 * has occurred since start, OR last word spoken. Matches Google3.
 */
const IDLE_TIMEOUT_MS: number = 1500;

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
  // All other errors, like network.
  OTHER = 1,
}

export enum VoiceSearchError {
  ABORTED = 0,
  AUDIO_CAPTURE = 1,
  BAD_GRAMMAR = 2,
  LANGUAGE_NOT_SUPPORTED = 3,
  NETWORK = 4,
  NO_MATCH = 5,
  NO_SPEECH = 6,
  NOT_ALLOWED = 7,
  OTHER = 8,
  SERVICE_NOT_ALLOWED = 9,
  MAX_VALUE = SERVICE_NOT_ALLOWED,
}

export enum VoiceSearchAction {
  ACTIVATED_BY_ICON = 0,
  ACTIVATED_BY_KEYBOARD = 1,
  CLOSED_BY_USER = 2,
  QUERY_SUBMITTED = 3,
  SUPPORT_LINK_CLICKED = 4,
  RETRY_BY_TRY_AGAIN_CLICKED = 5,
  MAX_VALUE = RETRY_BY_TRY_AGAIN_CLICKED,
}

export enum VoiceSearchMetricType {
  ACTION = 'Action',
  ERROR = 'Errors',
  STATE = 'State',
}

function toError(webkitError: string): VoiceSearchError {
  switch (webkitError) {
    case 'aborted':
      return VoiceSearchError.ABORTED;
    case 'audio-capture':
      return VoiceSearchError.AUDIO_CAPTURE;
    case 'language-not-supported':
      return VoiceSearchError.LANGUAGE_NOT_SUPPORTED;
    case 'network':
      return VoiceSearchError.NETWORK;
    case 'no-speech':
      return VoiceSearchError.NO_SPEECH;
    case 'not-allowed':
      return VoiceSearchError.NOT_ALLOWED;
    case 'service-not-allowed':
      return VoiceSearchError.SERVICE_NOT_ALLOWED;
    case 'bad-grammar':
      return VoiceSearchError.BAD_GRAMMAR;
    default:
      return VoiceSearchError.OTHER;
  }
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
    closeButton: HTMLElement,
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
      detailsUrl_: {type: String},
      detailedError_: {type: Number},
    };
  }

  protected accessor transcript_: string = '';
  protected accessor listeningPlaceholder_: string =
      loadTimeData.getString('voiceListening');
  protected accessor finalResult_: string = '';
  protected accessor interimResult_: string = '';
  protected accessor error_: Error|null = null;
  protected accessor errorMessage_: string = '';
  accessor detailedError_: VoiceSearchError|null = null;
  protected accessor detailsUrl_: string =
      `https://support.google.com/chrome/?p=ui_voice_search&hl=${
          window.navigator.language}`;
  private accessor state_: State = State.UNINITIALIZED;

  private pageHandler_: PageHandlerRemote =
      ComposeboxProxyImpl.getInstance().handler;
  private voiceRecognition_: SpeechRecognition;
  private timerId_: number|null = null;
  private searchboxHandler_: SearchboxPageHandlerRemote =
      ComposeboxProxyImpl.getInstance().searchboxHandler;

  constructor() {
    super();
    this.voiceRecognition_ = new window.webkitSpeechRecognition();
    this.voiceRecognition_.continuous = true;
    this.voiceRecognition_.interimResults = true;
    this.voiceRecognition_.lang = window.navigator.language;
    this.voiceRecognition_.onresult = this.onResult_.bind(this);
    this.voiceRecognition_.onend = this.onEnd_.bind(this);
    this.voiceRecognition_.onaudiostart = this.onAudioStart_.bind(this);
    this.voiceRecognition_.onspeechstart = this.onSpeechStart_.bind(this);
    this.voiceRecognition_.onerror = (e) => {
      const detailedError = toError(e.error);
      this.recordMetric_(
          VoiceSearchMetricType.ERROR, detailedError,
          VoiceSearchError.MAX_VALUE + 1);
      this.onError_(e.error);
    };
    this.voiceRecognition_.onnomatch = () => {
      this.recordMetric_(
          VoiceSearchMetricType.ERROR, VoiceSearchError.NO_MATCH,
          VoiceSearchError.MAX_VALUE + 1);
    };
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.voiceRecognition_.abort();
  }

  protected shouldShowErrorScrim_(): boolean {
    return !!this.errorMessage_;
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
    // If there is text transcribed, process it as final.
    if (this.transcript_) {
      this.onFinalResult_(this.transcript_);
      return;
    }
    this.recordMetric_(
        VoiceSearchMetricType.ERROR, VoiceSearchError.NO_MATCH,
        VoiceSearchError.MAX_VALUE + 1);

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
      this.onFinalResult_(this.finalResult_);
      return;
    }

    // Process interim results based on confidence.
    for (let j = 0; j < results.length; j++) {
      const resultList = results[j]!;
      const result = resultList[0];  // best guess
      assert(result);

      this.transcript_ += result.transcript;
      if (result.confidence > RECOGNITION_CONFIDENCE_THRESHOLD) {
        this.finalResult_ += result.transcript;  // Displayed
      } else {
        this.interimResult_ += result.transcript;
      }
    }
    this.fire('transcript-update', this.transcript_);
  }

  private onEnd_() {
    // TODO(crbug.com/455878144): Log specific errors for each state.
    switch (this.state_) {
      // If voiceRecognition calls `onEnd_` with the state being anything other
      // than RESULT_FINAL, close out voice search since there was an error.
      case State.STARTED:
        this.recordMetric_(
            VoiceSearchMetricType.ERROR, VoiceSearchError.AUDIO_CAPTURE,
            VoiceSearchError.MAX_VALUE + 1);
        this.fire('voice-search-cancel', /*canceled-by-user=*/ false);
        return;
      case State.AUDIO_RECEIVED:
        this.recordMetric_(
            VoiceSearchMetricType.ERROR, VoiceSearchError.NO_SPEECH,
            VoiceSearchError.MAX_VALUE + 1);
        this.fire('voice-search-cancel', /*canceled-by-user=*/ false);
        return;
      case State.SPEECH_RECEIVED:
      case State.RESULT_RECEIVED:
        this.recordMetric_(
            VoiceSearchMetricType.ERROR, VoiceSearchError.NO_MATCH,
            VoiceSearchError.MAX_VALUE + 1);
        this.fire('voice-search-cancel', /*canceled-by-user=*/ false);
        return;
      case State.ERROR_RECEIVED:
        // All other errors should close voice search.
        if (this.error_ !== Error.NOT_ALLOWED) {
          /* Cannot abort voice recognition here; will call `onEnd()_`
           * again if do that again, leading to infinite recursion.
           */
          this.resetState_();
          /* No metric recorded through this event firing.
           * This event is fired just to hide voice overlay:
           */
          this.recordMetric_(
              VoiceSearchMetricType.STATE,
              VoiceSearchState.VOICE_SEARCH_ERROR_AND_CANCELED,
              VoiceSearchState.MAX_VALUE + 1);
          this.fire('voice-search-cancel', /*canceled-by-user=*/ false);
          // Metric recorded through this event firing:
          this.fire('voice-search-error', /*canceled-by-error=*/ true);
        } else {
          this.recordMetric_(
              VoiceSearchMetricType.STATE, VoiceSearchState.VOICE_SEARCH_ERROR,
              VoiceSearchState.MAX_VALUE + 1);
          // Metric recorded through this event firing:
          this.fire('voice-search-error', /*canceled-by-error=*/ false);
        }
        return;
      case State.RESULT_FINAL:  // Query already submitted if is this state
        return;
      default:
        this.recordMetric_(
            VoiceSearchMetricType.ERROR, VoiceSearchError.OTHER,
            VoiceSearchError.MAX_VALUE + 1);
        return;
    }
  }

  private async recordMetric_(
      type: VoiceSearchMetricType, value: number, max: number) {
    // Safety return statement in rare case chrome metrics is not available.
    if (!chrome.metricsPrivate) {
      return;
    }

    const {metricSource} = await this.searchboxHandler_.getPageClassification();
    if (!metricSource) {
      return;
    }
    const metricName = `VoiceSearch.${type}.${metricSource}`;
    chrome.metricsPrivate.recordEnumerationValue(metricName, value, max);
  }



  private onError_(webkitError: string) {
    this.state_ = State.ERROR_RECEIVED;
    switch (webkitError) {
      case 'not-allowed':
        this.errorMessage_ = loadTimeData.getString('voicePermissionError');
        this.error_ = Error.NOT_ALLOWED;
        return;
      default:
        this.error_ = Error.OTHER;
        return;
    }
  }

  protected voiceModeEndCleanup_() {
    this.voiceRecognition_.abort();
    this.resetState_();
  }

  private onFinalResult_(result: string) {
    if (!result) {
      return;
    }
    this.state_ = State.RESULT_FINAL;
    // Metric recorded through this event firing:
    this.fire('voice-search-final-result', result);
    this.recordMetric_(
        VoiceSearchMetricType.ACTION, VoiceSearchAction.QUERY_SUBMITTED,
        VoiceSearchAction.MAX_VALUE + 1);
    this.recordMetric_(
        VoiceSearchMetricType.STATE, VoiceSearchState.SUCCESSFUL_TRANSCRIPT,
        VoiceSearchState.MAX_VALUE + 1);
    this.voiceModeEndCleanup_();
  }

  protected onCloseClick_() {
    this.voiceModeEndCleanup_();
    // Record metric by setting canceled-by-user param to true in this event:
    this.fire(
        'voice-search-cancel',
        /*canceled-by-user=*/ true);
    this.recordMetric_(
        VoiceSearchMetricType.ACTION, VoiceSearchAction.CLOSED_BY_USER,
        VoiceSearchAction.MAX_VALUE + 1);
    this.recordMetric_(
        VoiceSearchMetricType.STATE, VoiceSearchState.VOICE_SEARCH_CANCELED,
        VoiceSearchState.MAX_VALUE + 1);
  }

  private resetState_() {
    this.state_ = State.UNINITIALIZED;
    this.finalResult_ = '';
    this.transcript_ = '';
    this.interimResult_ = '';
    this.error_ = null;
    this.errorMessage_ = '';
    WindowProxy.getInstance().clearTimeout(this.timerId_);
    this.timerId_ = null;
  }

  protected onLinkClick_(e: Event) {
    // Manually handle navigation to support WebView environments where
    // default link clicks may be ignored.
    e.preventDefault();
    const href = (e.currentTarget as HTMLAnchorElement).href;
    if (href) {
      this.pageHandler_.navigateUrl(href);
    }
    /* Do not record metric by setting canceled-by-user
     * param to false in this event:
     */
    this.fire('voice-search-cancel', /*canceled-by-user=*/ false);
    this.recordMetric_(
        VoiceSearchMetricType.ACTION, VoiceSearchAction.SUPPORT_LINK_CLICKED,
        VoiceSearchAction.MAX_VALUE + 1);
  }

  protected onTryAgainClick_(e: Event) {
    e.preventDefault();
    e.stopPropagation();
    this.recordMetric_(
        VoiceSearchMetricType.ACTION,
        VoiceSearchAction.RETRY_BY_TRY_AGAIN_CLICKED,
        VoiceSearchAction.MAX_VALUE + 1);
    this.errorMessage_ = '';
    this.error_ = null;
    this.start();
  }
  }

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-voice-search': ComposeboxVoiceSearchElement;
  }
}

customElements.define(
    ComposeboxVoiceSearchElement.is, ComposeboxVoiceSearchElement);
