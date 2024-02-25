(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests target filter.');

  function normalizeTargets(targets) {
    return targets.sort(
        (a, b) => `${a.type}\n${a.url}`.localeCompare(`${b.type}\n${b.url}`));
  }
  const allTargetsCreated = await session.evaluateAsync(`
    Promise.all([
      new Promise(resolve => {
        window.worker = new SharedWorker('/inspector-protocol/fetch/resources/shared-worker.js');
        worker.port.onmessage = event => {
          if (event.data === 'ready')
            resolve();
        };
      }),
      navigator.serviceWorker.register('/inspector-protocol/fetch/resources/service-worker.js'),
      new Promise(resolve => {
        const iframe = document.createElement('iframe');
        iframe.addEventListener('load', resolve, false);
        iframe.src = 'http://devtools.oopif-b.test:8000/inspector-protocol/page.html';
        document.body.appendChild(iframe);
      }),
    ])
  `);
  if (typeof allTargetsCreated !== 'object' || allTargetsCreated.length !== 3) {
    testRunner.log(allTargetsCreated, 'FAIL: ');
    testRunner.completeTest();
    return;
  }
  const frames = await dp.Target.getTargets({filter: [{type: 'iframe'}]});
  testRunner.log(frames.result.targetInfos, 'type: frame ');
  const workers = await dp.Target.getTargets({filter: [{type: 'service_worker'}]});
  testRunner.log(workers.result.targetInfos, 'type: service_worker ');
  const frames_and_service_workers = await dp.Target.getTargets(
      {filter: [{type: 'iframe'}, {type: 'service_worker'}]});
  testRunner.log(normalizeTargets(frames_and_service_workers.result.targetInfos),
      'type: frames and service workers ');
  const all_but_frames = await dp.Target.getTargets(
      {filter: [{type: 'iframe', exclude: true}, {}]});
  testRunner.log(normalizeTargets(all_but_frames.result.targetInfos), 'type: all but frames ');

  const default_targets = await dp.Target.getTargets();
  testRunner.log(normalizeTargets(default_targets.result.targetInfos), 'default filter ');

  const all_targets = await dp.Target.getTargets({filter: [{}]});
  testRunner.log(normalizeTargets(all_targets.result.targetInfos),
      'wildcard pattern ');

  const targetsCreated = [];
  dp.Target.onTargetCreated(event => { targetsCreated.push(event.params); });
  await dp.Target.setDiscoverTargets({discover: true, filter: [{type: 'shared_worker'}]});
  testRunner.log(targetsCreated, `Discovered targets with type = shared_worker `);

  const targetsAttached = [];
  dp.Target.onAttachedToTarget(event => { targetsAttached.push(event.params); });
  await dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false, filter: [{type: 'iframe'}]});
  testRunner.log(targetsAttached, `Auto-attached targets with type = iframe `);

  testRunner.completeTest();
});
