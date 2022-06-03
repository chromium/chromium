(async function testRemoteObjects(testRunner) {
  const {dp} = await testRunner.startBlank('Test description generation for HTML Nodes.');
  dp.Runtime.enable();

  const result1 = await dp.Runtime.evaluate({ expression:
    `x = document.createElement("div"); x`
  });
  const result2 = await dp.Runtime.evaluate({ expression:
    `x.id = "x_id"; x`
  });
  const result3 = await dp.Runtime.evaluate({ expression:
    `x.className = "x_class"; x`
  });
  const result4 = await dp.Runtime.evaluate({ expression:
    `x.className = "x_class1 x_class2"; x`
  });

  testRunner.log(result1.result.result);
  testRunner.log(result2.result.result);
  testRunner.log(result3.result.result);
  testRunner.log(result4.result.result);
  testRunner.completeTest();
});