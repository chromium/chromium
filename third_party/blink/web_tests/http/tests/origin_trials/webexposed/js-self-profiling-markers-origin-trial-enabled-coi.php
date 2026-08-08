<?php
// crossOriginIsolated requires COOP+COEP, and Document-Policy must be a real
// HTTP header (Apache http/tests ignores sibling .headers files), so we emit
// all three from PHP here.
header("Cross-Origin-Opener-Policy: same-origin");
header("Cross-Origin-Embedder-Policy: require-corp");
header("Document-Policy: js-profiling");
?>
<!DOCTYPE html>
<meta charset="utf-8">
<!-- Same token as the non-COI test: it is tied to origin + feature, not to
     cross-origin isolation. Generate with:
tools/origin_trials/generate_token.py http://127.0.0.1:8000 JSSelfProfilingMarkers --expire-timestamp=2000000000
-- -->
<meta http-equiv="origin-trial" content="A74sIRKCNV45POVKBq7VMK/By2DfsAxEVNlK8FbdIPHD7Boq0BB51nySCTMoNytID74vAdWUzc54uW0gYzPQ3wkAAABeeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiSlNTZWxmUHJvZmlsaW5nTWFya2VycyIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ==" />
<title>JS Self-Profiling markers exposed by Origin Trial in COI contexts</title>

<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>

<div id="target"></div>

<script>
const SAMPLE_INTERVAL_MS = 10;
const FORCE_LAYOUT_SPIN_MS = 800;
const FORCE_GC_SPIN_MS = 2200;
const LAYOUT_VICTIM_COUNT = 4000;

// Must match kJSSelfProfilingMarkers in
// third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom
const kJSSelfProfilingMarkersUseCounter = 447;

function forceWorkload() {
  // Two phases so the full (unfiltered) marker set is exercised in a COI
  // document: a style/layout phase, then an allocation-heavy phase that forces
  // garbage collection. `gc` markers are dropped by the non-COI filter, so
  // observing them proves the filter is not applied in a COI document.
  const target = document.getElementById('target');
  target.textContent = '';
  for (let i = 0; i < LAYOUT_VICTIM_COUNT; i++) {
    const child = document.createElement('div');
    child.className = 'layout-victim';
    child.textContent = 'row ' + i;
    target.appendChild(child);
  }

  const style = document.createElement('style');
  style.textContent = `
    #target[data-variant="0"] .layout-victim { padding: 1px 2px; font-size: 10px; }
    #target[data-variant="1"] .layout-victim { padding: 2px 4px; font-size: 12px; }
    #target[data-variant="2"] .layout-victim { padding: 3px 6px; font-size: 14px; }
    #target[data-variant="3"] .layout-victim { padding: 4px 8px; font-size: 16px; }
  `;
  document.head.appendChild(style);

  let sink = 0;

  // Phase 1: synchronous style + layout.
  const layoutDeadline = performance.now() + FORCE_LAYOUT_SPIN_MS;
  let iter = 0;
  while (performance.now() < layoutDeadline) {
    target.setAttribute('data-variant', String(iter & 3));
    target.style.width = (200 + (iter & 15) * 8) + 'px';
    sink += target.offsetHeight;
    iter++;
  }

  // Phase 2: allocate large amounts of transient (and some retained) garbage to
  // force frequent scavenges plus major GCs, so the profiler samples `gc`.
  const gcDeadline = performance.now() + FORCE_GC_SPIN_MS;
  const retain = [];
  while (performance.now() < gcDeadline) {
    for (let k = 0; k < 200; k++) {
      const arr = new Array(10000);
      for (let j = 0; j < 10000; j++) {
        arr[j] = { v: j, s: 'garbage-' + j };
      }
      sink += arr[5000].v;
      // Occasionally retain arrays to promote objects and provoke major GCs.
      if ((k & 31) === 0) {
        retain.push(arr);
        if (retain.length > 64) {
          retain.shift();
        }
      }
    }
  }
  sink += retain.length;

  return sink;
}

promise_test(async t => {
  assert_true(self.crossOriginIsolated,
      'Precondition: this test must run in a cross-origin-isolated document ' +
      'so that the non-COI marker filter does not apply.');
  assert_true('Profiler' in self,
      'Precondition: JS Self-Profiling API must be available.');

  const profiler = new Profiler({
    sampleInterval: SAMPLE_INTERVAL_MS,
    maxBufferSize: 1000000,
  });
  forceWorkload();
  const trace = await profiler.stop();

  assert_greater_than(trace.samples.length, 0, 'expected samples to be captured');

  const markerCounts = {};
  for (const sample of trace.samples) {
    if (sample.marker !== undefined) {
      markerCounts[sample.marker] = (markerCounts[sample.marker] || 0) + 1;
    }
  }

  // In a COI document the OT exposes the full marker set (no filtering). `gc`
  // markers are emitted during garbage collection and are dropped by the
  // non-COI filter, so observing at least one proves the filter is not applied.
  assert_greater_than(markerCounts['gc'] || 0, 0,
      'A cross-origin-isolated document should surface `gc` markers ' +
      '(full set, unfiltered); observed markers: ' + JSON.stringify(markerCounts));

  // The cross-origin-safe subset (style/layout) is still present as well.
  assert_greater_than((markerCounts['style'] || 0) + (markerCounts['layout'] || 0),
      0, 'expected style/layout markers to also be present; observed markers: ' +
      JSON.stringify(markerCounts));

  // Sequenced after profiler.stop() so the UseCounter has been recorded.
  assert_true('internals' in self,
      'This assertion requires window.internals; only meaningful when ' +
      'running under content_shell.');
  assert_true(
      internals.isWebDXFeatureUseCounted(
          document, kJSSelfProfilingMarkersUseCounter),
      'kJSSelfProfilingMarkers WebDXFeature use counter should be recorded ' +
      'after markers are emitted in an OT-enabled document.');
}, 'JSSelfProfilingMarkers OT populates ProfilerSample.marker with the full ' +
   'marker set and records the use counter in cross-origin-isolated documents.');
</script>
