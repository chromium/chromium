description("Test SVGLength animation from px to exs.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "0");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("font-family", "Ahem");
rect.setAttribute("font-size", "10px");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "width");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "100px");
animate.setAttribute("to", "25ex");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBeCloseEnough("rect.width.animVal.value", "100");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "150");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "200");
    shouldBe("rect.width.baseVal.value", "100");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 2.0,   sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
