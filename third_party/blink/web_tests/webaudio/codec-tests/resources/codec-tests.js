// Helper functions for running a codec test, which ensures that the WebAudio
// implementation properly decodes specific containers by comparing the output
// waveform to an expected baseline.

// META: script=../../resources/buffer-loader.js

// Does a thorough comparison of the actual buffer, `decoderBuffer`, against
// an `expectedBuffer`. Both a file-level signal-to-noise ratio calculation
// (compared against `minSnrDb`) and a per sample comparison (checked against
// `absTolerance`) is performed.
function compareAudioBuffers(
    expectedBuffer, decodedBuffer, minSnrDb, absTolerance) {
  assert_equals(
      decodedBuffer.numberOfChannels, expectedBuffer.numberOfChannels,
      'Channel count should match');
  assert_equals(
      decodedBuffer.length, expectedBuffer.length, 'Frame count should match');

  for (let channel = 0; channel < expectedBuffer.numberOfChannels; ++channel) {
    const reference = expectedBuffer.getChannelData(channel);
    const actual = decodedBuffer.getChannelData(channel);

    // Ensure the SNR meets the minimum requirement.
    assert_greater_than_equal(
        computeSnrInDecibels(actual, reference), minSnrDb,
        `SNR for channel ${channel} should meet the minimum requirement`);

    // Sample-wise comparison with absolute tolerance.
    for (let i = 0; i < reference.length; ++i) {
      assert_approx_equals(
          actual[i], reference[i], absTolerance,
          `Channel ${channel}, sample ${i} should be approximately equal`);
    }
  }
}

// Generates the expected and actual filenames based on the name of the current
// test file.
function getFileNames() {
  const path = window.location.pathname;
  const parts = path.split('/');
  const testFile = parts[parts.length - 1];
  const testName = testFile.substring(0, testFile.lastIndexOf('.'));

  return {
    expected: `resources/${testName}-expected.wav`,
    actual: `${testName}-actual.wav`,
  };
}

// Minimum acceptable signal-to-noise ratio (SNR) in decibels for each
// channel. The current value of 89dB is chosen as a reasonable upper limit of
// what is audible to the human ear, combined with the limitations found from
// real world testing of various codecs. For example, decoding FLAC audio with
// FFmpeg versus Symphonia can produce SNRs of 90-95 dB. In contrast, bigger
// changes like switching sample type from S16 to F32 can produce SNRs of
// as low as 30 dB.
const DEFAULT_MIN_SNR_DB = 89.0;

// Performs the actual codec test, using `options` where provided, and default
// values when not provided.
function runCodecTest(options) {
  // types of problems may be causing issues with potential SNR failures.
  // The current tolerance has been chosen experimentally to align with
  // `kMinSnrDb`, meaning that you are quite unlikely to pass the SNR
  // analysis but then get a sample-level failure.
  const absTolerance = options.absTolerance ?? 5e-5;
  const minSnrDb = options.minSnrDb ?? DEFAULT_MIN_SNR_DB;

  const fileNames = getFileNames();

  promise_test(async t => {
    const context = new AudioContext({sampleRate: options.sampleRate});
    assert_equals(
        context.sampleRate, options.sampleRate, 'AudioContext sampleRate');

    let expectedBuffer, decodedBuffer;
    try {
      [expectedBuffer, decodedBuffer] = await loadBuffers(
          context, [fileNames.expected, options.encodedFileName]);
    } catch (e) {
      assert_unreached('Failed to load audio files: ' + e);
    }

    // Optionally save the decoded output for manual inspection.
    if (downloadAudioBuffer(decodedBuffer, fileNames.actual, true)) {
      assert_true(true, `Saved reference file: ${fileNames.actual}`);
    }

    compareAudioBuffers(expectedBuffer, decodedBuffer, minSnrDb, absTolerance);
  }, options.description);
}
