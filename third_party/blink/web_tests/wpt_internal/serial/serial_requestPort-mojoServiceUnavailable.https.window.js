// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/resources/test-only-api.js
// META: script=/serial/resources/automation.js

promise_test(async (t) => {
  await loadMojoResources([
    '/gen/mojo/public/mojom/base/unguessable_token.mojom.js',
    '/gen/third_party/blink/public/mojom/serial/serial.mojom.js',
  ]);

  let interceptor =
      new MojoInterfaceInterceptor(blink.mojom.SerialService.name);
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
