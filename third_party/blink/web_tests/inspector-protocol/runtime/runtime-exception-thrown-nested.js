(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <script>
      let nested = false;
      Error.prepareStackTrace = (err, stack) => {
        if (!nested) {
          nested = true;
          window.dispatchEvent(new Event('nested'));
        }
        return 'custom stack';
      };

      window.addEventListener('nested', () => {
        throw new Error('from prepareStackTrace');
      });

      let firstError = true;
      window.addEventListener('error', () => {
        if (firstError) {
          throw new Error('error listener 1');
        }
      });
      window.addEventListener('error', () => {
        if (firstError) {
          firstError = false;
          throw new Error('error listener 2');
        }
      });

      window.addEventListener('trigger', () => {
        throw 'initial';
      });
    </script>
  `,
      'Tests that nested exceptions thrown while reporting an exception to Runtime domain do not crash.');

  await dp.Runtime.enable();
  dp.Runtime.onExceptionThrown(event => {
    testRunner.log(event.params.exceptionDetails.exception.description ||
                   '<no description>');
  });
  await session.evaluate(`window.dispatchEvent(new Event('trigger'))`);
  testRunner.completeTest();
})
