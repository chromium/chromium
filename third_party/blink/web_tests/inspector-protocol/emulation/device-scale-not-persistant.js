(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/device-scale-not-persistant.html',
      'Test that srcset does not use wrong image when override scalefactor and then disabled.');

  function getSrcsetImage() {
    return session.evaluate(`document.getElementById('image-test').currentSrc`);
  }

  async function setScaleFactor(value) {
    testRunner.log('Set deviceScaleFactor: ' + value);
    await dp.Emulation.setDeviceMetricsOverride({
        deviceScaleFactor: value,
        width: 1,
        height: 1,
        mobile: false,
        fitWindow: false
    });
  }

  async function reloadPage() {
    testRunner.log('Reloading Page');
    dp.Page.reload();
    await dp.Page.onceLoadEventFired();
    testRunner.log('\nPage reloaded.\n');
  }

  async function dumpImageSrc() {
    var src = await getSrcsetImage();
    var relativeSrc = src.substring(src.lastIndexOf('/resources/'));
    testRunner.log('Used Image: ' + relativeSrc);
  }

  dp.Page.enable();
  var initialImage = await getSrcsetImage();

  await setScaleFactor(1);
  await reloadPage();
  await dumpImageSrc();

  await setScaleFactor(2);
  await reloadPage();
  await dumpImageSrc();

  testRunner.log('Clear deviceScaleFactor');
  await dp.Emulation.clearDeviceMetricsOverride();
  await reloadPage();
  var value = await getSrcsetImage();
  var initImageEqCurrentImg = initialImage === value ? 'Yes' : 'No';
  testRunner.log('Current image src equal initial image: ' + initImageEqCurrentImg);

  testRunner.completeTest();
})
