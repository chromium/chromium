// META: global=window,dedicatedworker
// META: script=/wpt_internal/webcodecs/encoder_utils.js

async function encode_decode_test(codec, avc_format) {
  const acc = "prefer-software";
  const w = 320;
  const h = 200;
  let next_ts = 0
  let frames_to_encode = 16;
  let frames_encoded = 0;
  let frames_decoded = 0;
  let errors = 0;

  let decoder = new VideoDecoder({
    output(frame) {
      assert_equals(frame.visibleRect.width, w, "visibleRect.width");
      assert_equals(frame.visibleRect.height, h, "visibleRect.height");
      assert_equals(frame.timestamp, next_ts++, "timestamp");
      frames_decoded++;
      assert_true(validateBlackDots(frame, frame.timestamp),
        "frame doesn't match. ts: " + frame.timestamp);
      frame.close();
    },
    error(e) {
      errors++;
      console.log(e.message);
    }
  });

  const encoder_init = {
    output(chunk, metadata) {
      let config = metadata.decoderConfig;
      if (config) {
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
    hardwareAcceleration: acc,
    width: w,
    height: h,
    bitrate: 1000000,
    bitrateMode: "constant"
  };

  if (avc_format != null) {
    encoder_config.avc = {format: avc_format};
  }

  let encoder = new VideoEncoder(encoder_init);
  encoder.configure(encoder_config);

  for (let i = 0; i < frames_to_encode; i++) {
    let frame = createFrame(w, h, i);
    let keyframe = (i % 5 == 0);
    encoder.encode(frame, { keyFrame: keyframe });
    frame.close();
  }
  await encoder.flush();
  await decoder.flush();
  encoder.close();
  decoder.close();
  assert_equals(frames_encoded, frames_to_encode);
  assert_equals(frames_decoded, frames_to_encode);
  assert_equals(errors, 0);
}

async function encode_test(codec) {
  const acc = "prefer-software";
  let w = 320;
  let h = 200;
  let next_ts = 0
  let frames_to_encode = 25;
  let frames_processed = 0;
  let errors = 0;

  let process_video_chunk = function (chunk, metadata) {
    assert_greater_than_equal(chunk.timestamp, next_ts++);
    let config = metadata.decoderConfig;
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
    hardwareAcceleration: acc,
    width: w,
    height: h,
    bitrate: 5000000,
    framerate: 24,
  };
  let encoder = new VideoEncoder(init);
  encoder.configure(params);
  for (let i = 0; i < frames_to_encode; i++) {
    let size_mismatch = (i % 16);
    let frame = createFrame(w + size_mismatch, h + size_mismatch, i);
    let keyframe = (i % 5 == 0);
    encoder.encode(frame, { keyFrame: keyframe });
    frame.close();
  }
  await encoder.flush();
  encoder.close();
  assert_equals(frames_processed, frames_to_encode);
  assert_equals(errors, 0);
}

promise_test(
    encode_test.bind(null, 'vp09.00.10.08'), 'encoding vp9 profile0');

promise_test(
    encode_test.bind(null, 'vp09.02.10.10'), 'encoding vp9 profile2');

promise_test(
    encode_decode_test.bind(null, 'vp09.02.10.10', null),
    'encoding and decoding vp9 profile2');

promise_test(encode_test.bind(null, 'vp8'), 'encoding vp8');

promise_test(
    encode_decode_test.bind(null, 'vp8', null),
    'encoding and decoding vp8');

promise_test(
    encode_decode_test.bind(null, 'avc1.42001E', 'annexb'),
    'encoding and decoding avc1.42001E (annexb)');

promise_test(
    encode_decode_test.bind(null, 'avc1.42001E', 'avc'),
    'encoding and decoding avc1.42001E (avc)');
