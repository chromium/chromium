(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Testing that touch emulation affects certain media queries');

  await dp.Emulation.setTouchEmulationEnabled({enabled: true, maxTouchPoints: 11});

  var mqs = [
    '(pointer: none)',
    '(pointer: coarse)',
    '(pointer: fine)',
    '(any-pointer: none)',
    '(any-pointer: coarse)',
    '(any-pointer: fine)',
    '(hover: none)',
    '(hover: hover)',
    '(any-hover: none)',
    '(any-hover: hover)'
  ];

  for (var mq of mqs) {
    var code = `window.matchMedia('${mq}').matches`;
    var result = await session.evaluate(code);
    testRunner.log(`${code} : ${result}`);

    var width = await session.evaluate(`
      var css = 'div { width: 10px; }\\n@media ${mq} { div { width: 20px; } }';
      var stylesheet = document.createElement('style');
      stylesheet.textContent = css;
      document.head.appendChild(stylesheet);

      var el = document.createElement('div');
      document.body.appendChild(el);
      var result = el.offsetWidth;
      document.body.removeChild(el);
      result
    `);
    testRunner.log(`${code} applied: ${width === 20}`);
  }

  testRunner.completeTest();
})
