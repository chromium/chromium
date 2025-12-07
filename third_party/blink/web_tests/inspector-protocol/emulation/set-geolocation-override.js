(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests that geolocation can be overridden.');

  await dp.Browser.grantPermissions({
    origin: location.origin,
    permissions: ['geolocation'],
  });

  async function logGeolocationData() {
    const original_geolocation_result = await session.evaluateAsync(`
      new Promise(
        resolve => window.navigator.geolocation.getCurrentPosition(
          position => resolve(position.coords.toJSON()),
          error => resolve({code: error.code, message: error.message}),
          {timeout: 200}
      ))`);
    testRunner.log(original_geolocation_result, 'Geolocation data: ');
  }

  testRunner.log('\nGet original geolocation data');
  await logGeolocationData();

  testRunner.log('\nSet required geolocation override fields');
  await dp.Emulation.setGeolocationOverride({
    latitude: 56.83,
    longitude: 60.63,
    accuracy: 1.23,
  });
  await logGeolocationData();

  testRunner.log('\nSet full geolocation override');
  await dp.Emulation.setGeolocationOverride({
    latitude: 52.52,
    longitude: 13.41,
    accuracy: 2.34,
    altitude: 122,
    altitudeAccuracy: 2.34,
    heading: 42,
    speed: 32
  });
  await logGeolocationData();

  testRunner.log('\nSet empty geolocation override');
  await dp.Emulation.setGeolocationOverride({});
  await logGeolocationData();

  testRunner.completeTest();
});
