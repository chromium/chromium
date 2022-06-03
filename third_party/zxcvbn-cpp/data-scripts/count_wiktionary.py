#!/usr/bin/python

import os
import sys
import codecs
import operator

from unidecode import unidecode

def usage():
    return '''
This script extracts words and counts from a 2006 wiktionary word frequency study over American
television and movies. To use, first visit the study and download, as .html files, all 26 of the
frequency lists:

https://en.wiktionary.org/wiki/Wiktionary:Frequency_lists#TV_and_movie_scripts

Put those into a single directory and point it to this script:

%s wiktionary_html_dir ../data/us_tv_and_film.txt

output.txt will include one line per word in the study, ordered by rank, of the form:

word1 count1
word2 count2
...
    ''' % sys.argv[0]

def parse_wiki_tokens(html_doc_str):
    '''fragile hax, but checks the result at the end'''
    results = []
    last3 = ['', '', '']
    header = True
    skipped = 0
    for line in html_doc_str.split('\n'):
        last3.pop(0)
        last3.append(line.strip())
        if all(s.startswith('<td>') and not s == '<td></td>' for s in last3):
            if header:
                header = False
                continue
            last3 = [s.replace('<td>', '').replace('</td>', '').strip() for s in last3]
            rank, token, count = last3
            rank = int(rank.split()[0])
            token = token.replace('</a>', '')
            token = token[token.index('>')+1:]
            token = normalize(token)
            # wikitonary has thousands of words that end in 's
            # keep the common ones (rank under 1000), discard the rest
            #
            # otherwise end up with a bunch of duplicates eg victor / victor's
            if token.endswith("'s") and rank > 1000:
                skipped += 1
                continue
            count = int(count)
            results.append((rank, token, count))
    # early docs have 1k entries, later 2k, last 1284
    assert len(results) + skipped in [1000, 2000, 1284]
    return results

def normalize(token):
    return unidecode(token).lower()

def main(wiktionary_html_root, output_filename):
    rank_token_count = [] # list of 3-tuples
    for filename in os.listdir(wiktionary_html_root):
        path = os.path.join(wiktionary_html_root, filename)
        with codecs.open(path, 'r', 'utf8') as f:
            rank_token_count.extend(parse_wiki_tokens(f.read()))
    rank_token_count.sort(key=operator.itemgetter(0))
    with codecs.open(output_filename, 'w', 'utf8') as f:
        for rank, token, count in rank_token_count:
            f.write('%-18s %d\n' % (token, count))

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print usage()
    else:
        main(*sys.argv[1:])
    sys.exit(0)
