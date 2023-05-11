
async function test_framework_version(framework, expected_decimal, variant = "default") {
  promise_test(async t => {
    const [major, minor] = expected_decimal.split('.').map(n => +n);
    const framework_version = `${framework}Version`;
    const recorder = internals.initializeUKMRecorder();
    await new Promise(resolve => window.addEventListener("load", resolve));
    await new Promise(resolve => t.step_timeout(resolve));
    const entries = recorder.getMetrics(
      "Blink.JavaScriptFramework.Versions", [framework_version]);
    console.log(JSON.stringify(entries));
    assert_equals(entries.length, 1);
    const metrics = entries[0];
    assert_equals(metrics[framework_version], (major << 8) | minor);
  }, `Test framework version for ${framework} (${variant})`);
}
