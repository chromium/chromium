(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
    <div contenteditable></div>
  `,
      `Tests Input.dispatchDragEvent method.`);


  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }
  await session.evaluate(`
  document.querySelector('div').addEventListener('dragover', event => {
    event.dataTransfer.dropEffect = 'copy';
  });

  document.querySelector('div').addEventListener('drop', event => {
    for (const item of event.dataTransfer.items) {
      if (item.kind === 'file') {
        event.target.innerHTML += item.getAsFile().name + '<br/>';
        event.preventDefault();
      }
    }
  })`);

  testRunner.log('\nDropping plain text');
  await drop({
    mimeType: 'text/plain',
    data: 'the drag data',
  });

  testRunner.log('\nDropping html with baseURL example.com');
  await drop({
    mimeType: 'text/html',
    data: '<a href="foo.html">foo</a>',
    baseURL: 'https://example.com',
  });

  testRunner.log('\nDropping a link to example.com');
  await drop({
    mimeType: 'text/uri-list',
    data: 'https://example.com',
    title: 'Example Website',
  });

  testRunner.log('\ndragOperationsMask = 0 should not drop');
  await drop({
    mimeType: 'text/plain',
    data: 'should not see this',
  }, 0);

  testRunner.log('\nwrong dragOperationsMask should not drop');
  await drop({
    mimeType: 'text/plain',
    data: 'should not see this',
  }, 2);

  testRunner.log('\nDropping files');
  await drop(null, 1, ['path1']);

  testRunner.completeTest();


  async function drop(item, dragOperationsMask = 1, files = undefined) {
    await session.evaluate(`document.querySelector('div').textContent = ''`);
    const data = {
      items: item ? [item] : [],
      files,
      dragOperationsMask,
    };
    dumpError(await dp.Input.dispatchDragEvent({
      type: 'dragEnter',
      data,
      x: 20,
      y: 20,
    }));
    dumpError(await dp.Input.dispatchDragEvent({
      type: 'dragOver',
      data,
      x: 20,
      y: 20,
    }));
    dumpError(await dp.Input.dispatchDragEvent({
      type: 'drop',
      data,
      x: 20,
      y: 20,
    }));
    testRunner.log('div innerHTML: ' + await session.evaluate(`document.querySelector('div').innerHTML`));
  }
});
