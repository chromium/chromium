(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests that evaluating V8-embedder callbacks allows side-effect-free DOM methods.');

  await session.evaluate(`
    var ns = 'http://www.w3.org/1999/xhtml';
    document.documentElement.setAttribute('xmlns', ns);

    var div = document.createElement('div');
    div.setAttribute('attr1', 'attr1-value');
    var textNode = document.createTextNode('footext');
    div.appendChild(textNode);
    document.body.appendChild(div);

    var span = document.createElement('span');
    document.body.appendChild(span);

    var otherDiv = document.createElement('div');
    document.body.appendChild(otherDiv);

    var namedNodeMap = div.attributes;
  `);

  // Document, Node, CharacterData, NamedNodeMap, and DOMStringList queries.
  await checkHasNoSideEffect(`document.createRange()`);
  await checkHasNoSideEffect(`div.getRootNode()`);
  await checkHasNoSideEffect(`div.getRootNode({composed: true})`);
  await checkHasNoSideEffect(`div.isEqualNode(otherDiv)`);
  await checkHasNoSideEffect(`div.isSameNode(div)`);
  await checkHasNoSideEffect(`div.compareDocumentPosition(span)`);
  await checkHasNoSideEffect(`div.lookupPrefix(ns)`);
  await checkHasNoSideEffect(`div.lookupNamespaceURI(null)`);
  await checkHasNoSideEffect(`div.isDefaultNamespace(ns)`);
  await checkHasNoSideEffect(`textNode.substringData(0, 3)`);
  await checkHasNoSideEffect(`namedNodeMap.getNamedItemNS(null, 'attr1')`);
  await checkHasNoSideEffect(`location.ancestorOrigins.item(0)`);
  await checkHasNoSideEffect(
      `location.ancestorOrigins.contains('http://example.com')`);

  // Dictionary side-effect getter termination and DOM mutator blocking.
  await checkHasSideEffect(
      `div.getRootNode({ get composed() { window.pwned = 1; return true; } })`);
  await checkHasSideEffect(`namedNodeMap.removeNamedItem('attr1')`);

  testRunner.completeTest();

  async function checkHasSideEffect(expression) {
    return checkExpression(expression, true);
  }

  async function checkHasNoSideEffect(expression) {
    return checkExpression(expression, false);
  }

  // V8 appends a stack trace to exception.description, so match with startsWith
  // and fail explicitly on non-EvalError exceptions such as TypeError.
  async function checkExpression(expression, expectSideEffect) {
    const {result: {exceptionDetails}} =
        await dp.Runtime.evaluate({expression, throwOnSideEffect: true});
    let hasSideEffect = false;
    if (exceptionDetails) {
      const description =
          exceptionDetails.exception?.description ?? exceptionDetails.text;
      if (!description.startsWith(
              'EvalError: Possible side-effect in debug-evaluate')) {
        testRunner.log(
            `FAIL: Unexpected exception for \`${expression}\`: ${description}`);
        return;
      }
      hasSideEffect = true;
    }
    const failed = hasSideEffect !== expectSideEffect;
    testRunner.log(`${failed ? 'FAIL: ' : ''}Expression \`${
        expression}\`\nhas side effect: ${hasSideEffect}, expected: ${
        expectSideEffect}`);
  }
})
