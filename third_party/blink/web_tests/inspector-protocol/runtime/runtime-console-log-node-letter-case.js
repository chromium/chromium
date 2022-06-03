(async function testRemoteObjects(testRunner) {
  const {dp} = await testRunner.startBlank('Test right letter case in description of Nodes.');
  dp.Runtime.enable();

  const result1 = await dp.Runtime.evaluate({ expression:
    `new DOMParser().parseFromString('<PascalCase></PascalCase>', 'application/xml').querySelector('PascalCase')`
  });
  const result2 = await dp.Runtime.evaluate({ expression:
    `new DOMParser().parseFromString('<PascalCase></PascalCase>', 'text/html').querySelector('PascalCase')`
  });

  testRunner.log(result1.result.result);
  testRunner.log(result2.result.result);
  testRunner.completeTest();
});