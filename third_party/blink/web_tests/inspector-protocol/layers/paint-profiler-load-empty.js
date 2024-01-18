(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that Paint Profiler refuses to load a picture with empty dimensions');

  var response = await dp.LayerTree.loadSnapshot({
    tiles: [{
        x: 351378,
        y: -15,
        picture: 'c2tpYXBpY3QqAAAAiYICRsMkWkF3URZGz3Z9QgcAAAABZGFlcgAAAAB0Y2FmBAAAAAAAAABjZnB0AAAAAHlhcmEAAAAAIGZvZQ=='
    }]
  });

  testRunner.log('Loading empty snapshot: ' + response.error);
  testRunner.completeTest();
})
