(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <body>
        <div class="test">
            <div lang="en" id="dingbats_segoe_ui_semilight">❘</div>
            <div lang="en" id="dingbats_unicode_block">✀✁✂✃✄✅✆✇✈✉✊✋✌✍✎✏✐✑✒✓✔✕✖✗✘✙✚✛✜✝✞✟✠✡✢✣✤✥✦✧✨✩✪✫✬✭✮✯✰✱✲✳✴✵✶✷✸✹✺✻✼✽✾✿❀❁❂❃❄❅❆❇❈❉❊❋❌❍❎❏❐❑❒❓❔❕❖❗❘❙❚❛❜❝❞❟❠❡❢❣❤❥❦❧❨❩❪❫❬❭❮❯❰❱❲❳❴❵❶❷❸❹❺❻❼❽❾❿➀➁➂➃➄➅➆➇➈➉➊➋➌➍➎➏➐➑➒➓➔➕➖➗➘➙➚➛➜➝➞➟➠➡➢➣➤➥➦➧➨➩➪➫➬➭➮➯➰➱➲➳➴➵➶➷➸➹➺➻➼➽➾➿</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();
  testRunner.log(`Test passes if the #dingbats_segoe_ui_semilight block uses the Segoe UI Semilight font instead of the Arial font to display a .notdef tofu block.`);

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  var results = await helper(testRunner, session);

  var segoe_ui_semilight = results.find(x => x.selector === '#dingbats_segoe_ui_semilight').usedFonts;
  var all_dingbats = results.find(x => x.selector === '#dingbats_unicode_block').usedFonts;
  var passed = (segoe_ui_semilight.length === 1 && segoe_ui_semilight[0].familyName.includes('Segoe UI Semilight'));
  passed = passed && (all_dingbats.length === 2
                      && all_dingbats[0].familyName.includes('Segoe UI Symbol')
                      && all_dingbats[1].familyName.includes('Segoe UI Emoji'));
  testRunner.log(passed ? 'PASS' : 'FAIL');
  testRunner.completeTest();
})
