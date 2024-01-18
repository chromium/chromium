(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='listeners1' onload='return 42;'></div>
    <div id='listeners2'></div>
  `, `Tests how DOMDebugger reports event listeners for nodes.`);

  function logGetListenersResult(title, response) {
    testRunner.log('Event listeners of ' + title);
    var listenersArray = response.result.listeners;
    listenersArray.sort((o1, o2) => o1.type === o2.type ? 0 : (o1.type < o2.type ? -1 : 1));
    for (var l of listenersArray) {
      testRunner.log('  type:' + l.type);
      testRunner.log('  useCapture:' + l.useCapture);
      testRunner.log('  lineNumber:' + l.lineNumber);
      testRunner.log('  columnNumber:' + l.columnNumber);
      if (l.handler) {
        testRunner.log('  handler.type:' + l.handler.type);
        testRunner.log('  handler.className:' + l.handler.className);
        testRunner.log('  handler.description:' + l.handler.description.replace(/(\r\n|\n|\r)/gm,''));
      }
      testRunner.log('');
    }
    testRunner.log('');
  }

  var objectId = (await dp.Runtime.evaluate({expression:
    `(function(){
            window.addEventListener('scroll', function(){ consol.log(42) }, false);
            window.addEventListener('scroll', function(){ consol.log(42) }, false);
            function clickHandler(event) { console.log('click - button - bubbling (registered before attribute)'); }
            window.addEventListener('click', clickHandler, true);
            window.addEventListener('hover', function hoverHandler(event) { console.log("hover - button - bubbling"); }, true);
            return window;
    })()
  `, objectGroup: 'event-listeners-test'})).result.result.objectId;
  logGetListenersResult('window', await dp.DOMDebugger.getEventListeners({objectId}));

  var objectId = (await dp.Runtime.evaluate({expression:
    `(function(){
            var div = document.getElementById('listeners1');
            function clickHandler(event) { console.log('click - button - bubbling (registered before attribute)'); }
            div.addEventListener('click', clickHandler, true);
            div.addEventListener('hover', function hoverHandler(event) { console.log("hover - button - bubbling"); }, true);
            return div;
    })()
  `, objectGroup: 'event-listeners-test'})).result.result.objectId;
  logGetListenersResult('div#listeners1', await dp.DOMDebugger.getEventListeners({objectId}));

  var objectId = (await dp.Runtime.evaluate({expression:
    `(function(){
      return document.getElementById('listeners2');
    })()
  `, objectGroup: 'event-listeners-test'})).result.result.objectId;
  logGetListenersResult('div#listeners2', await dp.DOMDebugger.getEventListeners({objectId}));

  testRunner.completeTest();
})
