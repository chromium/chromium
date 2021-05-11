`use strict`;

const browserLauncher = require('./browserLauncher.js');
const createBidiMapperSession = require('./bidiMapperSession.js');
const createBidiServer = require('./bidiServer.js');
const mapperReader = require('./mapperReader.js');


(async () => {
    try {
        let bidiMapperSession;
        let bidiServer;

        const cdpUrl = await browserLauncher.launch();
        const bidiMapperScript = await mapperReader.readMapper();

        bidiMapperSession = await createBidiMapperSession.create(bidiMapperScript, cdpUrl, (mapperMessage) => {
            console.log("Mapper message received:\n", mapperMessage);
            if (bidiServer)
                bidiServer.sendMessage(mapperMessage);
        });

        bidiServer = await createBidiServer.create((bidiMessage) => {
            console.log("Mapper message received:\n", bidiMessage);
            if (bidiMapperSession)
                bidiMapperSession.sendBidiCommand(bidiMessage);
        });
    } catch (e) {
        console.log("Error", e);
    }
})();



