(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <body>
        <div class="test">
            <div lang="ar" id="ar">&#x062A;&#x062D;</div>
            <div lang="hy-am" id="hy-am">&#x0540;&#x0541;</div>
            <div lang="bn" id="bn">&#x09B8;&#x09AE;</div>
            <div lang="en-us-brai" id="en-us-brai">&#x2870;&#x2871;</div>
            <div lang="bug" id="bug">&#x1A00;&#x1A01;</div>
            <div lang="cans" id="cans">&#x1410;&#x1411;</div>
            <div lang="xcr" id="xcr">&#x000102A0;&#x000102A1;</div>
            <div lang="chr" id="chr">&#x13A1;&#x13A2;</div>
            <div lang="copt" id="copt">&#x2C81;&#x2C82;</div>
            <div lang="akk" id="akk">&#x00012000;&#x0001200C;</div>
            <div lang="ecy" id="ecy">&#x00010800;&#x00010801;</div>
            <div lang="ru" id="ru">&#x0410;&#x0411;&#x0412;</div>
            <div lang="en" id="en">&#x00010400;&#x00010401;</div>
            <div lang="hi" id="hi">&#x0905;&#x0906;</div>
            <div lang="am" id="am">&#x1201;&#x1202;</div>
            <div lang="ka" id="ka">&#x10A0;&#x10A1;</div>
            <div lang="el" id="el">&#x0391;&#x0392;</div>
            <div lang="pa" id="pa">&#x0A21;&#x0A22;</div>
            <div lang="zh-CN" id="zh-CN">&#x6211;</div>
            <div lang="zh-HK" id="zh-HK">&#x6211;</div>
            <div lang="zh-Hans" id="zh-Hans">&#x6211;</div>
            <div lang="zh-Hant" id="zh-Hant">&#x6211;</div>
            <div lang="ja" id="ja">&#x6211;</div>
            <div lang="ko" id="ko">&#x1100;&#x1101;</div>
            <div lang="he" id="he">&#x05D1;&#x05D2;</div>
            <div lang="km" id="km">&#x1780;&#x1781;</div>
            <div lang="arc" id="arc">&#x00010841;&#x00010842;</div>
            <div lang="pal" id="pal">&#x00010B61;&#x00010B62;</div>
            <div lang="xpr" id="xpr">&#x00010B41;&#x00010B42;</div>
            <div lang="jv" id="jv">&#xA991;&#xA992;</div>
            <div lang="kn" id="kn">&#x0CA1;&#x0CA2;</div>
            <div lang="sa" id="sa">&#x00010A10;&#x00010A11;</div>
            <div lang="lo" id="lo">&#x0ED0;&#x0ED1;</div>
            <div lang="lis" id="lis">&#xA4D0;&#xA4D1;</div>
            <div lang="xlc" id="xlc">&#x00010281;&#x00010282;</div>
            <div lang="xld" id="xld">&#x00010921;&#x00010922;</div>
            <div lang="ml" id="ml">&#x0D21;&#x0D22;</div>
            <div lang="" id="script_meroitic">&#x000109A1;&#x000109A2;</div>
            <div lang="my" id="my">&#x1000;&#x1001;</div>
            <div lang="" id="script_new_tai_lue">&#x1981;&#x1982;</div>
            <div lang="nko" id="nko">&#x07C1;&#x07C2;</div>
            <div lang="" id="script_ogham">&#x1680;&#x1681;</div>
            <div lang="" id="script_ol_chiki">&#x1C51;&#x1C52;</div>
            <div lang="" id="script_old_italic">&#x00010301;&#x00010302;</div>
            <div lang="peo" id="peo">&#x000103A1;&#x000103A2;</div>
            <div lang="" id="script_old_south_arabian">&#x00010A61;&#x00010A62;</div>
            <div lang="or" id="or">&#x0B21;&#x0B22;</div>
            <div lang="" id="script_phags_pa">&#xA841;&#xA842;</div>
            <div lang="" id="script_runic">&#x16A0;&#x16A1;</div>
            <div lang="" id="script_shavian">&#x00010451;&#x00010452;</div>
            <div lang="si" id="si">&#x0D91;&#x0D92;</div>
            <div lang="" id="script_sora_sompeng">&#x000110D1;&#x000110D2;</div>
            <div lang="syr" id="syr">&#x0711;&#x0712;</div>
            <div lang="" id="script_tai_le">&#x1951;&#x1952;</div>
            <div lang="ta" id="ta">&#x0BB1;&#x0BB2;</div>
            <div lang="te" id="te">&#x0C21;&#x0C22;</div>
            <div lang="" id="script_thaana">&#x0781;&#x0782;</div>
            <div lang="th" id="th">&#x0e01;&#x0e02;</div>
            <div lang="bo" id="bo">&#x0F01;&#x0F02;</div>
            <div lang="" id="script_tifinagh">&#x2D31;&#x2D32;</div>
            <div lang="vai" id="vai">&#xA501;&#xA502;</div>
            <div lang="yi" id="yi">&#xA000;&#xA001;</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})
