// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Input.dispatchMouseEvent keeps wheel deltas and scrolling in CSS pixels at fractional page scales.');
  const armWheelProbe = await testRunner.loadScript(
      '../../http/tests/inspector-protocol/resources/wheel-event-helper.js');

  // The widget scrolling pipeline uses floats. These fixed tolerances allow
  // its rounding, while still detecting an omitted CSS-to-widget conversion.
  const deltaTolerance = 0.0001;
  const scrollTolerance = 0.5;
  const scaleTolerance = 0.000001;

  function ordinaryWheelCases() {
    return [
      {name: 'forward', deltaX: 55, deltaY: 70},
      {name: 'reverse', deltaX: -6, deltaY: -10},
    ];
  }

  function check(condition, label, actual) {
    testRunner.log(`${condition ? 'PASS' : 'FAIL'}: ${label}`);
    if (!condition)
      testRunner.log(actual);
    return condition;
  }

  function protocolSucceeded(response, label) {
    return !response.error ||
        check(false, `${label}: protocol error`, response.error);
  }

  function near(actual, expected, tolerance) {
    return Number.isFinite(actual) && Math.abs(actual - expected) <= tolerance;
  }

  async function resetScroller() {
    await session.evaluate(() => {
      document.documentElement.style.overflow = 'hidden';
      document.body.style.margin = '0';
      document.body.innerHTML = `
          <div id="scroller" style="width:200px;height:200px;overflow:scroll">
            <div id="content" style="width:2000px;height:2000px"></div>
          </div>`;
      // Admit the requested scales without enabling device-metrics emulation.
      internals.setPageScaleFactorLimits(1, 2);
    });
    await session.evaluateAsync(
        () => new Promise(resolve => requestAnimationFrame(
                              () => requestAnimationFrame(resolve))));
  }

  async function captureWheel(deltaX, deltaY, cancel, label) {
    await session.evaluate(armWheelProbe, cancel);
    const response = await dp.Input.dispatchMouseEvent(
        {type: 'mouseWheel', x: 15, y: 15, deltaX, deltaY});
    if (!protocolSucceeded(response, label)) {
      await session.evaluate(() => window.finalizeWheelProbe());
      return null;
    }
    let probe = await session.evaluateAsync(() => window.wheelProbe);
    if (!check(!probe.timedOut && probe.wheelReceived && probe.scrollEnded,
               `${label}: expected events completed`, probe)) {
      return null;
    }
    if (cancel) {
      // Read current offsets and all events after dispatch and renderer frames,
      // rather than the snapshot taken inside the first wheel listener.
      await session.evaluateAsync(
          () => new Promise(resolve => requestAnimationFrame(
                                () => requestAnimationFrame(resolve))));
      probe = await session.evaluate(() => window.finalizeWheelProbe());
    }
    const record = probe.records[0];
    check(probe.records.length === 1 && record.target === 'content' &&
              record.deltaMode === 0 && record.cancelled === cancel &&
              near(record.deltaX, deltaX, deltaTolerance) &&
              near(record.deltaY, deltaY, deltaTolerance),
          `${label}: wheel deltas in CSS pixels`, probe.records);
    return probe;
  }

  for (const scale of [1, 1.2, 1.5, 1.7]) {
    await resetScroller();
    // Changing page scale informs the browser through LocalMainFrameHost
    // before this associated renderer protocol response is sent.
    const response =
        await dp.Emulation.setPageScaleFactor({pageScaleFactor: scale});
    if (!protocolSucceeded(response, `scale ${scale}`))
      return testRunner.completeTest();
    await session.evaluateAsync(
        () => new Promise(resolve => requestAnimationFrame(
                              () => requestAnimationFrame(resolve))));
    const actualScale = await session.evaluate(() => visualViewport.scale);
    if (!check(near(actualScale, scale, scaleTolerance),
               `scale ${scale}: actual page scale`, actualScale)) {
      return testRunner.completeTest();
    }

    let expectedX = 0;
    let expectedY = 0;
    for (const {name, deltaX, deltaY} of ordinaryWheelCases()) {
      const label = `scale ${scale} ${name}`;
      const probe = await captureWheel(deltaX, deltaY, false, label);
      if (!probe)
        return testRunner.completeTest();
      expectedX += deltaX;
      expectedY += deltaY;
      check(near(probe.after[0], expectedX, scrollTolerance) &&
                near(probe.after[1], expectedY, scrollTolerance),
            `${label}: scroll distance in CSS pixels`, probe.after);
    }

    const label = `scale ${scale} fractional`;
    const probe = await captureWheel(0.1, -0.2, true, label);
    if (!probe)
      return testRunner.completeTest();
    check(probe.after[0] === probe.before[0] &&
              probe.after[1] === probe.before[1],
          `${label}: cancelled wheel leaves scroll offset unchanged`, probe);
  }

  const response = await dp.Emulation.setPageScaleFactor({pageScaleFactor: 1});
  if (protocolSucceeded(response, 'restore scale')) {
    check(near(await session.evaluate(() => visualViewport.scale), 1,
               scaleTolerance),
          'restored page scale', null);
  }
  testRunner.completeTest();
})
