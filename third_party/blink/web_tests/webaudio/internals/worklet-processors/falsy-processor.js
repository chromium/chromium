class FalsyProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  // Returns false immediately, and in turn this processor is going to end
  // shortly and be marked for GC. See more information on
  // AudioWorkletProcessor's return value:
  // https://www.w3.org/TR/webaudio/#callback-audioworketprocess-callback
  // https://www.w3.org/TR/webaudio/#rendering-a-graph
  process() {
    this.port.postMessage({});
    this.port.close();
    return false;
  }
}

registerProcessor('falsy-processor', FalsyProcessor);