import {HapticEffect, HapticsService, HapticsServiceReceiver,} from '/gen/services/device/public/mojom/haptics.mojom.m.js';

const EFFECT_NAMES = Object.fromEntries(
    Object.entries(HapticEffect)
        .filter(([key]) => key.startsWith('k'))
        .map(([key, value]) => [value, key.slice(1).toLowerCase()]));

export class MockHapticsService {
  constructor(targetWindow = window) {
    assert_true(
        typeof targetWindow.MojoInterfaceInterceptor === 'function',
        'MojoJS must be enabled to intercept device.mojom.HapticsService');
    this.played_ = [];
    this.waiters_ = [];
    this.bound_ = false;
    this.receiver_ = new HapticsServiceReceiver(this);
    this.channel_ = null;

    this.interceptor_ = new targetWindow.MojoInterfaceInterceptor(
        HapticsService.$interfaceName);
    if (targetWindow === window) {
      this.interceptor_.oninterfacerequest = e => {
        this.bound_ = true;
        this.receiver_.$.bindHandle(e.handle);
      };
    } else {
      // Transfer the intercepted MojoHandle into this realm before binding.
      this.channel_ = new MessageChannel();
      this.channel_.port2.onmessage = e => {
        this.receiver_.$.bindHandle(e.data);
      };
      this.interceptor_.oninterfacerequest = e => {
        this.bound_ = true;
        this.channel_.port1.postMessage(e.handle, [e.handle]);
      };
    }
    this.interceptor_.start();
  }

  get bound() {
    return this.bound_;
  }

  get pendingCount() {
    return this.played_.length;
  }

  playHaptics(effect, intensity) {
    const record = {
      effect: EFFECT_NAMES[effect] ?? `unknown(${effect})`,
      intensity,
    };
    if (this.waiters_.length > 0) {
      this.waiters_.shift().resolve(record);
    } else {
      this.played_.push(record);
    }
  }

  nextHaptic() {
    if (this.played_.length > 0) {
      return Promise.resolve(this.played_.shift());
    }
    return new Promise((resolve, reject) => {
      this.waiters_.push({resolve, reject});
    });
  }

  async nextHaptics(count) {
    const results = [];
    for (let i = 0; i < count; i++) {
      results.push(await this.nextHaptic());
    }
    return results;
  }

  stop() {
    this.interceptor_.stop();
    if (this.channel_) {
      this.channel_.port1.close();
      this.channel_.port2.close();
    }
    this.receiver_.$.close();
    while (this.waiters_.length > 0) {
      this.waiters_.shift().reject(new Error('MockHapticsService stopped'));
    }
  }
}

// Creates an isolated same-origin iframe with a fresh sticky user activation
// state and an active MockHapticsService, cleaned up automatically after `t`.
export async function createHapticsTestContext(t, {allow} = {}) {
  const iframe = document.createElement('iframe');
  if (allow) {
    iframe.allow = allow;
  }
  const loaded = new Promise(resolve => {
    iframe.onload = resolve;
  });
  iframe.srcdoc = '<!DOCTYPE html><button id="target">activate</button>';
  document.body.appendChild(iframe);
  await loaded;

  const target = iframe.contentDocument.getElementById('target');
  const mock = new MockHapticsService(iframe.contentWindow);
  t.add_cleanup(() => {
    mock.stop();
    iframe.remove();
  });

  return {
    window: iframe.contentWindow,
    navigator: iframe.contentWindow.navigator,
    activate: () => new test_driver.Actions()
                        .pointerMove(0, 0, {origin: target})
                        .pointerDown()
                        .pointerUp()
                        .send(),
    mock,
  };
}
