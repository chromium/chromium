// META: title=Ensure encoding labels match the Encoding standard
// META: script=/encoding/resources/encodings.js

// The list of encoding labels is not web-exposed. This test makes
// use of the "internals" API for testing and therefore can not be
// upstreamed to web-platform-tests.

let supported_labels = internals.supportedTextEncodingLabels().sort();

// Specified labels from: https://encoding.spec.whatwg.org/

let specified_labels = [];
encodings_table.forEach(section => {
  section.encodings.forEach(encoding => {
    encoding.labels.forEach(label => specified_labels.push(label));
  });
});

supported_labels = new Set(supported_labels.map(s => s.toLowerCase()));
specified_labels = new Set(specified_labels.map(s => s.toLowerCase()));
const union = new Set([...supported_labels, ...specified_labels]);

for (const label of union) {
  test(t => {
    assert_true(supported_labels.has(label),
                `${label} should be supported since it is specified`);
    assert_true(specified_labels.has(label),
                `${label} should only be supported if it is specified`);
  }, `Supported label: ${label}`);
}
