// META: title=Encoding API: ASCII supersets
// META: script=/encoding/resources/encodings.js

// Encodings that have escape codes in 0x00-0x7F
var escape_codes = {
    'ISO-2022-JP': [ 0x0E, 0x0F, 0x1B ]
};

encodings_table.forEach(function(section) {
    section.encodings.filter(function(encoding) {
        return encoding.name !== 'replacement';
    }).forEach(function(encoding) {
        if (['UTF-16LE', 'UTF-16BE'].indexOf(encoding.name) !== -1)
            return;

        test(function() {
            var string = '';
            var bytes = [];
            for (var i = 0; i < 128; ++i) {
                if (encoding.name in escape_codes && escape_codes[encoding.name].indexOf(i) !== -1)
                    continue;
                string += String.fromCharCode(i);
                bytes.push(i);
            }

            var decoder = new TextDecoder(encoding.name);
            var decoded = decoder.decode(new Uint8Array(bytes));
            assert_equals(decoded, string);
        }, 'ASCII superset encoding: ' + encoding.name);
    });
});
