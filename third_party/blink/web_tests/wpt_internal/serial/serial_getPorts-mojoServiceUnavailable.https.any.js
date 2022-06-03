// META: script=/resources/test-only-api.js

promise_test(async () => {
  const {SerialService} = await import(
      '/gen/third_party/blink/public/mojom/serial/serial.mojom.m.js');

  let interceptor =
      new MojoInterfaceInterceptor(SerialService.$interfaceName);
  interceptor.oninterfacerequest = e => e.handle.close();
  interceptor.start();

  let ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 0);

  interceptor.stop();
}, 'getPorts() returns empty list if Mojo service connection fails');
