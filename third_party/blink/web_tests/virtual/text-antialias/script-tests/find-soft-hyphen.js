description("Tests find for strings with soft hyphens in them.");

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

document.getElementById("console").style.display = "none";

var hyphen= String.fromCharCode(0x2010);
var softHyphen = String.fromCharCode(0x00AD);

shouldBe('canFind("ab", "a" + softHyphen + "b")', 'true');
shouldBe('canFind("ab", "a" + softHyphen + softHyphen + "b")', 'true');
shouldBe('canFind("a\u0300b", "a" + softHyphen + "b")', 'true');
shouldBe('canFind("ab", "a" + softHyphen + "\u0300b")', 'true');
shouldBe('canFind("ab", "a\u0300" + softHyphen + "b")', 'true');
shouldBe('canFind("a" + softHyphen + "b", "a" + softHyphen + "b")', 'true');
shouldBe('canFind("a" + softHyphen + "b", "ab")', 'true');

// Soft hyphen doesn't match hyphen and hyphen-minus.
shouldBe('canFind("a" + hyphen + "b", "a" + softHyphen + "b")', 'false');
shouldBe('canFind("a-b", "a" + softHyphen + "b")', 'false');

document.getElementById("console").style.removeProperty("display");
