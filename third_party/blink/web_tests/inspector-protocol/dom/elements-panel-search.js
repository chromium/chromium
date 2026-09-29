// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
      <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
      <html id="documentElement">
      <head><base href=${testRunner.url('')}></head>
      <body>
      <div>FooBar</div>
      <input value="InputVal">
      <div attr="foo"></div>
      <div id="terminator"></div>
      <div class="divclass"><span>Found by selector</span></div>
      <span class="foo koo"></span>
      <span class="CASELESS"></span>
      <span data-camel="insenstive"></span>
      <div id="shadow-host">
          <div id="shadow-host-content"></div>
      </div>
      <template id="shadow-dom-template">
        <div id="shadow-dom-outer">
            <content></content>
        </div>

      </template>
      <textarea></textarea>
      <div id="ua-shadow-host"></div>
      </body>
      </html>
    `,
      'Tests that elements panel search is returning proper results.');
  await session.evaluate(`
      function initializeShadowDOM()
      {
          var shadow = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
          var template = document.querySelector('#shadow-dom-template');

          // Avoid matching this function
          shadow.appendChild(template.content.cloneNode(true));

          var uaShadow = internals.createUserAgentShadowRoot(
              document.querySelector('#ua-shadow-host'));
          var uaShadowContent = document.createElement('div');
          uaShadowContent.id = 'ua-shadow-' + 'content';
          uaShadow.appendChild(uaShadowContent);
      }
  `);

  var omitInnerHTML;

  async function performSearch(query, includeUserAgentShadowDOM) {
    const {result: {searchId, resultCount}} =
        await dp.DOM.performSearch({query, includeUserAgentShadowDOM});
    if (resultCount == 0) {
      testRunner.log('Nothing found');
      await dp.DOM.discardSearchResults({searchId});
      return;
    }

    const {result: {nodeIds}} = await dp.DOM.getSearchResults(
        {searchId, fromIndex: 0, toIndex: resultCount});
    for (var i = 0; i < resultCount; ++i) {
      var markupValue =
          (await dp.DOM.getOuterHTML({nodeId: nodeIds[i]})).result.outerHTML;
      if (omitInnerHTML)
        markupValue = markupValue.substr(0, markupValue.indexOf('>') + 1);
      testRunner.log(markupValue.split('').join(' '));
    }

    await dp.DOM.discardSearchResults({searchId});
  }

  testRunner.runTestSuite([
    async function testSetUp() {
      await dp.DOM.enable();
      await dp.DOM.getDocument();
      await session.evaluate('initializeShadowDOM()');
    },

    async function testPlainText() {
      await performSearch('Fo' +
                              'o' +
                              'Bar',
                          false);
    },

    async function testPartialText() {
      await performSearch('oo' +
                              'Ba',
                          false);
    },

    async function testStartTag() {
      await performSearch('<inpu' +
                              't',
                          false);
    },

    async function testEndTag() {
      await performSearch('npu' +
                              't>',
                          false);
    },

    async function testPartialTag() {
      await performSearch('npu' +
                              't',
                          false);
    },

    async function testPartialAbsentTagStart() {
      await performSearch('<npu' +
                              't',
                          false);
    },

    async function testPartialAbsentTagEnd() {
      await performSearch('npu' +
                              '>',
                          false);
    },

    async function testFullTag() {
      await performSearch('<inpu' +
                              't>',
                          false);
    },

    async function testExactAttributeName() {
      await performSearch('valu' +
                              'e',
                          false);
    },

    async function testExactAttributeValue() {
      await performSearch('In' +
                              'putVa' +
                              'l',
                          false);
    },

    async function testExactAttributeValueOnRoot() {
      omitInnerHTML = true;
      await performSearch('documen' +
                              'tElement',
                          false);
    },

    async function testExactAttributeValueWithQuotes() {
      omitInnerHTML = false;
      await performSearch('"fo' +
                              'o"',
                          false);
    },

    async function testPartialAttributeValue() {
      await performSearch('n' +
                              'putVa' +
                              'l',
                          false);
    },

    async function testXPathAttribute() {
      await performSearch('//html' +
                              '//@attr',
                          false);
    },

    async function testSelector() {
      await performSearch('d' +
                              'iv.divclass span',
                          false);
    },

    async function testCaseUpperFindsLower() {
      await performSearch('K' +
                              'OO',
                          false);
    },

    async function testCaseLowerFindsUpper() {
      await performSearch('c' +
                              'aseless',
                          false);
    },

    async function testCaseAttribute() {
      await performSearch('C' +
                              'AMEL',
                          false);
    },

    async function testSearchShadowDOM() {
      await performSearch('<c' +
                              'ontent',
                          false);
    },

    async function testSearchUAShadowDOM() {
      testRunner.log('Searching UA shadow DOM with setting disabled:');
      await performSearch('ua-shadow-' +
                              'content',
                          false);
      testRunner.log('Searching UA shadow DOM with setting enabled:');
      await performSearch('ua-shadow-' +
                              'content',
                          true);
    },

    async function testSearchShadowHostChildren() {
      await performSearch('shadow-host-c' +
                              'ontent',
                          false);
    },

    async function testSearchClosingTag() {
      await performSearch('</textarea>', false);
    },
  ]);
})
