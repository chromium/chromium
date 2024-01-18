(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests Input.dispatchMouseEvent method after emulating a viewport.`);


  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }
  async function sendClick(protocol) {
    dumpError(await protocol.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      clickCount: 1,
      x: 55,
      y: 55
    }));
  }
  async function testClick() {
    await session.evaluate(`
      window.result = 'Not Clicked';
      window.button = document.createElement('button');
      document.body.appendChild(button);
      button.style.position = 'absolute';
      button.style.top = '50px';
      button.style.left = '50px';
      button.style.width = '10px';
      button.style.height = '10px';
      button.onmousedown = () => window.result = 'Clicked';
    `);
    await sendClick(dp);

    testRunner.log(await session.evaluate(`window.result`));
  }

  testRunner.log('Emulate mobile viewport and click');
  dumpError(await dp.Emulation.setDeviceMetricsOverride({
    deviceScaleFactor: 5,
    width: 400,
    height: 300,
    mobile: true
  }));
  await session.navigate('../resources/blank.html')
  await testClick();

  await session.evaluate(`window.result = 'FAIL: not Clicked'`);
  testRunner.log('Emulate mobile viewport and click via another session');
  const dp2 = (await page.createSession()).protocol;
  await sendClick(dp2);
  testRunner.log(await session.evaluate(`window.result`));

  testRunner.log('\nClick with viewport tag');
  await session.navigate('data:text/html,<head><meta name="viewport" content="width=device-width, initial-scale=1"></head>');
  await testClick();

  testRunner.log('\nClick after adding a blank frame');
  await session.navigate('../resources/blank.html');
  await session.evaluateAsync(`new Promise(done => {
    const frame = document.createElement('iframe');
    frame.onload = done;
    frame.src = 'about:blank';
    document.body.appendChild(frame);
  })`);
  await testClick();


  testRunner.completeTest();
})
