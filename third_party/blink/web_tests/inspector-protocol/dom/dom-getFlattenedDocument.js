(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL('resources/dom-getFlattenedDocument.html', 'Tests DOM.getFlattenedDocument method.');

  await session.evaluate(() => {
    var host = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    var template = document.querySelector('#shadow-template');
    host.appendChild(template.content);
    template.remove();
  });
  dp.DOM.enable();
  var response = await dp.DOM.getFlattenedDocument({depth: -1, pierce: true});
  testRunner.log(response.result);
  testRunner.completeTest();
})

