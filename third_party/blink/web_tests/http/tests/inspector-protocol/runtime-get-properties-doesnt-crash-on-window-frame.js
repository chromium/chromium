(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Runtime.getProperties doesn't crash on window.frames[0]. Should not crash.`);

  await session.evaluateAsync(`
    var frame = document.createElement('iframe');
    frame.src = 'data:text/plain, <b>bold</b>';
    document.body.appendChild(frame);
    new Promise(f => frame.onload = f);
  `);

  var response = await dp.Runtime.evaluate({expression: 'window.frames[0]'});
  await dp.Runtime.getProperties({objectId: response.result.result.objectId});
  testRunner.completeTest();
})
