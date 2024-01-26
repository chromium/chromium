(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests Page.windowOpen notification.');
  await dp.Page.enable();

  testRunner.log(`\nOpening with empty url`);
  session.evaluate(`window.open(); undefined`);
  var event = await dp.Page.onceWindowOpen();
  testRunner.log(`url: ${event.params.url}`);

  testRunner.log(`\nOpening with absolute url`);
  session.evaluate(`window.open('http://example.com/index.html'); undefined`);
  var event = await dp.Page.onceWindowOpen();
  testRunner.log(`url: ${event.params.url}`);

  testRunner.log(`\nOpening with relative url`);
  session.evaluate(`window.open('path.html'); undefined`);
  event = await dp.Page.onceWindowOpen();
  var url = event.params.url;
  url = url.substring(url.lastIndexOf('/'));
  testRunner.log(`url: ...${url}`);

  testRunner.log(`\nOpening from click`);
  session.evaluate(`
    var a = document.createElement('a');
    a.setAttribute('href', 'anchor.html');
    a.setAttribute('target', '_blank');
    document.body.appendChild(a);
    a.click();
  `);
  event = await dp.Page.onceWindowOpen();
  url = event.params.url;
  url = url.substring(url.lastIndexOf('/'));
  testRunner.log(`url: ...${url}`);

  testRunner.completeTest();
})
