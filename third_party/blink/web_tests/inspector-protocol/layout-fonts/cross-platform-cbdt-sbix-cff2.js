(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
     <style type="text/css">
        @font-face {
            font-family: 'adobevfproto';
            src: url("../../third_party/AdobeVF/AdobeVFPrototype.otf");
        }
        @font-face {
            font-family: 'chromacheck-sbix';
            src: url("../../third_party/ChromaCheck/chromacheck-sbix.woff");
        }
        @font-face {
            font-family: 'chromacheck-cbdt';
            src: url("../../third_party/ChromaCheck/chromacheck-cbdt.woff");
        }

        body {
            font-size: 40px;
        }

        .cff2_test {
            font-family: adobevfproto, sans-serif;
        }

        .sbix_test {
            font-family: chromacheck-sbix, sans-serif;
        }
        .cbdt_test {
            font-family: chromacheck-cbdt, sans-serif;
        }
    </style>
    <body>
        <div class="test">
            <div id="cff2_support__should_be_using_adobe_variable_font_protoptype_only" class="cff2_test">abcdefghijklmnopqrstuvwxyz</div>
            <div id="sbix_support__should_be_using_chromacheck_only" class="sbix_test">&#xE901;&#xE901;&#xE901;&#xE901;&#xE901;&#xE901;&#xE901;</div>
            <div id="cbdt_support__should_be_using_notocoloremoji_only" class="cbdt_test">&#xE903;&#xE903;&#xE903;&#xE903;&#xE903;&#xE903;&#xE903;</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})
