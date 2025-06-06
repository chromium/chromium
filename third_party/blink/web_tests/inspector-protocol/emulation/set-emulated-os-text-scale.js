(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startHTML(`
    <div style='font-size: calc(100px * env(preferred-text-scale, 3));'>a</div>
    <script>
      function getPreferredTextScale() {
        let div = document.querySelector('div');
        return parseInt(getComputedStyle(div).fontSize) / 100;
      }
    </script>
  `, 'Tests that the OS text scale can be emulated.');

  let initial = await session.evaluate("getPreferredTextScale()");
  testRunner.log(`Initial preferred-text-scale: ${initial}.`);

  // Test emulating a scale of 2.
  await dp.Emulation.setEmulatedOSTextScale({scale: 2});
  let set_2 = await session.evaluate("getPreferredTextScale()");
  testRunner.log(`After setting scale to 2, preferred-text-scale is ${set_2}.`);

  // Test emulating a scale of 2 when already emulating a scale of 2.
  await dp.Emulation.setEmulatedOSTextScale({scale: 2});
  let set_2_again = await session.evaluate("getPreferredTextScale()");
  testRunner.log(`After setting scale to 2 twice, preferred-text-scale is ${set_2_again}.`);

  // Test emulating a scale of 0.85.
  await dp.Emulation.setEmulatedOSTextScale({scale: 0.85});
  let set_085 = await session.evaluate("getPreferredTextScale()");
  testRunner.log(`After setting scale to 0.85, preferred-text-scale is ${set_085}.`);

  // Test stopping emulation with no scale.
  await dp.Emulation.setEmulatedOSTextScale({});
  let set_none = await session.evaluate("getPreferredTextScale()");
  testRunner.log(`After stopping emulation, preferred-text-scale is ${set_none}.`);

  testRunner.completeTest();
})
