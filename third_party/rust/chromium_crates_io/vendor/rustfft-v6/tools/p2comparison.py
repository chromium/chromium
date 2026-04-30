import sys
import math
from matplotlib import pyplot as plt


with open(sys.argv[1]) as f:
    lines = f.readlines()

results = {"f32": {"scalar": {}, "sse": {}, "avx":{}}, "f64": {"scalar": {}, "sse": {}, "avx":{}}}

for line in lines:
    if line.startswith("test ") and not line.startswith("test result"):
        name, result = line.split("... bench:")
        name = name.split()[1]
        _, length, ftype, algo = name.split("_")
        value = float(result.strip().split(" ")[0].replace(",", ""))
        results[ftype][algo][float(length)] = value

lengths = sorted(list(results["f32"]["scalar"].keys()))


scalar_32 = []
avx_32 = []
sse_32 = []
for l in lengths:
    sc32 = results["f32"]["scalar"][l]
    av32 = results["f32"]["avx"][l]
    ss32 = results["f32"]["sse"][l]
    scalar_32.append(100.0)
    sse_32.append(100.0 * sc32/ss32)
    avx_32.append(100.0 * sc32/av32)

scalar_64 = []
avx_64 = []
sse_64 = []
for l in lengths:
    sc64 = results["f64"]["scalar"][l]
    av64 = results["f64"]["avx"][l]
    ss64 = results["f64"]["sse"][l]
    scalar_64.append(100.0)
    sse_64.append(100.0 * sc64/ss64)
    avx_64.append(100.0 * sc64/av64)

lengths = [math.log(l, 2) for l in lengths]

plt.figure()
plt.plot(lengths, scalar_64, lengths, sse_64, lengths, avx_64)
plt.title("f64")
plt.ylabel("relative speed, %")
plt.xlabel("log2(length)")
plt.xticks(list(range(4,23)))
plt.grid()
plt.legend(["scalar", "sse", "avx"])

plt.figure()
plt.plot(lengths, scalar_32, lengths, sse_32, lengths, avx_32)
plt.title("f32")
plt.ylabel("relative speed, %")
plt.xlabel("log2(length)")
plt.legend(["scalar", "sse", "avx"])
plt.xticks(list(range(4,23)))
plt.grid()
plt.show()


