<?php
header("Content-Type: application/javascript");
if (isset($_GET["profiling_mode_only"])) {
  header("Document-Policy: js-profiling=?0, js-profiling-mode=lazy");
} elseif (!isset($_GET["without_document_policy"])) {
  header("Document-Policy: js-profiling");
}
if (isset($_GET["report_only"])) {
  header("Document-Policy-Report-Only: js-profiling=?0");
}
?>

async function profile(sample = false) {
  const hasProfiler = 'Profiler' in self;
  let created = false;
  let stopped = false;
  let trace;
  let error;

  if (hasProfiler) {
    try {
      const profiler = new Profiler({
        sampleInterval: 10,
        maxBufferSize: 100,
      });
      created = true;
      if (sample) {
        // Match ProfileUtils.forceSample()'s sampling margin.
        for (const deadline = performance.now() + 510;
             performance.now() < deadline;) {
        }
      }
      trace = await profiler.stop();
      stopped = profiler.stopped;
    } catch (exception) {
      error = {
        name: exception.name,
        message: exception.message,
      };
    }
  }

  return {hasProfiler, created, stopped, trace, error};
}

async function run() {
  const params = new URL(self.location.href).searchParams;
  const scheme = params.get('child_scheme');
  if (scheme === null) {
    return profile(params.has('sample'));
  }

  const source =
      `(${profile.toString()})().then(result => self.postMessage(result));`;
  let url;
  if (scheme === 'blob') {
    url = URL.createObjectURL(new Blob([source], {type: 'text/javascript'}));
  } else if (scheme === 'data') {
    url = 'data:text/javascript,' + encodeURIComponent(source);
  } else {
    throw new Error(`Unsupported child scheme: ${scheme}`);
  }
  const worker = new Worker(url, {type: 'module'});
  try {
    return await new Promise((resolve, reject) => {
      worker.onmessage = event => resolve(event.data);
      worker.onerror = event => {
        reject(new Error(event.message || `Failed to load ${scheme} worker`));
      };
    });
  } finally {
    worker.terminate();
    if (scheme === 'blob') {
      URL.revokeObjectURL(url);
    }
  }
}

run().then(result => {
  performance.mark('worker-use-counter-barrier', {detail: 1});
  self.postMessage(result);
}, error => {
  self.postMessage({workerError: `${error.name}: ${error.message}`});
});
