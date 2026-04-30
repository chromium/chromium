import sys
import math
from matplotlib import pyplot as plt


with open(sys.argv[1]) as f:
    lines = f.readlines()

results = {"f32": {"scalar": {}, "wasmsimd": {}}, "f64": {"scalar": {}, "wasmsimd": {}}}

for line in lines:
    if line.startswith("test ") and not line.startswith("test result"):
        name, result = line.split("... bench:")
        name = name.split()[1]
        _, length, ftype, algo = name.split("_")
        value = float(result.strip().split(" ")[0].replace(",", ""))
        results[ftype][algo][float(length)] = value

lengths = sorted(list(results["f32"]["scalar"].keys()))


scalar_32 = []
wasmsimd_32 = []
for l in lengths:
    sc32 = results["f32"]["scalar"][l]
    nn32 = results["f32"]["wasmsimd"][l]
    scalar_32.append(100.0)
    wasmsimd_32.append(100.0 * sc32/nn32)

scalar_64 = []
wasmsimd_64 = []
for l in lengths:
    sc64 = results["f64"]["scalar"][l]
    nn64 = results["f64"]["wasmsimd"][l]
    scalar_64.append(100.0)
    wasmsimd_64.append(100.0 * sc64/nn64)

lengths = [math.log(l, 2) for l in lengths]

plt.figure()
plt.plot(lengths, scalar_64, lengths, wasmsimd_64)
plt.title("f64")
plt.ylabel("relative speed, %")
plt.xlabel("log2(length)")
plt.xticks(list(range(4,23)))
plt.grid()
plt.legend(["scalar", "wasmsimd"])

plt.figure()
plt.plot(lengths, scalar_32, lengths, wasmsimd_32)
plt.title("f32")
plt.ylabel("relative speed, %")
plt.xlabel("log2(length)")
plt.legend(["scalar", "wasmsimd"])
plt.xticks(list(range(4,23)))
plt.grid()
plt.show()


