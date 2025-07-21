function testTailTime(should, context, options) {
  let src = new ConstantSourceNode(context, {offset: 1});
  let f = new BiquadFilterNode(context, options.filterOptions);

  src.connect(f).connect(context.destination);
  src.start();
  src.stop(1 / context.sampleRate);

  let expectedTailFrame = computeTailFrame(f);

  // The internal Biquad time computation limits he tail time to a
  // maximum of 30 sec. We need to limit the computed tail frame to
  // that limit as well.
  expectedTailFrame = Math.min(expectedTailFrame, 30 * context.sampleRate);

  return context.startRendering().then(renderedBuffer => {
    let s = renderedBuffer.getChannelData(0);
    let prefix = options.prefix + ': Biquad(' +
        JSON.stringify(options.filterOptions) + ')';

    // Round actual tail frame to a render boundary
    let quantumIndex = Math.floor(expectedTailFrame / RENDER_QUANTUM_FRAMES);
    let expectedTailBoundary = RENDER_QUANTUM_FRAMES * quantumIndex;

    // Find the actual tail frame.  That is, the last point where the
    // output is not zero.
    let actualTailFrame;

    for (actualTailFrame = s.length; actualTailFrame > 0; --actualTailFrame) {
      if (Math.abs(s[actualTailFrame - 1]) > 0)
        break;
    }

    should(actualTailFrame, `${prefix}: Actual Tail Frame ${actualTailFrame}`)
        .beGreaterThanOrEqualTo(expectedTailFrame);

    // Verify each render quanta is not identically zero up to the
    // boundary.
    for (let k = 0; k <= quantumIndex; ++k) {
      let firstFrame = RENDER_QUANTUM_FRAMES * k;
      let lastFrame = firstFrame + RENDER_QUANTUM_FRAMES - 1;
      should(
          s.slice(firstFrame, lastFrame + 1),
          `${prefix}: output[${firstFrame}:${lastFrame}]`)
          .notBeConstantValueOf(0);
    }
    // The frames after the tail should be zero.  Because the
    // implementation uses approximations to simplify the
    // computations, the nodes tail time may be greater than the real
    // impulse response tail.  Thus, we just verify that the output
    // over the tail is less than the tail threshold value.
    let zero = new Float32Array(s.length);
    should(
        s.slice(expectedTailBoundary + RENDER_QUANTUM_FRAMES + 256),
        prefix + ': output[' +
            (expectedTailBoundary + RENDER_QUANTUM_FRAMES + 256) + ':]')
        .beCloseToArray(
            zero.slice(expectedTailBoundary + RENDER_QUANTUM_FRAMES + 256),
            {absoluteThreshold: options.threshold || 0});
  })
}

// TODO(saqlain): testTailTime_W3CTH() is the testharness.js-compatible version
// of testTailTime(). It replaces the old audit.js-style assertions with
// standard testharness.js assertions. Once all audit.js tests are migrated,
// rename this to testTailTime() and delete the old one.
function testTailTime_W3CTH(context, options) {
  const src = new ConstantSourceNode(context, {offset: 1});
  const f = new BiquadFilterNode(context, options.filterOptions);

  src.connect(f).connect(context.destination);
  src.start();
  src.stop(1 / context.sampleRate);

  let expectedTailFrame = computeTailFrame(f);

  // The internal Biquad time computation limits the tail time to a
  // maximum of 30 sec. We need to limit the computed tail frame to
  // that limit as well.
  expectedTailFrame = Math.min(expectedTailFrame, 30 * context.sampleRate);

  return context.startRendering().then(renderedBuffer => {
    const s = renderedBuffer.getChannelData(0);
    const prefix = options.prefix + ': Biquad(' +
        JSON.stringify(options.filterOptions) + ')';

    // Round actual tail frame to a render boundary
    const quantumIndex = Math.floor(expectedTailFrame / RENDER_QUANTUM_FRAMES);
    const expectedTailBoundary = RENDER_QUANTUM_FRAMES * quantumIndex;

    // Find the actual tail frame. That is, the last point where the
    // output is not zero.
    let actualTailFrame;
    for (actualTailFrame = s.length; actualTailFrame > 0; --actualTailFrame) {
      if (Math.abs(s[actualTailFrame - 1]) > 0)
        break;
    }

    assert_greater_than_equal(
        actualTailFrame, expectedTailFrame,
        `${prefix}: actualTailFrame (${actualTailFrame}) >= ` +
            `expectedTailFrame (${expectedTailFrame})`);

    // Verify each render quanta is not identically zero up to the
    // boundary.
    for (let k = 0; k <= quantumIndex; ++k) {
      const firstFrame = RENDER_QUANTUM_FRAMES * k;
      const lastFrame = firstFrame + RENDER_QUANTUM_FRAMES - 1;
      const slice = s.slice(firstFrame, lastFrame + 1);
      const isConstantZero = slice.every(v => v === 0);
      assert_false(
          isConstantZero,
          `${prefix}: output[${firstFrame}:${lastFrame}] should not be 0`);
    }

    // The frames after the tail should be zero. Because the
    // implementation uses approximations to simplify the
    // computations, the node's tail time may be greater than the real
    // impulse response tail. Thus, we just verify that the output
    // over the tail is less than the tail threshold value.
    const silenceStart = expectedTailBoundary + RENDER_QUANTUM_FRAMES + 256;
    const actual = s.slice(silenceStart);
    const expected = new Float32Array(actual.length);

    for (let i = 0; i < actual.length; ++i) {
      assert_approx_equals(
          actual[i], expected[i], options.threshold || 0,
          `${prefix}: output[${silenceStart + i}] ≈ 0 within ` +
              `${options.threshold || 0}`);
    }
  });
}

function computeTailFrame(filterNode) {
  // Compute the impuluse response for the filter |filterNode| by
  // filtering the impulse directly ourself.
  let coef = createFilter(
      filterNode.type,
      filterNode.frequency.value / filterNode.context.sampleRate * 2,
      filterNode.Q.value, filterNode.gain.value);

  let impulse = new Float32Array(filterNode.context.length);
  impulse[0] = 1;

  let filtered = filterData(coef, impulse, impulse.length);

  // Compute the magnitude and find out where the imuplse is small enough.
  let tailFrame = 0;
  if (Math.abs(filtered[filtered.length - 1]) >= 1 / 32768) {
    tailFrame = filtered.length - 1;
  } else {
    for (let k = filtered.length - 1; k >= 0; --k) {
      if (Math.abs(filtered[k]) >= 1 / 32768) {
        tailFrame = k + 1;
        break;
      }
    }
  }

  return tailFrame;
}
