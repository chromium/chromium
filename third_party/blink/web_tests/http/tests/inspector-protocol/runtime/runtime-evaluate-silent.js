(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that Runtime.evaluate logs usage to use counter correctly when muted/unmuted.`);
  testRunner.runTestSuite([
    async function testUseCounterMuted() {
      testRunner.log('UseCounter should be silenced when muted');
      var expression = `
          var d = document.createElement('datalist');
          d.setAttribute('id', 'test');
          var o = document.createElement('option');
          o.setAttribute('value', 'option');
          d.appendChild(o);
          var input = document.createElement('input');
          input.setAttribute('list', 'test');
          document.body.appendChild(input);
        `;
      await dp.Runtime.evaluate({expression: expression, silent: true});
      // WebFeature::kListAttribute = 41.
      testRunner.log(await dp.Runtime.evaluate({expression: 'window.internals.isUseCounted(document, 41)'}));
      testRunner.log('UseCounter should be unsilenced when unmuted');
      await dp.Runtime.evaluate({expression: expression, silent: false});
      // WebFeature::kListAttribute = 41.
      testRunner.log(await dp.Runtime.evaluate({expression: 'window.internals.isUseCounted(document, 41)'}));
    }
  ]);
})
