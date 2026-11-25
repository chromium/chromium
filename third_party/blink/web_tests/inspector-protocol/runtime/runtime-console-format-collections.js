// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <div style="display:none" class="c1 c2 c3">
        <form id="f">
          <select id="sel" name="sel">
            <option value="1">one</option>
            <option value="2">two</option>
          </select>
          <input type="radio" name="x" value="x1"> x1
          <input type="radio" name="x" value="x2"> x2
        </form>
      </div>
      `,
      'Tests that Runtime.consoleAPICalled generates array-like previews for HTMLCollections, NodeLists, DOMTokenLists, and Arguments without invoking custom length getters.');

  await dp.Runtime.enable();

  function formatProp(p) {
    return String(Number(p.name)) === p.name ? `${p.value}` :
                                               `${p.name}: ${p.value}`;
  }

  dp.Runtime.onConsoleAPICalled(({params}) => {
    for (const arg of params.args) {
      if (arg.preview) {
        const props = arg.preview.properties.map(formatProp).join(', ');
        testRunner.log(`${arg.className} (subtype=${arg.subtype ?? 'none'}): ${
            arg.preview.description} [${props}]`);
      } else {
        testRunner.log(`${arg.type}: ${arg.value ?? arg.description}`);
      }
    }
  });

  await session.evaluate(`
    function logToConsole() {
      var formElement = document.getElementById("f");
      var selectElement = document.getElementById("sel");
      var spanElement = document.getElementById("span");

      // NodeList
      var nodelist = document.getElementsByTagName("select");
      console.log(nodelist);

      // HTMLCollection
      var htmlcollection = document.head.children;
      console.log(htmlcollection);

      // HTMLOptionsCollection
      var options = selectElement.options;
      console.log(options);

      // HTMLAllCollection
      var all = document.all;
      console.log(all);

      // HTMLFormControlsCollection
      var formControls = formElement.elements;
      console.log(formControls);

      // RadioNodeList
      var radioNodeList = formElement.x;
      console.log(radioNodeList);

      // Cross-referencing arrays.
      var arrayX = [1];
      var arrayY = [2, arrayX];
      arrayX.push(arrayY);
      console.log(arrayX);

      var nonArray = new NonArrayWithLength();
      console.log(nonArray);

      // Arguments
      function generateArguments(foo, bar) {
        return arguments;
      }
      console.log(generateArguments(1, "2"));

      // DOMTokenList
      var div = document.getElementsByTagName("div")[0];
      console.log(div.classList);
    }

    logToConsole();
    function NonArrayWithLength() {
      this.keys = [];
    }

    NonArrayWithLength.prototype.__defineGetter__("length", function() {
      console.log("FAIL: 'length' should not be called");
      return this.keys.length;
    });
  `);

  await session.evaluate('logToConsole()');
  testRunner.completeTest();
});
