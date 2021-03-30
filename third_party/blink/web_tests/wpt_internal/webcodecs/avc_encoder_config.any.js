// META: global=window,dedicatedworker
// META: script=/wpt_internal/webcodecs/encoder_utils.js

const defaultWidth = 640;
const defaultHeight = 360;

let frameNumber = 0;

async function configureAndEncode(encoder, config) {
  encoder.configure(config);

  let frame = await createFrame(defaultWidth, defaultHeight, ++frameNumber);
  encoder.encode(frame, { keyFrame : true });
  return encoder.flush()
}

function cycleAvcOutputFormats(acc, desc) {
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
      hardwareAcceleration: acc,
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
    assert_not_equals(output.chunk.data, null, "chunk.data annexb");

    let startCode = new Int8Array(output.chunk.data.slice(0,4));

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
  }, desc);
}

cycleAvcOutputFormats("deny",
  "Test AvcConfig supports 'avc' and 'annexb' (acceleration = 'deny')");

cycleAvcOutputFormats("allow",
  "Test AvcConfig supports 'avc' and 'annexb' (acceleration = 'allow')");

/* Uncomment this for manual testing, before we have GPU tests for that */
// cycleAvcOutputFormats("require",
//   "Test AvcConfig supports 'avc' and 'annexb' (acceleration = 'require')");

promise_test(async t => {
  let encoder = new VideoEncoder({
    error: () => t.unreached_func("Unexpected error"),
    output: () => t.unreached_func("Unexpected output"),
  });

  const vp8Config = {
    codec: 'vp8',
    hardwareAcceleration: "allow",
    width: defaultWidth,
    height: defaultHeight,
    avc: { outputFormat: "avc" },
  };

  assert_throws_js(TypeError,
    () => { encoder.configure(vp8Config); },
    "Only H264 should support avcOptions");

  encoder.close();
}, "Make sure non-H264 configurations reject avcOptions");
