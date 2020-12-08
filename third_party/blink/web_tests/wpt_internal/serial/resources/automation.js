// Returns a SerialPort instance and associated FakeSerialPort instance.
async function getFakeSerialPort(fake) {
  let token = fake.addPort();
  let fakePort = fake.getFakePort(token);

  let ports = await navigator.serial.getPorts();
  assert_equals(ports.length, 1);

  let port = ports[0];
  assert_true(port instanceof SerialPort);

  return { port, fakePort };
}

let fakeSerialService = undefined;

function serial_test(func, name, properties) {
  promise_test(async (test) => {
    if (fakeSerialService === undefined) {
      await loadMojoResources([
        '/gen/mojo/public/mojom/base/unguessable_token.mojom.js',
        '/gen/services/device/public/mojom/serial.mojom.js',
        '/gen/third_party/blink/public/mojom/serial/serial.mojom.js',
      ]);
      await loadScript('resources/fake-serial.js');
    }

    fakeSerialService.start();
    try {
      await func(test, fakeSerialService);
    } finally {
      fakeSerialService.stop();
      fakeSerialService.reset();
    }
  }, name, properties);
}

function trustedClick() {
  return new Promise(resolve => {
    let button = document.createElement('button');
    button.textContent = 'click to continue test';
    button.style.display = 'block';
    button.style.fontSize = '20px';
    button.style.padding = '10px';
    button.onclick = () => {
      document.body.removeChild(button);
      resolve();
    };
    document.body.appendChild(button);
    test_driver.click(button);
  });
}
