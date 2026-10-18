// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Input.dispatchMouseEvent rejects wheel deltas outside the scaled float range and delivers representable large deltas.');
  const scaleTolerance = 0.000001;
  // Only this large-value probe uses a relative tolerance. The wheel pipeline
  // stores floats and divides through float and double coordinate transforms.
  // Eight float epsilons bound that rounding without accepting a missing scale
  // conversion. Ordinary deltas retain their separate fixed 0.0001 tolerance.
  const largeRelativeTolerance = 2 ** -20;

  function check(condition, label, actual) {
    testRunner.log(`${condition ? 'PASS' : 'FAIL'}: ${label}`);
    if (!condition)
      testRunner.log(actual);
    return condition;
  }

  async function settle() {
    await session.evaluateAsync(
        () => new Promise(resolve => requestAnimationFrame(
                              () => requestAnimationFrame(resolve))));
  }

  async function setScale(scale) {
    const response =
        await dp.Emulation.setPageScaleFactor({pageScaleFactor: scale});
    if (!check(!response.error, `scale ${scale}: protocol success`, response))
      return false;
    await settle();
    const actual = await session.evaluate(() => visualViewport.scale);
    return check(
        Number.isFinite(actual) && Math.abs(actual - scale) <= scaleTolerance,
        `scale ${scale}: actual page scale`, actual);
  }

  await session.evaluate(() => {
    document.documentElement.style.overflow = 'hidden';
    document.body.style.margin = '0';
    document.body.innerHTML = `
        <div id="scroller" style="width:200px;height:200px;overflow:scroll">
          <div id="content" style="width:2000px;height:2000px"></div>
        </div>`;
    internals.setPageScaleFactorLimits(1, 2);
    window.wheelRecords = [];
    window.addEventListener('wheel', event => {
      event.preventDefault();
      wheelRecords.push({
        deltaX: event.deltaX,
        deltaY: event.deltaY,
        deltaMode: event.deltaMode,
        target: event.target.id,
        cancelled: event.defaultPrevented,
      });
      if (window.resolveWheel)
        resolveWheel();
    }, {passive: false});
  });
  await settle();

  for (const {scale,
              value} of [{scale: 1, value: 1e308}, {scale: 2, value: 2e38}]) {
    if (!await setScale(scale))
      return testRunner.completeTest();
    for (const axis of ['X', 'Y']) {
      const params = {type: 'mouseWheel', x: 15, y: 15, deltaX: 0, deltaY: 0};
      params[`delta${axis}`] = value;
      const response = await dp.Input.dispatchMouseEvent(params);
      const label = `scale ${scale} delta${axis} ${value}`;
      check(response.error?.code === -32602,
            `${label}: rejected with InvalidParams`, response);
      await settle();
      const records = await session.evaluate(() => window.wheelRecords);
      check(records.length === 0, `${label}: no wheel delivered`, records);
    }
  }

  if (!await setScale(1.2))
    return testRunner.completeTest();
  const deltaX = 123456789;
  const deltaY = -987654321;
  await session.evaluate(() => {
    wheelRecords = [];
    const scroller = document.getElementById('scroller');
    window.offsetBefore = [scroller.scrollLeft, scroller.scrollTop];
    window.largeWheelProbe = new Promise(resolve => {
      const timer = setTimeout(() => {
        window.resolveWheel = null;
        resolve(true);
      }, 5000);
      window.resolveWheel = () => {
        clearTimeout(timer);
        window.resolveWheel = null;
        resolve(false);
      };
    });
  });
  const response = await dp.Input.dispatchMouseEvent(
      {type: 'mouseWheel', x: 15, y: 15, deltaX, deltaY});
  if (!check(!response.error, 'representable large wheel: protocol success',
             response)) {
    return testRunner.completeTest();
  }
  const timedOut = await session.evaluateAsync(() => window.largeWheelProbe);
  await settle();
  const probe = await session.evaluate(() => {
    const scroller = document.getElementById('scroller');
    return {
      records: wheelRecords,
      before: offsetBefore,
      after: [scroller.scrollLeft, scroller.scrollTop],
    };
  });
  const record = probe.records[0];
  check(!timedOut && probe.records.length === 1 &&
            record.target === 'content' && record.deltaMode === 0 &&
            record.cancelled,
        'representable large wheel: exactly one canceled target event', probe);
  const relativelyNear = (actual, expected) => Number.isFinite(actual) &&
      Math.abs(actual - expected) <=
          Math.abs(expected) * largeRelativeTolerance;
  check(record && relativelyNear(record.deltaX, deltaX) &&
            relativelyNear(record.deltaY, deltaY),
        'representable large wheel: CSS deltas within relative float tolerance',
        probe.records);
  check(
      probe.after[0] === probe.before[0] && probe.after[1] === probe.before[1],
      'representable large wheel: canceled event leaves offset unchanged',
      probe);

  const restored = await dp.Emulation.setPageScaleFactor({pageScaleFactor: 1});
  await settle();
  check(!restored.error &&
            Math.abs(await session.evaluate(() => visualViewport.scale) - 1) <=
                scaleTolerance,
        'restored page scale', restored);
  testRunner.completeTest();
})
