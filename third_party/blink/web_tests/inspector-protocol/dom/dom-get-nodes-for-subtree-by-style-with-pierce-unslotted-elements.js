(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <template id="my-element">
      <style>
        slot {
          display: grid;
        }
      </style>
      <div>
        <slot class="default-slot" name="my-slot">
          <span>default element</span>
        </slot>
      </div>
    </template>
    <script>
      customElements.define('my-element',
        class extends HTMLElement {
          constructor() {
            super();
            const template = document.getElementById('my-element');
            const shadowRoot = this
              .attachShadow({mode: 'open'})
              .appendChild(template.content.cloneNode(true));
          }
        }
      );
    </script>
    <my-element>
      <span class="inserted-slot" slot="my-slot" style="display: grid;">custom element</span>
    </my-element>
    <div class="grid-outside-shadow-dom" style="display: grid;"></div>
    `, 'Tests finding DOM nodes by computed styles on a page containing a custom element with unslotted nodes.');

  await dp.DOM.enable();
  const response = await dp.DOM.getDocument();
  const rootNodeId = response.result.root.nodeId;

  const nodesResponse = await dp.DOM.getNodesForSubtreeByStyle({
    nodeId: rootNodeId,
    pierce: true,
    computedStyles: [
      { name: 'display', value: 'grid' },
    ],
  });

  testRunner.log('Expected nodeIds length: 3');
  testRunner.log('Actual nodeIds length: ' + nodesResponse.result.nodeIds.length);

  testRunner.log('Nodes:');
  for (const nodeId of nodesResponse.result.nodeIds) {
    const nodeResponse = await dp.DOM.describeNode({ nodeId });
    testRunner.log(nodeResponse.result);
  }

  testRunner.completeTest();
})

