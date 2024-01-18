(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <div id='shadow-host'></div>
    <iframe id='iframe'></iframe>
  `, 'Tests finding DOM nodes by computed styles traversing in-process iframes and shadow roots.');

  // Insert shadow root.
  await session.evaluate(() => {
    const host = document.querySelector('#shadow-host');
    const root = host.attachShadow({mode: 'open'});
    root.innerHTML = `<style>
      .shadow-grid {
        display: grid;
      }
      </style>
      <div class="shadow-grid"></div>
    `;
  });

  // Insert an iframe.
  await session.evaluate(() => {
    const iframe = document.querySelector('#iframe');
    iframe.srcdoc = `<!DOCTYPE html>
      <style>
        .iframe-grid {
          display: grid;
        }
      </style>
      <div class="iframe-grid"></div>
    `;
  });

  await dp.DOM.enable();
  const response = await dp.DOM.getDocument();
  const rootNodeId = response.result.root.nodeId;

  nodesResponse = await dp.DOM.getNodesForSubtreeByStyle({
    nodeId: rootNodeId,
    computedStyles: [
      { name: 'display', value: 'grid' },
      { name: 'display', value: 'inline-grid' },
    ],
    pierce: true,
  });

  testRunner.log("Expected nodeIds length: 2");
  testRunner.log("Actual nodeIds length: " + nodesResponse.result.nodeIds.length);

  testRunner.log("Nodes:");
  for (const nodeId of nodesResponse.result.nodeIds) {
    const nodeResponse = await dp.DOM.describeNode({ nodeId });
    testRunner.log(nodeResponse.result);
  }


  testRunner.completeTest();
})

