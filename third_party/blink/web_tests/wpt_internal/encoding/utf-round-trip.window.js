// META: title=Encoding API: UTF encoding round trips
// META: script=/encoding/resources/encodings.js

var BATCH_SIZE = 0x1000; // Convert in batches spanning this many code points.
var SKIP_SIZE = 0x77; // For efficiency, don't test every code point.

function fromCodePoint(cp) {
    if (0xD800 <= cp && cp <= 0xDFFF) throw new Error('Invalid code point');

    if (cp <= 0xFFFF)
        return String.fromCharCode(cp);

    // outside BMP - encode as surrogate pair
    return String.fromCharCode(0xD800 + ((cp >> 10) & 0x3FF), 0xDC00 + (cp & 0x3FF));
}

function makeBatch(cp) {
    var string = '';
    for (var i = cp; i < cp + BATCH_SIZE && cp < 0x10FFFF; i += SKIP_SIZE) {
        if (0xD800 <= i && i <= 0xDFFF) {
            // surrogate half
            continue;
        }
        string += fromCodePoint(i);
    }
    return string;
 }

['utf-8', 'utf-16le', 'utf-16be'].forEach(function(encoding) {
    if (encoding === 'utf-8') {
        test(function() {
            for (var i = 0; i < 0x10FFFF; i += BATCH_SIZE) {
                var string = makeBatch(i);
                var encoded = new TextEncoder().encode(string);
                var decoded = new TextDecoder(encoding).decode(encoded);
                assert_equals(string, decoded);
            }
        }, encoding + ' - encode/decode round trip');
    }
});

['utf-16le', 'utf-16be'].forEach(function(encoding) {
    test(function() {
        for (var i = 0; i < 0x10FFFF; i += BATCH_SIZE) {
            var string = makeBatch(i);

            if (encoding === 'utf-16le')
                var encoded = encode_utf16(string, true);
            else
                var encoded = encode_utf16(string, false);

            var decoded = new TextDecoder(encoding).decode(encoded);
            assert_equals(string, decoded);
        }
    }, encoding + ' - encode/decode round trip');
});

function encode_utf16(s, littleEndian) {
    var a = new Uint8Array(s.length * 2), view = new DataView(a.buffer);
    s.split('').forEach(function(c, i) {
        view.setUint16(i * 2, c.charCodeAt(0), littleEndian);
    });
    return a;
}

// Inspired by:
// http://ecmanaut.blogspot.com/2006/07/encoding-decoding-utf8-in-javascript.html
function encode_utf8(string) {
    var utf8 = unescape(encodeURIComponent(string));
    var octets = [];
    for (var i = 0; i < utf8.length; i += 1)
        octets.push(utf8.charCodeAt(i));
    return octets;
}

function decode_utf8(octets) {
    var utf8 = String.fromCharCode.apply(null, octets);
    return decodeURIComponent(escape(utf8));
}

test(function() {
    for (var i = 0; i < 0x10FFFF; i += BATCH_SIZE) {
        var string = makeBatch(i);
        var expected = encode_utf8(string);
        var actual = new TextEncoder().encode(string);
        assert_array_equals(actual, expected);
    }
}, 'UTF-8 encoding (compare against unescape/encodeURIComponent)');

test(function() {
    for (var i = 0; i < 0x10FFFF; i += BATCH_SIZE) {
        var string = makeBatch(i);
        var encoded = encode_utf8(string);
        var expected = decode_utf8(encoded);
        var actual = new TextDecoder().decode(new Uint8Array(encoded));
        assert_equals(actual, expected);
    }
}, 'UTF-8 decoding (compare against decodeURIComponent/escape)');
