// META: title=Encoding API: Latin-1 decoders
// META: script=/encoding/resources/encodings.js

// Blink uses separate decoder object intances for these encoding aliases,
// so test that they are behaving identically.

test(function() {

    var labels;
    encodings_table.forEach(function(section) {
        section.encodings.forEach(function(encoding) {
            if (encoding.name === 'windows-1252')
                labels = encoding.labels;
        });
    });
    labels = labels.filter(function(label) { return label !== 'windows-1252'; });

    var array = new Uint8Array(256);
    for (var cp = 0; cp <= 255; ++cp) {
        array[cp] = cp;
    }

    var windows1252 = new TextDecoder('windows-1252');

    labels.forEach(function(label) {
        var decoder = new TextDecoder(label);
        assert_equals(decoder.decode(array), windows1252.decode(array));
    });

}, 'Latin-1 decoders (windows-1252, iso-8859-1, us-ascii, etc) decode identically.');
