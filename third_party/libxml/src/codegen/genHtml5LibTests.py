#!/usr/bin/env python3

import glob
import json
import re

state_map = {
    'Data state':          0,
    'RCDATA state':        1,
    'RAWTEXT state':       2,
    'PLAINTEXT state':     3,
    'Script data state':   4,
    'CDATA section state': 5,
}

for filename in sorted(glob.glob('../html5lib-tests/tokenizer/*.test')):
    match = re.search('/([^/]*).test$', filename)
    if match is None:
        continue
    testname = match[1]
    if testname == 'xmlViolation':
        continue

    with open(filename) as json_data:
        root = json.load(json_data)

    test_out = open(f'test/html-tokenizer/{testname}.test', 'w')
    result_out = open(f'result/html-tokenizer/{testname}.test', 'w')

    counter = 0

    for tests in root.values():
        for test in tests:
            input = test['input']

            # Skip surrogate tests
            if re.search(r'\\uD[89A-F]', input, re.I):
                continue

            input = re.sub(r'\\u([A-Fa-f0-9]{4})',
                           lambda m: chr(int(m[1], 16)),
                           input)

            output = ''
            for token in test['output']:
                if token[1] == '\0':
                    continue

                output += token[0] + '\n'

                if token[0] == 'DOCTYPE':
                    for i in range(1, 4):
                        if token[i] is None:
                            output += '<none>\n'
                        else:
                            output += token[i] + '\n'
                else:
                    output += token[1]
                    if token[0] == 'StartTag':
                        for name, value in token[2].items():
                            output += f' {name}={value}'
                    output += '\n'

            output = re.sub(r'\\u([A-Fa-f0-9]{4})',
                            lambda m: chr(int(m[1], 16)),
                            output)

            # The HTML5 spec splits handling of U+0000 across
            # tokenizer and tree builder. We already ignore
            # U+0000 in body text when tokenizing.
            output = re.sub(r'\x00', '', output)

            for state in test.get('initialStates', ['Data state']):
                state_no = state_map.get(state)
                if state_no is None:
                    raise Exception(f'{filename}: unknown state: {state}')
                if state_no == 5:
                    continue

                start_tag = test.get('lastStartTag', '-')

                test_out.write(f'{counter} {start_tag} {state_no} '
                               f'{len(input.encode())}\n')
                test_out.write(input)
                test_out.write('\n')

                result_out.write(f'{counter}\n')
                result_out.write(output)

                counter += 1

        test_out.close()
        result_out.close()
