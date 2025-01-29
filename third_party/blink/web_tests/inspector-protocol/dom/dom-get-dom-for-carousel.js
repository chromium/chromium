(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
      <style>
        #carousel {
          overflow: auto;
          scroll-snap-type: x mandatory;
          anchor-name: --carousel;

          &::scroll-button(right),
          &::scroll-button(left) {
            position: fixed;
            position-anchor: --carousel;
          }

          &::scroll-button(right) {
            --_inner: center span-inline-start;
            --_outer: inline-end center;

            position-area: var(--_outer);
            content: '->';
          }

          &::scroll-button(left) {
            --_inner: center span-inline-end;
            --_outer: inline-start center;

            position-area: var(--_outer);
            content: '<-';
          }

          scroll-marker-group: after;

          &.paginated {
            columns: 1;
            block-size: 25cqi;
            text-align: center;

            &::column {
              scroll-snap-align: center;
            }

            &::column::scroll-marker {
              content: ' ';

              &:target-current {
                background: blue;
              }
            }
          }

          &:not(.paginated) {
            & > li {
              scroll-snap-align: center;
            }
            & > li::scroll-marker {
              content: ' ';

              &:target-current {
                background: blue;
              }
            }
          }
        }
      </style>
      <div id="carouselParent">
        <ul id="carousel" class="paginated">
          <li>1</li>
          <li>2</li>
          <li>3</li>
          <li>4</li>
          <li>5</li>
          <li>6</li>
          <li>7</li>
          <li>8</li>
          <li>9</li>
          <li>10</li>
          <li>11</li>
          <li>12</li>
          <li>13</li>
          <li>14</li>
          <li>15</li>
          <li>16</li>
          <li>17</li>
        </ul>
      </div>`,
      'The test verifies functionality of querying DOM structure for carousel related pseudo elements');

  await dp.DOM.enable();
  await dp.CSS.enable();
  await dp.Runtime.enable();
  const documentResponse = await dp.DOM.getDocument();
  const carouselParentResponse = await dp.DOM.querySelector({
    nodeId: documentResponse.result.root.nodeId,
    selector: '#carouselParent'
  });

  const setChildNodesResponses = [];
  dp.DOM.onSetChildNodes((message) => setChildNodesResponses.push(message));
  await dp.DOM.requestChildNodes(
      {nodeId: carouselParentResponse.result.nodeId});

  assert(
      'setChildNodes event was fired once', setChildNodesResponses.length, 1);
  assert(
      'Carousel is only child of carouselParent',
      setChildNodesResponses[0].params.nodes.length, 1);
  const carousel = setChildNodesResponses[0].params.nodes[0];
  const nodeMap = new Map();

  testRunner.log('Carousel DOM tree:');
  dumpDom(carousel, nodeMap);

  const scrollMarker = carousel.pseudoElements.at(-1).pseudoElements.at(-1);
  assert(
      'Nested scroll marker is present', scrollMarker.pseudoType,
      'scroll-marker');

  testRunner.log('Scroll marker styles:');
  await dumpStyles(scrollMarker);

  testRunner.log('Changing carousel class');

  const messages = [];
  let resolvePromise = null;
  const donePromise = new Promise((resolve) => {
    resolvePromise = resolve;
  });
  function checkMessages() {
    if (messages.length === 21) {
      resolvePromise();
    }
  };
  dp.DOM.onPseudoElementAdded((message) => {
    messages.push(`${message.params.pseudoElement.nodeName} added to ${
        nodeMap.get(message.params.parentId).nodeName}`);
    checkMessages();
  });
  dp.DOM.onPseudoElementRemoved((message) => {
    messages.push(
        `${nodeMap.get(message.params.pseudoElementId).nodeName} removed from ${
            nodeMap.get(message.params.parentId).nodeName}`);
    checkMessages();
  });
  await dp.Runtime.evaluate({
    expression:
        'document.querySelector("#carousel").classList.remove("paginated")'
  });

  testRunner.log('Wait for events for pseudo element changes');
  await donePromise;
  testRunner.log(
      'Check that columns were removed and scroll markers were moved to list items');

  for (const message of messages.sort()) {
    testRunner.log(message);
  }

  testRunner.completeTest();

  function dumpDom(node, nodeMap, indent = 1) {
    const indentString = '  '.repeat(indent);
    testRunner.log(indentString + node.nodeName);
    nodeMap.set(node.nodeId, node);
    if (node.children) {
      for (const child of node.children) {
        dumpDom(child, nodeMap, indent + 1);
      }
    }
    if (node.pseudoElements) {
      for (const child of node.pseudoElements) {
        dumpDom(child, nodeMap, indent + 1);
      }
    }
  }

  async function dumpStyles(node) {
    const styles = await dp.CSS.getMatchedStylesForNode({nodeId: node.nodeId});
    for (const style of styles.result.matchedCSSRules) {
      for (const matcher of style.rule.selectorList.selectors) {
        testRunner.log(matcher.text);
      }
      testRunner.log('{');
      for (const property of style.rule.style.cssProperties) {
        if (property.text) {
          testRunner.log('  ' + property.text);
        }
      }
      testRunner.log('}');
    }
  }

  function assert(message, actual, expected) {
    if (actual === expected) {
      testRunner.log("PASS: " + message);
    } else {
      testRunner.log("FAIL: " + message + ", expected \"" + expected + "\" but got \"" + actual + "\"");
      testRunner.completeTest();
    }
  };
});
