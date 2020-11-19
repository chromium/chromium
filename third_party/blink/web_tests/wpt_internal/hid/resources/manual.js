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

function manual_hid_test(func, name, properties) {
  promise_test(async (test) => {
    await func(test, await getDeviceForManualTest());
  }, name, properties);
}
