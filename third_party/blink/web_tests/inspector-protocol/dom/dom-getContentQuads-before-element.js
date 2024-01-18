(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <style>
    span::before {
      content: '\e003';
    }
    </style>
    <span onclick='javascript:window.CLICKED = 42;'></span>
  `, 'Tests DOM.getContentQuads method with single before element.');

  const document = (await dp.DOM.getDocument()).result.root;
  const node = (await dp.DOM.querySelector({nodeId: document.nodeId, selector: 'span'})).result;
  const quads = (await dp.DOM.getContentQuads({nodeId: node.nodeId})).result.quads;
  testRunner.log('Returned quads amount: ' + quads.length);
  const center = middlePoint(quads[0]);
  await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'left',
    buttons: 1,
    clickCount: 1,
    x: center.x,
    y: center.y,
  });
  await dp.Input.dispatchMouseEvent({
    type: 'mouseReleased',
    button: 'left',
    buttons: 1,
    clickCount: 1,
    x: center.x,
    y: center.y,
  });
  testRunner.log('window.CLICKED =  ' + (await session.evaluate(`window.CLICKED`)));

  testRunner.completeTest();

  function middlePoint(quad) {
    let x = 0, y = 0;
    for (let i = 0; i < 8; i += 2) {
      x += quad[i];
      y += quad[i + 1];
    }
    return {
      x: Math.round(x / 4),
      y: Math.round(y / 4)
    };
  }
})

