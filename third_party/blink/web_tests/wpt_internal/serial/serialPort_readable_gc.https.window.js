// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/gen/layout_test_data/mojo/public/js/mojo_bindings.js
// META: script=/gen/mojo/public/mojom/base/unguessable_token.mojom.js
// META: script=/gen/third_party/blink/public/mojom/serial/serial.mojom.js
// META: script=/serial/resources/common.js
// META: script=resources/automation.js

serial_test(async (t, fake) => {
  let fakePort;
  let chunkReceived;
  await (async () => {
    let port;
    ({port, fakePort} = await getFakeSerialPort(fake));

    // Select a buffer size larger than the amount of data transferred.
    await port.open({baudRate: 9600, bufferSize: 64});

    let writable;
    chunkReceived = new Promise(resolve => {
      writable = new WritableStream({
        write: function(chunk) {
          resolve();
        }
      });
    });

    port.readable.pipeTo(writable);
  })();

  GCController.collectAll();

  await fakePort.writable();
  const data = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
  fakePort.write(data);

  await chunkReceived;
}, 'Dropping references to a port does not close its streams');
