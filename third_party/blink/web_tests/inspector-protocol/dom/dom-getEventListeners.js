(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <template id='shadow-template' onclick='clickTemplate'>
    <style>
    :host {
        color: red;
    }
    </style>
    <div></div><h1>Hi from a template!</h1></div>
    </template>
    </head>
    <body class='body-class' onload='runTest()'>
        <div id='A'> A
          <div id='B'> B
            <div id='C'> C
            </div>
          </div>
        </div>

        <iframe src='../dom/resources/iframe-with-listener.html' width='400' height='200'></iframe>
        <div id='shadow-host'></div>
    </body>
  `, 'Tests retrieving event listeners from DOMDebugger.');

  await session.evaluate(() => {
    var host = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    var template = document.querySelector('#shadow-template');
    host.appendChild(template.content);
    template.remove();
    document.getElementById('A').addEventListener('listenerA', () => {});
    document.getElementById('B').addEventListener('listenerB', () => {});
    document.getElementById('C').addEventListener('listenerC', () => {});
    document.addEventListener('documentListener', () => {});
  });

  dp.DOM.enable();
  dp.Runtime.enable();
  var result = await dp.Runtime.evaluate({expression: 'document'});
  var objectId = result.result.result.objectId;

  await dumpListeners(objectId);
  await dumpListeners(objectId, 1);
  await dumpListeners(objectId, 4);
  await dumpListeners(objectId, -1);
  await dumpListeners(objectId, -1, true);
  testRunner.completeTest();

  async function dumpListeners(objectId, depth, pierce) {
    testRunner.log(`Fetching listeners for depth = ${depth} and pierce = ${pierce}`);
    var {result} = await dp.DOMDebugger.getEventListeners({objectId, depth, pierce});
    testRunner.log(result);
  }
})
