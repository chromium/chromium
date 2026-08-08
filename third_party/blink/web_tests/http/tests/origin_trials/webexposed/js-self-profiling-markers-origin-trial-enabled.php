<?php
// Document-Policy must be sent as a real HTTP header (Apache http/tests
// ignores sibling .headers files), so we emit it from PHP here.
header("Document-Policy: js-profiling");
?>
<!DOCTYPE html>
<meta charset="utf-8">
<!-- Generate this token with the command:
tools/origin_trials/generate_token.py http://127.0.0.1:8000 JSSelfProfilingMarkers --expire-timestamp=2000000000
-- -->
<meta http-equiv="origin-trial" content="A74sIRKCNV45POVKBq7VMK/By2DfsAxEVNlK8FbdIPHD7Boq0BB51nySCTMoNytID74vAdWUzc54uW0gYzPQ3wkAAABeeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiSlNTZWxmUHJvZmlsaW5nTWFya2VycyIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==" />
<title>JS Self-Profiling markers exposed by Origin Trial in non-COI contexts</title>

<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>

<div id="target"></div>

<script>
const SAMPLE_INTERVAL_MS = 10;
const FORCE_LAYOUT_SPIN_MS = 2000;
const LAYOUT_VICTIM_COUNT = 4000;

// Must match kJSSelfProfilingMarkers in
// third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom
const kJSSelfProfilingMarkersUseCounter = 447;

function forceLayoutWorkload() {
  // Large subtree so each style+layout pass takes several ms and the
  // 10 ms profiler can catch Blink inside its style/layout scope; otherwise
  // every sample lands in JS and gets tagged `script`, which the spec's
  // non-COI filter drops (see WICG/js-self-profiling PR #89).
  const target = document.getElementById('target');
  target.textContent = '';
  const children = [];
  for (let i = 0; i < LAYOUT_VICTIM_COUNT; i++) {
    const child = document.createElement('div');
    child.className = 'layout-victim';
    child.textContent = 'row ' + i;
    target.appendChild(child);
  }

  // Descendant + attribute selectors so each recalc touches the whole subtree.
  const style = document.createElement('style');
  style.textContent = `
    #target[data-variant="0"] .layout-victim { padding: 1px 2px; font-size: 10px; }
    #target[data-variant="1"] .layout-victim { padding: 2px 4px; font-size: 12px; }
    #target[data-variant="2"] .layout-victim { padding: 3px 6px; font-size: 14px; }
    #target[data-variant="3"] .layout-victim { padding: 4px 8px; font-size: 16px; }
  `;
  document.head.appendChild(style);

  const deadline = performance.now() + FORCE_LAYOUT_SPIN_MS;
  let sink = 0;
  let iter = 0;
  while (performance.now() < deadline) {
    // Invalidate style on every descendant, forcing a full recalc plus a
    // full layout on the next synchronous read below.
    target.setAttribute('data-variant', String(iter & 3));
    target.style.width = (200 + (iter & 15) * 8) + 'px';
    // Force synchronous style + layout of the whole subtree.
    sink += target.offsetHeight;
    iter++;
  }
  return sink;
}

promise_test(async t => {
  assert_false(self.crossOriginIsolated,
      'Precondition: this test must run in a non-COI document so that ' +
      'markers require the Origin Trial to appear.');
  assert_true('Profiler' in self,
      'Precondition: JS Self-Profiling API must be available.');

  const profiler = new Profiler({
    sampleInterval: SAMPLE_INTERVAL_MS,
    maxBufferSize: 1000000,
  });
  forceLayoutWorkload();
  const trace = await profiler.stop();

  assert_greater_than(trace.samples.length, 0, 'expected samples to be captured');

  const markerCounts = {};
  for (const sample of trace.samples) {
    if (sample.marker !== undefined) {
      markerCounts[sample.marker] = (markerCounts[sample.marker] || 0) + 1;
    }
  }

  // Per WICG/js-self-profiling PR #89, non-COI contexts with the OT expose
  // only `style` and `layout`; `script`/`gc`/`paint` MUST NOT appear.
  const allowedMarkers = new Set(['style', 'layout']);
  for (const marker of Object.keys(markerCounts)) {
    assert_true(allowedMarkers.has(marker),
        'Unexpected marker "' + marker + '" surfaced in a non-COI ' +
        'document; only style/layout are allowed by the OT.');
  }
  const totalMarkerSamples = Object.values(markerCounts)
      .reduce((sum, n) => sum + n, 0);
  assert_greater_than(totalMarkerSamples, 0,
      'Origin-Trial should surface at least one style/layout marker in ' +
      'a non-COI document; observed markers: ' + JSON.stringify(markerCounts));

  // Sequenced after profiler.stop() so the UseCounter has been recorded.
  assert_true('internals' in self,
      'This assertion requires window.internals; only meaningful when ' +
      'running under content_shell.');
  assert_true(
      internals.isWebDXFeatureUseCounted(
          document, kJSSelfProfilingMarkersUseCounter),
      'kJSSelfProfilingMarkers WebDXFeature use counter should be recorded ' +
      'after markers are emitted in an OT-enabled document.');
}, 'JSSelfProfilingMarkers OT populates ProfilerSample.marker and records ' +
   'the use counter in non-COI documents.');
</script>

