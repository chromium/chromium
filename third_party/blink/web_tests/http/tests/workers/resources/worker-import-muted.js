// This is a same-origin script that will be used as the Worker's main script.
// It imports a cross-origin script and triggers an error during execution.
// We use localhost:8000 which is cross-origin to the test's default
// 127.0.0.1:8000.
const CROSS_ORIGIN_URL =
    'http://localhost:8000/workers/resources/worker-cross-origin-throw.js';
importScripts(CROSS_ORIGIN_URL);
throwFromImportedScript();
