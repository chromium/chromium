(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <body class='body-class'>
    <div class='class1'></div>
    <div class='class2'>
      <ul class='class3'>
        <li class='class4'></li>
      </ul>
    </div>
    <div class='class5 class6'></div>
    <div id='shadow-host'></div>
    </body>
  `, 'Tests collecting class names in DOM domain.');

  await session.evaluate(() => {
    var host = document.querySelector('#shadow-host');
    var root = host.attachShadow({mode: 'open'});
    root.innerHTML = '<div class="shadow-class"></div>';
  });

  dp.DOM.enable();
  var response = await dp.DOM.getDocument();
  var rootNode = response.result.root;
  dp.DOM.collectClassNamesFromSubtree({nodeId: rootNode.nodeId});

  var response = await dp.DOM.collectClassNamesFromSubtree({nodeId: rootNode.nodeId});
  var allClassNames = response.result.classNames;
  allClassNames.sort();
  dp.DOM.requestChildNodes({nodeId: rootNode.children[0].children[1].nodeId});

  var message = await dp.DOM.onceSetChildNodes();
  var nodes = message.params.nodes;
  var response = await dp.DOM.collectClassNamesFromSubtree({nodeId: nodes[1].nodeId});
  var subtreeClassNames = response.result.classNames.sort();
  testRunner.log('All class names: ');
  for (var i = 0; i < allClassNames.length; i++)
    testRunner.log(allClassNames[i]);
  testRunner.log('Subtree class names: ');
  for (var i = 0; i < subtreeClassNames.length; i++)
    testRunner.log(subtreeClassNames[i]);
  testRunner.completeTest();
})

