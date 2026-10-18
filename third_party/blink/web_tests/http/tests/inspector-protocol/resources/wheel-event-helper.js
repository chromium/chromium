// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Install with session.evaluate(armWheelProbe, cancel) before dispatching.
(function armWheelProbe(cancel) {
  const scroller = document.getElementById('scroller');
  const records = [];
  const before = [scroller.scrollLeft, scroller.scrollTop];
  let wheelReceived = false;
  // Canceled cases need no scrollend; this is a completion sentinel.
  let scrollEnded = cancel;
  let timer;
  let resolve;
  window.wheelProbe = new Promise(callback => resolve = callback);
  const snapshot = timedOut => ({
    timedOut,
    wheelReceived,
    scrollEnded,
    records,
    before,
    after: [scroller.scrollLeft, scroller.scrollTop],
  });
  const cleanup = () => {
    clearTimeout(timer);
    scroller.removeEventListener('wheel', onWheel);
    scroller.removeEventListener('scrollend', onScrollEnd);
  };
  let completed = false;
  const finish = timedOut => {
    if (completed)
      return;
    completed = true;
    clearTimeout(timer);
    // Keep canceled-event collection active until the final sample.
    if (!cancel || timedOut)
      cleanup();
    resolve(snapshot(timedOut));
  };
  window.finalizeWheelProbe = () => {
    const probe = snapshot(false);
    cleanup();
    return probe;
  };
  const onWheel = event => {
    if (cancel)
      event.preventDefault();
    records.push({
      deltaX: event.deltaX,
      deltaY: event.deltaY,
      deltaMode: event.deltaMode,
      clientX: event.clientX,
      clientY: event.clientY,
      target: event.target.id,
      cancelled: event.defaultPrevented,
    });
    wheelReceived = true;
    if (scrollEnded)
      finish(false);
  };
  const onScrollEnd = event => {
    if (event.target !== scroller)
      return;
    scrollEnded = true;
    if (wheelReceived)
      finish(false);
  };
  scroller.addEventListener('wheel', onWheel, {passive: false});
  if (!cancel)
    scroller.addEventListener('scrollend', onScrollEnd);
  // Report missing events as a failure instead of leaving a pending renderer
  // promise until the whole protocol test times out.
  timer = setTimeout(() => finish(true), 5000);
})
