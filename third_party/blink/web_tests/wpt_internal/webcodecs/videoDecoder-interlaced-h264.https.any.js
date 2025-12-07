// META: global=window,dedicatedworker

// From external/wpt/webcodecs/h264_interlaced.mp4
const CONFIG = {
  codec: 'avc1.64000b',
  codedWidth: 320,
  codedHeight: 240,
  description: new Uint8Array([
    0x01, 0x64, 0x00, 0x15, 0xFF, 0xE1, 0x00, 0x1A, 0x67, 0x64, 0x00, 0x15,
    0xAC, 0xD9, 0x41, 0x41, 0x0F, 0xCB, 0x80, 0x88, 0x00, 0x00, 0x03, 0x00,
    0x08, 0x00, 0x00, 0x03, 0x00, 0xA0, 0xF8, 0xA1, 0x4C, 0xB0, 0x01, 0x00,
    0x06, 0x68, 0xFB, 0xA3, 0xCB, 0x22, 0xC0, 0xFD, 0xF8, 0xF8, 0x00
  ]),
  displayAspectWidth: 320,
  displayAspectHeight: 240,
};

async function assert_supports_h264() {
  const support = await VideoDecoder.isConfigSupported({codec: 'avc1.64000b'});
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
}, 'Test interlaced h.264 through isConfigSupported');
