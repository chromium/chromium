(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
      <div style='width:100px; height:100px; view-transition-name: shared;'></div>`,
      'The test verifies functionality of querying DOM structure for view-transition pseudo elements');

  await session.evaluateAsync(`
     new Promise( async (resolve) => {
       // Wait for the promise below and query style to ensure all
       // pseudo-elements are generated before using the devtools API.
       await document.startViewTransition().ready;
       window.getComputedStyle(document.documentElement, "::view-transition-new(root)").background;
       resolve();
     });
  `);

  const response = await dp.DOM.getDocument();
  const rootNode = response.result.root.children[0];
  assert("Root has correct number of pseudo-elements", rootNode.pseudoElements.length, 1);

  const viewTransition = rootNode.pseudoElements[0];
  assert("Root has ::view-transition pseudo-element", viewTransition.nodeName, "::view-transition");
  assert("::view-transition has 2 group pseudos", viewTransition.pseudoElements.length, 2);

  const viewTransitionGroupRoot = viewTransition.pseudoElements[0];
  assert("::view-transition has group for root", viewTransitionGroupRoot.nodeName, "::view-transition-group");
  assert("::view-transition-group for root is painted first", viewTransitionGroupRoot.pseudoIdentifier, "root");
  validateGroupPseudoTree(viewTransitionGroupRoot, "root");

  const viewTransitionGroupShared = viewTransition.pseudoElements[1];
  assert("::view-transition has group for shared", viewTransitionGroupShared.nodeName, "::view-transition-group");
  assert("::view-transition-group for shared is painted second", viewTransitionGroupShared.pseudoIdentifier, "shared");
  validateGroupPseudoTree(viewTransitionGroupShared, "shared");

  testRunner.completeTest();

  function assert(message, actual, expected) {
    if (actual === expected) {
      testRunner.log("PASS: " + message);
    } else {
      testRunner.log("FAIL: " + message + ", expected \"" + expected + "\" but got \"" + actual + "\"");
      testRunner.completeTest();
    }
  };

  function validateGroupPseudoTree(viewTransitionGroup, name) {
    assert("::view-transition-group has correct identifier", viewTransitionGroup.pseudoIdentifier, name);
    assert("::view-transition-group has 1 pseudo", viewTransitionGroup.pseudoElements.length, 1);

    const viewTransitionImagePair = viewTransitionGroup.pseudoElements[0];
    assert("::view-transition-group's pseudo is an image-pair", viewTransitionImagePair.nodeName, "::view-transition-image-pair");
    assert("::view-transition-image-pair has correct view-transition-name", viewTransitionImagePair.pseudoIdentifier, name);
    assert("::view-transition-image-pair has 2 pseudos", viewTransitionImagePair.pseudoElements.length, 2);

    const viewTransitionOld = viewTransitionImagePair.pseudoElements[0];
    assert("::view-transition-old is first child", viewTransitionOld.nodeName, "::view-transition-old");
    assert("::view-transition-old has correct view-transition-name", viewTransitionOld.pseudoIdentifier, name);
    assert("::view-transition-old has no children", viewTransitionOld.pseudoElements, undefined);

    const viewTransitionNew = viewTransitionImagePair.pseudoElements[1];
    assert("::view-transition-new is second child", viewTransitionNew.nodeName, "::view-transition-new");
    assert("::view-transition-new has correct view-transition-name", viewTransitionNew.pseudoIdentifier, name);
    assert("::view-transition-new has no children", viewTransitionNew.pseudoElements, undefined);
  };
});

