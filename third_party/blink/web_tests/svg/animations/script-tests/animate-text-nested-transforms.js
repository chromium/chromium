description("test to determine whether auto-rotate animateMotion path animations pre-muliply or post-multiply animation transform matrix");
createSVGTestCase();

rootSVGElement.setAttribute("width", 800)

var text = createSVGElement("text")
text.setAttribute("transform", "translate(300, 30)")
text.textContent = "This is some text."
text.setAttribute("onclick", "executeTest()")

var animateMotion = createSVGElement("animateMotion")
animateMotion.setAttribute("id", "animation")
animateMotion.setAttribute("dur", "40s")
animateMotion.setAttribute("repeatCount", "1")
animateMotion.setAttribute("rotate", "auto")
animateMotion.setAttribute("path", "M 100,250 C 100,50 400,50 400,250")
animateMotion.setAttribute("begin", "click")
text.appendChild(animateMotion)
rootSVGElement.appendChild(text)

function startSample() {
    shouldBeCloseEnough("rootSVGElement.getBBox().x", "115", 1);
    shouldBeCloseEnough("rootSVGElement.getBBox().y", "-154", 1);
}

function endSample() {
    shouldBeCloseEnough("rootSVGElement.getBBox().x", "367", 1);
    shouldBeCloseEnough("rootSVGElement.getBBox().y", "550", 1);
}

function executeTest() {
    const expectedValues = [
        ["animation", 0.001, startSample],
        ["animation", 39.999, endSample]
    ];

    runAnimationTest(expectedValues);
}

window.clickX = 310;
window.clickY = 28;
var successfullyParsed = true;
