(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <div id="target">original</div>
    <div id="host">
      <template shadowrootmode="open" shadowrootcustomelementregistry>
        <div id="shadow-target">original</div>
      </template>
    </div>
    <script>
      class MyElement extends HTMLElement {
        connectedCallback() {
          this.setAttribute('data-upgraded', 'true');
        }
      }
      customElements.define('my-element', MyElement);
    </script>
  `, 'Tests that DOM.setOuterHTML properly upgrades custom elements when scoped custom element registry is enabled.');

  // --- Test 1: Global registry ---
  testRunner.log('Test 1: Global registry');
  const doc = await dp.DOM.getDocument({depth: -1, pierce: true});
  const bodyId = (await dp.DOM.querySelector({nodeId: doc.result.root.nodeId, selector: 'body'})).result.nodeId;
  const targetId = (await dp.DOM.querySelector({nodeId: bodyId, selector: '#target'})).result.nodeId;

  await dp.DOM.setOuterHTML({
    nodeId: targetId,
    outerHTML: '<my-element id="target">edited</my-element>'
  });

  const result1 = await session.evaluate(() => {
    const el = document.getElementById('target');
    return {
      tagName: el.tagName.toLowerCase(),
      textContent: el.textContent,
      upgraded: el.getAttribute('data-upgraded'),
    };
  });
  testRunner.log('tag: ' + result1.tagName);
  testRunner.log('content: ' + result1.textContent);
  testRunner.log('upgraded: ' + result1.upgraded);

  // --- Test 2: Declarative shadow root initialized with scoped registry ---
  testRunner.log('');
  testRunner.log('Test 2: Declarative shadow root with scoped registry');

  // Initialize the declarative shadow root with a scoped registry first.
  await session.evaluate(() => {
    const registry = new CustomElementRegistry();
    class ScopedElement extends HTMLElement {
      connectedCallback() {
        this.setAttribute('data-upgraded', 'true');
      }
    }
    registry.define('scoped-el', ScopedElement);
    registry.initialize(document.getElementById('host').shadowRoot);
  });

  const hostId = (await dp.DOM.querySelector({nodeId: bodyId, selector: '#host'})).result.nodeId;
  const hostNode = (await dp.DOM.describeNode({nodeId: hostId, depth: -1, pierce: true})).result.node;
  const shadowTargetId = hostNode.shadowRoots[0].children[0].nodeId;

  await dp.DOM.setOuterHTML({
    nodeId: shadowTargetId,
    outerHTML: '<scoped-el id="shadow-target">edited</scoped-el>'
  });

  const result2 = await session.evaluate(() => {
    const el = document.getElementById('host').shadowRoot.getElementById('shadow-target');
    return {
      tagName: el.tagName.toLowerCase(),
      textContent: el.textContent,
      upgraded: el.getAttribute('data-upgraded'),
    };
  });
  testRunner.log('tag: ' + result2.tagName);
  testRunner.log('content: ' + result2.textContent);
  testRunner.log('upgraded: ' + result2.upgraded);

  testRunner.completeTest();
})
