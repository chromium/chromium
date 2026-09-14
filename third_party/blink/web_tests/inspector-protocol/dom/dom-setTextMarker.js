(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
    <p id=t>The quick brown fox</p>
    <p id=cross>The <b>quick</b> fox</p>
    <input id=i value="teh lazy dog">
    <textarea id=ta>ab
cd</textarea>
    <p id=gone>to be removed</p>
    <div id=host></div>
    <p id=hidden style="display:none">hidden text</p>
    <script>
      document.querySelector('#host').attachShadow({mode: 'open'}).innerHTML =
          '<span id=inner>shadow text</span>';
    </script>
  `,
      'Tests DOM.setTextMarker and DOM.clearTextMarkers.');

  // Runs in the page. Reports every text node under |rootExpr| with its
  // spelling and grammar marker counts and ranges.
  function readMarkers(rootExpr) {
    const root = eval(rootExpr);
    const texts = [];
    const walk = node => {
      for (const child of node.childNodes) {
        if (child.nodeType === Node.TEXT_NODE) {
          texts.push(child);
        } else {
          walk(child);
        }
      }
    };
    walk(root);
    return texts
        .map(text => {
          const parts = [];
          for (const type of ['spelling', 'grammar']) {
            const n = internals.markerCountForNode(text, type);
            let s = `${type}=${n}`;
            for (let i = 0; i < n; i++) {
              const r = internals.markerRangeForNode(text, type, i);
              s += `[${r.startOffset},${r.endOffset})`;
            }
            parts.push(s);
          }
          return `${JSON.stringify(text.data)} ${parts.join(' ')}`;
        })
        .join(' | ');
  }

  async function objectIdFor(expression) {
    return (await dp.Runtime.evaluate({expression})).result.result.objectId;
  }

  async function setSpellcheckerMarker(selector, start, end, type) {
    await session.evaluate((selector, start, end, type) => {
      const text = document.querySelector(selector).firstChild;
      const r = document.createRange();
      r.setStart(text, start);
      r.setEnd(text, end);
      internals.setMarker(document, r, type);
    }, selector, start, end, type);
    testRunner.log(
        `Unforced ${type} marker on ${selector} [${start}, ${end}): ` +
        await session.evaluate(readMarkers,
                               `document.querySelector('${selector}')`));
  }

  await dp.DOM.enable();

  const t = `document.querySelector('#t')`;
  const cross = `document.querySelector('#cross')`;
  const input = `document.querySelector('#i')`;
  const inputEditor = `internals.innerEditorElement(${input})`;
  const textarea = `document.querySelector('#ta')`;
  const textareaEditor = `internals.innerEditorElement(${textarea})`;
  const gone = `document.querySelector('#gone')`;
  const host = `document.querySelector('#host')`;
  const shadowRoot = `${host}.shadowRoot`;
  const inner = `${shadowRoot}.querySelector('#inner')`;
  const hidden = `document.querySelector('#hidden')`;

  const tId = await objectIdFor(t);
  const crossId = await objectIdFor(cross);
  const inputId = await objectIdFor(input);
  const textareaId = await objectIdFor(textarea);
  const goneId = await objectIdFor(gone);
  const hostId = await objectIdFor(host);
  const innerId = await objectIdFor(inner);
  const hiddenId = await objectIdFor(hidden);

  async function setAndLog(label, objectId, type, start, end, readExpr) {
    await dp.DOM.setTextMarker({objectId, type, start, end});
    testRunner.log(`${label}: ` +
        await session.evaluate(readMarkers, readExpr));
  }

  async function logError(label, params) {
    testRunner.log(label);
    testRunner.log(await dp.DOM.setTextMarker(params));
  }

  // Set unforced marker on 'brown' as though created by the real spellchecker.
  await setSpellcheckerMarker('#t', 10, 15, 'spelling');

  await setAndLog('After spelling marker on #t [4, 9)', tId, 'spelling', 4, 9,
                  t);
  await setAndLog('After grammar marker on #t [4, 9)', tId, 'grammar', 4, 9, t);
  await setAndLog('After spelling marker on input value [0, 3)', inputId,
                  'spelling', 0, 3, inputEditor);
  await setAndLog('After spelling marker spanning <b> on #cross [4, 9)',
                  crossId, 'spelling', 4, 9, cross);
  await setAndLog('After spelling marker on textarea value "ab\\ncd" [3, 5)',
                  textareaId, 'spelling', 3, 5, textareaEditor);
  await setAndLog('After spelling marker on element inside shadow root [0, 6)',
                  innerId, 'spelling', 0, 6, shadowRoot);

  await dp.DOM.clearTextMarkers();
  testRunner.log('After clearTextMarkers:');
  for (const [label, expr] of [['#t', t], ['input', inputEditor],
                               ['#cross', cross], ['textarea', textareaEditor],
                               ['shadow root', shadowRoot]]) {
    testRunner.log(`  ${label}: ` + await session.evaluate(readMarkers, expr));
  }

  await setAndLog('Set single marker before disabling', tId, 'spelling', 0, 6,
                  t);
  await dp.DOM.disable();
  testRunner.log('Markers after DOM disable: ' +
                 await session.evaluate(readMarkers, t));

  // A session that never enabled DOM can set markers and detaching clears them.
  const other = await page.createSession();
  const otherId = (await other.protocol.Runtime.evaluate({
                    expression: t
                  })).result.result.objectId;
  await other.protocol.DOM.setTextMarker(
      {objectId: otherId, type: 'spelling', start: 0, end: 6});
  testRunner.log('Set by a session without DOM.enable: ' +
                 await session.evaluate(readMarkers, t));
  await other.disconnect();
  testRunner.log('After that session detached: ' +
                 await session.evaluate(readMarkers, t));
  await dp.DOM.enable();

  // Clearing removes any marker of the type overlapping a forced range, even
  // if it was set by the spellchecker.
  await setAndLog('After spelling marker on #t [8, 12)', tId, 'spelling', 8, 12,
                  t);
  await dp.DOM.clearTextMarkers();
  testRunner.log(`After clearTextMarkers with overlap, #t: ` +
                 await session.evaluate(readMarkers, t));

  await setAndLog('After grammar marker on #gone [6, 12)', goneId, 'grammar', 6,
                  12, gone);

  await setSpellcheckerMarker('#t', 10, 15, 'spelling');
  await session.evaluate(() => {
    document.querySelector('#gone').remove();
  });
  await dp.DOM.clearTextMarkers();
  testRunner.log('After clearTextMarkers with the marked node removed, #t: ' +
                 await session.evaluate(readMarkers, t));

  await logError('End before start',
                 {objectId: tId, type: 'spelling', start: 2, end: 1});
  await logError('Negative start',
                 {objectId: tId, type: 'spelling', start: -1, end: 5});
  await logError('Start beyond end of text',
                 {objectId: tId, type: 'spelling', start: 100, end: 101});
  await logError('End beyond end of text',
                 {objectId: tId, type: 'spelling', start: 0, end: 9999});
  await logError('End one past textarea value',
                 {objectId: textareaId, type: 'spelling', start: 0, end: 6});
  await logError('Shadow content addressed through the host',
                 {objectId: hostId, type: 'spelling', start: 0, end: 3});
  await logError('display:none element',
                 {objectId: hiddenId, type: 'spelling', start: 0, end: 3});
  await logError('Detached element', {
    objectId: await objectIdFor(`document.createElement('p')`),
    type: 'spelling',
    start: 0,
    end: 1
  });
  await logError('Text node instead of element', {
    objectId: await objectIdFor(`${t}.firstChild`),
    type: 'spelling',
    start: 0,
    end: 1
  });
  await logError('Unknown marker type',
                 {objectId: tId, type: 'speling', start: 0, end: 3});

  // A never-enabled session gets no DidCommitLoad, so after a navigation its
  // entry still points at the old document. Detaching must skip it.
  const stale = await page.createSession();
  const staleId = (await stale.protocol.Runtime.evaluate({
                    expression: t
                  })).result.result.objectId;
  await stale.protocol.DOM.setTextMarker(
      {objectId: staleId, type: 'spelling', start: 0, end: 6});
  await stale.protocol.Page.navigate({url: 'about:blank'});
  await stale.disconnect();

  testRunner.completeTest();
})
