(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests enabling/disabling Page domain while recording Timeline.');

  var log = [];
  dp.Timeline.onEventRecorded(msg => {
    if (msg.params.record.type === 'Program') {
      var children = msg.params.record.children;
      for (var i = 0; i < children.length; ++i) {
        var record = children[i];
        if (record.type === 'GCEvent')
          continue;
        log.push('Timeline.eventRecorded: ' + record.type);
      }
      return;
    }
    testRunner.log('FAIL: Unexpected records arrived');
    testRunner.log(msg);
  });

  await dp.Timeline.start();
  log.push('Timeline started');
  await dp.Page.enable();
  log.push('Page enabled');
  await dp.Page.disable();
  log.push('Page disabled');

  await dp.Domain.NotExistingCommand();
  await dp.Timeline.stop();
  log.push('Timeline stopped');
  for (var i = 0; i < log.length; ++i)
    testRunner.log(log[i]);
  testRunner.completeTest();
})
