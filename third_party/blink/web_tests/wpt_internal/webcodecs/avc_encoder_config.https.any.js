// META: global=window,dedicatedworker
// META: script=/wpt_internal/webcodecs/encoder_utils.js

const defaultWidth = 640;
const defaultHeight = 360;

let frameNumber = 0;

async function configureAndEncode(encoder, config) {
  encoder.configure(config);

  let frame = createFrame(defaultWidth, defaultHeight, ++frameNumber);
  encoder.encode(frame, { keyFrame : true });
  frame.close();
  return encoder.flush()
}

promise_test(async t => {
  var output = undefined;

  let encoderInit = {
    error: () => t.unreached_func("Unexpected error"),
    output: (chunk, metadata) => {
      let config = metadata.decoderConfig;
      assert_equals(output, undefined, "output undefined sanity");
      output = {
        chunk: chunk,
        config: config,
      };
    },
  };

  let encoder = new VideoEncoder(encoderInit);

  let encoderConfig = {
    codec: "avc1.42001E",
    width: defaultWidth,
    height: defaultHeight,
  };

  // Configure an encoder with no avcOptions (should default to avc format).
  await configureAndEncode(encoder, encoderConfig);

  // avc chunks should output a config with an avcC description.
  assert_not_equals(output, undefined, "output default");
  assert_not_equals(output.chunk, null, "chunk default");
  assert_not_equals(output.config, null, "config default");
  assert_not_equals(output.config.description, null, "desc default");

  output = undefined;

  // Configure with annex-b.
  encoderConfig.avc = { format: "annexb" };
  await configureAndEncode(encoder, encoderConfig);

  // annexb chunks should start with a start code.
  assert_not_equals(output, undefined, "output annexb");
  assert_not_equals(output.chunk, null, "chunk annexb");
  assert_greater_than(output.chunk.byteLength, 4, "chunk annexb data");

  let chunkData = new Uint8Array(output.chunk.byteLength);
  output.chunk.copyTo(chunkData);

  let startCode = new Int8Array(chunkData.buffer, 0, 4);
  assert_equals(startCode[0], 0x00, "startCode [0]");
  assert_equals(startCode[1], 0x00, "startCode [1]");
  assert_equals(startCode[2], 0x00, "startCode [2]");
  assert_equals(startCode[3], 0x01, "startCode [3]");

  // There should not be an avcC 'description' with annexb.
  assert_not_equals(output.config, null, "config annexb");
  assert_equals(output.config.description, undefined, "desc annexb");

  output = undefined;

  // Configure with avc.
  encoderConfig.avc = { format: "avc" };
  await configureAndEncode(encoder, encoderConfig);

  // avc should output a config with an avcC description.
  assert_not_equals(output, undefined, "output avc");
  assert_not_equals(output.chunk, null, "chunk avc");
  assert_not_equals(output.config, null, "config avc");
  assert_not_equals(output.config.description, null, "desc avc");

  encoder.close();
}, "Test AvcConfig supports 'avc' and 'annexb'");

promise_test(async t => {
  let goodEncoderConfig = {codec: 'avc1.42001E', width: 128, height: 128};
  let support = await VideoEncoder.isConfigSupported(goodEncoderConfig);
  assert_true(support.supported);

  let badEncoderConfig = {codec: 'avc1.42001E', width: 129, height: 129};
  support = await VideoEncoder.isConfigSupported(badEncoderConfig);
  assert_false(support.supported);
}, 'Test H.264 only supports even sizes');

promise_test(async t => {
  // Spot test a few levels.

  // level 3.0.
  const LEVEL_30 = 'avc1.42001E';
  let support = await VideoEncoder.isConfigSupported(
      {codec: LEVEL_30, width: 720, height: 576});
  assert_true(support.supported);
  support = await VideoEncoder.isConfigSupported(
      {codec: LEVEL_30, width: 722, height: 576});
  assert_false(support.supported);
  support = await VideoEncoder.isConfigSupported(
      {codec: LEVEL_30, width: 720, height: 578});
  assert_false(support.supported);

  // level 4.0.
  const LEVEL_40 = 'avc1.420028';
  support = await VideoEncoder.isConfigSupported(
      {codec: LEVEL_40, width: 2048, height: 1024});
  if (support.supported) {
    support = await VideoEncoder.isConfigSupported(
        {codec: LEVEL_40, width: 2050, height: 1024});
    assert_false(support.supported);
    support = await VideoEncoder.isConfigSupported(
        {codec: LEVEL_40, width: 2048, height: 1026});
    assert_false(support.supported);
  }

  // level 5.1.
  const LEVEL_51 = 'avc1.420033';
  support = await VideoEncoder.isConfigSupported(
      {codec: LEVEL_51, width: 4096, height: 2304});
  if (support.supported) {
    support = await VideoEncoder.isConfigSupported(
        {codec: LEVEL_51, width: 4098, height: 2304});
    assert_false(support.supported);
    support = await VideoEncoder.isConfigSupported(
        {codec: LEVEL_51, width: 4096, height: 2306});
    assert_false(support.supported);
  }

  // level 6.0.
  const LEVEL_60 = 'avc1.42003C';
  support = await VideoEncoder.isConfigSupported(
      {codec: LEVEL_60, width: 8192, height: 4320});
  if (support.supported) {
    support = await VideoEncoder.isConfigSupported(
        {codec: LEVEL_60, width: 8194, height: 4320});
    assert_false(support.supported);
    support = await VideoEncoder.isConfigSupported(
        {codec: LEVEL_60, width: 8192, height: 4322});
    assert_false(support.supported);
  }
}, 'Resolutions exceeding H.264 level are rejected');
