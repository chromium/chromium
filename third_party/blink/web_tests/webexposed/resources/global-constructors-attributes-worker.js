if (this.importScripts)
    importScripts('../../resources/js-test.js');

description("Test to ensure that global constructors in workers environment have the right attributes");

function descriptorShouldBe(object, property, expected) {
    var test = "Object.getOwnPropertyDescriptor(" + object + ", " + property + ")";
    if ("writable" in expected) {
        shouldBe(test + ".value", "" + expected.value);
        shouldBeFalse(test + ".hasOwnProperty('get')");
        shouldBeFalse(test + ".hasOwnProperty('set')");
    } else {
        shouldBe(test + ".get", "" + expected.get);
        shouldBe(test + ".set", "" + expected.set);
        shouldBeFalse(test + ".hasOwnProperty('value')");
        shouldBeFalse(test + ".hasOwnProperty('writable')");
    }
    shouldBe(test + ".enumerable", "" + expected.enumerable);
    shouldBe(test + ".configurable", "" + expected.configurable);
}

var global = this;

descriptorShouldBe("global", "'DataView'", {writable: true, enumerable: false, configurable: true, value:"DataView"});
descriptorShouldBe("global", "'EventSource'", {writable: true, enumerable: false, configurable: true, value:"EventSource"});
descriptorShouldBe("global", "'FileReaderSync'", {writable: true, enumerable: false, configurable: true, value:"FileReaderSync"});
descriptorShouldBe("global", "'Float64Array'", {writable: true, enumerable: false, configurable: true, value:"Float64Array"});
descriptorShouldBe("global", "'MessageChannel'", {writable: true, enumerable: false, configurable: true, value:"MessageChannel"});
descriptorShouldBe("global", "'WorkerLocation'", {writable: true, enumerable: false, configurable: true, value:"WorkerLocation"});
descriptorShouldBe("global", "'XMLHttpRequest'", {writable: true, enumerable: false, configurable: true, value:"XMLHttpRequest"});

finishJSTest();
