import brotli

with open('ipsum.raw') as f:
    decoded = f.read()

with open('ipsum.brotli') as f:
    c = brotli.Compressor()
    c.process(decoded)
    encoded = c.finish()
    f.write(encoded)
