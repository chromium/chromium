`use strict`;

import browserLauncher from './browserLauncher.js';
import mapperReader from './mapperReader.js';
import { MapperServer } from './mapperServer.js';
import { BidiServerRunner } from './bidiServerRunner.js';

(async () => {
    try {
        BidiServerRunner.run(async bidiServer => {
            // Launch browser.
            const { cdpUrl, closeBrowser } = await browserLauncher();
            // Get BiDi Mapper script.
            const bidiMapperScript = await mapperReader();

            // Run BiDi Mapper script on the browser.
            const mapperServer = await MapperServer.create(cdpUrl, bidiMapperScript);

            // Forward messages from BiDi Mapper to the client.
            mapperServer.setOnMessage(async message => {
                await bidiServer.sendMessage(message)
            });

            // Forward messages from the client to BiDi Mapper.
            bidiServer.setOnMessage(async message => {
                await mapperServer.sendMessage(message);
            });

            // Save handler `closeBrowser` to use after the client disconnected.
            return { closeBrowser };
        }, ({ closeBrowser }) => {
            // Client disconnected. Close browser.
            closeBrowser();
        });
    } catch (e) {
        console.log("Error", e);
    }
})();
