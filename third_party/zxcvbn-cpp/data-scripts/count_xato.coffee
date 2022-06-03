matching = require '../lib/matching'
scoring = require '../lib/scoring'

fs = require 'fs'
byline = require 'byline'
sprintf = require('sprintf-js').sprintf


check_usage = () ->
  usage = '''

  Run a frequency count on the raw 10M xato password set and keep counts over CUTOFF in
  descending frequency. That file can be found by googling around for:
  "xato 10-million-combos.txt"

  Passwords that both:
  -- fully match according to zxcvbn's date, year, repeat, sequence or keyboard matching algs
  -- have a higher rank than the corresponding match guess number

  are excluded from the final password set, since zxcvbn would score them lower through
  other means anyhow. in practice this rules out dates and years most often and makes room
  for more useful data.

  To use, first run from zxcvbn base dir:

  npm run build

  then change into data-scripts directory and run:

  coffee count_xato.coffee --nodejs xato_file.txt ../data/passwords.txt

  '''
  valid = process.argv.length == 5
  valid = valid and process.argv[0] == 'coffee' and process.argv[2] in ['--nodejs', '-n']
  valid = valid and __dirname.split('/').slice(-1)[0] == 'data-scripts'
  unless valid
    console.log usage
    process.exit(0)

# after all passwords are counted, discard pws with counts <= COUNTS
CUTOFF = 10

# to save memory, after every batch of size BATCH_SIZE, go through counts and delete
# long tail of entries with only one count.
BATCH_SIZE = 1000000

counts = {}       # maps pw -> count
skipped_lines = 0 # skipped lines in xato file -- lines w/o two tokens
line_count = 0    # current number of lines processed

normalize = (token) ->
  token.toLowerCase()

should_include = (password, xato_rank) ->
  for i in [0...password.length]
    if password.charCodeAt(i) > 127
      # xato mostly contains ascii-only passwords, so in practice
      # this will only skip one or two top passwords over the cutoff.
      # were that not the case / were this used on a different data source, consider using
      # a unidecode-like library instead, similar to count_wikipedia / count_wiktionary
      console.log "SKIPPING non-ascii password=#{password}, rank=#{xato_rank}"
      return false
  matches = []
  for matcher in [
    matching.spatial_match
    matching.repeat_match
    matching.sequence_match
    matching.regex_match
    matching.date_match
    ]
    matches.push.apply matches, matcher.call(matching, password)
  matches = matches.filter (match) ->
    # only keep matches that span full password
    match.i == 0 and match.j == password.length - 1
  for match in matches
    if scoring.estimate_guesses(match, password) < xato_rank
      # filter out this entry: non-dictionary matching will assign
      # a lower guess estimate.
      return false
  return true

prune = (counts) ->
  for pw, count of counts
    if count == 1
      delete counts[pw]

main = (xato_filename, output_filename) ->
  stream = byline.createStream fs.createReadStream(xato_filename, encoding: 'utf8')
  stream.on 'readable', ->
    while null != (line = stream.read())
      line_count += 1
      if line_count % BATCH_SIZE == 0
        console.log 'counting tokens:', line_count
        prune counts
      tokens = line.trim().split /\s+/
      unless tokens.length == 2
        skipped_lines += 1
        continue
      [username, password] = tokens[..1]
      password = normalize password
      if password of counts
        counts[password] += 1
      else
        counts[password] = 1
  stream.on 'end', ->
    console.log 'skipped lines:', skipped_lines
    pairs = []
    console.log 'copying to tuples'
    for pw, count of counts
      if count > CUTOFF
        pairs.push [pw, count]
      delete counts[pw] # save memory to avoid v8 1GB limit
    console.log 'sorting'
    pairs.sort (p1, p2) ->
      # sort by count. higher counts go first.
      p2[1] - p1[1]
    console.log 'filtering'
    pairs = pairs.filter (pair, i) ->
      rank = i + 1
      [pw, count] = pair
      should_include pw, rank
    output_stream = fs.createWriteStream output_filename, encoding: 'utf8'
    for pair in pairs
      [pw, count] = pair
      output_stream.write sprintf("%-15s %d\n", pw, count)
    output_stream.end()

check_usage()
main process.argv[3], process.argv[4]
