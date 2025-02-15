# Description:
#   Eigen is a C++ template library for linear algebra: vectors,
#   matrices, and related algorithms.

licenses([
    # Note: Eigen is an MPL2 library that includes GPL v3 and LGPL v2.1+ code.
    #       We've taken special care to not reference any restricted code.
    "reciprocal",  # MPL2
    "notice",  # Portions BSD
])

exports_files(["LICENSE"])

cc_library(
    name = "eigen3",
    hdrs = [
        "Eigen/Cholesky",
        "Eigen/Core",
        "Eigen/Eigenvalues",
        "Eigen/LU",
        "Eigen/QR",
        "Eigen/SVD",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@eigen_archive//:eigen",
    ],
)
