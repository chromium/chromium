class EmptyProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    return true;
  }
}

registerProcessor('empty-processor', EmptyProcessor);