description("Test animation of use element width/height");
createSVGTestCase();

// Setup test document
var symbol = createSVGElement("symbol");
symbol.setAttribute("id", "symbol");

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "10");
rect.setAttribute("y", "10");
rect.setAttribute("width", "100%");
rect.setAttribute("height", "100%");
rect.setAttribute("fill", "green");
symbol.appendChild(rect);
rootSVGElement.appendChild(symbol);

var use = createSVGElement("use");
use.setAttribute("id", "use");
use.setAttributeNS('http://www.w3.org/1999/xlink', 'xlink:href', '#symbol');
use.setAttribute("x", "0");
use.setAttribute("y", "0");
use.setAttribute("width", "100");
use.setAttribute("height", "100");
use.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(use);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animate");
animate.setAttribute("attributeName", "width");
animate.setAttribute("attributeType", "XML");
animate.setAttribute("begin", "1s");
animate.setAttribute("dur", "10s");
animate.setAttribute("from", "100");
animate.setAttribute("to", "200");
use.appendChild(animate);

var animate2 = createSVGElement("animate");
animate2.setAttribute("id", "animate2");
animate2.setAttribute("attributeName", "height");
animate2.setAttribute("attributeType", "XML");
animate2.setAttribute("begin", "1s");
animate2.setAttribute("dur", "10s");
animate2.setAttribute("from", "100");
animate2.setAttribute("to", "200");
use.appendChild(animate2);

var shadowRoot = internals.shadowRoot(use);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("use.width.animVal.value", "100");
    shouldBe("use.width.baseVal.value", "100");
    shouldBe("use.height.animVal.value", "100");
    shouldBe("use.height.baseVal.value", "100");
    shouldBe("use.getAttribute('width')", "'100'");
    shouldBe("use.getAttribute('height')", "'100'");
    shouldBe("shadowRoot.firstChild.width.animVal.value", "100");
    shouldBe("shadowRoot.firstChild.height.animVal.value", "100");
}

function sample2() {
    shouldBe("use.width.animVal.value", "105");
    shouldBe("use.width.baseVal.value", "100");
    shouldBe("use.height.animVal.value", "105");
    shouldBe("use.height.baseVal.value", "100");
    shouldBe("use.getAttribute('width')", "'100'");
    shouldBe("use.getAttribute('height')", "'100'");
    shouldBe("shadowRoot.firstChild.width.animVal.value", "105");
    shouldBe("shadowRoot.firstChild.height.animVal.value", "105");
}

function sample3() {
    shouldBe("use.width.animVal.value", "115");
    shouldBe("use.width.baseVal.value", "100");
    shouldBe("use.height.animVal.value", "115");
    shouldBe("use.height.baseVal.value", "100");
    shouldBe("use.getAttribute('width')", "'100'");
    shouldBe("use.getAttribute('height')", "'100'");
    shouldBe("shadowRoot.firstChild.width.animVal.value", "115");
    shouldBe("shadowRoot.firstChild.height.animVal.value", "115");
}

function sample4() {
    shouldBe("use.width.animVal.value", "125");
    shouldBe("use.width.baseVal.value", "100");
    shouldBe("use.height.animVal.value", "125");
    shouldBe("use.height.baseVal.value", "100");
    shouldBe("use.getAttribute('width')", "'100'");
    shouldBe("use.getAttribute('height')", "'100'");
    shouldBe("shadowRoot.firstChild.width.animVal.value", "125");
    shouldBe("shadowRoot.firstChild.height.animVal.value", "125");
}

function sample5() {
    shouldBe("use.width.animVal.value", "135");
    shouldBe("use.width.baseVal.value", "100");
    shouldBe("use.height.animVal.value", "135");
    shouldBe("use.height.baseVal.value", "100");
    shouldBe("use.getAttribute('width')", "'100'");
    shouldBe("use.getAttribute('height')", "'100'");
    shouldBe("shadowRoot.firstChild.width.animVal.value", "135");
    shouldBe("shadowRoot.firstChild.height.animVal.value", "135");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animate2", 0.0,   sample1],
        ["animate2", 0.5,   sample2],
        ["animate2", 1.5,   sample3],
        ["animate2", 2.5,   sample4],
        ["animate2", 3.5,   sample5]
    ];

    runAnimationTest(expectedValues);
}

window.clickX = 50;
window.clickY = 50;
var successfullyParsed = true;
