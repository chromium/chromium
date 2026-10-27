# SentencePiece Performance Benchmark

This directory contains scripts to benchmark the performance of the SentencePiece Python wrapper against Hugging Face Fast tokenizers and Tiktoken on a balanced raw multilingual dataset.

## How to Run

Follow these steps in order from this `benchmark` directory:

### 1. Set Up the Environment
Run the setup script to create a virtual environment, build SentencePiece C++ and Python wrapper from local source, and install all dependencies:
```bash
./setup_env.sh
```
*Note: This script will activate the virtual environment (`venv`) for the installation.*

### 2. Activate the Virtual Environment
For subsequent steps, ensure the virtual environment is activated:
```bash
source venv/bin/activate
```

### 3. Download Models
Download the required model tokenizer files (T5 and Gemma 3) from Hugging Face:
```bash
python download_models.py
```
*Note: `google/gemma-3-4b` is a gated model. You may need to run `huggingface-cli login` first or set the `HF_TOKEN` environment variable.*

### 4. Prepare the Multilingual Dataset
Download parallel sentences from FLORES-200 and generate the interleaved, balanced raw text corpus:
```bash
python prepare_balanced_raw_multilingual.py
```
This saves the dataset to `data/multilingual_raw_balanced.txt` (~11.3 MB).

### 5. Run the Benchmark
Execute the benchmark configurations. This runs each tokenizer/thread configuration in a separate process to ensure clean isolation and respects thread settings:
```bash
python run_bench.py
```
This will run the benchmarks and save the raw performance metrics to `results.json`.

### 6. Analyze and View Results
Print the formatted performance comparison tables:
```bash
python analyze_results.py
```
