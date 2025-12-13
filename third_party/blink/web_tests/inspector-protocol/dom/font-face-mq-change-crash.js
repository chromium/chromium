(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startHTML(`
<!DOCTYPE html>
<style>body { color: red; }</style>
<style>@font-face {}</style>
<body>
<script>
  function crashme() {
    document.styleSheets[0].disabled = true;
    document.body.offsetTop;
    document.styleSheets[1].media = "print";
    document.body.offsetTop;
  }
</script>
`, 'Verify that modifying active @font-face with pending style invalidations does not crash via Devtools');

  await dp.DOM.enable();
  await dp.CSS.enable();
  await session.evaluate('crashme();');
  testRunner.completeTest();
});
