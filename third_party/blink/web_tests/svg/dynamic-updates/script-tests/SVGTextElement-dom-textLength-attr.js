// [Name] SVGTextElement-dom-textLength-attr.js
// [Expected rendering result] Text streteched using spaces with a length of 200 - and a series of PASS messages

description("Tests dynamic updates of the 'textLength' attribute of the SVGTextElement object")
createSVGTestCase();

var textElement = createSVGElement("text");
textElement.setAttribute("x", "0");
textElement.setAttribute("y", "215");
textElement.textContent = "Stretched text";
rootSVGElement.appendChild(textElement);

shouldBeNull("textElement.getAttribute('textLength')");
shouldBeTrue("lastLength = textElement.getComputedTextLength(); lastLength > 0 && lastLength < 200");
shouldBeTrue("lastLength == textElement.textLength.baseVal.value");

function repaintTest() {
    textElement.setAttribute("textLength", "200");
    shouldBeEqualToString("textElement.getAttribute('textLength')", "200");
    shouldBe("textElement.textLength.baseVal.value", "200");
    shouldBe("textElement.getComputedTextLength()", "lastLength", false, 0.1);
}

var successfullyParsed = true;
