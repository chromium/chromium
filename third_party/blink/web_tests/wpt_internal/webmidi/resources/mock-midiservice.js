'use strict'

let MICROSECONDS_TO_MILLISECONDS = 0.001;

class MockMIDIService {
  constructor() {
    this.next_input_port_index_ = 0;
    this.next_output_port_index_ = 0;
    this.start_session_result_ = midi.mojom.Result.OK;

    this.interceptor_ = new MojoInterfaceInterceptor(
        midi.mojom.MidiSessionProvider.name);
    this.binding_ = new mojo.Binding(midi.mojom.MidiSessionProvider, this);
    this.session_binding_ = new mojo.Binding(midi.mojom.MidiSession, this);
    this.interceptor_.oninterfacerequest = e => {
      this.binding_.bind(e.handle);
    };
    this.interceptor_.start();
  }

  setStartSessionResult(result) {
    this.start_session_result_ = result;
  }

  addInputPort(portState) {
    this.client_.addInputPort(new midi.mojom.PortInfo({
      id: `MockInputID-${this.next_input_port_index_++}`,
      manufacturer: 'MockInputManufacturer',
      name: 'MockInputName',
      version: 'MockInputVersion',
      state: portState
    }));
  }

  addOutputPort(portState) {
    this.client_.addOutputPort(new midi.mojom.PortInfo({
      id: `MockOutputID-${this.next_output_port_index_++}`,
      manufacturer: 'MockOutputManufacturer',
      name: 'MockOutputName',
      version: 'MockOutputVersion',
      state: portState
    }));
  }

  startSession(request, client) {
    this.client_ = client;
    this.session_binding_.bind(request);
    this.addInputPort(midi.mojom.PortState.CONNECTED);
    this.addOutputPort(midi.mojom.PortState.CONNECTED);
    this.client_.sessionStarted(this.start_session_result_);
  }

  sendData(port, data, timestamp) {
    if (timestamp.internalValue > internals.currentTimeTicks()) {
      setTimeout(
          this.sendData.bind(this, port, data, timestamp),
          (timestamp.internalValue - internals.currentTimeTicks())
            * MICROSECONDS_TO_MILLISECONDS);
      return;
    }
    if (port < this.next_input_port_index_) {
      this.client_.dataReceived(port, data, timestamp);
    }
  }
}

let mockMIDIService = undefined;
try { mockMIDIService = new MockMIDIService(); }
catch (err) {
  // InvalidModificationError can be thrown if an interceptor has already been
  // created in the same process. In this case, we just rely on the mock
  // that's already been created.
  if (err.name != "InvalidModificationError")
    throw err;
}
