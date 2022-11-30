// META: global=window,dedicatedworker

async function supports_h264() {
  const config = {codec: 'avc1.64000b'};
  const support = await VideoDecoder.isConfigSupported(config);
  return support.supported;
}

promise_test(async t => {
  if (!(await supports_h264())) {
    return;
  }

  const config = {
    codec: 'avc1.64000b',
    codedWidth: 24000,
    codedHeight: 24000,
  };

  // Should not throw TypeError.
  const support = await VideoDecoder.isConfigSupported(config);
  assert_false(support.supported, 'supported');
}, 'Test isConfigSupported() with unsupported coded size');
