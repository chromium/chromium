// META: global=window,dedicatedworker
// META: script=/wpt_internal/webcodecs/encoder_utils.js

async function encode_decode_test(codec, acc, avc_options) {
  const w = 640;
  const h = 360;
  let next_ts = 0
  let frames_to_encode = 16;
  let frames_encoded = 0;
  let frames_decoded = 0;
  let errors = 0;

  let decoder = new VideoDecoder({
    output(frame) {
      assert_equals(frame.cropWidth, w, "cropWidth");
      assert_equals(frame.cropHeight, h, "cropHeight");
      assert_equals(frame.timestamp, next_ts++, "timestamp");
      frames_decoded++;
      frame.close();
    },
    error(e) {
      errors++;
      console.log(e.message);
    }
  });

  const encoder_init = {
    output(chunk, config) {
      var data = new Uint8Array(chunk.data);
      if (decoder.state != "configured" || config.description) {
        decoder.configure(config);
      }
      decoder.decode(chunk);
      frames_encoded++;
    },
    error(e) {
      errors++;
      console.log(e.message);
    }
  };

  let encoder_config = {
    codec: codec,
    acceleration: acc,
    width: w,
    height: h,
    bitrate: 5000000,
  };

  if (avc_options !== null) {
    encoder_config.avcOptions = avc_options;
  }

  let encoder = new VideoEncoder(encoder_init);
  encoder.configure(encoder_config);

  for (let i = 0; i < frames_to_encode; i++) {
    let frame = await createFrame(w, h, i);
    let keyframe = (i % 5 == 0);
    encoder.encode(frame, { keyFrame: keyframe });

    // Wait to prevent queueing all frames before encoder.configure() completes.
    // Queuing them all at once should still work, but would not be as
    // repesentative of a real world scenario.
    await delay(1);
  }
  await encoder.flush();
  await decoder.flush();
  encoder.close();
  decoder.close();
  assert_equals(frames_encoded, frames_to_encode);
  assert_equals(frames_decoded, frames_to_encode);
  assert_equals(errors, 0);
}

async function encode_test(codec, acc) {
  let w = 640;
  let h = 360;
  let next_ts = 0
  let frames_to_encode = 25;
  let frames_processed = 0;
  let errors = 0;

  let process_video_chunk = function (chunk, config) {
    assert_greater_than_equal(chunk.timestamp, next_ts++);
    let data = new Uint8Array(chunk.data);
    let type = (chunk.timestamp % 5 == 0) ? "key" : "delta";
    assert_equals(chunk.type, type);
    assert_greater_than_equal(data.length, 0);
    if (config) {
      assert_equals(config.codec, codec);
      assert_equals(config.codedWidth, w);
      assert_equals(config.codedHeight, h);
      let data = new Uint8Array(config.description);
    }
    frames_processed++;
  };

  const init = {
    output: process_video_chunk,
    error: (e) => {
      errors++;
      console.log(e.message);
    },
  };
  const params = {
    codec: codec,
    acceleration: acc,
    width: w,
    height: h,
    bitrate: 5000000,
    framerate: 24,
  };
  let encoder = new VideoEncoder(init);
  encoder.configure(params);
  for (let i = 0; i < frames_to_encode; i++) {
    let size_mismatch = (i % 16);
    let frame = await createFrame(w + size_mismatch, h + size_mismatch, i);
    let keyframe = (i % 5 == 0);
    encoder.encode(frame, { keyFrame: keyframe });
    await delay(1);
  }
  await encoder.flush();
  encoder.close();
  assert_equals(frames_processed, frames_to_encode);
  assert_equals(errors, 0);
}

promise_test(encode_test.bind(null, "vp09.00.10.08", "allow"),
  "encoding vp9 profile0");

promise_test(encode_test.bind(null, "vp09.02.10.10", "allow"),
  "encoding vp9 profile2");

promise_test(encode_decode_test.bind(null, "vp09.02.10.10", "allow", null),
  "encoding and decoding vp9 profile2");

promise_test(encode_test.bind(null, "vp8", "allow"),
  "encoding vp8");

promise_test(encode_decode_test.bind(null, "vp8", "allow", null),
  "encoding and decoding vp8");

promise_test(
  encode_decode_test.bind(null, "avc1.42001E", "allow", { outputFormat: "annexb"}),
  "encoding and decoding avc1.42001E (annexb)");

promise_test(
  encode_decode_test.bind(null, "avc1.42001E", "allow", { outputFormat: "avc"}),
  "encoding and decoding avc1.42001E (avc)");

/* Uncomment this for manual testing, before we have GPU tests for that */
// promise_test(encode_test.bind(null, "avc1.42001E", "require"),
//  "encoding avc1.42001E");

// promise_test(encode_decode_test.bind(null, "avc1.42001E", "require"),
//  "encoding and decoding avc1.42001E req");
