(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <body>
        <div class="test">
            <div id="zh-CN" lang="zh-CN">zh-CN: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="zh-TW" lang="zh-TW">zh-TW: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="zh-HK" lang="zh-HK">zh-HK: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="ja" lang="ja">ja: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="ja-JP" lang="ja-JP">ja-JP: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="ko" lang="ko">ko: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="ko-KR" lang="ko-KR">ko-KR: &#x8AA4;&#x904E;&#x9AA8;</div>

            <div id="en-CN" lang="en-CN">en-CN: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="en-JP" lang="en-JP">en-JP: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="en-KR" lang="en-KR">en-KR: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="en-HK" lang="en-HK">en-HK: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="en-TW" lang="en-TW">en-TW: &#x8AA4;&#x904E;&#x9AA8;</div>

            <div id="en-HanS" lang="en-HanS">en-HanS: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="en-HanT" lang="en-HanT">en-HanT: &#x8AA4;&#x904E;&#x9AA8;</div>

            <div id="en-HanS-JP" lang="en-HanS-JP">en-HanS-JP: &#x8AA4;&#x904E;&#x9AA8;</div>
            <div id="en-HanT-JP" lang="en-HanT-JP">en-HanT-JP: &#x8AA4;&#x904E;&#x9AA8;</div>

            <div id="en-US" lang="en-US">en-US: &#x8AA4;&#x904E;&#x9AA8;</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})
