// META: global=window,dedicatedworker
// META: script=/wpt_internal/webcodecs/encoder_utils.js

async function change_encoding_params_test(codec) {
  const acc = "prefer-software";
  let original_w = 800;
  let original_h = 600;
  let original_bitrate = 5_000_000;

  let new_w = 640;
  let new_h = 480;
  let new_bitrate = 3_000_000;

  let next_ts = 0
  let reconf_ts = 0;
  let frames_to_encode = 16;
  let before_reconf_frames = 0;
  let after_reconf_frames = 0;
  let errors = 0;

  let process_video_chunk = function (chunk, metadata) {
    let config = metadata.decoderConfig;
    var data = new Uint8Array(chunk.data);
    assert_greater_than_equal(data.length, 0);
    let after_reconf = (reconf_ts != 0) && (chunk.timestamp >= reconf_ts);
    if (after_reconf) {
      after_reconf_frames++;
      if (config) {
        assert_equals(config.codedWidth, new_w);
        assert_equals(config.codedHeight, new_h);
      }
    } else {
      before_reconf_frames++;
      if (config) {
        assert_equals(config.codedWidth, original_w);
        assert_equals(config.codedHeight, original_h);
      }
    }
  };

  const init = {
    output: (chunk, md) => {
      try {
        process_video_chunk(chunk, md);
      } catch (e) {
        errors++;
        console.log(e.message);
      }
    },
    error: (e) => {
      errors++;
      console.log(e.message);
    },
  };
  const params = {
    codec: codec,
    hardwareAcceleration: acc,
    width: original_w,
    height: original_h,
    bitrate: original_bitrate,
    scalabilityMode: "L1T2",
    framerate: 30,
  };
  let encoder = new VideoEncoder(init);
  encoder.configure(params);

  // Remove this flush after crbug.com/1275789 is fixed
  await encoder.flush();

  // Encode |frames_to_encode| frames with original settings
  for (let i = 0; i < frames_to_encode; i++) {
    var frame = createFrame(original_w, original_h, next_ts++);
    encoder.encode(frame, {});
  }

  params.width = new_w;
  params.height = new_h;
  params.bitrate = new_bitrate;

  // Reconfigure encoder and run |frames_to_encode| frames with new settings
  encoder.configure(params);
  reconf_ts = next_ts;

  for (let i = 0; i < frames_to_encode; i++) {
    var frame = createFrame(new_w, new_h, next_ts++);
    encoder.encode(frame, {});
  }

  await encoder.flush();

  // Configure back to original config
  params.width = original_w;
  params.height = original_h;
  params.bitrate = original_bitrate;
  encoder.configure(params);
  await encoder.flush();

  encoder.close();
  assert_equals(before_reconf_frames, frames_to_encode);
  assert_equals(after_reconf_frames, frames_to_encode);
  assert_equals(errors, 0);
}

promise_test(
    change_encoding_params_test.bind(null, 'vp8'),
    'reconfiguring vp8');

promise_test(
    change_encoding_params_test.bind(null, 'vp09.00.10.08'),
    'reconfiguring vp9 profile0');

promise_test(
    change_encoding_params_test.bind(null, 'vp09.02.10.10'),
    'reconfiguring vp9 profile2');

promise_test(
    change_encoding_params_test.bind(null, 'avc1.42001E'),
    'reconfiguring avc1.42001E');

