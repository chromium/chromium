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

  testRunner.log('\nSet geolocation override and check the position timestamp');
  await dp.Emulation.setGeolocationOverride({
    latitude: 50,
    longitude: 100,
    accuracy: 95,
  });
  await logGeolocationData();
  const timestampIsRecent = await session.evaluateAsync(`
    new Promise(
      resolve => window.navigator.geolocation.getCurrentPosition(
        position => resolve(Math.abs(Date.now() - position.timestamp) < 60000),
        () => resolve(false),
        {timeout: 200}
    ))`);
  testRunner.log('Position timestamp is recent: ' + timestampIsRecent);

  testRunner.log('\nSet geolocation override with invalid parameter type, ' +
                 'previous override is kept');
  const invalidParamResponse = await dp.Emulation.setGeolocationOverride({
    latitude: true,
    longitude: 100,
    accuracy: 95,
  });
  testRunner.log(invalidParamResponse.error, 'Error: ');
  await logGeolocationData();

  testRunner.log(
      '\nSet out of range geolocation override, previous override is kept');
  const invalidGeolocationResponse = await dp.Emulation.setGeolocationOverride({
    latitude: 200,
    longitude: 300,
    accuracy: 95,
  });
  testRunner.log(invalidGeolocationResponse.error, 'Error: ');
  await logGeolocationData();

  testRunner.log('\nClear geolocation override');
  await dp.Emulation.clearGeolocationOverride();
  await logGeolocationData();

  testRunner.completeTest();
});
