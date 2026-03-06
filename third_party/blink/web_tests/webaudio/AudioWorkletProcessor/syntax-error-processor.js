class SyntaxErrorProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.syntaxError = new .
  }

  process(inputs, outputs, parameters) {
    return true;
  }
}

registerProcessor('syntax-error-processor', SyntaxErrorProcessor);
