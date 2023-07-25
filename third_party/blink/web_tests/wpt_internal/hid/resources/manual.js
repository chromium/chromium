let manualTestDevices = [];

navigator.hid.addEventListener('disconnect', (e) => {
  if (manualTestDevices.includes(e.device)) {
    manualTestDevices = [];
  }
})

async function getDeviceForManualTest() {
  if (manualTestDevices.length > 0) {
    return manualTestDevices;
  }

  const button = document.createElement('button');
  button.textContent = 'Click to connect to a HID device';
  button.style.display = 'block';
  button.style.fontSize = '20px';
  button.style.padding = '10px';

  await new Promise((resolve) => {
    button.onclick = () => {
      document.body.removeChild(button);
      resolve();
    };
    document.body.appendChild(button);
  });

  manualTestDevices = await navigator.hid.requestDevice({filters: []});
  assert_greater_than(manualTestDevices.length, 0);
  for (const d of manualTestDevices) {
    assert_true(d instanceof HIDDevice);
  }

  return manualTestDevices;
}

// Returns the first top-level collection in `device.collections` with a
// usage matching `usagePage` and `usage`, or `undefined` if no matching
// collection was found.
function getCollectionByUsage(device, usagePage, usage) {
  return device.collections.find(c => {
      return c.usagePage == usagePage && c.usage == usage;});
}

// Returns the first device in `devices` with a top-level collection
// matching `usagePage` and `usage`, or `undefined` if no matching device
// was found.
function getDeviceByCollectionUsage(devices, usagePage, usage) {
  return devices.find(d => {
    return getCollectionByUsage(d, usagePage, usage) !== undefined;
  });
}

// Returns the first report in `devices` with matching `reportType` and
// `reportId`, or `undefined` if no matching report was found.
function getReport(devices, reportType, reportId) {
  for (const d of devices) {
    for (const c of d.collections) {
      let reports = [];
      if (reportType == 'input')
        reports = c.inputReports;
      else if (reportType == 'output')
        reports = c.outputReports;
      else if (reportType == 'feature')
        reports = c.featureReports;

      const r = reports.find(r => { return r.reportId == reportId; });
      if (r !== undefined)
        return r;
    }
  }
  return undefined;
}

// Returns `true` if `devices` contains a device with matching `vendorId`
// and `productId`.
function hasDeviceIds(devices, vendorId, productId) {
  return devices.find(d => {
    return d.vendorId == vendorId && d.productId == productId;
  }) !== undefined;
}

function manual_hid_test(func, name, properties) {
  promise_test(async (test) => {
    await func(test, await getDeviceForManualTest());
  }, name, properties);
}
