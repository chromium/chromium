(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
  <div id=host>
    <template shadowrootmode=open>
      <style>
        #comments {
          top: 10px;
          .other1 {}
          /* margin-left: 10px; */
          .other2 {}
        }
      </style>
      <div id=comments></div>
    </template>
  </div>
`,
'The test verifies that ghost rules are present for elements in shadow roots.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const hostId = await cssHelper.requestNodeId(documentNodeId, '#host');
  const {result} = await dp.DOM.describeNode({'nodeId' : hostId});

  testRunner.log(`Number of shadow roots: ${result.node.shadowRoots.length}`);
  let shadowRootId = result.node.shadowRoots[0].nodeId;

  testRunner.log('Ghost rules appear for elements inside shadow roots');
  await cssHelper.loadAndDumpInlineAndMatchingRules(shadowRootId, '#comments');

  testRunner.completeTest();
});
