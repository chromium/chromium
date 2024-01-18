(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <head>
    <style type='text/css'>
    .composited {
       position: absolute;
       top: 25px;
       left: 25px;
       width: 50px;
       height: 50px;
       background-color: blue;
       transform: translateZ(10px);
    }
    </style>
    </head>
    <body>
      <div class='composited'>
        Sanity test for DevTools Paint Profiler.
      </div>
    </body>
  `, 'Sanity test for DevTools Paint Profiler.');

  await dp.DOM.getDocument();
  dp.LayerTree.enable();
  var layers = (await dp.LayerTree.onceLayerTreeDidChange()).params.layers;
  var matchingLayers = layers.filter(layer => !!(layer.backendNodeId && layer.transform));
  testRunner.log('matchingLayers.length: ' + matchingLayers.length);

  var layerId = matchingLayers[0].layerId;
  var snapshotId = (await dp.LayerTree.makeSnapshot({layerId})).result.snapshotId;
  var timings = (await dp.LayerTree.profileSnapshot({snapshotId, minRepeatCount: 4, minDuration: 0})).result.timings;
  testRunner.log('Profile array length: ' + timings.length);
  for (var i = 0; i < timings.length; ++i) {
    testRunner.log('Profile subarray ' + i + ' length: ' + timings[i].length);
    for (var j = 0; j < timings[i].length; ++j)
      testRunner.log('Profile timing [' + i + '][' + j + '] is a number: ' + (timings[i][j] >= 0));
  }

  var image = (await dp.LayerTree.replaySnapshot({snapshotId, fromStep: 2, toStep: timings[0].length - 2})).result.dataURL;
  testRunner.log('LayerTree.replaySnapshot returned valid image: ' + /^data:image\/png;base64,/.test(image));
  testRunner.log('DONE!');
  testRunner.completeTest();
})
