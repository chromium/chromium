(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startHTML(`
    <script>
      // Function returns true if the JavaScript time does advance during the
      // computation.
      function doesJavaScriptTimeAdvance() {
        let event = new Event('test');
        let start = Date.now();
        addEventListener('test', () => {
          let f = (x) => x > 1 ? f(x-1) + x : 1;
          for (let x = 0; x < 100; x++) { f(1000); }
        }, false);
        dispatchEvent(event);
        return Date.now() > start;
      }
    </script>
  `, 'Tests that perf metrics return real time even if there is a virtual time override in place.');

  let v = await session.evaluate("doesJavaScriptTimeAdvance()");
  testRunner.log(`Does real time advance? ${v}.`);

  await dp.Performance.enable();
  await dp.Emulation.setVirtualTimePolicy(
      {policy: 'advance', initialVirtualTime: 1234567890});
  let before = await getScriptDuration();
  v = await session.evaluate("doesJavaScriptTimeAdvance()");
  testRunner.log(`Does virtual time advance? ${v}.`);
  let after = await getScriptDuration();
  testRunner.log(`Is script duration increased? ${after > before}.`);

  async function getScriptDuration() {
    const {result:{metrics}} = await dp.Performance.getMetrics();
    //testRunner.log(metrics);
    const metric = metrics.find(metric => metric.name === "ScriptDuration");
    return metric.value;
  }

  testRunner.completeTest();
})
