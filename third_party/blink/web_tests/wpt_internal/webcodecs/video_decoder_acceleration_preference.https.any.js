// META: global=window,dedicatedworker
// META: script=/wpt_internal/webcodecs/encoder_utils.js

const defaultWidth = 640;
const defaultHeight = 360;

function cycleAccelerationPreferences(codec, expected_success, desc) {
  promise_test(async t => {
    var config_success;

    let decoderInit = {
      error: e => {
        config_success = false;
      },
      output: t.unreached_func('Unexpected output')
    };

    for (const [key, value] of Object.entries(expected_success)) {
      var iteration_name = "acceleration=" + key;

      let decoder = new VideoDecoder(decoderInit);

      config_success = true;
      let decoderConfig = {
        codec: codec,
        hardwareAcceleration: key,
        codedWidth: defaultWidth,
        codedHeight: defaultHeight,
      };

      var support = await VideoDecoder.isConfigSupported(decoderConfig);

      assert_equals(support.supported, value, iteration_name);
      assert_object_equals(support.config, decoderConfig, iteration_name);

      decoder.configure(decoderConfig);

      try {
        // A failed configure will cause flush to throw an exception.
        await decoder.flush();
        assert_true(value, iteration_name);
      } catch {
        assert_false(value, iteration_name);

        // The error callback may not have run yet.
        await t.step_wait(_ => config_success === false);
      }

      if (decoder.state != 'closed')
        decoder.close();
    }
  }, desc);
}

cycleAccelerationPreferences(
    'vp8', {
      'prefer-hardware': false,
      'prefer-software': true,
      'no-preference': true,
    },
    'Test VP8 configurations with all acceleration preferences');

/* Uncomment this for manual testing, before we have GPU tests for that */
/* Note: these values might vary per platform */
// cycleAccelerationPreferences(
//     'avc1.42001E', {
//       'prefer-hardware': true,
//       'prefer-software': true,
//       'no-preference': true,
//     },
//     'Test H264 configurations with all acceleration preferences');
