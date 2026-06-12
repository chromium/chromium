// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './composebox_submit.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {Size} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';

import {SubmitButtonIconType} from './composebox.js';
import type {PageHandlerRemote} from './composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from './composebox_proxy.js';
import {getCss} from './composebox_voice_search.css.js';
import {getHtml} from './composebox_voice_search.html.js';
import {WindowProxy} from './window_proxy.js';

export interface VoicePermissionPromptState {
  isOpened: boolean;
  height: number;
  width: number;
}

/**
 * Threshold for considering an interim speech transcript result as "confident
 * enough". The more confident the API is about a transcript, the higher the
 * confidence (number between 0 and 1).
 */
const RECOGNITION_CONFIDENCE_THRESHOLD: number = 0.7;

/**
 * Time in milliseconds to wait before automatically closing the UI after a
 * NO_MATCH error occurs (when the error timer is enabled).
 */
const ERROR_TIMEOUT_LONG_MS: number = 24000;

/**
 * Time in milliseconds to wait before automatically closing the UI after a non
 * NO_MATCH error occurs (when the error timer is enabled).
 */
const ERROR_TIMEOUT_SHORT_MS: number = 9000;

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
export enum VoiceSearchAction {
  ACTIVATED_BY_ICON = 0,
  ACTIVATED_BY_KEYBOARD = 1,
  // CLOSED_OVERLAY = 2, // Obsolete, replaced by CANCELED_BY_USER
  QUERY_SUBMITTED = 3,
  SUPPORT_LINK_CLICKED = 4,
  RETRY_BY_TRY_AGAIN_CLICKED = 5,
  // TRY_AGAIN_MIC_BUTTON = 6, // Obsolete. Deprecated as of 09/2022.
  STOP_BUTTON_CLICKED = 7,
  RESUME_BUTTON_CLICKED = 8,
  ERROR_NON_CANCELING = 9,
  ERROR_CANCELING = 10,
  CANCELED_BY_USER = 11,
  MAX_VALUE = CANCELED_BY_USER,
}

export enum VoiceSearchMetricType {
  ACTION = 'Action',
  ERROR = 'Errors',
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
      submitStopButtonsEnabled: {type: Boolean},
      liveTranscriptEnabled: {type: Boolean},
      pageCallbackRouter: {type: Object},
      transcript_: {type: String},
      listeningPlaceholder_: {type: String},
      state_: {type: Number},
      finalResult_: {type: String},
      interimResult_: {type: String},
      errorMessage_: {type: String},
      error_: {type: Number},
      detailsUrl_: {type: String},
      detailedError_: {type: Number},
      hasErrorTimer: {type: Boolean},
      isPermissionPromptOpen_: {type: Boolean, reflect: true},
      submitButtonIconType: {type: String},
      /**
       * Determines whether to automatically submit the query upon receiving a
       * final speech recognition result. When disabled, it will instead close
       * the voice search and populate the searchbox with the transcribed text.
       */
      autosubmitEnabled: {type: Boolean},
      /**
       * Controls the `continuous` parameter of the Webkit Speech API. When
       * enabled, the API dynamically determines when to stop listening based on
       * speech patterns. If this is enabled, `idleTimeout` is ignored.
       */
      dynamicTimeoutEnabled: {type: Boolean},
      /**
       * Maximum number of characters recognized before force-submitting a
       * query. Includes characters of non-confident recognition transcripts.
       * TODO(crbug.com/510393520): Enforce a 120-character limit for the
       * Searchbox surface.
       */
      queryLengthLimit: {type: Number},
      /**
       * Time in milliseconds to wait before closing the UI if no interaction
       * has occurred since start, OR last word spoken. The default
       * value matches the Google3 AIM implementation. If
       * `dynamicTimeoutEnabled` is set, then this number is ignored.
       */
      idleTimeout: {type: Number},
    };
  }

  accessor submitStopButtonsEnabled: boolean = false;
  accessor liveTranscriptEnabled: boolean = true;
  // Accept page callback router attribute asynchronously, so that the parent
  // and voice search component can share the same mojo connection and source of
  // truth (to avoid race conditions).
  accessor pageCallbackRouter: PageCallbackRouter|null = null;
  protected accessor transcript_: string = '';
  protected accessor listeningPlaceholder_: string =
      loadTimeData.getString('voiceListening');
  protected accessor finalResult_: string = '';
  protected accessor interimResult_: string = '';
  protected accessor error_: VoiceSearchError|null = null;
  protected accessor errorMessage_: string = '';
  accessor detailedError_: VoiceSearchError|null = null;
  protected accessor detailsUrl_: string =
      `https://support.google.com/chrome/?p=ui_voice_search&hl=${
          window.navigator.language}`;
  protected accessor isPermissionPromptOpen_: boolean = false;

  private accessor state_: State = State.UNINITIALIZED;
  private metricSource_: string = '';
  private blurTimeoutId_: number|null = null;

  // Shared statically to coordinate the singleton SpeechRecognition service
  // across element instances (which are destroyed/recreated on toggle) and
  // prevent InvalidStateError crashes on rapid restarts.
  private static activeRecognition_: SpeechRecognition|null = null;
  private static pendingStartInstance_: ComposeboxVoiceSearchElement|null = null;

  private pageHandler_: PageHandlerRemote =
      ComposeboxProxyImpl.getInstance().handler;
  private voiceRecognition_: SpeechRecognition;
  private timerId_: number|null = null;
  private searchboxHandler_: SearchboxPageHandlerRemote =
      ComposeboxProxyImpl.getInstance().searchboxHandler;
  private listenerIds_: number[] = [];
  accessor hasErrorTimer: boolean = false;
  accessor submitButtonIconType: SubmitButtonIconType =
      SubmitButtonIconType.FORWARD;
  accessor autosubmitEnabled: boolean = false;
  accessor dynamicTimeoutEnabled: boolean = false;
  accessor queryLengthLimit: number|undefined = undefined;
  accessor idleTimeout: number = 3000;

  constructor() {
    super();
    this.voiceRecognition_ = new window.webkitSpeechRecognition();
    this.voiceRecognition_.interimResults = true;
    this.voiceRecognition_.lang = window.navigator.language;
    this.voiceRecognition_.onresult = this.onResult_.bind(this);
    this.voiceRecognition_.onend = this.onEnd_.bind(this);
    this.voiceRecognition_.onaudiostart = this.onAudioStart_.bind(this);
    this.voiceRecognition_.onspeechstart = this.onSpeechStart_.bind(this);
    this.voiceRecognition_.onerror = (e) => {
      this.onError_(toError(e.error));
    };
    this.voiceRecognition_.onnomatch = () => {
      this.onError_(VoiceSearchError.NO_MATCH);
    };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.searchboxHandler_.getPageClassification().then(({metricSource}) => {
      this.metricSource_ = metricSource || '';
    });
  }

  override disconnectedCallback() {
    this.listenerIds_.forEach(
        (id: number) => assert(this.pageCallbackRouter!.removeListener(id)));
    this.listenerIds_ = [];
    this.removeOutsideListeners_();
    if (ComposeboxVoiceSearchElement.pendingStartInstance_ === this) {
      ComposeboxVoiceSearchElement.pendingStartInstance_ = null;
    }
    this.voiceRecognition_.abort();
    super.disconnectedCallback();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    // When `pageCallbackRouter` is set by the parent,
    // add all listeners for the callback router if not already added.
    if (changedProperties.has('pageCallbackRouter') &&
        this.pageCallbackRouter && this.listenerIds_.length === 0) {
      this.listenerIds_.push(
          this.pageCallbackRouter.onPermissionPromptChanged.addListener(
              this.onVoicePermissionPromptChanged.bind(this)),
      );
    }
  }

  protected shouldShowErrorScrim_(): boolean {
    return !!this.errorMessage_;
  }

  start() {
    if (this.state_ !== State.UNINITIALIZED &&
        this.state_ !== State.ERROR_RECEIVED) {
      return;
    }
    if (ComposeboxVoiceSearchElement.activeRecognition_ !== null) {
      ComposeboxVoiceSearchElement.pendingStartInstance_ = this;
      ComposeboxVoiceSearchElement.activeRecognition_.abort();
      return;
    }
    this.errorMessage_ = '';
    // If continuous is false, then speech webkit determines when to end, and
    // there is no manual set timeout.
    this.voiceRecognition_.continuous = !this.dynamicTimeoutEnabled;
    this.voiceRecognition_.start();
    ComposeboxVoiceSearchElement.activeRecognition_ = this.voiceRecognition_;
    this.state_ = State.STARTED;
    this.resetIdleTimer_();
    // TODO(crbug.com/504726157): When the NTP searchbox migrates to use this
    // component, it will need to log VoiceSearchAction.ACTIVATED_BY_KEYBOARD.
    this.recordMetric_(
        VoiceSearchMetricType.ACTION, VoiceSearchAction.ACTIVATED_BY_ICON,
        VoiceSearchAction.MAX_VALUE + 1);
    this.addOutsideListeners_();
  }

  private onOutsideInteraction_ = (e: Event) => {
    if (e.type === 'pointerdown') {
      if (e.composedPath().includes(this)) {
        return;
      }
      this.onStopClick_();
      return;
    }

    if (e.type === 'blur') {
      // If permission prompt is open currently, ignore the blur event,
      // as the blur is from the permission prompt, and need voice search
      // to stay open during prompt.
      if (this.isPermissionPromptOpen_) {
        return;
      }

      // Add a timeout before calling `stop()`. This gives a small delay for
      // `onVoicePermissionPromptChanged` to run after any setup lag
      // (~15ms). This way, it is certain that this blur event is not due to
      // a permission prompt popping up.
      this.blurTimeoutId_ = WindowProxy.getInstance().setTimeout(() => {
        if (!this.isPermissionPromptOpen_) {
          this.onStopClick_();
        }
        this.blurTimeoutId_ = null;
      }, 100);
    }
  };

  /**
   * Adds global listeners to close voice search on outside interactions.
   * `pointerdown` captures clicks on flat DOMs (e.g., NTP).
   * `blur` captures clicks inside isolated webviews (e.g., Contextual Tasks
   * `#threadFrame`) which steal focus but do not bubble click events.
   */
  private addOutsideListeners_() {
    WindowProxy.getInstance().setTimeout(() => {
      document.addEventListener('pointerdown', this.onOutsideInteraction_);
      window.addEventListener('blur', this.onOutsideInteraction_);
    }, 0);
  }

  private removeOutsideListeners_() {
    document.removeEventListener('pointerdown', this.onOutsideInteraction_);
    window.removeEventListener('blur', this.onOutsideInteraction_);
  }


  protected onStopClick_(e?: Event) {
    if (e) {
      e.preventDefault();
      e.stopPropagation();
    }
    this.fire('recording-stopped', this.transcript_);
    this.recordMetric_(
        VoiceSearchMetricType.ACTION, VoiceSearchAction.STOP_BUTTON_CLICKED,
        VoiceSearchAction.MAX_VALUE + 1);
    this.voiceRecognition_.stop();
    this.voiceModeEndCleanup_();
  }

  private onVoicePermissionPromptChanged(isOpened: boolean, promptSize: Size) {
    // Track the state for the blur event handler to ignore if
    // permission prompt open.
    this.isPermissionPromptOpen_ = isOpened;

    // If the prompt just opened, cancel any pending closure triggered by a
    // premature blur event.
    if (isOpened && this.blurTimeoutId_) {
      WindowProxy.getInstance().clearTimeout(this.blurTimeoutId_);
      this.blurTimeoutId_ = null;
    }
    // Fire event so `composebox_mixin.ts` knows to hide voice animation when
    // `isOpened`=`true`. `composebox_mixin.ts` will fire another event if
    // conditions are met so `aim_app.ts` (omnibox popup) can resize to be at
    // least as big as the permission prompt if needed. This voice component
    // does not control any resizing or hiding:
    this.fire('voice-permission-changed', {
      'isOpened': isOpened,
      'height': promptSize.height,
      'width': promptSize.width,
    } as VoicePermissionPromptState);
    if (isOpened) {
      this.clearTimer_();
    } else {
      this.resetIdleTimer_();
    }
  }

  private clearTimer_() {
    if (this.timerId_ !== null) {
      WindowProxy.getInstance().clearTimeout(this.timerId_);
      this.timerId_ = null;
    }
  }

  private resetIdleTimer_() {
    WindowProxy.getInstance().clearTimeout(this.timerId_);
    this.timerId_ = WindowProxy.getInstance().setTimeout(
        this.onIdleTimeout_.bind(this), this.idleTimeout);
  }

  protected onSubmitClick_(e: Event) {
    e.preventDefault();
    e.stopPropagation();

    this.onFinalResult_(this.transcript_, /*forceSubmit=*/ true);
  }

  private onIdleTimeout_() {
    if (this.state_ === State.RESULT_FINAL) {
      // Waiting for query redirect.
      return;
    }
    // If there is text transcribed, process it as final.
    if (this.transcript_) {
      this.onFinalResult_(this.transcript_, /* force_submit=*/ true);
      return;
    }
    this.onError_(VoiceSearchError.NO_SPEECH);
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
    // First reset, in case of early return before second reset
    // (done again to not include time taken to process results in timeout).
    this.resetIdleTimer_();
    switch (this.state_) {
      case State.STARTED:
        // Network bugginess (the onspeechstart packet was lost).
        this.onAudioStart_();
        this.onSpeechStart_();
        break;
      case State.AUDIO_RECEIVED:
        // Network bugginess (the onaudiostart packet was lost).
        this.onSpeechStart_();
        break;
      case State.SPEECH_RECEIVED:
      case State.RESULT_RECEIVED:
        // Normal, expected states for processing results.
        break;
      default:
        // Not expecting results in any other states.
        return;
    }
    const results = e.results;
    if (results.length === 0) {
      return;
    }

    this.state_ = State.RESULT_RECEIVED;
    this.interimResult_ = '';
    this.finalResult_ = '';
    this.transcript_ = '';

    const speechResult = results[e.resultIndex];
    assert(speechResult);

    // Process interim results based on confidence.
    for (let j = 0; j < results.length; j++) {
      const resultList = results[j]!;
      const result = resultList[0];  // best guess
      assert(result);

      this.transcript_ += result.transcript;
      if (result.confidence > RECOGNITION_CONFIDENCE_THRESHOLD) {
        this.finalResult_ += result.transcript;  // Displayed
      } else {
        // TODO(crbug.com/511795841) Delete this deprecated feature.
        this.interimResult_ += result.transcript;
      }
    }
    this.fire('transcript-update', this.transcript_);

    // Once all the work is done (may have delays depending on load), to avoid
    // including that time in the timeout, reset the timer again. This schedules
    // a potential submission of the transcript. Once `this.idleTimeout`
    // timespan has passed, the timeout callback will submit the query.
    this.resetIdleTimer_();

    // Force-stop long queries if queryLengthLimit is not undefined.
    if (this.queryLengthLimit !== undefined &&
        this.interimResult_.length > this.queryLengthLimit) {
      this.onFinalResult_(this.transcript_);
    }
  }

  private onEnd_() {
    if (ComposeboxVoiceSearchElement.activeRecognition_ === this.voiceRecognition_) {
      ComposeboxVoiceSearchElement.activeRecognition_ = null;
    }

    if (ComposeboxVoiceSearchElement.pendingStartInstance_ !== null) {
      const pending = ComposeboxVoiceSearchElement.pendingStartInstance_;
      ComposeboxVoiceSearchElement.pendingStartInstance_ = null;
      pending.start();
      return;
    }

    switch (this.state_) {
        // If voiceRecognition calls `onEnd_` with the state being anything
        // other than `RESULT_FINAL` or `ERROR_RECEIVED` or `SPEECH_RECEIVED` or
        // `RESULT_RECEIVED`, close out voice search since there was a silent
        // failure with setting up audio or transcription. `RESULT_RECEIVED` is
        // not okay if `continuous` is false since that means that speech webkit
        // is the source of truth for timing out, and it has decided to time out
        // and quit by itself by sending `onend` even though this frontend
        // component is still expecting results.
      case State.STARTED:
        this.onError_(VoiceSearchError.AUDIO_CAPTURE);
        return;
      case State.AUDIO_RECEIVED:
        this.onError_(VoiceSearchError.NO_SPEECH);
        return;
      case State.SPEECH_RECEIVED:
      case State.RESULT_RECEIVED:
        // If `continuous` is disabled, that means that speech
        // webkit is the source of truth for timing out and has
        // decided to time out.
        if (!this.dynamicTimeoutEnabled) {
          this.onError_(VoiceSearchError.NO_MATCH);
        }
        return;
      case State.UNINITIALIZED:
      case State.ERROR_RECEIVED:
      case State.RESULT_FINAL:
        return;
      default:
        this.onError_(VoiceSearchError.OTHER);
        return;
    }
  }

  private recordMetric_(
      type: VoiceSearchMetricType, metricEnumValue: number, max: number) {
    // Safety return statement in rare case chrome metrics is not available.
    if (!chrome.metricsPrivate) {
      return;
    }
    if (!this.metricSource_) {
      return;
    }

    const metricName = `VoiceSearch.${type}.${this.metricSource_}`;
    chrome.metricsPrivate.recordEnumerationValue(
        metricName, metricEnumValue, max);

    const aggregateMetricName = `VoiceSearch.${type}`;
    chrome.metricsPrivate.recordEnumerationValue(
        aggregateMetricName, metricEnumValue, max);

    // TODO(b/501544449): This dual-logging block is temporary to ensure data
    // continuity. Remove this once the unified VoiceSearch metrics are
    // validated.
    if (this.metricSource_ === 'NTP_REALBOX') {
      if (type === VoiceSearchMetricType.ACTION) {
        // Handle the case that NewTabPage metric `CLOSE_OVERLAY` is replaced by
        // `CANCELED_BY_USER`.
        let legacyMetricEnumValue = metricEnumValue;
        if (metricEnumValue === VoiceSearchAction.CANCELED_BY_USER) {
          legacyMetricEnumValue = 2;
        }
        chrome.metricsPrivate.recordEnumerationValue(
            'NewTabPage.VoiceActions', legacyMetricEnumValue, max);
      } else if (type === VoiceSearchMetricType.ERROR) {
        chrome.metricsPrivate.recordEnumerationValue(
            'NewTabPage.VoiceErrors', metricEnumValue, max);
      }
    }
  }

  private onError_(error: VoiceSearchError) {
    if (this.state_ === State.ERROR_RECEIVED && this.error_ === error) {
      return;
    }
    // Record the specific error type.
    this.recordMetric_(
        VoiceSearchMetricType.ERROR, error, VoiceSearchError.MAX_VALUE + 1);

    if (error === VoiceSearchError.ABORTED) {
      return;
    }
    WindowProxy.getInstance().clearTimeout(this.timerId_);
    this.state_ = State.ERROR_RECEIVED;
    this.error_ = error;
    this.detailedError_ = error;

    // Handle error display and dismissal behavior based on the embedder.
    if (!this.hasErrorTimer) {
      if (error === VoiceSearchError.NO_MATCH || error === VoiceSearchError.NO_SPEECH) {
        // Without a timer, NO_MATCH and NO_SPEECH errors close immediately with no message.
        this.errorMessage_ = '';
        this.resetState_();
        this.recordMetric_(
            VoiceSearchMetricType.ACTION, VoiceSearchAction.ERROR_CANCELING,
            VoiceSearchAction.MAX_VALUE + 1);
        // This fire event does not record metric.
        this.fire('voice-search-cancel', /*canceled-by-user=*/ false);
        // This fire event records metric for contextual tasks and cancels voice
        // search.
        this.fire('voice-search-error', /*canceled-by-error=*/ true);
      } else {
        // Without a timer, other errors show a message and stay open
        // permanently.
        this.errorMessage_ = this.getErrorText_(error);
        this.recordMetric_(
            VoiceSearchMetricType.ACTION, VoiceSearchAction.ERROR_NON_CANCELING,
            VoiceSearchAction.MAX_VALUE + 1);
        // this fire event records metric for contextual tasks but does not
        // cancel voice search.
        this.fire('voice-search-error', /*canceled-by-error=*/ false);
      }
    } else {
      // If there is a timer, an error message would show up.
      this.errorMessage_ = this.getErrorText_(error);

      if (error === VoiceSearchError.NO_MATCH || error === VoiceSearchError.NO_SPEECH) {
        // NO_MATCH and NO_SPEECH errors auto-close after a longer delay.
        this.timerId_ = WindowProxy.getInstance().setTimeout(() => {
          this.recordMetric_(
              VoiceSearchMetricType.ACTION, VoiceSearchAction.ERROR_CANCELING,
              VoiceSearchAction.MAX_VALUE + 1);
          this.resetState_();
          // This fire event does not record metric.
          this.fire('voice-search-cancel', /*canceled-by-user=*/ false);
        }, ERROR_TIMEOUT_LONG_MS);
      } else {
        // Other errors auto-close after a shorter delay.
        this.timerId_ = WindowProxy.getInstance().setTimeout(() => {
          this.recordMetric_(
              VoiceSearchMetricType.ACTION, VoiceSearchAction.ERROR_CANCELING,
              VoiceSearchAction.MAX_VALUE + 1);
          this.resetState_();
          this.fire('voice-search-cancel', /*canceled-by-user=*/ false);
        }, ERROR_TIMEOUT_SHORT_MS);
      }
    }
  }

  private getErrorText_(error: VoiceSearchError): string {
    switch (error) {
      case VoiceSearchError.NO_SPEECH:
        return loadTimeData.getString('noVoice');
      case VoiceSearchError.AUDIO_CAPTURE:
        return loadTimeData.getString('audioError');
      case VoiceSearchError.NETWORK:
        return loadTimeData.getString('networkError');
      case VoiceSearchError.NOT_ALLOWED:
      case VoiceSearchError.SERVICE_NOT_ALLOWED:
        return loadTimeData.getString('voicePermissionError');
      case VoiceSearchError.LANGUAGE_NOT_SUPPORTED:
        return loadTimeData.getString('languageError');
      case VoiceSearchError.NO_MATCH:
        return loadTimeData.getString('noTranslation');
      case VoiceSearchError.BAD_GRAMMAR:
      case VoiceSearchError.ABORTED:
      case VoiceSearchError.OTHER:
      default:
        return loadTimeData.getString('otherError');
    }
  }

  protected voiceModeEndCleanup_() {
    this.removeOutsideListeners_();
    this.voiceRecognition_.abort();
    this.resetState_();
  }

  private onFinalResult_(result: string, forceSubmit: boolean = false) {
    if (!result) {
      return;
    }
    if (!this.autosubmitEnabled && !forceSubmit) {
      this.fire('recording-stopped', this.transcript_);
      this.voiceModeEndCleanup_();
      return;
    }
    this.state_ = State.RESULT_FINAL;
    // Metric recorded through this event firing:
    this.fire('voice-search-final-result', result);
    this.recordMetric_(
        VoiceSearchMetricType.ACTION, VoiceSearchAction.QUERY_SUBMITTED,
        VoiceSearchAction.MAX_VALUE + 1);
    this.voiceModeEndCleanup_();
  }

  protected onCloseClick_() {
    this.voiceModeEndCleanup_();
    // Record metric by setting canceled-by-user param to true in this event:
    this.fire(
        'voice-search-cancel',
        /*canceled-by-user=*/ true);
    this.recordMetric_(
        VoiceSearchMetricType.ACTION, VoiceSearchAction.CANCELED_BY_USER,
        VoiceSearchAction.MAX_VALUE + 1);
  }

  private resetState_() {
    this.state_ = State.UNINITIALIZED;
    this.finalResult_ = '';
    this.transcript_ = '';
    this.interimResult_ = '';
    this.error_ = null;
    this.errorMessage_ = '';
    this.detailedError_ = null;
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
