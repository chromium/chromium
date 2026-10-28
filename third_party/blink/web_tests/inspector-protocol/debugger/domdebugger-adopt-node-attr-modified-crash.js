(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `<!DOCTYPE html><iframe></iframe>`,
      `Tests attribute-modified DOM breakpoint during cross-quirks-mode adoptNode.`);

  await dp.Debugger.enable();
  await dp.DOM.enable();
  await dp.DOM.getDocument({depth: -1, pierce: true});

  async function runCase(attrName, attrValue, sprayValue) {
    testRunner.log(`Testing ${attrName}="${attrValue}"`);
    const evalRes = await dp.Runtime.evaluate({
      expression: `(() => {
        const frame = document.querySelector('iframe');
        const qdoc = frame.contentDocument;
        qdoc.open();
        qdoc.write('<div></div>');
        qdoc.close();
        const el = qdoc.createElement('div');
        el.setAttribute('a0', '0');
        el.setAttribute('${attrName}', '${attrValue}');
        for (let i = 1; i <= 6; i++) {
          el.setAttribute('a' + i, String(i));
        }
        window.el = el;
        return el;
      })()`
    });
    await dp.DOM.getDetachedDomNodes();
    const objectId = evalRes.result.result.objectId;
    const {result: {nodeId}} = await dp.DOM.requestNode({objectId});
    await dp.DOMDebugger.setDOMBreakpoint({nodeId, type: 'attribute-modified'});

    const pausedPromise = dp.Debugger.oncePaused();
    const adoptPromise = dp.Runtime.evaluate({
      expression: `(() => {
        document.adoptNode(window.el);
        return window.el.getAttribute('${attrName}');
      })()`
    });

    await pausedPromise;
    await dp.Runtime.evaluate({
      expression: `(() => {
        for (let i = 0; i < 64; i++) {
          window.el.setAttribute('g' + i, 'v');
        }
        const spray = '${sprayValue}';
        for (let i = 0; i < 32; i++) {
          window.el.setAttribute('s' + i, spray + String(i).padStart(2, '0'));
        }
      })()`
    });
    await dp.Debugger.resume();

    const adoptRes = await adoptPromise;
    testRunner.log(`Result: ${adoptRes.result.result.value}`);
  }

  // 114 chars + 2 chars pad = 116 chars (128-byte StringImpl allocation).
  // Characters 12..19 (offset 24..31 in 128-byte slot) hold 0x41 ('A').
  const spray = 'A'.repeat(114);
  await runCase('id', 'Foo', spray);
  await runCase('class', 'Bar', spray);

  testRunner.completeTest();
})
