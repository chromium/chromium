(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
<style>
#element {
  background-color: white;
  animation:
    1s --color infinite,
    1s --width-and-color infinite;
  transition: 100s background-color;
}

#element::before {
  content: '';
  background-color: white;
  animation: 1s --color infinite;
  transition: 100s background-color;
}

#element.with-background-color {
  background-color: black;
}

#element.with-background-color::before {
  background-color: black;
}

.grand-parent {
  background-color: white;
  animation: 1s --color infinite;
  transition: 100s background-color;
}

.grand-parent.with-background-color {
  background-color: black;
}

#element-for-pseudo::before {
  content: '';
  background-color: white;
  animation: 1s --color infinite;
}

#sda-non-scrolling {
  width: 120px;
  height: 120px;
  animation: --color linear;
  animation-timeline: scroll();
}

#animation-start-delay {
  width: 120px;
  height: 120px;
  animation: --color linear;
  animation-start-delay: 100s;
}

@keyframes --color {
  from {
    color: red;
  }

  to {
    color: blue;
  }
}

@keyframes --width-and-color {
  from {
    width: 10px;
    color: purple;
  }

  to {
    width: 20px;
    color: green;
  }
}
</style>

<div class='grand-parent'>
  <div class='parent'>
    <div id='element'>Text</div>
    <div id='element-for-pseudo'></div>
    <div id='sda-non-scrolling'></div>
    <div id='animation-start-delay'></div>
  </div>
</div>
`,
      'Tests getAnimatedStylesForNode behavior');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const NodeTracker =
      await testRunner.loadScript('../resources/node-tracker.js');
  const nodeTracker = new NodeTracker(dp);
  const DOMHelper = await testRunner.loadScript('../resources/dom-helper.js');

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const elementNodeId =
      await cssHelper.requestNodeId(documentNodeId, '#element');
  testRunner.log('\nAnimation styles for the element');
  let {result: {animationStyles, inherited}} =
      await dp.CSS.getAnimatedStylesForNode({nodeId: elementNodeId});
  logCondition(
      'There are 2 animation styles for the element',
      animationStyles.length === 2);
  logCondition(
      'Animation styles are ordered by their composite ordering',
      animationStyles[0].name === '--width-and-color' &&
          animationStyles[1].name === '--color');
  logCondition(
      'There are 2 animated CSS properties for --width-and-color animation',
      animationStyles[0].style.cssProperties.length === 2);
  logCondition(
      'There is 1 animated CSS property for --color animation',
      animationStyles[1].style.cssProperties.length === 1);

  testRunner.log('\nWAAPI animations for the element');
  await dp.Runtime.evaluate({
    expression:
        'document.querySelector(\'#element\').animate([{ transform: \'rotate(0) scale(1)\' }, { transform: \'rotate(360deg) scale(0)\' }], { duration: 10000, iterations: 1 })'
  });
  ({result: {animationStyles, inherited}} =
       await dp.CSS.getAnimatedStylesForNode({nodeId: elementNodeId}));
  logCondition(
      'There are 3 animations with the WAAPI animation',
      animationStyles.length === 3);
  logCondition(
      'WAAPI animation has the highest compositing order and it doesn\'t have a name',
      animationStyles[0].name === undefined);

  testRunner.log('\nInherited animations for element');
  logCondition(
      'First inherited entry is empty (.parent does not have any animations)',
      inherited[0].animationsStyle === undefined);
  logCondition(
      'Second inherited entry contains the color animation (for .grand-parent)',
      inherited[1].animationStyles[0].name === '--color');

  testRunner.log('\nTransitions style for element');
  await dp.Runtime.evaluate({
    expression:
        'document.querySelector("#element").classList.add("with-background-color")'
  });
  const {result: {transitionsStyle}} =
      await dp.CSS.getAnimatedStylesForNode({nodeId: elementNodeId});
  logCondition(
      'There is 1 transitioned CSS property',
      transitionsStyle.cssProperties.length === 1);
  logCondition(
      'background-color is transitioned',
      transitionsStyle.cssProperties.some(
          property => property.name === 'background-color'));

  testRunner.log('\nInherited transitions for element');
  await dp.Runtime.evaluate({
    expression:
        'document.querySelector(".grand-parent").classList.add("with-background-color")'
  });
  const {result: {inherited: inheritedForTransitionsStyle}} =
      await dp.CSS.getAnimatedStylesForNode({nodeId: elementNodeId});
  logCondition(
      'First inherited entry is empty (.parent does not have any transitions)',
      inheritedForTransitionsStyle[0].transitionsStyle === undefined);
  logCondition(
      'Second inherited entry contains background-color transition (for .grand-parent)',
      inheritedForTransitionsStyle[1].transitionsStyle.cssProperties[0].name ===
          'background-color');

  testRunner.log('\nAnimation styles for pseudo ::before');
  const node = nodeTracker.nodes().find(
      node => DOMHelper.attributes(node).get('id') === 'element');
  const beforeNodeId = getPseudoElement(node, 'before').nodeId;
  const {
    result: {
      animationStyles: animationStylesForPseudo,
      inherited: inheritedForPseudo
    }
  } = await dp.CSS.getAnimatedStylesForNode({nodeId: beforeNodeId});
  logCondition(
      'There is only 1 animation', animationStylesForPseudo.length === 1);
  logCondition(
      'The name of the animation is --color',
      animationStylesForPseudo[0].name === '--color');
  logCondition(
      'The color property is animated',
      animationStylesForPseudo[0].style.cssProperties[0].name === 'color');

  testRunner.log('\nInherited animations for pseudo ::before');
  logCondition(
      'First inherited entry contains animations from "#element"',
      inheritedForPseudo[0].animationStyles[0].name === undefined &&
          inheritedForPseudo[0].animationStyles[1].name ===
              '--width-and-color' &&
          inheritedForPseudo[0].animationStyles[2].name === '--color');
  logCondition(
      'Second inherited entry is empty (.parent does not have any animations)',
      inheritedForPseudo[1].animationStyles.length === 0);

  testRunner.log('\nInherited transitions for pseudo ::before');
  logCondition(
      'First inherited entry contains transitions from "#element"',
      inheritedForPseudo[0].transitionsStyle.cssProperties.find(
          property => property.name === 'background-color'));
  logCondition(
      'Second inherited entry is empty (.parent does not have any transitions)',
      inheritedForPseudo[1].transitionsStyle === undefined);

  testRunner.log('\nAnimations for pseudo ::before when its origin element does not have any animations');
  const nodeForPseudo = nodeTracker.nodes().find(
    node => DOMHelper.attributes(node).get('id') === 'element-for-pseudo');
  const beforeNodeIdForPseudo = getPseudoElement(nodeForPseudo, 'before').nodeId;
  const {
    result: {
      animationStyles: animationStylesForPseudoWithoutParentAnimations,
    }
  } = await dp.CSS.getAnimatedStylesForNode({nodeId: beforeNodeIdForPseudo});
  logCondition(
    'There is only 1 animation', animationStylesForPseudoWithoutParentAnimations.length === 1);
  logCondition(
      'The name of the animation is --color',
      animationStylesForPseudoWithoutParentAnimations[0].name === '--color');
  logCondition(
      'The color property is animated',
      animationStylesForPseudoWithoutParentAnimations[0].style.cssProperties[0].name === 'color');

  testRunner.log('\nScroll driven animations in a non-scrollable container');
  const sdaNodeId = nodeTracker.nodes().find(
    node => DOMHelper.attributes(node).get('id') === 'sda-non-scrolling').nodeId;
  const {
    result: {
      animationStyles: animationStylesForNonScrollingSda,
    }
  } = await dp.CSS.getAnimatedStylesForNode({nodeId: sdaNodeId});
  logCondition(
    'There is no animation', animationStylesForNonScrollingSda.length === 0);

  testRunner.log('\nAnimations with `animation-start-delay` before the delay passes');
  const animationStartDelayNodeId = nodeTracker.nodes().find(
    node => DOMHelper.attributes(node).get('id') === 'animation-start-delay').nodeId;
  const {
    result: {
      animationStyles: animationStartDelayAnimationStyles,
    }
  } = await dp.CSS.getAnimatedStylesForNode({nodeId: animationStartDelayNodeId});
  logCondition(
    'There is no animation', animationStartDelayAnimationStyles.length === 0);

  testRunner.completeTest();
  function logCondition(text, condition) {
    testRunner.log(`${text}: ${condition ? 'TRUE' : 'FALSE'}`)
  }

  function getPseudoElement(node, ...pseudoTypes) {
    for (const pseudoType of pseudoTypes)
      node = node.pseudoElements.find(
          pseudoElement => pseudoElement.pseudoType === pseudoType);
    return node;
  }
});
