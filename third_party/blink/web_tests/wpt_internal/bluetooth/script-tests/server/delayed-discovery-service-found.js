'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('DelayedServicesDiscoveryAdapter')
    .then(() => requestDeviceWithTrustedClick({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gatt => gatt.CALLS([
      getPrimaryService('heart_rate')|
      getPrimaryServices()|
      getPrimaryServices('heart_rate')[UUID]]))
    .then(services => {
      services = [].concat(services);
      for (let service of services)
        assert_equals(service.uuid, heart_rate.uuid);
    });
}, 'Request for service. Must return even when the services are not ' +
   'immediately discovered');
