// META: global=window,dedicatedworker
// META: script=/wpt_internal/webcodecs/encoder_utils.js

async function svc_test(codec, layers, base_layer_decimator) {
  const w = 320;
  const h = 200;
  const acc = "prefer-software";
  let frames_to_encode = 40;
  let frames_decoded = 0;
  let frames_encoded = 0;
  let errors = 0;
  let chunks = [];
  let corrupted_frames = [];

  const encoder_init = {
    output(chunk, metadata) {
      frames_encoded++;

      // Filter out all frames, but base layer.
      assert_own_property(metadata, "svc");
      assert_own_property(metadata.svc, "temporalLayerId");
      assert_less_than(metadata.svc.temporalLayerId, layers);
      if (metadata.svc.temporalLayerId == 0)
        chunks.push(chunk);
    },
    error(e) {
      errors++;
    }
  };

  let encoder_config = {
    codec: codec,
    hardwareAcceleration: acc,
    width: w,
    height: h,
    bitrate: 5000000,
    scalabilityMode: "L1T" + layers,
  };

  if (codec.includes("avc"))
    encoder_config.avc = {format: "annexb"};

  let encoder = new VideoEncoder(encoder_init);
  encoder.configure(encoder_config);

  for (let i = 0; i < frames_to_encode; i++) {
    let frame = createFrame(w, h, i);
    encoder.encode(frame, { keyFrame: false });
    frame.close();
  }
  await encoder.flush();
  assert_equals(errors, 0);

  let decoder = new VideoDecoder({
    output(frame) {
      frames_decoded++;
      // Check that we have intended number of dots and no more.
      // Completely black frame shouldn't pass the test.
      if(!validateBlackDots(frame, frame.timestamp) ||
         validateBlackDots(frame, frame.timestamp + 1)) {
        corrupted_frames.push(frame.timestamp)
      }
      frame.close();
    },
    error(e) {
      errors++;
    }
  });

  let decoder_config = {
    codec: codec,
    hardwareAcceleration: acc,
    codedWidth: w,
    codedHeight: h,
  };
  decoder.configure(decoder_config);

  for (let chunk of chunks) {
    decoder.decode(chunk);
    await delay(1);
  }
  await decoder.flush();
  assert_equals(errors, 0);

  encoder.close();
  decoder.close();
  assert_equals(frames_encoded, frames_to_encode);

  let base_layer_frames = frames_to_encode / base_layer_decimator;
  assert_equals(chunks.length, base_layer_frames);
  assert_equals(frames_decoded, base_layer_frames);
  assert_equals(corrupted_frames.length, 0,
    `corrupted_frames: ${corrupted_frames}`);
}

promise_test(svc_test.bind(null, "avc1.42001E", 2, 2), "SVC H264 L1T2");
promise_test(svc_test.bind(null, "avc1.42001E", 3, 4), "SVC H264 L1T3");

promise_test(svc_test.bind(null, "vp8", 2, 2), "SVC VP8 L1T2");
promise_test(svc_test.bind(null, "vp8", 3, 4), "SVC VP8 L1T3");

promise_test(svc_test.bind(null, "vp09.00.10.08", 2, 2), "SVC VP9 8 L1T2");
promise_test(svc_test.bind(null, "vp09.00.10.08", 3, 4), "SVC VP9 8 L1T3");

promise_test(svc_test.bind(null, "vp09.02.10.10", 2, 2), "SVC VP9 10 L1T2");
promise_test(svc_test.bind(null, "vp09.02.10.10", 3, 4), "SVC VP9 10 L1T3");

promise_test(async t => {
  for (let i = 0; i < 10; i++) {
    let frame = createFrame(320, 200, i);
    assert_true(validateBlackDots(frame, i));
    assert_false(validateBlackDots(frame, i + 1));
  }
}, "Dot counting self test");
