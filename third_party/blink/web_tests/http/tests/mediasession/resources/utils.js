function assert_image_equals(expected, observed) {
  assert_equals(expected.src, observed.src);
  assert_equals(expected.type, observed.type);
  assert_equals(expected.sizes, observed.sizes);
}

export function assert_metadata_equals(expected, observed) {
  assert_equals(expected.title, observed.title, 'metadata.title');
  assert_equals(expected.artist, observed.artist, 'metadata.artist');
  assert_equals(expected.album, observed.album, 'metadata.album');
  assert_equals(
      expected.artwork.length, observed.artwork.length,
      'metadata.artwork.length');
  for (var i = 0; i < expected.artwork.length; i++)
    assert_image_equals(expected.artwork[i], observed.artwork[i]);
}
