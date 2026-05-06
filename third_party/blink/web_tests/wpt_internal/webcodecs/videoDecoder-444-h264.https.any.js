// META: global=window,dedicatedworker

// From external/wpt/webcodecs/h264_444.mp4
const CONFIG = {
  codec: 'avc1.f4000c',
  codedWidth: 320,
  codedHeight: 240,
  description: new Uint8Array([
    0x01, 0xf4, 0x00, 0x0c, 0xff, 0xe1, 0x00, 0x19, 0x67, 0xf4, 0x00,
    0x0c, 0x91, 0x9b, 0x28, 0x28, 0x3f, 0x60, 0x22, 0x00, 0x00, 0x03,
    0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x28, 0x1e, 0x28, 0x53, 0x2c,
    0x01, 0x00, 0x06, 0x68, 0xeb, 0xe3, 0xc4, 0x48, 0x44
  ]),
  displayAspectWidth: 320,
  displayAspectHeight: 240,
};

async function assert_supports_h264() {
  const support = await VideoDecoder.isConfigSupported({codec: 'avc1.f4000c'});
  assert_implements_optional(support.supported, 'h264 supported');
}

promise_test(async t => {
  await assert_supports_h264();

  let support = await VideoDecoder.isConfigSupported(CONFIG);
  assert_true(support.supported, 'supported');

  CONFIG.hardwareAcceleration = 'no-preference';
  support = await VideoDecoder.isConfigSupported(CONFIG);
  assert_true(support.supported, 'supported');

  CONFIG.hardwareAcceleration = 'prefer-software';
  support = await VideoDecoder.isConfigSupported(CONFIG);
  assert_true(support.supported, 'supported');

  CONFIG.hardwareAcceleration = 'prefer-hardware';
  support = await VideoDecoder.isConfigSupported(CONFIG);
  assert_false(support.supported, 'supported');
}, 'Test 4:4:4 h.264 through isConfigSupported');
