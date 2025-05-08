(function() {

class BluetoothHelper {
  static PRECONNECTED_PERIPHERAL_ADDRESS = '09:09:09:09:09:09';
  static PRECONNECTED_PERIPHERAL_NAME = 'BLE Test Device';
  static HEART_RATE_SERVICE_UUID = '0000180d-0000-1000-8000-00805f9b34fb';
  static BATTERY_SERVICE_UUID = '0000180f-0000-1000-8000-00805f9b34fb';
  static MEASUREMENT_INTERVAL_CHARACTERISTIC_UUID =
      '00002a21-0000-1000-8000-00805f9b34fb';
  static DATE_TIME_CHARACTERISTIC_UUID = '00002a08-0000-1000-8000-00805f9b34fb';
  static CHARACTERISTIC_USER_DESCRIPTION_DESCRIPTOR_UUID =
      '00002901-0000-1000-8000-00805f9b34fb';
  static CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID =
      '00002902-0000-1000-8000-00805f9b34fb';

  static HCI_SUCCESS = 0x0000;
  static HCI_CONNECTION_TIMEOUT = 0x0008;

  constructor(testRunner, protocol, session) {
    this.testRunner_ = testRunner;
    this.protocol_ = protocol;
    this.session_ = session;
    this.peripheralAddress_ = BluetoothHelper.PRECONNECTED_PERIPHERAL_ADDRESS;
    this.peripheralName_ = BluetoothHelper.PRECONNECTED_PERIPHERAL_NAME;
  }

  async setupPreconnectedPeripheral() {
    const bp = this.testRunner_.browserP();
    await bp.BluetoothEmulation.enable(
        {state: 'powered-on', leSupported: true});
    await bp.BluetoothEmulation.simulatePreconnectedPeripheral({
      address: this.peripheralAddress_,
      name: this.peripheralName_,
      manufacturerData: [],
      knownServiceUuids: [],
    });
  }

  async setupGattOperationHandler() {
    const bp = this.testRunner_.browserP();
    await bp.BluetoothEmulation.onGattOperationReceived(
        ({params: {address, type}}) => {
          return bp.BluetoothEmulation.simulateGATTOperationResponse(
              {address, type, code: BluetoothHelper.HCI_SUCCESS});
        });
  }

  async requestDevice(...args) {
    // Setup the handler to select the first device in the prompt.
    await this.protocol_.DeviceAccess.enable();
    const deviceRequestPromptedPromise =
        this.protocol_.DeviceAccess.onceDeviceRequestPrompted().then(
            ({params: {id, devices}}) => {
              const deviceId = devices[0].id;
              return this.protocol_.DeviceAccess.selectPrompt({id, deviceId});
            });
    const requestDevicePromise =
        this.session_.evaluateAsyncWithUserGesture(async (options) => {
          return navigator.bluetooth.requestDevice(options);
        }, ...args);
    await Promise.all([deviceRequestPromptedPromise, requestDevicePromise]);
  }

  peripheralAddress() {
    return this.peripheralAddress_;
  }

  peripheralName() {
    return this.peripheralName_;
  }
}

return BluetoothHelper;
})()
