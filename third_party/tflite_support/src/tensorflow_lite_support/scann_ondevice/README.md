# On-device Implementation of ScaNN

[ScaNN](https://github.com/google-research/google-research/tree/master/scann)
(Scalable Nearest Neighbors) is a method for efficient vector similarity search
at scale. This is a simplified version of
[ScaNN](https://github.com/google-research/google-research/tree/master/scann)
that requires less resources to run and only for inference. There's no support
for K-Means partitioning training and quantization training. It supports
retrieval with the following features:

1.  K-Means tree space partitioning.
2.  [Asymmetric Hashing](https://research.google/pubs/pub41694/) (AH)
    quantization.
3.  `dot_product` and `squared_l2` distance measures. Note that for
    `dot_product` distance, we return the *negative* dot product. This is to
    ensure consistency with `squared_l2` that smaller means closer.
4.  Indexing new embeddings, including assigning them to closest partitions and
    AH quantize them.
