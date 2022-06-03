test = require 'tape'

ROOT = "../src"
if process.env.ROOT
  ROOT = process.env.ROOT

scoring = require (ROOT + '/scoring')
matching = require (ROOT + '/matching')

log2 = scoring.log2
log10 = scoring.log10
nCk = scoring.nCk
EPSILON = 1e-10 # truncate to 10th decimal place
truncate_float = (float) -> Math.round(float / EPSILON) * EPSILON
approx_equal = (t, actual, expected, msg) ->
  t.equal truncate_float(actual), truncate_float(expected), msg

test 'nCk', (t) ->
  for [n, k, result] in [
    [ 0,  0, 1 ]
    [ 1,  0, 1 ]
    [ 5,  0, 1 ]
    [ 0,  1, 0 ]
    [ 0,  5, 0 ]
    [ 2,  1, 2 ]
    [ 4,  2, 6 ]
    [ 33, 7, 4272048 ]
    ]
    t.equal nCk(n, k), result, "nCk(#{n}, #{k}) == #{result}"
  n = 49
  k = 12
  t.equal nCk(n, k), nCk(n, n-k), "mirror identity"
  t.equal nCk(n, k), nCk(n-1, k-1) + nCk(n-1, k), "pascal's triangle identity"
  t.end()

test 'log', (t) ->
  for [n, result] in [
    [ 1,  0 ]
    [ 2,  1 ]
    [ 4,  2 ]
    [ 32, 5 ]
    ]
    t.equal log2(n), result, "log2(#{n}) == #{result}"
  for [n, result] in [
    [ 1, 0]
    [ 10, 1]
    [ 100, 2]
    ]
    t.equal log10(n), result, "log10(#{n}) == #{result}"
  n = 17
  p = 4
  approx_equal t, log10(n * p), log10(n) + log10(p), "product rule"
  approx_equal t, log10(n / p), log10(n) - log10(p), "quotient rule"
  approx_equal t, log10(Math.E), 1 / Math.log(10), "base switch rule"
  approx_equal t, log10(Math.pow(n, p)), p * log10(n), "power rule"
  approx_equal t, log10(n), Math.log(n) / Math.log(10), "base change rule"
  t.end()

test 'search', (t) ->
  m = (i, j, guesses) ->
    i: i
    j: j
    guesses: guesses
  password = '0123456789'

  # for tests, set additive penalty to zero.
  exclude_additive = true

  msg = (s) -> "returns one bruteforce match given an empty match sequence: #{s}"
  result = scoring.most_guessable_match_sequence password, []
  t.equal result.sequence.length, 1, msg("result.length == 1")
  m0 = result.sequence[0]
  t.equal m0.pattern, 'bruteforce', msg("match.pattern == 'bruteforce'")
  t.equal m0.token, password, msg("match.token == #{password}")
  t.deepEqual [m0.i, m0.j], [0, 9], msg("[i, j] == [#{m0.i}, #{m0.j}]")

  msg = (s) -> "returns match + bruteforce when match covers a prefix of password: #{s}"
  matches = [m0] = [m(0, 5, 1)]
  result = scoring.most_guessable_match_sequence password, matches, exclude_additive
  t.equal result.sequence.length, 2, msg("result.match.sequence.length == 2")
  t.equal result.sequence[0], m0, msg("first match is the provided match object")
  m1 = result.sequence[1]
  t.equal m1.pattern, 'bruteforce', msg("second match is bruteforce")
  t.deepEqual [m1.i, m1.j], [6, 9], msg("second match covers full suffix after first match")

  msg = (s) -> "returns bruteforce + match when match covers a suffix: #{s}"
  matches = [m1] = [m(3, 9, 1)]
  result = scoring.most_guessable_match_sequence password, matches, exclude_additive
  t.equal result.sequence.length, 2, msg("result.match.sequence.length == 2")
  m0 = result.sequence[0]
  t.equal m0.pattern, 'bruteforce', msg("first match is bruteforce")
  t.deepEqual [m0.i, m0.j], [0, 2], msg("first match covers full prefix before second match")
  t.equal result.sequence[1], m1, msg("second match is the provided match object")

  msg = (s) -> "returns bruteforce + match + bruteforce when match covers an infix: #{s}"
  matches = [m1] = [m(1, 8, 1)]
  result = scoring.most_guessable_match_sequence password, matches, exclude_additive
  t.equal result.sequence.length, 3, msg("result.length == 3")
  t.equal result.sequence[1], m1, msg("middle match is the provided match object")
  m0 = result.sequence[0]
  m2 = result.sequence[2]
  t.equal m0.pattern, 'bruteforce', msg("first match is bruteforce")
  t.equal m2.pattern, 'bruteforce', msg("third match is bruteforce")
  t.deepEqual [m0.i, m0.j], [0, 0], msg("first match covers full prefix before second match")
  t.deepEqual [m2.i, m2.j], [9, 9], msg("third match covers full suffix after second match")

  msg = (s) -> "chooses lower-guesses match given two matches of the same span: #{s}"
  matches = [m0, m1] = [m(0, 9, 1), m(0, 9, 2)]
  result = scoring.most_guessable_match_sequence password, matches, exclude_additive
  t.equal result.sequence.length, 1, msg("result.length == 1")
  t.equal result.sequence[0], m0, msg("result.sequence[0] == m0")
  # make sure ordering doesn't matter
  m0.guesses = 3
  result = scoring.most_guessable_match_sequence password, matches, exclude_additive
  t.equal result.sequence.length, 1, msg("result.length == 1")
  t.equal result.sequence[0], m1, msg("result.sequence[0] == m1")

  msg = (s) -> "when m0 covers m1 and m2, choose [m0] when m0 < m1 * m2 * fact(2): #{s}"
  matches = [m0, m1, m2] = [m(0, 9, 3), m(0, 3, 2), m(4, 9, 1)]
  result = scoring.most_guessable_match_sequence password, matches, exclude_additive
  t.equal result.guesses, 3, msg("total guesses == 3")
  t.deepEqual result.sequence, [m0], msg("sequence is [m0]")

  msg = (s) -> "when m0 covers m1 and m2, choose [m1, m2] when m0 > m1 * m2 * fact(2): #{s}"
  m0.guesses = 5
  result = scoring.most_guessable_match_sequence password, matches, exclude_additive
  t.equal result.guesses, 4, msg("total guesses == 4")
  t.deepEqual result.sequence, [m1, m2], msg("sequence is [m1, m2]")
  t.end()

test 'calc_guesses', (t) ->
  match =
    guesses: 1
  msg = "estimate_guesses returns cached guesses when available"
  t.equal scoring.estimate_guesses(match, ''), 1, msg
  match =
    pattern: 'date'
    token: '1977'
    year: 1977
    month: 7
    day: 14
  msg = "estimate_guesses delegates based on pattern"
  t.equal scoring.estimate_guesses(match, '1977'), scoring.date_guesses(match), msg
  t.end()

test 'repeat guesses', (t) ->
  for [token, base_token, repeat_count] in [
    [ 'aa',   'a',  2]
    [ '999',  '9',  3]
    [ '$$$$', '$',  4]
    [ 'abab', 'ab', 2]
    [ 'batterystaplebatterystaplebatterystaple', 'batterystaple', 3]
    ]
    base_guesses = scoring.most_guessable_match_sequence(
      base_token
      matching.omnimatch base_token
    ).guesses
    match =
      token: token
      base_token: base_token
      base_guesses: base_guesses
      repeat_count: repeat_count
    expected_guesses = base_guesses * repeat_count
    msg = "the repeat pattern '#{token}' has guesses of #{expected_guesses}"
    t.equal scoring.repeat_guesses(match), expected_guesses, msg
  t.end()

test 'sequence guesses', (t) ->
  for [token, ascending, guesses] in [
    [ 'ab',   true,  4 * 2 ]      # obvious start * len-2
    [ 'XYZ',  true,  26 * 3 ]     # base26 * len-3
    [ '4567', true,  10 * 4 ]     # base10 * len-4
    [ '7654', false, 10 * 4 * 2 ] # base10 * len 4 * descending
    [ 'ZYX',  false, 4 * 3 * 2 ]  # obvious start * len-3 * descending
    ]
    match =
      token: token
      ascending: ascending
    msg = "the sequence pattern '#{token}' has guesses of #{guesses}"
    t.equal scoring.sequence_guesses(match), guesses, msg
  t.end()

test 'regex guesses', (t) ->
  match =
    token: 'aizocdk'
    regex_name: 'alpha_lower'
    regex_match: ['aizocdk']
  msg = "guesses of 26^7 for 7-char lowercase regex"
  t.equal scoring.regex_guesses(match), Math.pow(26, 7), msg

  match =
    token: 'ag7C8'
    regex_name: 'alphanumeric'
    regex_match: ['ag7C8']
  msg = "guesses of 62^5 for 5-char alphanumeric regex"
  t.equal scoring.regex_guesses(match), Math.pow(2 * 26 + 10, 5), msg

  match =
    token: '1972'
    regex_name: 'recent_year'
    regex_match: ['1972']
  msg = "guesses of |year - REFERENCE_YEAR| for distant year matches"
  t.equal scoring.regex_guesses(match), Math.abs(scoring.REFERENCE_YEAR - 1972), msg

  match =
    token: '2005'
    regex_name: 'recent_year'
    regex_match: ['2005']
  msg = "guesses of MIN_YEAR_SPACE for a year close to REFERENCE_YEAR"
  t.equal scoring.regex_guesses(match), scoring.MIN_YEAR_SPACE, msg
  t.end()

test 'date guesses', (t) ->
  match =
    token: '1123'
    separator: ''
    has_full_year: false
    year: 1923
    month: 1
    day: 1
  msg = "guesses for #{match.token} is 365 * distance_from_ref_year"
  t.equal scoring.date_guesses(match), 365 * Math.abs(scoring.REFERENCE_YEAR - match.year), msg

  match =
    token: '1/1/2010'
    separator: '/'
    has_full_year: true
    year: 2010
    month: 1
    day: 1
  msg = "recent years assume MIN_YEAR_SPACE."
  msg += " extra guesses are added for separators."
  t.equal scoring.date_guesses(match), 365 * scoring.MIN_YEAR_SPACE * 4, msg
  t.end()

test 'spatial guesses', (t) ->
  match =
    token: 'zxcvbn'
    graph: 'qwerty'
    turns: 1
    shifted_count: 0
  base_guesses = (
    scoring.KEYBOARD_STARTING_POSITIONS *
    scoring.KEYBOARD_AVERAGE_DEGREE *
    # - 1 term because: not counting spatial patterns of length 1
    # eg for length==6, multiplier is 5 for needing to try len2,len3,..,len6
    (match.token.length - 1)
    )
  msg = "with no turns or shifts, guesses is starts * degree * (len-1)"
  t.equal scoring.spatial_guesses(match), base_guesses, msg

  match.guesses = null
  match.token = 'ZxCvbn'
  match.shifted_count = 2
  shifted_guesses = base_guesses * (nCk(6, 2) + nCk(6, 1))
  msg = "guesses is added for shifted keys, similar to capitals in dictionary matching"
  t.equal scoring.spatial_guesses(match), shifted_guesses, msg

  match.guesses = null
  match.token = 'ZXCVBN'
  match.shifted_count = 6
  shifted_guesses = base_guesses * 2
  msg = "when everything is shifted, guesses are doubled"
  t.equal scoring.spatial_guesses(match), shifted_guesses, msg

  match =
    token: 'zxcft6yh'
    graph: 'qwerty'
    turns: 3
    shifted_count: 0
  guesses = 0
  L = match.token.length
  s = scoring.KEYBOARD_STARTING_POSITIONS
  d = scoring.KEYBOARD_AVERAGE_DEGREE
  for i in [2..L]
    for j in [1..Math.min(match.turns, i-1)]
      guesses += nCk(i-1, j-1) * s * Math.pow(d, j)
  msg = "spatial guesses accounts for turn positions, directions and starting keys"
  t.equal scoring.spatial_guesses(match), guesses, msg
  t.end()

test 'dictionary_guesses', (t) ->
  match =
    token: 'aaaaa'
    rank: 32
  msg = "base guesses == the rank"
  t.equal scoring.dictionary_guesses(match), 32, msg

  match =
    token: 'AAAaaa'
    rank: 32
  msg = "extra guesses are added for capitalization"
  t.equal scoring.dictionary_guesses(match), 32 * scoring.uppercase_variations(match), msg

  match =
    token: 'aaa'
    rank: 32
    reversed: true
  msg = "guesses are doubled when word is reversed"
  t.equal scoring.dictionary_guesses(match), 32 * 2, msg

  match =
    token: 'aaa@@@'
    rank: 32
    l33t: true
    sub: {'@': 'a'}
  msg = "extra guesses are added for common l33t substitutions"
  t.equal scoring.dictionary_guesses(match), 32 * scoring.l33t_variations(match), msg

  match =
    token: 'AaA@@@'
    rank: 32
    l33t: true
    sub: {'@': 'a'}
  msg = "extra guesses are added for both capitalization and common l33t substitutions"
  expected = 32 * scoring.l33t_variations(match) * scoring.uppercase_variations(match)
  t.equal scoring.dictionary_guesses(match), expected, msg
  t.end()

test 'uppercase variants', (t) ->
  for [word, variants] in [
    [ '', 1 ]
    [ 'a', 1 ]
    [ 'A', 2 ]
    [ 'abcdef', 1 ]
    [ 'Abcdef', 2 ]
    [ 'abcdeF', 2 ]
    [ 'ABCDEF', 2 ]
    [ 'aBcdef', nCk(6,1) ]
    [ 'aBcDef', nCk(6,1) + nCk(6,2) ]
    [ 'ABCDEf', nCk(6,1) ]
    [ 'aBCDEf', nCk(6,1) + nCk(6,2) ]
    [ 'ABCdef', nCk(6,1) + nCk(6,2) + nCk(6,3) ]
    ]
    msg = "guess multiplier of #{word} is #{variants}"
    t.equal scoring.uppercase_variations(token: word), variants, msg
  t.end()

test 'l33t variants', (t) ->
  match = l33t: false
  t.equal scoring.l33t_variations(match), 1, "1 variant for non-l33t matches"
  for [word, variants, sub] in [
    [ '',  1, {} ]
    [ 'a', 1, {} ]
    [ '4', 2, {'4': 'a'} ]
    [ '4pple', 2, {'4': 'a'} ]
    [ 'abcet', 1, {} ]
    [ '4bcet', 2, {'4': 'a'} ]
    [ 'a8cet', 2, {'8': 'b'} ]
    [ 'abce+', 2, {'+': 't'} ]
    [ '48cet', 4, {'4': 'a', '8': 'b'} ]
    [ 'a4a4aa',  nCk(6, 2) + nCk(6, 1), {'4': 'a'} ]
    [ '4a4a44',  nCk(6, 2) + nCk(6, 1), {'4': 'a'} ]
    [ 'a44att+', (nCk(4, 2) + nCk(4, 1)) * nCk(3, 1), {'4': 'a', '+': 't'} ]
    ]
    match =
      token: word
      sub: sub
      l33t: not matching.empty(sub)
    msg = "extra l33t guesses of #{word} is #{variants}"
    t.equal scoring.l33t_variations(match), variants, msg
  match =
    token: 'Aa44aA'
    l33t: true
    sub: {'4': 'a'}
  variants = nCk(6, 2) + nCk(6, 1)
  msg = "capitalization doesn't affect extra l33t guesses calc"
  t.equal scoring.l33t_variations(match), variants, msg
  t.end()
