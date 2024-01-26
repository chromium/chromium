(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <template>
      <p>
        Hello
      </p>
    </template>
  `, 'Makes sure that nodes in inactive documents cannot be highlighted.');

  await dp.DOM.enable();
  await dp.Overlay.enable();

  const { result: { root }} = await dp.DOM.getDocument({depth: -1, pierce: true});

  function findTemplateContent(root) {
    if (root.templateContent) {
      return root.templateContent;
    }
    for (const child of root.children) {
      const content = findTemplateContent(child);
      if (content) {
        return content;
      }
    }
  }

  const result = await dp.Overlay.highlightNode({
    highlightConfig: {},
    backendNodeId: findTemplateContent(root).backendNodeId,
  });
  testRunner.log(result);

  // Wait for the node to be highlighted to catch potential cases if the backend attempts highlighting.
  await session.evaluateAsync(() => {
    return new Promise(resolve => requestAnimationFrame(resolve));
  });

  testRunner.completeTest();
});

