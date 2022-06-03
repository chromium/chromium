'use strict';
const test_desc = 'Descriptor gets removed. Reject with InvalidStateError.';
const expected = new DOMException('GATT Descriptor no longer exists.',
    'InvalidStateError');
let descriptor, fake_descriptor, fake_characteristic, fake_peripheral;

bluetooth_test(() => getUserDescriptionDescriptor()
    .then(_ =>
      ({descriptor, fake_descriptor, fake_characteristic, fake_peripheral} = _))
    .then(() => fake_descriptor.remove())
    .then(() => fake_peripheral.simulateGATTServicesChanged())
    .then(() => assert_promise_rejects_with_message(
        descriptor.CALLS([
          readValue()|
          writeValue(new Uint8Array(1 /* length */))
        ]),
        expected,
        'Descriptor got removed')),
    test_desc);
