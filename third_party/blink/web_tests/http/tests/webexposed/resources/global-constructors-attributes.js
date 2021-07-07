description("Test to ensure that global constructors have the right attributes");

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

descriptorShouldBe("global", "'CSSRuleList'", {writable: true, enumerable: false, configurable: true, value:"CSSRuleList"});
descriptorShouldBe("global", "'Document'", {writable: true, enumerable: false, configurable: true, value:"Document"});
descriptorShouldBe("global", "'Element'", {writable: true, enumerable: false, configurable: true, value:"Element"});
descriptorShouldBe("global", "'HTMLDivElement'", {writable: true, enumerable: false, configurable: true, value:"HTMLDivElement"});
descriptorShouldBe("global", "'ProgressEvent'", {writable: true, enumerable: false, configurable: true, value:"ProgressEvent"});
descriptorShouldBe("global", "'Selection'", {writable: true, enumerable: false, configurable: true, value:"Selection"});
descriptorShouldBe("global", "'XMLHttpRequest'", {writable: true, enumerable: false, configurable: true, value:"XMLHttpRequest"});
