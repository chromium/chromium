# Tests that link to libopus
opus_tests = [
  ['test_opus_api'],
  ['test_opus_decode', [], 60],
  ['test_opus_encode', 'opus_encode_regressions.c', 120],
  ['test_opus_padding'],
  ['test_opus_projection'],
]

foreach t : opus_tests
  test_name = t.get(0)
  extra_srcs = t.get(1, [])

  test_kwargs = {}
  if t.length() > 2
    test_kwargs += {'timeout': t[2]}
  endif

  exe_kwargs = {}
  # This test uses private symbols
  if test_name == 'test_opus_projection'
    exe_kwargs = {
      'link_with': [celt_lib, silk_lib],
      'objects': opus_lib.extract_all_objects(),
    }
  endif

  exe = executable(test_name, '@0@.c'.format(test_name), extra_srcs,
    include_directories: opus_includes,
    dependencies: [libm, opus_dep],
    install: false,
    kwargs: exe_kwargs)
  test(test_name, exe, kwargs: test_kwargs)
endforeach
