// META: global=window,dedicatedworker
// META: script=/wpt_internal/webcodecs/encoder_utils.js

const defaultWidth = 640;
const defaultHeight = 360;

function cycleAccelerationPreferences(codec, expected_success, desc) {
  promise_test(async t => {
    var config_success;

    let decoderInit = {
      error: t.step_func(e => {
        config_success = false;
      }),
      output: t.unreached_func("Unexpected output")
    };

    for (const [key, value] of Object.entries(expected_success)) {
      let decoder = new VideoDecoder(decoderInit);

      config_success = true;
      let decoderConfig = {
        codec: codec,
        acceleration: key,
        width: defaultWidth,
        height: defaultHeight,
      };

      decoder.configure(decoderConfig);

      try {
        // A failed configure might cause flush to throw an exception.
        await decoder.flush();
      } catch {
        assert_equals(config_success, value, "acceleration=" + key);
      }

      if(decoder.state != "closed")
        decoder.close();
    }
  }, desc);
}

cycleAccelerationPreferences("vp8",
  { "require" : false, "deny": true, "allow" : true, },
  "Test VP8 configurations with all acceleration preferences");

/* Uncomment this for manual testing, before we have GPU tests for that */
/* Note: these values might vary per platform */
// cycleAccelerationPreferences("avc1.42001E",
//   { "require" : true, "deny": true, "allow" : true, },
//   "Test H264 configurations with all acceleration preferences");
