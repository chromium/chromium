(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='mydiv'>div from page</div>
  `, `Tests that console.memory works correct.`);

  dp.Runtime.enable();
  session.evaluate(`
    var frame = document.documentElement.appendChild(document.createElement('iframe'));
    frame.src = '${testRunner.url('../resources/iframe.html')}';
    frame.onload = function() {
      var location = frame.contentWindow.location;
      frame.remove();
      memory = console.__lookupGetter__('memory').call(location);
      console.log(memory.constructor.constructor('return document.querySelector("#mydiv").textContent')());
    }
  `);
  var result = await dp.Runtime.onceConsoleAPICalled();
  testRunner.log('=== Dump console message ===');
  testRunner.log(result.params.args[0].value);
  testRunner.completeTest();
})
