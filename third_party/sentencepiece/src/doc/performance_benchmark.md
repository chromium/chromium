# Tokenizer Performance Analysis (Balanced Raw Multilingual)

This report presents a detailed performance comparison of **SentencePiece**, **Hugging Face Fast Tokenizers**, and **Tiktoken** using a **balanced raw multilingual dataset** derived from FLORES-200.

## Benchmark Setup

*   **Environment**: 24-core CPU, Python 3.13, SentencePiece built from source (master head).
*   **Dataset**: Derived from **FLORES-200** test split (parallel sentences). We use **English, Chinese (Simplified), Japanese, and Thai** (1,012 sentences per language, replicated 15 times to reach **11.29 MB**, 60,720 lines).
    *   **No pre-segmentation**: Chinese, Japanese, and Thai texts are raw and do not contain artificial space delimiters.
    *   **Perfect balance**: Equal number of sentences for each language.
*   **Batch Request Size**: The entire dataset of **60,720 sentences** is fed to the tokenizers as a **single batch request** (a single Python `list[str]`) to measure internal C++/Rust parallelization and Python wrapper overhead.
*   **Models**:
    *   **Unigram**: `t5-base` (SentencePiece vs Hugging Face).
    *   **BPE (Gemma 3)**: `gemma-3-4b` tokenizer (SentencePiece vs Hugging Face).
    *   **BPE (GPT2)**: `gpt2` (Hugging Face vs Tiktoken).

To reproduce these benchmarks, see the reproduction steps in the [benchmark directory README](../benchmark/README.md).

---

## 1. Multi-Threaded Comparison (SentencePiece vs. Hugging Face)

This section compares the scaling performance of SentencePiece and Hugging Face tokenizers on the multilingual dataset.

### Unigram Model: T5

**Encoding Throughput (MB/s):**

| Tokenizer | 1 Thread | 2 Threads | 4 Threads | 8 Threads | 16 Threads | 24 Threads |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **SentencePiece (T5)** | **27.41** | **43.83** | **71.62** | **102.08** | **123.33** | **127.60** |
| Hugging Face (T5) | 3.78 | 7.15 | 12.45 | 20.33 | 27.00 | 31.49 |

### BPE Model: Gemma 3 (256k Vocabulary)

**Encoding Throughput (MB/s):**

| Tokenizer | 1 Thread | 2 Threads | 4 Threads | 8 Threads | 16 Threads | 24 Threads |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **SentencePiece (Gemma 3)** | **7.44** | **12.82** | **23.03** | **36.66** | **48.65** | **52.43** |
| Hugging Face (Gemma 3) | 3.66 | 6.37 | 10.45 | 15.54 | 21.05 | 20.48 |

---

## 2. The Scaling Bottleneck (GIL-Locked Conversion)

A key observation across all tokenizers is that **performance does not scale linearly with the number of threads**. 

*   **Parallel core execution**: The core tokenization algorithms (written in C++ for SentencePiece and Rust for Hugging Face) run in parallel, releasing the GIL and utilizing multiple CPU cores efficiently.
*   **Sequential Python conversion**: After the native threads finish, the resulting native arrays (C++ `std::vector<std::vector<int>>` or Rust `Vec<Vec<u32>>`) must be converted into Python objects (`list[list[int]]` or Hugging Face's `Encoding` objects).
*   **GIL constraint**: This conversion step **must be executed sequentially on Python's main thread** because creating Python objects requires acquiring the Global Interpreter Lock (GIL).

*   **Hugging Face vs. SentencePiece**: Hugging Face is slower and saturates earlier because it instantiates complex Python `Encoding` wrapper objects for each sentence, which is much heavier than SentencePiece's direct conversion to a nested list of raw integers.

---

## 3. Single-Threaded Reference Comparison

This section provides a reference comparison of all tokenizers using their standard models on a **single thread** on the multilingual dataset.
*Note: This is not an apples-to-apples comparison as the models and vocabulary sizes differ.*

**Single-Thread Encoding Throughput (MB/s) Reference:**

| Model Type | Model Name (Vocab Size) | Tokenizer | Multilingual (Balanced Raw) |
| :--- | :--- | :--- | :---: |
| **Unigram** | T5 (32k) | **SentencePiece** | **27.41** |
| Unigram | T5 (32k) | Hugging Face | 3.78 |
| **BPE** | Gemma 3 (256k) | **SentencePiece** | **7.44** |
| BPE | Gemma 3 (256k) | Hugging Face | 3.66 |
| BPE | GPT2 (50k) | Hugging Face | 1.94 |
| BPE | GPT2 (50k) | **Tiktoken** | **3.86** |

