description("Tests find for strings with Russian letters й and ё in them.");

function canFind(target, specimen)
{
    getSelection().empty();
    var textNode = document.createTextNode(specimen);
    document.body.appendChild(textNode);
    document.execCommand("FindString", false, target);
    var result = getSelection().rangeCount != 0;
    getSelection().empty();
    document.body.removeChild(textNode);
    return result;
}

var letterCyrillicI = String.fromCharCode(0x0418);
var letterCyrillicSmallI = String.fromCharCode(0x0438);
var letterCyrillicShortI = String.fromCharCode(0x0419);
var letterCyrillicSmallShortI = String.fromCharCode(0x0439);
var letterCyrillicE = String.fromCharCode(0x0415);
var letterCyrillicSmallE = String.fromCharCode(0x0435);
var letterCyrillicYO = String.fromCharCode(0x0401);
var letterCyrillicSmallYO = String.fromCharCode(0x0451);
var combiningDiaeresis = String.fromCharCode(0x0308);
var combiningBreve = String.fromCharCode(0x0306);

var decomposedCyrillicShortI = letterCyrillicI + combiningBreve;
var decomposedCyrillicSmallShortI = letterCyrillicSmallI + combiningBreve;
var decomposedCyrillicYO = letterCyrillicE + combiningDiaeresis;
var decomposedCyrillicSmallYO = letterCyrillicSmallE + combiningDiaeresis;

debug('Exact matches first as a baseline');
debug('');

shouldBe('canFind(decomposedCyrillicShortI, decomposedCyrillicShortI)', 'true');
shouldBe('canFind(decomposedCyrillicSmallShortI, decomposedCyrillicSmallShortI)', 'true');
shouldBe('canFind(letterCyrillicShortI, letterCyrillicShortI)', 'true');
shouldBe('canFind(letterCyrillicSmallShortI, letterCyrillicSmallShortI)', 'true');
shouldBe('canFind("й", "йод")', 'true');
shouldBe('canFind("ё", "мумиё")', 'true');


debug('');
debug('Composed and decomposed forms: Must be treated as equal');
debug('');

shouldBe('canFind(decomposedCyrillicYO, decomposedCyrillicYO)', 'true');
shouldBe('canFind(decomposedCyrillicSmallYO, decomposedCyrillicSmallYO)', 'true');
shouldBe('canFind(letterCyrillicShortI, decomposedCyrillicShortI)', 'true');
shouldBe('canFind(letterCyrillicSmallShortI, decomposedCyrillicSmallShortI)', 'true');
shouldBe('canFind(letterCyrillicYO, decomposedCyrillicYO)', 'true');
shouldBe('canFind(letterCyrillicSmallYO, decomposedCyrillicSmallYO)', 'true');
shouldBe('canFind(decomposedCyrillicShortI, letterCyrillicShortI)', 'true');
shouldBe('canFind(decomposedCyrillicSmallShortI, letterCyrillicSmallShortI)', 'true');
shouldBe('canFind(decomposedCyrillicYO, letterCyrillicYO)', 'true');
shouldBe('canFind(decomposedCyrillicSmallYO, letterCyrillicSmallYO)', 'true');

debug('');
debug('Small and capital letters: Must be treated as equal');
debug('');

shouldBe('canFind(letterCyrillicSmallI, letterCyrillicI)', 'true');
shouldBe('canFind(letterCyrillicSmallYO, letterCyrillicYO)', 'true');
shouldBe('canFind(letterCyrillicI, letterCyrillicSmallI)', 'true');
shouldBe('canFind(letterCyrillicYO, letterCyrillicSmallYO)', 'true');
shouldBe('canFind(decomposedCyrillicSmallShortI, letterCyrillicI)', 'true');
shouldBe('canFind(decomposedCyrillicSmallYO, letterCyrillicYO)', 'true');
shouldBe('canFind(decomposedCyrillicShortI, letterCyrillicSmallI)', 'true');
shouldBe('canFind(decomposedCyrillicYO, letterCyrillicSmallYO)', 'true');
shouldBe('canFind(letterCyrillicSmallI + letterCyrillicSmallYO, letterCyrillicSmallI + letterCyrillicYO)', 'true');
shouldBe('canFind("й", "Йод")', 'true');
shouldBe('canFind("ё", "МУМИЁ")', 'true');
shouldBe('canFind("Й", "йод")', 'true');
shouldBe('canFind("Ё", "мумиё")', 'true');

debug('');
debug('Е and Ё: Must be treated as equal');
debug('');

shouldBe('canFind("мумие", "мумиё")', 'true');
shouldBe('canFind("МУМИЕ", "МУМИЁ")', 'true');
shouldBe('canFind("мумиё", "мумие")', 'true');
shouldBe('canFind("МУМИЁ", "МУМИЕ")', 'true');

debug('');
debug('Й and и: Must *not* be treated as equal');
debug('');

shouldBe('canFind("зайка", "заика")', 'false');
shouldBe('canFind("заика", "зайка")', 'false');

debug('');
