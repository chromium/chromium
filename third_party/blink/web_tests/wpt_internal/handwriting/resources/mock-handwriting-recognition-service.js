import {
  CreateHandwritingRecognizerResult,
  HandwritingRecognitionService,
  HandwritingRecognitionServiceReceiver,
  HandwritingRecognizerReceiver,
  HandwritingRecognizerRemote,
  HandwritingRecognitionType,
  HandwritingInputType,
} from '/gen/third_party/blink/public/mojom/handwriting/handwriting.mojom.m.js';

// Generates the prediction result based on strokes and hints.
// The segmentation result is empty.
function transformHandwritingMojoStroke(stroke) {
  return stroke.points.map(point => ({
    x: Math.round(point.location.x),
    y: Math.round(point.location.y),
    t: Math.round(Number(point.t.microseconds) / 1000)}));
}

function transformHandwritingIDLStroke(stroke) {
  return stroke.getPoints().map(point => ({
    x: Math.round(point.x),
    y: Math.round(point.y),
    t: Math.round(point.t)}));
}

// We need to export this function because we will verify whether the prediction
// result is as expected.
export function generateHandwritingPrediction(strokes, hints) {
  const result = { strokes: [] };
  for (let i = 0; i < strokes.length; i++) {
    // Check which kind of stroke it is. Mojo Stroke should have a `points`
    // member and IDL stroke does not.
    // Note that `strokes[i] instanceof HandwritingStroke` does not work here.
    if ('points' in strokes[i]) {
      result.strokes.push(transformHandwritingMojoStroke(strokes[i]));
    } else {
      result.strokes.push(transformHandwritingIDLStroke(strokes[i]));
    }
  }
  result.hints = hints;
  return [{text: JSON.stringify(result), segmentationResult: []}];
}

class MockHandwritingRecognizer {
  // In this mock impl, we ignore the `modelConstraint`.
  constructor(modelConstraint) {}

  bind(request) {
    this.receiver_ = new HandwritingRecognizerReceiver(this);
    this.receiver_.$.bindHandle(request.handle);
  }

  async getPrediction(strokes, hints) {
    return {prediction: generateHandwritingPrediction(strokes, hints)};
  }
}

let mockHandwritingRecognizer =
    new MockHandwritingRecognizer({languages: ['en']});

class MockHandwritingRecognitionService {
  constructor() {
    this.interceptor_ = new MojoInterfaceInterceptor(
        HandwritingRecognitionService.$interfaceName);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.receiver_ = new HandwritingRecognitionServiceReceiver(this);

    this.interceptor_.start();
  }

  bind(handle) {
    this.receiver_.$.bindHandle(handle);
  }

  async createHandwritingRecognizer(modelConstraint) {
    const handwritingRecognizer = new HandwritingRecognizerRemote();
    mockHandwritingRecognizer.bind(
        handwritingRecognizer.$.bindNewPipeAndPassReceiver());

    return {
      result: CreateHandwritingRecognizerResult.kOk,
      handwritingRecognizer: handwritingRecognizer,
    };
  }

  async queryHandwritingRecognizer(constraint) {
    // Pretend to support all features.
    let desc =  {
      textAlternatives: true,
      textSegmentation: true,
      hints: {
        recognitionType: [
          HandwritingRecognitionType.kText,
        ],
        inputType: [
          HandwritingInputType.kMouse,
          HandwritingInputType.kStylus,
          HandwritingInputType.kTouch,
        ],
        textContext: true,
        alternatives: true,
      }
    };

    return { result: desc };
  }
}

let mockHandwritingRecognitionService = new MockHandwritingRecognitionService();
