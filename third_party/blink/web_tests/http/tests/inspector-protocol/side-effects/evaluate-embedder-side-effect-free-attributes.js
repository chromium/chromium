(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that evaluating V8-embedder callbacks allows side-effect-free attribute getters. Should not crash.`);

  await session.evaluate(`
    var div = document.createElement('div');
    div.id = 'foo';
    div.className = 'bar baz';
    div.setAttribute('attr1', 'attr1-value');
    div.tabIndex = -1;
    div.style.color = 'red';

    var textNode = document.createTextNode('footext');
    div.appendChild(textNode);
    var textNode2 = document.createTextNode('bartext');
    div.appendChild(textNode2);

    var shadowContainer = document.createElement('div');
    var shadowRoot = shadowContainer.attachShadow({mode: 'open'});
    var divInShadow = document.createElement('div');
    shadowRoot.appendChild(divInShadow);

    var url = new URL('http://localhost:12345');
  `);

  // Sanity check: test that setters are not allowed on whitelisted accessors.
  await checkHasSideEffect(`document.title = "foo"`);

  // Document
  await checkHasNoSideEffect(`document.domain`);
  await checkHasNoSideEffect(`document.referrer`);
  await checkHasNoSideEffect(`document.cookie`);
  await checkHasNoSideEffect(`document.title`);
  await checkHasNoSideEffect(`document.documentElement`);
  await checkHasNoSideEffect(`document.scrollingElement`);
  await checkHasNoSideEffect(`document.body`);
  await checkHasNoSideEffect(`document.head`);
  await checkHasNoSideEffect(`document.location`);
  await checkHasNoSideEffect(`document.defaultView`);

  // DocumentOrShadowRoot
  await checkHasNoSideEffect(`document.activeElement`);

  // Element
  await checkHasNoSideEffect(`div.tagName`);
  await checkHasNoSideEffect(`div.id`);
  await checkHasNoSideEffect(`div.className`);
  await checkHasNoSideEffect(`div.classList`);
  await checkHasNoSideEffect(`div.attributes`);
  await checkHasNoSideEffect(`shadowContainer.shadowRoot`);
  await checkHasNoSideEffect(`div.innerHTML`);
  await checkHasNoSideEffect(`div.outerHTML`);

  // HTMLElement
  await checkHasNoSideEffect(`div.hidden`);
  await checkHasNoSideEffect(`div.tabIndex`);
  await checkHasNoSideEffect(`div.style`);

  // Location
  await checkHasNoSideEffect(`location.href`);

  // Navigator
  await checkHasNoSideEffect(`navigator.userAgent`);

  // Node
  var testNodes = ['div', 'document', 'textNode'];
  for (var node of testNodes) {
    await checkHasNoSideEffect(`${node}.nodeType`);
    await checkHasNoSideEffect(`${node}.nodeName`);
    await checkHasNoSideEffect(`${node}.nodeValue`);
    await checkHasNoSideEffect(`${node}.textContent`);
    await checkHasNoSideEffect(`${node}.isConnected`);
    await checkHasNoSideEffect(`${node}.parentNode`);
    await checkHasNoSideEffect(`${node}.parentElement`);
    await checkHasNoSideEffect(`${node}.childNodes`);
    await checkHasNoSideEffect(`${node}.firstChild`);
    await checkHasNoSideEffect(`${node}.lastChild`);
    await checkHasNoSideEffect(`${node}.previousSibling`);
    await checkHasNoSideEffect(`${node}.nextSibling`);
    await checkHasNoSideEffect(`${node}.ownerDocument`);
  }

  // ParentNode
  for (var node of testNodes) {
    await checkHasNoSideEffect(`${node}.childElementCount`);
    await checkHasNoSideEffect(`${node}.children`);
    await checkHasNoSideEffect(`${node}.firstElementChild`);
    await checkHasNoSideEffect(`${node}.lastElementChild`);
  }

  // URL
  await checkHasNoSideEffect('url.hash');
  await checkHasNoSideEffect('url.host');
  await checkHasNoSideEffect('url.hostname');
  await checkHasNoSideEffect('url.origin');
  await checkHasNoSideEffect('url.password');
  await checkHasNoSideEffect('url.pathname');
  await checkHasNoSideEffect('url.port');
  await checkHasNoSideEffect('url.protocol');
  await checkHasNoSideEffect('url.search');
  await checkHasNoSideEffect('url.searchParams');
  await checkHasNoSideEffect('url.username');

  // Window
  await checkHasNoSideEffect(`devicePixelRatio`);
  await checkHasNoSideEffect(`screenX`);
  await checkHasNoSideEffect(`screenY`);
  await checkHasNoSideEffect(`document`);
  await checkHasNoSideEffect(`history`);
  await checkHasNoSideEffect(`navigator`);
  await checkHasNoSideEffect(`performance`);
  await checkHasNoSideEffect(`window`);
  await checkHasNoSideEffect(`location`);

  // May update layout/scroll/style
  await checkHasNoSideEffect(`div.scrollTop`);
  await checkHasNoSideEffect(`div.scrollLeft`);
  await checkHasNoSideEffect(`div.scrollWidth`);
  await checkHasNoSideEffect(`div.scrollHeight`);
  await checkHasNoSideEffect(`div.clientTop`);
  await checkHasNoSideEffect(`div.clientLeft`);
  await checkHasNoSideEffect(`div.clientWidth`);
  await checkHasNoSideEffect(`div.clientHeight`);
  await checkHasNoSideEffect(`innerWidth`);
  await checkHasNoSideEffect(`innerHeight`);
  await checkHasNoSideEffect(`outerWidth`);
  await checkHasNoSideEffect(`outerHeight`);
  await checkHasNoSideEffect(`div.offsetParent`);
  await checkHasNoSideEffect(`div.offsetTop`);
  await checkHasNoSideEffect(`div.offsetLeft`);
  await checkHasNoSideEffect(`div.offsetWidth`);
  await checkHasNoSideEffect(`div.offsetHeight`);
  await checkHasNoSideEffect(`div.innerText`);
  await checkHasNoSideEffect(`div.outerText`);
  await checkHasNoSideEffect(`div.style.border`);

  testRunner.completeTest();


  async function checkHasSideEffect(expression) {
    return checkExpression(expression, true);
  }

  async function checkHasNoSideEffect(expression) {
    return checkExpression(expression, false);
  }

  async function checkExpression(expression, expectSideEffect) {
    var response = await dp.Runtime.evaluate({expression, throwOnSideEffect: true});
    var hasSideEffect = false;
    var exceptionDetails = response.result.exceptionDetails;
    if (exceptionDetails &&
        exceptionDetails.exception.description.startsWith('EvalError: Possible side-effect in debug-evaluate'))
      hasSideEffect = true;
    const failed = (hasSideEffect !== expectSideEffect);
    testRunner.log(`${failed ? 'FAIL: ' : ''}Expression \`${expression}\`\nhas side effect: ${hasSideEffect}, expected: ${expectSideEffect}`);
  }
})
