class NewInProcess extends AudioWorkletProcessor {
  constructor() {
    super();
    this.message = {threw: false};
  }
  process(inputs, outputs, parameters){
    try {
      new AudioWorkletProcessor();
    } catch (e) {
      this.message.threw = true;
      this.message.errorName = e.name;
      this.port.postMessage(this.message);
    }
  }
}
registerProcessor("new-in-process", NewInProcess);
