(async function testRemoteObjects(testRunner) {
  const {dp} = await testRunner.startBlank('Test description generation for DocumentType Nodes.');
  dp.Runtime.enable();

  const result = await dp.Runtime.evaluate({ expression:
    `document.implementation.createDocumentType('svg:svg', '-//W3C//DTD SVG 1.1//EN', 'http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd');`
  });

  testRunner.log(result.result.result);
  testRunner.completeTest();
});