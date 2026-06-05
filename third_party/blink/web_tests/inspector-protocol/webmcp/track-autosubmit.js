(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <form id="declarative"
            toolname="declarative_tool"
            tooldescription="A declarative WebMCP tool"
            action="some_action.html">
        <input type="text" name="text">
      </form>
      `,
      'Tests that WebMCP toolautosubmit attribute changes are tracked.');

  let addedCount = 0;
  let addedTarget = 0;
  let addedResolvers = null;
  dp.WebMCP.onToolsAdded(e => {
    for (const tool of e.params.tools) {
      testRunner.log(`Tool added: ${tool.name}, autosubmit=${!!(tool.annotations && tool.annotations.autosubmit)}`);
    }
    addedCount += e.params.tools.length;
    if (addedResolvers && addedCount >= addedTarget) {
      addedResolvers.resolve();
      addedResolvers = null;
    }
  });

  let removedCount = 0;
  let removedTarget = 0;
  let removedResolvers = null;
  dp.WebMCP.onToolsRemoved(e => {
    for (const tool of e.params.tools) {
      testRunner.log(`Tool removed: ${tool.name}`);
    }
    removedCount += e.params.tools.length;
    if (removedResolvers && removedCount >= removedTarget) {
      removedResolvers.resolve();
      removedResolvers = null;
    }
  });

  async function waitAdded(count) {
    addedTarget += count;
    if (addedCount >= addedTarget) return;
    addedResolvers = Promise.withResolvers();
    return addedResolvers.promise;
  }

  async function waitRemoved(count) {
    removedTarget += count;
    if (removedCount >= removedTarget) return;
    removedResolvers = Promise.withResolvers();
    return removedResolvers.promise;
  }

  testRunner.log('Enabling WebMCP...');
  let enablePromise = waitAdded(1);
  await dp.WebMCP.enable();
  await enablePromise;

  testRunner.log('Adding toolautosubmit attribute...');
  let removePromise = waitRemoved(1);
  let addPromise = waitAdded(1);
  await dp.Runtime.evaluate({expression: `
    document.getElementById('declarative').setAttribute('toolautosubmit', '');
  `});
  await Promise.all([removePromise, addPromise]);

  testRunner.log('Removing toolautosubmit attribute...');
  let removePromise2 = waitRemoved(1);
  let addPromise2 = waitAdded(1);
  await dp.Runtime.evaluate({expression: `
    document.getElementById('declarative').removeAttribute('toolautosubmit');
  `});
  await Promise.all([removePromise2, addPromise2]);

  testRunner.completeTest();
})
