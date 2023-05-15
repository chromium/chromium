class GCProcessor extends AudioWorkletProcessor {
  constructor() {
    super();

    this._toBeCollected = false;

    this.port.onmessage = async (event) => {
      switch (event.data.command) {
        case 'finish':
          this._toBeCollected = true;
          break;
        case 'gc':
          await gc({execution: 'async'});
          this.port.postMessage({status: 'collected'});
          break;
        default:
          console.error('NOT REACHED');
      }
   };

   this.port.postMessage({status: 'created'});
  }

  // When asked from the main thread, this processor is going to terminate
  // itself to be marked for GC.
  process() {
    if (!this._toBeCollected) {
      return true;
    }

    this.port.postMessage({status: 'finished'});
    this.port.close();
    return false;
  }
}

registerProcessor('gc-processor', GCProcessor);