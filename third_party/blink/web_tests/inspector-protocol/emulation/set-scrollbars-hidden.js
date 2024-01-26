(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests Emulation.setScrollbarsHidden.');
  await dp.Emulation.setScrollbarsHidden({ hidden: true });
  await session.navigate('../resources/set-scrollbars-hidden.html');
  testRunner.log('Scrollbar width = ' + await session.evaluate(`
    var outer = document.querySelector('.outer');
    outer.scrollTop = 200;
    outer.offsetWidth - outer.clientWidth
  `));
  testRunner.completeTest();
})
