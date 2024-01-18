(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/stacking_context.html', 'Tests DOMSnapshot.getSnapshot method returning paint order indexes.');

  await session.evaluate(`
    var host = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    var template = document.querySelector('#shadow-template');
    host.appendChild(template.content);
    template.remove();
    document.body.offsetWidth;
  `);

  function logPaintOrderList(result) {
    let entries = [];

    for (const layout_node of result.layoutTreeNodes) {
      let attrs = result.domNodes[layout_node.domNodeIndex].attributes;
      if (!attrs) continue;
      for (const attr of attrs) {
        if (attr.name === 'id') {
          entries.push({
            'id': attr.value,
            'paintOrder': layout_node.paintOrder
          });
          break;
        }
      }
    }

    entries.sort(function(a, b) {
      return a['paintOrder'] - b['paintOrder'];
    }).forEach(function(e) {
      testRunner.log(e['id'] + ' - ' + e['paintOrder']);
    });
  }

  const response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': [], 'includePaintOrder': true});
  if (response.error)
    testRunner.log(response);
  else
    logPaintOrderList(response.result);
  testRunner.completeTest();
})
