// This depends on audit.js to define the |should| function used herein.
//
// Test that setPosition throws an error if there is already a
// setValueCurve scheduled during the same time period.
function testPositionSetterVsCurve(should, context, options) {
  // Create the graph consisting of a source node and the panner.
  let src = new ConstantSourceNode(context, {offset: 1});
  let panner = new PannerNode(context);
  src.connect(panner).connect(context.destination);

  let curve = Float32Array.from([-10, 10]);

  // Determine if we're testing the panner or the listener and set the node
  // appropriately.
  let testNode = options.nodeName === 'panner' ? panner : context.listener;

  let prefix = options.nodeName === 'panner' ? 'panner.' : 'listener.';

  let message = prefix + options.paramName + '.setValueCurve(..., 0, ' +
      options.curveDuration + ')';

  // If the coordinate name has 'position', we're testing setPosition;
  // otherwise assume we're testing setOrientation.
  let methodName =
      options.paramName.includes('position') ? 'setPosition' : 'setOrientation';

  // Sanity check that we're testing the right node. (The |testName| prefix is
  // to make each message unique for testharness.)
  if (options.nodeName === 'panner') {
    should(
        testNode instanceof PannerNode,
        options.testName + ': Node under test is a PannerNode')
        .beTrue();
  } else {
    should(
        testNode instanceof AudioListener,
        options.testName + ': Node under test is an AudioLIstener')
        .beTrue();
  }

  // Set the curve automation on the specified axis.
  should(() => {
    testNode[options.paramName].setValueCurveAtTime(
        curve, 0, options.curveDuration);
  }, message).notThrow();

  let resumeContext = context.resume.bind(context);

  // Get correct argument string for the setter for printing the message.
  let setterArguments;
  if (options.nodeName === 'panner') {
    setterArguments = '(1,1,1)';
  } else {
    if (methodName === 'setPosition') {
      setterArguments = '(1,1,1)';
    } else {
      setterArguments = '(1,1,1,1,1,1)';
    }
  }

  let setterMethod = () => {
    // It's ok to give extra args.
    testNode[methodName](1, 1, 1, 1, 1, 1);
  };

  if (options.suspendFrame) {
    // We're testing setPosition after the curve has ended to verify that
    // we don't throw an error.  Thus, suspend the context and call
    // setPosition.
    let suspendTime = options.suspendFrame / context.sampleRate;
    context.suspend(suspendTime)
        .then(() => {
          should(
              setterMethod,
              prefix + methodName + setterArguments + ' for ' +
                  options.paramName + ' at time ' + suspendTime)
              .notThrow();
        })
        .then(resumeContext);
  } else {
    // Basic test where setPosition is called where setValueCurve is
    // already active.
    context.suspend(0)
        .then(() => {
          should(
              setterMethod,
              prefix + methodName + setterArguments + ' for ' +
                  options.paramName)
              .throw(DOMException, 'NotSupportedError');
        })
        .then(resumeContext);
  }

  src.start();
  return context.startRendering();
}

// TODO(saqlain): testPositionSetterVsCurve_W3CTH() is the
// testharness.js-compatible version of testPositionSetterVsCurve().
// It replaces the old audit.js-style assertions with
// standard testharness.js assertions. Once all audit.js tests are migrated,
// rename this to testPositionSetterVsCurve() and delete the old one.

// Test that setPosition throws an error if there is already a
// setValueCurve scheduled during the same time period.
async function testPositionSetterVsCurve_W3CTH(context, options) {
  // Create the graph consisting of a source node and the panner.
  const src = new ConstantSourceNode(context, { offset: 1 });
  const panner = new PannerNode(context);
  src.connect(panner).connect(context.destination);

  const curve = Float32Array.from([-10, 10]);

  // Determine if we're testing the panner or the listener and set the node
  // appropriately.
  const testNode = options.nodeName === 'panner' ? panner : context.listener;
  const prefix = options.nodeName === 'panner' ? 'panner.' : 'listener.';

  const message = prefix + options.paramName +
      `.setValueCurve(..., 0, ${options.curveDuration})`;

  const methodName =
      options.paramName.includes('position') ? 'setPosition' : 'setOrientation';

  if (options.nodeName === 'panner') {
    assert_true(
        testNode instanceof PannerNode,
        `${options.testName}: Node under test is a PannerNode`);
  } else {
    assert_true(
        testNode instanceof AudioListener,
        `${options.testName}: Node under test is an AudioListener`);
  }

  // Set the curve automation on the specified axis.
  try {
    testNode[options.paramName].setValueCurveAtTime(
        curve, 0, options.curveDuration);
  } catch (e) {
    assert_unreached(`${message} threw exception: ${e.message}`);
  }

  const setterMethod = () => {
    testNode[methodName](1, 1, 1, 1, 1, 1);  // Extra args are fine
  };

  if (options.suspendFrame) {
    // Case: No error expected — call after curve has ended
    const suspendTime = options.suspendFrame / context.sampleRate;
    context.suspend(suspendTime).then(() => {
      try {
        setterMethod();
      } catch (e) {
        assert_unreached(
            `${prefix}${methodName}() unexpectedly threw: ${e.message}`);
      }
      context.resume();
    });
  } else {
    // Case: Expect error because curve is active
    context.suspend(0).then(() => {
      assert_throws_dom(
          'NotSupportedError',
          setterMethod,
          `${prefix}${methodName}() must throw when curve is active`);
      context.resume();
    });
  }

  src.start();
  await context.startRendering();
}
