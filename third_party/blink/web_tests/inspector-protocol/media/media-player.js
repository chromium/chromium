(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Test video element logging protocol.`);
  function testProperties() {
    return new Promise(function(resolve, reject) {
      dp.Media.onPlayerPropertiesChanged((propEvent) => {
        if (propEvent.params.properties.length == 0) return;
        resolve([propEvent.params.playerId, propEvent.params.properties]);
      });
    });
  }

  function testEvents() {
    return new Promise(function(resolve, reject) {
      dp.Media.onPlayerEventsAdded((propEvent) => {
        if (propEvent.params.events.length == 0) return;
        resolve([propEvent.params.playerId, propEvent.params.events]);
      });
    });
  }

  function testMessages() {
    return new Promise(function(resolve, reject) {
      dp.Media.onPlayerMessagesLogged((propEvent) => {
        // Allow zero messages, since there often are none.
        resolve([propEvent.params.playerId, propEvent.params.messages]);
      });
    });
  }

  function testErrors() {
    return new Promise(function(resolve, reject) {
      dp.Media.onPlayerErrorsRaised((propEvent) => {
        // Allow zero errors, since there often are none.
        resolve([propEvent.params.playerId, propEvent.params.errors]);
      });
    });
  }

  const promises = [ testEvents(), testMessages(), testProperties(), testErrors() ];

  dp.Media.onPlayerCreated(async (event) => {
    testRunner.log(`Received creation event for 1 player`);
    const {playerId, domNodeId} = event.params.player;

    // Verify that the domNodeId received from onPlayerCreated matches the
    // actual node id.
    const documentNodeId = (await dp.DOM.getDocument({})).result.root.nodeId;
    const videoElementId = (await dp.DOM.querySelector({
                             nodeId: documentNodeId,
                             selector: 'video',
                           })).result.nodeId;
    const videoElementBackendNodeId = (await dp.DOM.describeNode({
                                        nodeId: videoElementId,
                                      })).result.node.backendNodeId;
    testRunner.log(
        `The domNodeId received from onPlayerCreated matches the actual node id: ${
            videoElementBackendNodeId === domNodeId}`);

    Promise.all(promises).then(([events, messages, properties, errors]) => {
      if (events[0] === playerId && messages[0] === playerId && properties[0] === playerId, errors[0] == playerId)
        testRunner.log('All IDs match');

      testRunner.log(`events length: ${events[1].length}`);
      testRunner.log(`messages length: ${messages[1].length}`);
      testRunner.log(`properties length: ${properties[1].length}`);
      testRunner.log(`errors length: ${errors[1].length}`);

      testRunner.completeTest();
    });
  });

  testRunner.log('Creating player');
  await session.evaluate(`
    const video = document.createElement('video');
    video.src = './inspector-protocol/media/resources/baseball.webm';
    document.body.appendChild(video);
  `);

  testRunner.log('enabling media logging');
  await dp.Media.enable();
})
