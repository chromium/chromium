if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's event interfaces.");

function test()
{
    shouldBeTrue("'IDBVersionChangeEvent' in self");

    if ('document' in self) {
        shouldBeTrue("'oldVersion' in new IDBVersionChangeEvent('versionchange')");
        shouldBeTrue("'newVersion' in new IDBVersionChangeEvent('versionchange')");
        shouldBeTrue("'dataLoss' in new IDBVersionChangeEvent('versionchange')");
    }

    finishJSTest();
}

test();
