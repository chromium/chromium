(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
    <style>
      body {
        margin: 0;
        width: 800px;
        height: 800px;
      }

      .container {
        width: 200px;
        height: 200px;
        overflow: auto;
        scroll-behavior: smooth;
        border: 1px solid black;
      }

      .container::-webkit-scrollbar {
        width: 0;
        height: 0;
      }

      .child {
        width: 500px;
        height: 500px;
        margin-top: 680px;
        margin-left: 300px;
        background: red;
      }
    </style>
    <script>
    function getScroll() {
      const c = document.querySelector('.container');
      return c.scrollLeft + ';' + c.scrollTop;
    }
    </script>
    <div class=container>
      <div style="height:20px;width:200px">
        <button style="display:contents">a button</button>
      </div>
      <div class=child>
      </div>
    </div>
    <input type=hidden>
  `,
      'Tests DOM.scrollIntoViewIfNeeded.');

  const response = await dp.Runtime.evaluate(
      {expression: `document.querySelector('.child')`});
  const objectId = response.result.result.objectId;

  testRunner.log(await session.evaluate(`getScroll()`));

  // Do not await scrollIntoViewIfNeeded to ensure we got synchronous scrolling.
  dp.DOM.scrollIntoViewIfNeeded({objectId});
  testRunner.log(await session.evaluate(`getScroll()`));

  // Ensure that we update layout before scrolling.
  session.evaluate(
      `document.querySelector('.child').style.marginTop = '1980px'`);
  dp.DOM.scrollIntoViewIfNeeded({objectId});
  testRunner.log(await session.evaluate(`getScroll()`));

  // Top-left corner should be visible.
  dp.DOM.scrollIntoViewIfNeeded(
      {objectId, rect: {x: 1, y: 1, width: 0, height: 0}});
  testRunner.log(await session.evaluate(`getScroll()`));

  // Almost bottom-right corner should be visible.
  dp.DOM.scrollIntoViewIfNeeded(
      {objectId, rect: {x: 490, y: 480, width: 5, height: 7}});
  testRunner.log(await session.evaluate(`getScroll()`));

  // Specific 200x200 rect should be visible.
  dp.DOM.scrollIntoViewIfNeeded(
      {objectId, rect: {x: 123, y: 234, width: 200, height: 200}});
  testRunner.log(await session.evaluate(`getScroll()`));

  // display:contents element should be scrolled into view as well.
  const buttonObjectId = (await dp.Runtime.evaluate({expression: `document.querySelector('button')`})).result.result.objectId;
  dp.DOM.scrollIntoViewIfNeeded({objectId: buttonObjectId});
  testRunner.log(await session.evaluate(`getScroll()`));

  // hidden elements should return an error, but not crash.
  const inputObjectId = (await dp.Runtime.evaluate({expression: `document.querySelector('input')`})).result.result.objectId;
  testRunner.log(await dp.DOM.scrollIntoViewIfNeeded({objectId: inputObjectId}));

  testRunner.completeTest();
})
