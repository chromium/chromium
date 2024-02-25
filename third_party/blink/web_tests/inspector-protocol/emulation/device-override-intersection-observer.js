(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  const {page, session, dp} = await testRunner.startBlank(
      `Tests that intersection observer works with device emulation.`);

  const params = {
      deviceScaleFactor: 1,
      width: 1024,
      height: 1024,
      mobile: false,
      screenWidth: 1024,
      screenHeight: 1024,
      viewport: {x: 0, y: 0, width: 1024, height: 1024, scale: 1}
  };
  await dp.Emulation.setDeviceMetricsOverride(params);
  dp.Runtime.enable();
  dp.Runtime.onConsoleAPICalled(event => {
    testRunner.log(event.params.args[0].value);
  });
  await page.loadHTML(`
    <html><head>
    <style>
      .tall { background-color: green; height: 3000px; }
      .short { background-color: red; height: 100px; }
    </style></head>
    <body>
      <div id='target' class='short'></div>
      <div class='tall'></div>
    </body>
  `);
  await session.evaluateAsync(`
    const observer = new IntersectionObserver((entries, observer) => {
      for (const entry of entries)
        console.log(entry.target.id + ' intersecting ' + entry.isIntersecting);
      if (window.intersectionCallback)
        window.intersectionCallback();
    });
    new Promise(resolve => {
      window.intersectionCallback = resolve;
      observer.observe(document.querySelector('#target'));
    })
  `);
  await session.evaluateAsync(`
    new Promise(resolve => {
      window.intersectionCallback = resolve;
      window.scrollBy(0, 200);
    });
  `);
  await session.evaluateAsync(`
    new Promise(resolve => {
      window.intersectionCallback = resolve;
      window.scrollTo(0, 0);
    });
  `);
  testRunner.completeTest();
})
