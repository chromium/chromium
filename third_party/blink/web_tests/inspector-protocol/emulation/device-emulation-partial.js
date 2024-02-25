(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <head>
    <style>
    body {
      margin: 0;
      min-height: 1000px;
    }
    </style>
    </head>
  `, 'Tests that overriding only a single parameter does not affect others.');

  function dumpMetrics() {
    return JSON.stringify({
      width: window.innerWidth,
      height: window.innerHeight,
      screenWidth: window.screen.width,
      screenHeight: window.screen.height,
      screenX: window.screenX,
      screenY: window.screenY,
      deviceScaleFactor: window.devicePixelRatio
    });
  }

  var initialMetrics;

  async function testPartialOverride(name, value) {
    var params = {width: 0, height: 0, deviceScaleFactor: 0, mobile: false, fitWindow: false};
    if (name === null) {
      await dp.Emulation.clearDeviceMetricsOverride();
    } else {
      if (name)
        params[name] = value;
      await dp.Emulation.setDeviceMetricsOverride(params);
    }

    var metrics = JSON.parse(await session.evaluate(dumpMetrics));
    var fail = false;
    for (var key in initialMetrics) {
      var expected = key === name ? value : initialMetrics[key];
      if (metrics[key] !== expected) {
        testRunner.log('[FAIL]: ' + metrics[key] + ' instead of ' + expected + ' for ' + key);
        fail = true;
      }
    }
    if (!fail)
      testRunner.log(name ? ('[PASS]: ' + name + '=' + value) : '[PASS]');
  }

  await testRunner.runTestSuite([
    async function collectMetrics() {
      initialMetrics = JSON.parse(await session.evaluate(dumpMetrics));
    },

    async function noOverrides() {
      await testPartialOverride('', 0);
    },

    async function width() {
      await testPartialOverride('width', 300);
    },

    async function height() {
      await testPartialOverride('height', 400);
    },

    async function deviceScaleFactor1() {
      await testPartialOverride('deviceScaleFactor', 1);
    },

    async function deviceScaleFactor2() {
      await testPartialOverride('deviceScaleFactor', 2);
    },

    async function clear() {
      await testPartialOverride(null, null);
    },

    async function anotherWidth() {
      await testPartialOverride('width', 400);
    },

    async function anotherHeight() {
      await testPartialOverride('height', 300);
    },

    async function deviceScaleFactor3() {
      await testPartialOverride('deviceScaleFactor', 3);
    },

    async function clear() {
      await testPartialOverride(null, null);
    }
  ]);
})
