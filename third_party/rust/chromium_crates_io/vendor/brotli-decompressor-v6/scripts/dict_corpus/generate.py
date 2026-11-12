#!/usr/bin/env python3
"""Regenerates testdata/dict_corpus from the reference C implementation.

The corpus is a set of (dictionary, content, compressed) triples produced by
the C encoder with a dictionary attached, and verified to roundtrip with the
C decoder. The Rust test test_dictionary_corpus decompresses every *.br file
and compares against the matching *.content file, keeping the Rust decoder
differentially tested against the C implementation without needing a C
toolchain at test time.

usage: generate.py /path/to/google-brotli-checkout

Requires gcc. Deterministic given the same C and Python implementation versions.
"""
import os
import random
import shutil
import struct
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CORPUS_DIR = os.path.normpath(
    os.path.join(SCRIPT_DIR, '..', '..', 'testdata', 'dict_corpus'))
HARNESS = os.path.join(SCRIPT_DIR, 'harness')


def build_harness(brotli_root):
    import glob
    sources = (glob.glob(os.path.join(brotli_root, 'c', 'common', '*.c')) +
               glob.glob(os.path.join(brotli_root, 'c', 'dec', '*.c')) +
               glob.glob(os.path.join(brotli_root, 'c', 'enc', '*.c')))
    subprocess.check_call(
        ['gcc', '-O1', '-DBROTLI_EXPERIMENTAL=1',
         '-I', os.path.join(brotli_root, 'c', 'include'),
         os.path.join(SCRIPT_DIR, 'harness.c')] + sources +
        ['-o', HARNESS, '-lm'])


def varint(n):
    out = bytearray()
    while True:
        b = n & 127
        n >>= 7
        if n:
            out.append(b | 128)
        else:
            out.append(b)
            return bytes(out)


def gen_serialized(seed):
    """Random serialized dictionary + content that references it heavily."""
    rng = random.Random(seed)
    word_lists = []
    all_words = []
    for _ in range(rng.choice([1, 1, 2])):
        size_bits = [0] * 32
        data = bytearray()
        for length in sorted(rng.sample(range(4, 16), rng.randrange(1, 4))):
            bits = rng.randrange(1, 4)
            size_bits[length] = bits
            for _ in range(1 << bits):
                word = bytes(rng.choice(b'abcdefghijklmnopqrstuvwxyz ')
                             for _ in range(length))
                data += word
                all_words.append(word)
        word_lists.append(bytes(size_bits[4:32]) + bytes(data))
    transform_lists = []
    for _ in range(rng.choice([0, 1, 1])):
        strs = [b' ', b'ing', b'er', b' the']
        rng.shuffle(strs)
        strs.append(b'')  # the empty stringlet must terminate the table
        ps = bytearray()
        for st in strs:
            ps.append(len(st))
            ps += st
        out = bytearray(struct.pack('<H', len(ps))) + ps
        ids = list(range(len(strs)))
        empty_id = len(strs) - 1
        transforms = [(empty_id, 0, empty_id)]  # ["", IDENTITY, ""]
        has_params = False
        for _ in range(rng.randrange(0, 6)):
            ttype = rng.choice([0, 1, 2, 9, 10, 11, 12, 21, 22])
            if ttype in (21, 22):
                has_params = True
            transforms.append((rng.choice(ids), ttype, rng.choice(ids)))
        out.append(len(transforms))
        for tr in transforms:
            out += bytes(tr)
        if has_params:
            for (_, ttype, _) in transforms:
                out += struct.pack(
                    '<H', rng.randrange(1, 1000) if ttype in (21, 22) else 0)
        transform_lists.append(bytes(out))
    prefix_rng = random.Random(seed + 1)
    prefix = bytes(prefix_rng.choice(b'abcdefgh ')
                   for _ in range(rng.choice([0, 200, 3000])))
    blob = bytearray(b'\x91\x00')
    blob += varint(len(prefix)) + prefix
    blob.append(len(word_lists))
    for w in word_lists:
        blob += w
    blob.append(len(transform_lists))
    for t in transform_lists:
        blob += t
    if word_lists or transform_lists:
        num_dicts = rng.choice([1, 2, min(3, len(word_lists) + 1)])
        blob.append(num_dicts)
        for _ in range(num_dicts):
            blob.append(rng.randrange(0, len(word_lists) + 1))
            blob.append(rng.randrange(0, len(transform_lists) + 1))
        if num_dicts > 1 and rng.random() < 0.7:
            blob.append(1)  # CONTEXT_ENABLED
            blob += bytes(rng.randrange(0, num_dicts) for _ in range(64))
        else:
            blob.append(0)
    rng2 = random.Random(seed + 2)
    content = bytearray()
    while len(content) < rng2.choice([2000, 20000]):
        r = rng2.random()
        if all_words and r < 0.5:
            content += rng2.choice(all_words)
        elif prefix and r < 0.7:
            start = rng2.randrange(0, max(1, len(prefix) - 50))
            content += prefix[start:start + rng2.randrange(10, 50)]
        else:
            content += bytes(rng2.randrange(256)
                             for _ in range(rng2.randrange(1, 30)))
        content += b' '
    return bytes(blob), bytes(content)


def gen_raw(seed):
    """Random raw dictionary + content; content + dict exceed small windows
    so dictionary references outlive the ring buffer wrap (issue #42)."""
    rng = random.Random(seed)
    n_dict = rng.choice([1000, 4096, 100000])
    n_content = rng.choice([5000, 65536, 200000])
    words = [bytes([rng.randrange(97, 123)] * rng.randrange(1, 9))
             for _ in range(50)]

    def gen(n):
        out = bytearray()
        while len(out) < n:
            out += rng.choice(words)
            if rng.randrange(10) == 0:
                out.append(rng.randrange(256))
        return bytes(out[:n])

    dictionary = gen(n_dict)
    content = bytearray()
    while len(content) < n_content:
        start = rng.randrange(0, max(1, len(dictionary) - 100))
        content += dictionary[start:start + rng.randrange(20, 200)]
        content += gen(rng.randrange(0, 50))
    return dictionary, bytes(content[:n_content])


def emit(name, kind, dictionary, content, settings):
    dict_path = os.path.join(CORPUS_DIR, '%s.%s.dict' % (name, kind))
    content_path = os.path.join(CORPUS_DIR, '%s.content' % name)
    with open(dict_path, 'wb') as f:
        f.write(dictionary)
    with open(content_path, 'wb') as f:
        f.write(content)
    for (quality, lgwin) in settings:
        br_path = os.path.join(CORPUS_DIR, '%s.q%dw%d.br' % (name, quality, lgwin))
        subprocess.check_call([HARNESS, 'enc', kind, dict_path, content_path,
                               br_path, str(quality), str(lgwin)])
        # Verify the C decoder agrees before checking the fixture in.
        out_path = br_path + '.ctmp'
        subprocess.check_call([HARNESS, 'dec', kind, dict_path, br_path,
                               out_path])
        with open(out_path, 'rb') as f:
            if f.read() != content:
                raise AssertionError('C roundtrip failed for ' + br_path)
        os.unlink(out_path)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    build_harness(sys.argv[1])
    os.makedirs(CORPUS_DIR, exist_ok=True)
    for stale in os.listdir(CORPUS_DIR):
        stale_path = os.path.join(CORPUS_DIR, stale)
        if os.path.isdir(stale_path):
            shutil.rmtree(stale_path)
        else:
            os.unlink(stale_path)
    windows = [12, 18, 22]
    for i in range(8):
        blob, content = gen_serialized(i * 1000)
        emit('case%02d' % i, 'serialized', blob, content,
             [(5, windows[i % 3]), (11, windows[(i + 1) % 3])])
    raw_settings = [(9, 10), (11, 16), (9, 22), (11, 26)]
    for i in range(4):
        dictionary, content = gen_raw(i * 1000 + 7)
        emit('raw%02d' % i, 'raw', dictionary, content, [raw_settings[i]])
    print('corpus regenerated in', CORPUS_DIR)


if __name__ == '__main__':
    main()
