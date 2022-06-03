test = require 'tape'

ROOT = "../src"
if process.env.ROOT
  ROOT = process.env.ROOT

matching = require (ROOT + '/matching')
adjacency_graphs = require (ROOT + '/adjacency_graphs')

# takes a pattern and list of prefixes/suffixes
# returns a bunch of variants of that pattern embedded
# with each possible prefix/suffix combination, including no prefix/suffix
# returns a list of triplets [variant, i, j] where [i,j] is the start/end of the pattern, inclusive
genpws = (pattern, prefixes, suffixes) ->
  prefixes = prefixes.slice()
  suffixes = suffixes.slice()
  for lst in [prefixes, suffixes]
    lst.unshift '' if '' not in lst
  result = []
  for prefix in prefixes
    for suffix in suffixes
      [i, j] = [prefix.length, prefix.length + pattern.length - 1]
      result.push [prefix + pattern + suffix, i, j]
  result

check_matches = (prefix, t, matches, pattern_names, patterns, ijs, props) ->
  if typeof pattern_names is "string"
    # shortcut: if checking for a list of the same type of patterns,
    # allow passing a string 'pat' instead of array ['pat', 'pat', ...]
    pattern_names = (pattern_names for i in [0...patterns.length])

  is_equal_len_args = pattern_names.length == patterns.length == ijs.length
  for prop, lst of props
    # props is structured as: keys that points to list of values
    is_equal_len_args = is_equal_len_args and (lst.length == patterns.length)
  throw 'unequal argument lists to check_matches' unless is_equal_len_args

  msg = "#{prefix}: matches.length == #{patterns.length}"
  t.equal matches.length, patterns.length, msg
  for k in [0...patterns.length]
    match = matches[k]
    pattern_name = pattern_names[k]
    pattern = patterns[k]
    [i, j] = ijs[k]
    msg = "#{prefix}: matches[#{k}].pattern == '#{pattern_name}'"
    t.equal match.pattern, pattern_name, msg
    msg = "#{prefix}: matches[#{k}] should have [i, j] of [#{i}, #{j}]"
    t.deepEqual [match.i, match.j], [i, j], msg
    msg = "#{prefix}: matches[#{k}].token == '#{pattern}'"
    t.equal match.token, pattern, msg
    for prop_name, prop_list of props
      prop_msg = prop_list[k]
      prop_msg = "'#{prop_msg}'" if typeof(prop_msg) == 'string'
      msg = "#{prefix}: matches[#{k}].#{prop_name} == #{prop_msg}"
      t.deepEqual match[prop_name], prop_list[k], msg


test 'matching utils', (t) ->
  return t.end() if matching.no_util
  t.ok matching.empty([]), ".empty returns true for an empty array"
  t.ok matching.empty({}), ".empty returns true for an empty object"
  for obj in [
    [1]
    [1, 2]
    [[]]
    {a: 1}
    {0: {}}
    ]
    t.notOk matching.empty(obj), ".empty returns false for non-empty objects and arrays"

  lst = []
  matching.extend lst, []
  t.deepEqual lst, [], "extending an empty list with an empty list leaves it empty"
  matching.extend lst, [1]
  t.deepEqual lst, [1], "extending an empty list with another makes it equal to the other"
  matching.extend lst, [2, 3]
  t.deepEqual lst, [1, 2, 3], "extending a list with another adds each of the other's elements"
  [lst1, lst2] = [[1], [2]]
  matching.extend lst1, lst2
  t.deepEqual lst2, [2], "extending a list by another doesn't affect the other"

  chr_map = {a: 'A', b: 'B'}
  for [string, map, result] in [
    ['a',    chr_map, 'A']
    ['c',    chr_map, 'c']
    ['ab',   chr_map, 'AB']
    ['abc',  chr_map, 'ABc']
    ['aa',   chr_map, 'AA']
    ['abab', chr_map, 'ABAB']
    ['',     chr_map, '']
    ['',     {},      '']
    ['abc',  {},      'abc']
    ]
    msg = "translates '#{string}' to '#{result}' with provided charmap"
    t.equal matching.translate(string, map), result, msg

  for [[dividend, divisor], remainder] in [
    [[ 0, 1],  0]
    [[ 1, 1],  0]
    [[-1, 1],  0]
    [[ 5, 5],  0]
    [[ 3, 5],  3]
    [[-1, 5],  4]
    [[-5, 5],  0]
    [[ 6, 5],  1]
    ]
    msg = "mod(#{dividend}, #{divisor}) == #{remainder}"
    t.equal matching.mod(dividend, divisor), remainder, msg

  t.deepEqual matching.sorted([]), [], "sorting an empty list leaves it empty"
  [m1, m2, m3, m4, m5, m6] = [
    {i: 5, j: 5}
    {i: 6, j: 7}
    {i: 2, j: 5}
    {i: 0, j: 0}
    {i: 2, j: 3}
    {i: 0, j: 3}
  ]
  msg = "matches are sorted on i index primary, j secondary"
  t.deepEqual matching.sorted([m1, m2, m3, m4, m5, m6]), [m4, m6, m5, m3, m1, m2], msg
  t.end()


test 'dictionary matching', (t) ->
  dm = (pw) -> matching.dictionary_match pw, test_dicts
  test_dicts =
    d1:
      motherboard: 1
      mother: 2
      board: 3
      abcd: 4
      cdef: 5
    d2:
      'z': 1
      '8': 2
      '99': 3
      '$': 4
      'asdf1234&*': 5

  matches = dm 'motherboard'
  patterns = ['mother', 'motherboard', 'board']
  msg = "matches words that contain other words"
  check_matches msg, t, matches, 'dictionary', patterns, [[0,5], [0,10], [6,10]],
    matched_word: ['mother', 'motherboard', 'board']
    rank: [2, 1, 3]
    dictionary_name: ['d1', 'd1', 'd1']

  matches = dm 'abcdef'
  patterns = ['abcd', 'cdef']
  msg = "matches multiple words when they overlap"
  check_matches msg, t, matches, 'dictionary', patterns, [[0,3], [2,5]],
    matched_word: ['abcd', 'cdef']
    rank: [4, 5]
    dictionary_name: ['d1', 'd1']

  matches = dm 'BoaRdZ'
  patterns = ['BoaRd', 'Z']
  msg = "ignores uppercasing"
  check_matches msg, t, matches, 'dictionary', patterns, [[0,4], [5,5]],
    matched_word: ['board', 'z']
    rank: [3, 1]
    dictionary_name: ['d1', 'd2']

  prefixes = ['q', '%%']
  suffixes = ['%', 'qq']
  word = 'asdf1234&*'
  for [password, i, j] in genpws word, prefixes, suffixes
    matches = dm password
    msg = "identifies words surrounded by non-words"
    check_matches msg, t, matches, 'dictionary', [word], [[i,j]],
      matched_word: [word]
      rank: [5]
      dictionary_name: ['d2']

  for name, dict of test_dicts
    for word, rank of dict
      continue if word is 'motherboard' # skip words that contain others
      matches = dm word
      msg = "matches against all words in provided dictionaries"
      check_matches msg, t, matches, 'dictionary', [word], [[0, word.length - 1]],
        matched_word: [word]
        rank: [rank]
        dictionary_name: [name]

  # test the default dictionaries
  matches = matching.dictionary_match 'wow'
  patterns = ['wow']
  ijs = [[0,2]]
  msg = "default dictionaries"
  check_matches msg, t, matches, 'dictionary', patterns, ijs,
    matched_word: patterns
    rank: [322]
    dictionary_name: ['us_tv_and_film']

  matching.set_user_input_dictionary ['foo', 'bar']
  matches = matching.dictionary_match 'foobar'
  matches = matches.filter (match) ->
    match.dictionary_name == 'user_inputs'
  msg = "matches with provided user input dictionary"
  check_matches msg, t, matches, 'dictionary', ['foo', 'bar'], [[0, 2], [3, 5]],
    matched_word: ['foo', 'bar']
    rank: [1, 2]
  t.end()

test 'reverse dictionary matching', (t) ->
  test_dicts =
    d1:
      123: 1
      321: 2
      456: 3
      654: 4
  password = '0123456789'
  matches = matching.reverse_dictionary_match password, test_dicts
  msg = 'matches against reversed words'
  check_matches msg, t, matches, 'dictionary', ['123', '456'], [[1, 3], [4, 6]],
    matched_word: ['321', '654']
    reversed: [true, true]
    dictionary_name: ['d1', 'd1']
    rank: [2, 4]
  t.end()


test 'l33t matching', (t) ->
  test_table =
    a: ['4', '@']
    c: ['(', '{', '[', '<']
    g: ['6', '9']
    o: ['0']

  for [pw, expected] in [
    [ '', {} ]
    [ 'abcdefgo123578!#$&*)]}>', {} ]
    [ 'a',     {} ]
    [ '4',     {'a': ['4']} ]
    [ '4@',    {'a': ['4','@']} ]
    [ '4({60', {'a': ['4'], 'c': ['(','{'], 'g': ['6'], 'o': ['0']} ]
    ]
    msg = "reduces l33t table to only the substitutions that a password might be employing"
    t.deepEquals matching.relevant_l33t_subtable(pw, test_table), expected, msg

  for [table, subs] in [
    [ {},                        [{}] ]
    [ {a: ['@']},                [{'@': 'a'}] ]
    [ {a: ['@','4']},            [{'@': 'a'}, {'4': 'a'}] ]
    [ {a: ['@','4'], c: ['(']},  [{'@': 'a', '(': 'c' }, {'4': 'a', '(': 'c'}] ]
    ]
    msg = "enumerates the different sets of l33t substitutions a password might be using"
    t.deepEquals matching.enumerate_l33t_subs(table), subs, msg

  lm = (pw) -> matching.l33t_match pw, dicts, test_table
  dicts =
    words:
      aac: 1
      password: 3
      paassword: 4
      asdf0: 5
    words2:
      cgo: 1

  t.deepEquals lm(''), [], "doesn't match ''"
  t.deepEquals lm('password'), [], "doesn't match pure dictionary words"
  for [password, pattern, word, dictionary_name, rank, ij, sub] in [
    [ 'p4ssword',    'p4ssword', 'password', 'words',  3, [0,7],  {'4': 'a'} ]
    [ 'p@ssw0rd',    'p@ssw0rd', 'password', 'words',  3, [0,7],  {'@': 'a', '0': 'o'} ]
    [ 'aSdfO{G0asDfO', '{G0',    'cgo',      'words2', 1, [5, 7], {'{': 'c', '0': 'o'} ]
    ]
    msg = "matches against common l33t substitutions"
    check_matches msg, t, lm(password), 'dictionary', [pattern], [ij],
      l33t: [true]
      sub: [sub]
      matched_word: [word]
      rank: [rank]
      dictionary_name: [dictionary_name]

  matches = lm '@a(go{G0'
  msg = "matches against overlapping l33t patterns"
  check_matches msg, t, matches, 'dictionary', ['@a(', '(go', '{G0'], [[0,2], [2,4], [5,7]],
    l33t: [true, true, true]
    sub: [{'@': 'a', '(': 'c'}, {'(': 'c'}, {'{': 'c', '0': 'o'}]
    matched_word: ['aac', 'cgo', 'cgo']
    rank: [1, 1, 1]
    dictionary_name: ['words', 'words2', 'words2']

  msg = "doesn't match when multiple l33t substitutions are needed for the same letter"
  t.deepEqual lm('p4@ssword'), [], msg

  msg = "doesn't match single-character l33ted words"
  matches = matching.l33t_match '4 1 @'
  t.deepEqual matches, [], msg

  # known issue: subsets of substitutions aren't tried.
  # for long inputs, trying every subset of every possible substitution could quickly get large,
  # but there might be a performant way to fix.
  # (so in this example: {'4': a, '0': 'o'} is detected as a possible sub,
  # but the subset {'4': 'a'} isn't tried, missing the match for asdf0.)
  # TODO: consider partially fixing by trying all subsets of size 1 and maybe 2
  msg = "doesn't match with subsets of possible l33t substitutions"
  t.deepEqual lm('4sdf0'), [], msg
  t.end()


test 'spatial matching', (t) ->
  for password in ['', '/', 'qw', '*/']
    msg = "doesn't match 1- and 2-character spatial patterns"
    t.deepEqual matching.spatial_match(password), [], msg

  # for testing, make a subgraph that contains a single keyboard
  _graphs = qwerty: adjacency_graphs.qwerty
  pattern = '6tfGHJ'
  matches = matching.spatial_match "rz!#{pattern}%z", _graphs
  msg = "matches against spatial patterns surrounded by non-spatial patterns"
  check_matches msg, t, matches, 'spatial', [pattern], [[3, 3 + pattern.length - 1]],
    graph: ['qwerty']
    turns: [2]
    shifted_count: [3]

  for [pattern, keyboard, turns, shifts] in [
    [ '12345',        'qwerty',     1, 0 ]
    [ '@WSX',         'qwerty',     1, 4 ]
    [ '6tfGHJ',       'qwerty',     2, 3 ]
    [ 'hGFd',         'qwerty',     1, 2 ]
    [ '/;p09876yhn',  'qwerty',     3, 0 ]
    [ 'Xdr%',         'qwerty',     1, 2 ]
    [ '159-',         'keypad',     1, 0 ]
    [ '*84',          'keypad',     1, 0 ]
    [ '/8520',        'keypad',     1, 0 ]
    [ '369',          'keypad',     1, 0 ]
    [ '/963.',        'mac_keypad', 1, 0 ]
    [ '*-632.0214',   'mac_keypad', 9, 0 ]
    [ 'aoEP%yIxkjq:', 'dvorak',     4, 5 ]
    [ ';qoaOQ:Aoq;a', 'dvorak',    11, 4 ]
    ]
    _graphs = {}
    _graphs[keyboard] = adjacency_graphs[keyboard]
    matches = matching.spatial_match pattern, _graphs
    msg = "matches '#{pattern}' as a #{keyboard} pattern"
    check_matches msg, t, matches, 'spatial', [pattern], [[0, pattern.length - 1]],
      graph: [keyboard]
      turns: [turns]
      shifted_count: [shifts]
  t.end()

test 'sequence matching', (t) ->
  for password in ['', 'a', '1']
    msg = "doesn't match length-#{password.length} sequences"
    t.deepEqual matching.sequence_match(password), [], msg

  matches = matching.sequence_match 'abcbabc'
  msg = "matches overlapping patterns"
  check_matches msg, t, matches, 'sequence', ['abc', 'cba', 'abc'], [[0, 2], [2, 4], [4, 6]],
    ascending: [true, false, true]

  prefixes = ['!', '22']
  suffixes = ['!', '22']
  pattern = 'jihg'
  for [password, i, j] in genpws pattern, prefixes, suffixes
    matches = matching.sequence_match password
    msg = "matches embedded sequence patterns #{password}"
    check_matches msg, t, matches, 'sequence', [pattern], [[i, j]],
      sequence_name: ['lower']
      ascending: [false]

  for [pattern, name, is_ascending] in [
    ['ABC',   'upper',  true]
    ['CBA',   'upper',  false]
    ['PQR',   'upper',  true]
    ['RQP',   'upper',  false]
    ['XYZ',   'upper',  true]
    ['ZYX',   'upper',  false]
    ['abcd',  'lower',  true]
    ['dcba',  'lower',  false]
    ['jihg',  'lower',  false]
    ['wxyz',  'lower',  true]
    ['zxvt',  'lower',  false]
    ['0369', 'digits', true]
    ['97531', 'digits', false]
    ]
    matches = matching.sequence_match pattern
    msg = "matches '#{pattern}' as a '#{name}' sequence"
    check_matches msg, t, matches, 'sequence', [pattern], [[0, pattern.length - 1]],
      sequence_name: [name]
      ascending: [is_ascending]
  t.end()


test 'repeat matching', (t) ->
  for password in ['', '#']
    msg = "doesn't match length-#{password.length} repeat patterns"
    t.deepEqual matching.repeat_match(password), [], msg

  # test single-character repeats
  prefixes = ['@', 'y4@']
  suffixes = ['u', 'u%7']
  pattern = '&&&&&'
  for [password, i, j] in genpws pattern, prefixes, suffixes
    matches = matching.repeat_match password
    msg = "matches embedded repeat patterns"
    check_matches msg, t, matches, 'repeat', [pattern], [[i, j]],
      base_token: ['&']

  for length in [3, 12]
    for chr in ['a', 'Z', '4', '&']
      pattern = Array(length + 1).join(chr)
      matches = matching.repeat_match pattern
      msg = "matches repeats with base character '#{chr}'"
      check_matches msg, t, matches, 'repeat', [pattern], [[0, pattern.length - 1]],
        base_token: [chr]

  matches = matching.repeat_match 'BBB1111aaaaa@@@@@@'
  patterns = ['BBB','1111','aaaaa','@@@@@@']
  msg = 'matches multiple adjacent repeats'
  check_matches msg, t, matches, 'repeat', patterns, [[0, 2],[3, 6],[7, 11],[12, 17]],
    base_token: ['B', '1', 'a', '@']

  matches = matching.repeat_match '2818BBBbzsdf1111@*&@!aaaaaEUDA@@@@@@1729'
  msg = 'matches multiple repeats with non-repeats in-between'
  check_matches msg, t, matches, 'repeat', patterns, [[4, 6],[12, 15],[21, 25],[30, 35]],
    base_token: ['B', '1', 'a', '@']

  # test multi-character repeats
  pattern = 'abab'
  matches = matching.repeat_match pattern
  msg = 'matches multi-character repeat pattern'
  check_matches msg, t, matches, 'repeat', [pattern], [[0, pattern.length - 1]],
    base_token: ['ab']

  pattern = 'aabaab'
  matches = matching.repeat_match pattern
  msg = 'matches aabaab as a repeat instead of the aa prefix'
  check_matches msg, t, matches, 'repeat', [pattern], [[0, pattern.length - 1]],
    base_token: ['aab']

  pattern = 'abababab'
  matches = matching.repeat_match pattern
  msg = 'identifies ab as repeat string, even though abab is also repeated'
  check_matches msg, t, matches, 'repeat', [pattern], [[0, pattern.length - 1]],
    base_token: ['ab']
  t.end()


test 'regex matching', (t) ->
  for [pattern, name] in [
    ['1922', 'recent_year']
    ['2017', 'recent_year']
    ]
    matches = matching.regex_match pattern
    msg = "matches #{pattern} as a #{name} pattern"
    check_matches msg, t, matches, 'regex', [pattern], [[0, pattern.length - 1]],
      regex_name: [name]
  t.end()


test 'date matching', (t) ->
  for sep in ['', ' ', '-', '/', '\\', '_', '.']
    password = "13#{sep}2#{sep}1921"
    matches = matching.date_match password
    msg = "matches dates that use '#{sep}' as a separator"
    check_matches msg, t, matches, 'date', [password], [[0, password.length - 1]],
      separator: [sep]
      year: [1921]
      month: [2]
      day: [13]

  for order in ['mdy', 'dmy', 'ymd', 'ydm']
    [d,m,y] = [8,8,88]
    password = order
      .replace 'y', y
      .replace 'm', m
      .replace 'd', d
    matches = matching.date_match password
    msg = "matches dates with '#{order}' format"
    check_matches msg, t, matches, 'date', [password], [[0, password.length - 1]],
      separator: ['']
      year: [1988]
      month: [8]
      day: [8]

  password = '111504'
  matches = matching.date_match password
  msg = "matches the date with year closest to REFERENCE_YEAR when ambiguous"
  check_matches msg, t, matches, 'date', [password], [[0, password.length - 1]],
    separator: ['']
    year: [2004] # picks '04' -> 2004 as year, not '1504'
    month: [11]
    day: [15]

  for [day, month, year] in [
    [1,  1,  1999]
    [11, 8,  2000]
    [9,  12, 2005]
    [22, 11, 1551]
    ]
    password = "#{year}#{month}#{day}"
    matches = matching.date_match password
    msg = "matches #{password}"
    check_matches msg, t, matches, 'date', [password], [[0, password.length - 1]],
      separator: ['']
      year: [year]
    password = "#{year}.#{month}.#{day}"
    matches = matching.date_match password
    msg = "matches #{password}"
    check_matches msg, t, matches, 'date', [password], [[0, password.length - 1]],
      separator: ['.']
      year: [year]

  password = "02/02/02"
  matches = matching.date_match password
  msg = "matches zero-padded dates"
  check_matches msg, t, matches, 'date', [password], [[0, password.length - 1]],
    separator: ['/']
    year: [2002]
    month: [2]
    day: [2]

  prefixes = ['a', 'ab']
  suffixes = ['!']
  pattern = '1/1/91'
  for [password, i, j] in genpws pattern, prefixes, suffixes
    matches = matching.date_match password
    msg = "matches embedded dates"
    check_matches msg, t, matches, 'date', [pattern], [[i, j]],
      year: [1991]
      month: [1]
      day: [1]

  matches = matching.date_match '12/20/1991.12.20'
  msg = "matches overlapping dates"
  check_matches msg, t, matches, 'date', ['12/20/1991', '1991.12.20'], [[0, 9], [6,15]],
    separator: ['/', '.']
    year: [1991, 1991]
    month: [12, 12]
    day: [20, 20]

  matches = matching.date_match '912/20/919'
  msg = "matches dates padded by non-ambiguous digits"
  check_matches msg, t, matches, 'date', ['12/20/91'], [[1, 8]],
    separator: ['/']
    year: [1991]
    month: [12]
    day: [20]
  t.end()


test 'omnimatch', (t) ->
  t.deepEquals matching.omnimatch(''), [], "doesn't match ''"
  password = 'r0sebudmaelstrom11/20/91aaaa'
  matches = matching.omnimatch password
  for [pattern_name, [i, j]] in [
    [ 'dictionary',  [0, 6] ]
    [ 'dictionary',  [7, 15] ]
    [ 'date',        [16, 23] ]
    [ 'repeat',      [24, 27] ]
    ]
    included = false
    for match in matches
      included = true if match.i == i and match.j == j and match.pattern == pattern_name
    msg = "for #{password}, matches a #{pattern_name} pattern at [#{i}, #{j}]"
    t.ok included, msg
  t.end()
