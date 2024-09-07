import {SpeechAudioErrorDetails} from '/gen/media/mojo/mojom/speech_recognition_error.mojom.m.js';
import {SpeechRecognitionErrorCode} from '/gen/media/mojo/mojom/speech_recognition_error_code.mojom.m.js';
import {SpeechRecognitionSessionReceiver, SpeechRecognizer, SpeechRecognizerReceiver} from '/gen/media/mojo/mojom/speech_recognizer.mojom.m.js';

// MockSpeechRecognizer is a mock implementation of blink.mojom.SpeechRecognizer
// and the browser speech recognition service. Mock results can be set using
// addMockSpeechRecognitionResult, and setMockSpeechRecognitionError can be used
// to simulate an error state. If no mock results are set, a NoMatch error is
// sent to the client.
export class MockSpeechRecognizer {
  constructor() {
    this.results_ = [];
    this.session_ = null;
    this.session_client_ = null;
    this.error_ = null;
    this.lastSetTimeout_ = null;

    this.task_queue_ = [];

    this.receiver_ = new SpeechRecognizerReceiver(this);
    this.interceptor_ =
        new MojoInterfaceInterceptor(SpeechRecognizer.$interfaceName);
    this.interceptor_.oninterfacerequest =
        e => this.receiver_.$.bindHandle(e.handle);
    this.interceptor_.start();
  }

  reset() {
    this.receiver_.closeBindings();
    this.results_ = [];
    this.session_ = null;
    this.session_client_ = null;
    this.error_ = null;
  }

  addMockSpeechRecognitionResult(transcript, confidence) {
    const toString16 = function(string) {
      const data = new Array(string.length);
      for (let i = 0; i < string.length; ++i) {
        data[i] = string.charCodeAt(i);
      }
      return {data};
    }
    const hypothesis =
        {utterance: toString16(transcript), confidence: confidence};
    this.results_.push({hypotheses: [hypothesis], isProvisional: false});
  }

  setMockSpeechRecognitionError(errorCode) {
    this.error_ = {code: errorCode, details: SpeechAudioErrorDetails.kNone};
  }

  dispatchError() {
    this.session_client_.errorOccurred(this.error_);
    this.error_ = null;
    this.session_client_.ended();
  }

  dispatchResult() {
    this.session_client_.started();
    this.session_client_.audioStarted();
    this.session_client_.soundStarted();

    if (this.results_.length) {
      this.results_.forEach(result => {
        this.session_client_.resultRetrieved([result]);
      });
      this.results_ = [];
    } else {
      this.session_client_.errorOccurred({
        code: SpeechRecognitionErrorCode.kNoMatch,
        details: SpeechAudioErrorDetails.kNone
      });
    }
    this.session_client_.soundEnded();
    this.session_client_.audioEnded();
    this.session_client_.ended();
  }

  start(params) {
    this.session_ = new MockSpeechRecognitionSession(params.sessionReceiver, this);
    this.session_client_ = params.client;

    // if setMockSpeechRecognitionError was called
    if (this.error_) {
      this.dispatchError();
      return;
    }

    this.dispatchResult();
  }

  onDeviceWebSpeechAvailable(params) {
    return Promise.resolve(false);
  }

  installOnDeviceSpeechRecognition(params) {
    return Promise.resolve(false);
  }
}

class MockSpeechRecognitionSession {
  constructor(request, recognizer) {
    this.receiver_ = new SpeechRecognitionSessionReceiver(this);
    this.receiver_.$.bindHandle(request.handle);
    this.recognizer_ = recognizer;
  }

  abort() {}
  stopCapture() {}
}
