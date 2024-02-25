(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <html>
    <body>
      <host-element>
        <template shadowrootmode="open">
          <h2>Open shadow root</h2>
        </template>
      </host-element>
      <host-element>
        <template shadowrootmode="closed">
          <h2>Closed shadow root</h2>
        </template>
      </host-element>
      <textarea>test for ua shadow roots are not returned by DOMSnapshot.captureSnapshot</textarea>
    </body>
    </html>`, 'Tests that DOMSnapshot.captureSnapshot properly reports shadow root types');

  const { result } = await dp.DOMSnapshot.captureSnapshot({'computedStyles': []});
  const nodeValue = result.documents[0].nodes.nodeValue;
  const shadowRootType = result.documents[0].nodes.shadowRootType;
  const shadowRootTypes = new Map(shadowRootType.index.map((nodeIdx, i) => {
    return [nodeIdx, result.strings[shadowRootType.value[i]]];
  }));
  for (let i = 0; i < nodeValue.length; i++) {
    const value = nodeValue[i];
    const nodeName = result.documents[0].nodes.nodeName[i];
    testRunner.log([
      'node: ',
      'value=' + String(result.strings[value]).trim(),
      'name=' + result.strings[nodeName],
      'shadowRootType=' + shadowRootTypes.get(i),
    ].join(' '));
  }
  testRunner.completeTest();
})
