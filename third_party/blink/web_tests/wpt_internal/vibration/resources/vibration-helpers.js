import {VibrationManager, VibrationManagerReceiver} from '/gen/services/device/public/mojom/vibration_manager.mojom.m.js';

// A helper for forwarding MojoHandle instances from one frame to another.
class CrossFrameHandleProxy {
  constructor(callback) {
    let {handle0, handle1} = Mojo.createMessagePipe();
    this.sender_ = handle0;
    this.receiver_ = handle1;
    this.receiver_.watch({readable: true}, () => {
      var message = this.receiver_.readMessage();
      callback(message.handles[0]);
    });
  }

  forwardHandle(handle) {
    this.sender_.writeMessage(new ArrayBuffer, [handle]);
  }
}

class MockVibrationManager {
  constructor() {
    this.receiver_ = new VibrationManagerReceiver(this);
    this.interceptor_ =
        new MojoInterfaceInterceptor(VibrationManager.$interfaceName);
    this.interceptor_.oninterfacerequest =
        e => this.receiver_.$.bindHandle(e.handle);
    this.interceptor_.start();
    this.crossFrameHandleProxy_ = new CrossFrameHandleProxy(
        handle => this.receiver_.$.bindHandle(handle));

    this.vibrate_milliseconds_ = -1;
    this.cancelled_ = false;
  }

  attachToWindow(otherWindow) {
    otherWindow.vibrationManagerInterceptor =
        new otherWindow.MojoInterfaceInterceptor(
            VibrationManager.$interfaceName);
    otherWindow.vibrationManagerInterceptor.oninterfacerequest =
        e => this.crossFrameHandleProxy_.forwardHandle(e.handle);
    otherWindow.vibrationManagerInterceptor.start();
  }

  vibrate(milliseconds) {
    this.vibrate_milliseconds_ = Number(milliseconds);
    window.postMessage('Vibrate', '*');
    return Promise.resolve();
  }

  cancel() {
    this.cancelled_ = true;
    window.postMessage('Cancel', '*');
    return Promise.resolve();
  }

  getDuration() {
    return this.vibrate_milliseconds_;
  }

  isCancelled() {
    return this.cancelled_;
  }

  reset() {
    this.vibrate_milliseconds_ = -1;
    this.cancelled_ = false;
  }
}

let mockVibrationManager = new MockVibrationManager();

export function vibration_test(func, name, properties) {
  promise_test(async function() {
    try {
      await Promise.resolve(func({
        mockVibrationManager: mockVibrationManager
      }));
    } finally {
      mockVibrationManager.reset();
    }
  }, name, properties);
}
