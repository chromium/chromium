(async function(testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <style>
        div { border: 1px solid; padding: 0 8px; margin: 8px 0; }
        span.ahem { font-family: 'Ahem'; }
        span.courier { font-family: 'Courier New', 'Courier', 'Cousine'; }

        @font-face {
            font-family: 'test1';
            src: local('Times'), local('Tinos-Regular');
        }
        @font-face {
            font-family: 'test1';
            src: url('../../resources/Ahem.ttf');
            unicode-range: U+0041;
        }

        @font-face {
            font-family: 'test2';
            src: local('Times'), local('Tinos-Regular');
        }
        @font-face {
            font-family: 'test2';
            src: url('../../resources/Ahem.ttf');
            unicode-range: U+004?;
        }

        @font-face {
            font-family: 'test3';
            src: local('Times'), local('Tinos-Regular');
        }
        @font-face {
            font-family: 'test3';
            src: url('../../resources/Ahem.ttf');
            unicode-range: U+0042-0044;
        }

        @font-face {
            font-family: 'test4';
            src: local('Times'), local('Tinos-Regular');
        }
        @font-face {
            font-family: 'test4';
            src: url('../../resources/Ahem.ttf');
            unicode-range: U+0050-0058;
        }
        @font-face {
            font-family: 'test4';
            src: local('Courier New'), local('Courier'), local('Cousine-Regular');
            unicode-range: U+004F-0051;
        }

        @font-face {
            font-family: 'test5';
            src: local('Times'), local('Times Roman'), local('Tinos-Regular'), local('Times New Roman');
        }
        @font-face {
            font-family: 'test5';
            src: url('../../resources/Ahem.ttf');
            unicode-range: U+0050-0058;
        }
        @font-face {
            font-family: 'test5';
            src: local('Courier New'), local('Courier'), local('Cousine-Regular');
            unicode-range: U+0052-0055;
        }

        @font-face {
            font-family: 'test6';
            src: local('Courier New'), local('Courier'), local('Cousine-Regular');
        }
        @font-face {
            font-family: 'test6';
            src: url('../../resources/Ahem.ttf');
            unicode-range: U+0027;  /* missing glyph */
        }

        @font-face {
            font-family: dummy;
            src: local(fails_to_find_font);
        }
    </style>
    <div class="test">
    <div id="test_1" style="font-family: test1;">
            ABCDEFGHIJKLMNOPQRSTUVWXYZ
    </div>
    <div id="font_usage_reference__must_match_test_1">
            <span class="ahem">A</span>BCDEFGHIJKLMNOPQRSTUVWXYZ
    </div>
    <div id="test_2" style="font-family: test2;">
            ABCDEFGHIJKLMNOPQRSTUVWXYZ
    </div>
    <div id="font_usage_reference__must_match_test_2">
            <span class="ahem">ABCDEFGHIJKLMNO</span>PQRSTUVWXYZ
    </div>
    <div id="test_3" style="font-family: test3;">
            ABCDEFGHIJKLMNOPQRSTUVWXYZ
    </div>
    <div id="font_usage_reference__must_match_test_3">
            A<span class="ahem">BCD</span>EFGHIJKLMNOPQRSTUVWXYZ
    </div>
    <div id="test_4" style="font-family: test4;">
            ABCDEFGHIJKLMNOPQRSTUVWXYZ
    </div>
    <div id="font_usage_reference__must_match_test_4">
            ABCDEFGHIJKLMN<span class="courier">OPQ</span><span class="ahem">RSTUVWX</span>YZ
    </div>
    <div id="test_5" style="font-family: test5;">
            ABCDEFGHIJKLMNOPQRSTUVWXYZ
    </div>
    <div id="font_usage_must_match_test_5">
            ABCDEFGHIJKLMNO<span class="ahem">PQ</span><span class="courier">RSTU</span><span class="Ahem">VWX</span>YZ
    </div>
    <div id="test_5a" style="font-family: dummy, test5;">
            ABCDEFGHIJKLMNOPQRSTUVWXYZ
    </div>
    <div id="font_usage_reference__must_match_test_5a">
            ABCDEFGHIJKLMNO<span class="ahem">PQ</span><span class="courier">RSTU</span><span class="Ahem">VWX</span>YZ
    </div>
    <div id="test_6" style="font-family: test6;">
            '''
    </div>
    <div id="font_usage_reference__must_match_test_6">
            <span class="courier">'''</span>
    </div>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})
