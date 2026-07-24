class SimpleProcessor extends AudioWorkletProcessor {
  process(inputs, outputs, parameters) {
    return true;
  }
}
registerProcessor('simple-processor', SimpleProcessor);
