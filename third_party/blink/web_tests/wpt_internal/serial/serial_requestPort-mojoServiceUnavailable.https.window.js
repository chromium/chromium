// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

promise_test(async (t) => {
  const {SerialService} = await import(
      '/gen/third_party/blink/public/mojom/serial/serial.mojom.m.js');

  let interceptor =
      new MojoInterfaceInterceptor(SerialService.$interfacName);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();

  await trustedClick();
  try {
    await promise_rejects_dom(
        t, 'NotFoundError', navigator.serial.requestPort());
  } finally {
    interceptor.stop();
  }
}, 'requestPort() rejects if Mojo service connection fails');
