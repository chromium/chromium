// META: script=/resources/test-only-api.js

promise_test(async () => {
  await loadMojoResources([
    '/gen/mojo/public/mojom/base/unguessable_token.mojom.js',
    '/gen/third_party/blink/public/mojom/serial/serial.mojom.js',
  ]);

  let interceptor =
      new MojoInterfaceInterceptor(blink.mojom.SerialService.name);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();

  let ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 0);

  interceptor.stop();
}, 'getPorts() returns empty list if Mojo service connection fails');
