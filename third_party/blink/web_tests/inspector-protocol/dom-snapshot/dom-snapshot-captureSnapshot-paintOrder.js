(async function(testRunner) {
  var {session, dp} = await testRunner.startURL('../resources/stacking_context.html', 'Tests DOMSnapshot.captureSnapshot method returning paint order indexes.');

  await session.evaluate(`
    var host = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    var template = document.querySelector('#shadow-template');
    host.appendChild(template.content);
    template.remove();
    document.body.offsetWidth;
  `);

  var { result } = await dp.DOMSnapshot.captureSnapshot({'computedStyles': [], 'includePaintOrder': true});
  let entries = [];
  for (const doc of result.documents) {
    for (let i = 0; i < doc.layout.nodeIndex.length; ++i) {
      const nodeIndex = doc.layout.nodeIndex[i];
      const attrs = doc.nodes.attributes[nodeIndex];
      for (let j = 0; j < attrs.length; j += 2) {
        const name = result.strings[attrs[j]];
        const value = result.strings[attrs[j + 1]];
        if (name === 'id') {
          entries.push({
            'node': value,
            'paintOrder': doc.layout.paintOrders[i]
          });
          break;
        }
      }
    }
  }

  entries.sort(function(a, b) {
    return a['paintOrder'] - b['paintOrder'];
  });
  testRunner.log(entries);
  testRunner.completeTest();
})
