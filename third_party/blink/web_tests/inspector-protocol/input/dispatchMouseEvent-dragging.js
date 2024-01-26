self.testRunner.disableAutomaticDragDrop();

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
        #drag {
          width: 100px;
          height: 100px;
        }
        #drop {
          width: 100px;
          height: 100px;
        }
      </style>
      <div id="drag" draggable="true">cdp_test</div>
      <div id="drop">drop here</div>
    `,
      `Tests Input.dispatchMouseEvent method for dragging`);

  await session.evaluate(`
    document.getElementById('drag').addEventListener('dragstart', event => {
      event.dataTransfer.setData("text/plain", event.target.textContent);
    });

    const drop = document.getElementById('drop');
    drop.addEventListener('dragenter', (event) => event.preventDefault());
    drop.addEventListener('dragover', (event) => event.preventDefault());

    var logs = [];
    function log(text) {
      logs.push(text);
    }

    function logEvent(event) {
      log('-----Event-----');
      log('type: ' + event.type);
      log('button: ' + event.button);
      log('buttons: ' + event.buttons);
      log('clientX: ' + event.clientX);
      log('clientY: ' + event.clientY);
      log('screenX: ' + event.screenX);
      log('screenY: ' + event.screenY);
    }

    window.addEventListener('mousedown', logEvent, true);
    window.addEventListener('mousemove', logEvent, true);
    window.addEventListener('mouseup', logEvent, true);

    window.addEventListener('dragstart', logEvent, true);
    window.addEventListener('dragenter', logEvent, true);
    window.addEventListener('dragleave', logEvent, true);
    window.addEventListener('dragover', logEvent, true);
    window.addEventListener('dragend', logEvent, true);

    window.addEventListener('drag', logEvent, true);
    window.addEventListener('drop', logEvent, true);
  `);

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  testRunner.log('Drag');
  {
    dumpError(await dp.Input.dispatchMouseEvent({
      type: 'mouseMoved',
      button: 'left',
      buttons: 0,
      x: 50,
      y: 50,
    }));
    dumpError(await dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      buttons: 0,
      clickCount: 1,
      x: 50,
      y: 50,
    }));
    dumpError(await dp.Input.dispatchMouseEvent({
      type: 'mouseMoved',
      button: 'left',
      buttons: 1,
      x: 50,
      y: 150,
    }));
  }
  testRunner.log(await session.evaluate(`window.logs.join('\\n')`));
  await session.evaluate(`window.logs=[]`);

  testRunner.log('Drop');
  {
    dumpError(await dp.Input.dispatchMouseEvent({
      type: 'mouseReleased',
      button: 'left',
      buttons: 1,
      clickCount: 1,
      x: 50,
      y: 150,
    }));
  }
  testRunner.log(await session.evaluate(`window.logs.join('\\n')`));
  await session.evaluate(`window.logs=[]`);

  testRunner.log('Drag and drop with movements between elements');
  {
    dumpError(await dp.Input.dispatchMouseEvent({
      type: 'mouseMoved',
      button: 'left',
      buttons: 0,
      x: 50,
      y: 50,
    }));
    dumpError(await dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      buttons: 0,
      clickCount: 1,
      x: 50,
      y: 50,
    }));
    for (let i = 1; i < 5; ++i) {
      dumpError(await dp.Input.dispatchMouseEvent({
        type: 'mouseMoved',
        button: 'left',
        buttons: 1,
        x: 50,
        y: 50 + i * 25,
      }));
    }
    dumpError(await dp.Input.dispatchMouseEvent({
      type: 'mouseReleased',
      button: 'left',
      buttons: 1,
      clickCount: 1,
      x: 50,
      y: 150,
    }));
    for (let i = 1; i < 3; ++i) {
      dumpError(await dp.Input.dispatchMouseEvent({
        type: 'mouseMoved',
        button: 'left',
        buttons: 1,
        x: 50,
        y: 50 + i,
      }));
    }
  }
  testRunner.log(await session.evaluate(`window.logs.join('\\n')`));
  await session.evaluate(`window.logs=[]`);

  testRunner.log('Drag and drop with cancellation');
  {
    dumpError(await dp.Input.dispatchMouseEvent({
      type: 'mouseMoved',
      button: 'left',
      buttons: 0,
      x: 50,
      y: 50,
    }));
    dumpError(await dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      buttons: 0,
      clickCount: 1,
      x: 50,
      y: 50,
    }));
    for (let i = 1; i < 5; ++i) {
      dumpError(await dp.Input.dispatchMouseEvent({
        type: 'mouseMoved',
        button: 'left',
        buttons: 1,
        x: 50,
        y: 50 + i * 25,
      }));
    }
    dumpError(await dp.Input.cancelDragging());
    for (let i = 1; i < 3; ++i) {
      dumpError(await dp.Input.dispatchMouseEvent({
        type: 'mouseMoved',
        button: 'left',
        buttons: 1,
        x: 50,
        y: 50 + i,
      }));
    }
  }
  testRunner.log(await session.evaluate(`window.logs.join('\\n')`));

  testRunner.completeTest();
});
