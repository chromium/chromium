import {MidiSessionReceiver, MidiSessionProvider, MidiSessionProviderReceiver, PortState, Result} from '/gen/media/midi/midi_service.mojom.m.js';

const MICROSECONDS_TO_MILLISECONDS = 0.001;

export class MockMIDIService {
  constructor() {
    this.next_input_port_index_ = 0;
    this.next_output_port_index_ = 0;
    this.start_session_result_ = Result.OK;

    this.interceptor_ = new MojoInterfaceInterceptor(
        MidiSessionProvider.$interfaceName);
    this.receiver_ = new MidiSessionProviderReceiver(this);
    this.sessionReceiver_ = new MidiSessionReceiver(this);
    this.interceptor_.oninterfacerequest =
        e => this.receiver_.$.bindHandle(e.handle);
    this.interceptor_.start();
  }

  setStartSessionResult(result) {
    this.start_session_result_ = result;
  }

  addInputPort(portState) {
    this.client_.addInputPort({
      id: `MockInputID-${this.next_input_port_index_++}`,
      manufacturer: 'MockInputManufacturer',
      name: 'MockInputName',
      version: 'MockInputVersion',
      state: portState
    });
  }

  addOutputPort(portState) {
    this.client_.addOutputPort({
      id: `MockOutputID-${this.next_output_port_index_++}`,
      manufacturer: 'MockOutputManufacturer',
      name: 'MockOutputName',
      version: 'MockOutputVersion',
      state: portState
    });
  }

  startSession(receiver, client) {
    this.client_ = client;
    this.sessionReceiver_.$.bindHandle(receiver.handle);
    this.addInputPort(PortState.CONNECTED);
    this.addOutputPort(PortState.CONNECTED);
    this.client_.sessionStarted(this.start_session_result_);
  }

  sendData(port, data, timestamp) {
    if (timestamp.internalValue > BigInt(internals.currentTimeTicks())) {
      const delayMicroseconds = Number(
          timestamp.internalValue - BigInt(internals.currentTimeTicks()));
      setTimeout(
          () => this.sendData(port, data, timestamp),
          delayMicroseconds * MICROSECONDS_TO_MILLISECONDS);
      return;
    }
    if (port < this.next_input_port_index_) {
      this.client_.dataReceived(port, data, timestamp);
    }
  }
}
