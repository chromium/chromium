import os
import time
import json
import gc
from multiprocessing import Process, Queue
import sentencepiece as spm
from tokenizers import Tokenizer
import tiktoken

# Configuration
NUM_RUNS = 5
THREADS = [1, 2, 4, 8, 16, 24]
DATASETS = {
    "multi": "data/multilingual_raw_balanced.txt"
}
REPLICATIONS = {
    "multi": 1  # Already ~11MB
}

def load_data(lang):
    path = DATASETS[lang]
    rep = REPLICATIONS[lang]
    with open(path, "r", encoding="utf-8") as f:
        lines = f.readlines()
    # Remove empty lines and strip
    lines = [line.strip() for line in lines if line.strip()]
    
    # Replicate
    full_batch = []
    for _ in range(rep):
        full_batch.extend(lines)
        
    total_bytes = sum(len(line.encode("utf-8")) for line in full_batch)
    total_chars = sum(len(line) for line in full_batch)
    return full_batch, total_bytes, total_chars

def get_tokenizer(tok_type, model_name):
    if tok_type == "sentencepiece":
        if model_name == "t5-base":
            return spm.SentencePieceProcessor(model_file="models/spiece.model")
        elif model_name == "gemma3":
            return spm.SentencePieceProcessor(model_file="models/gemma3/tokenizer.model")
        else:
            raise ValueError(f"Unknown SP model: {model_name}")
    elif tok_type == "huggingface":
        if model_name == "t5-base":
            return Tokenizer.from_file("models/tokenizer.json")
        elif model_name == "gpt2":
            return Tokenizer.from_pretrained("gpt2")
        elif model_name == "gemma3":
            return Tokenizer.from_file("models/gemma3/tokenizer.json")
        else:
            raise ValueError(f"Unknown HF model: {model_name}")
    elif tok_type == "tiktoken":
        if model_name == "gpt2":
            return tiktoken.get_encoding("gpt2")
        else:
            raise ValueError(f"Unknown Tiktoken model: {model_name}")
    else:
        raise ValueError(f"Unknown tokenizer type: {tok_type}")

def benchmark_worker(tok_type, model_name, action, lang, num_threads, queue):
    # Set Rayon threads for HF
    os.environ["RAYON_NUM_THREADS"] = str(num_threads)
    
    # Load data
    batch, total_bytes, total_chars = load_data(lang)
    
    # Load tokenizer
    tokenizer = get_tokenizer(tok_type, model_name)
    
    # Prepare inputs/outputs for warm up and measure
    if action == "encode":
        # Warmup
        if tok_type == "sentencepiece":
            pool = spm.ThreadPool(num_threads)
            _ = tokenizer.encode(batch[:100], thread_pool=pool)
        elif tok_type == "huggingface":
            _ = tokenizer.encode_batch(batch[:100])
        elif tok_type == "tiktoken":
            _ = tokenizer.encode_ordinary_batch(batch[:100], num_threads=num_threads)
            
        gc.collect()
        
        # Measure
        start_time = time.perf_counter()
        if tok_type == "sentencepiece":
            ids = tokenizer.encode(batch, thread_pool=pool)
        elif tok_type == "huggingface":
            # encode_batch returns list of Encoding objects
            encodings = tokenizer.encode_batch(batch)
            ids = [x.ids for x in encodings]
        elif tok_type == "tiktoken":
            ids = tokenizer.encode_ordinary_batch(batch, num_threads=num_threads)
        end_time = time.perf_counter()
        
        total_tokens = sum(len(x) for x in ids)
        
    duration = end_time - start_time
    
    result = {
        "duration": duration,
        "bytes_per_sec": total_bytes / duration,
        "chars_per_sec": total_chars / duration,
        "tokens_per_sec": total_tokens / duration,
        "total_tokens": total_tokens,
        "total_bytes": total_bytes,
        "total_chars": total_chars
    }
    queue.put(result)

def run_single_benchmark(tok_type, model_name, action, lang, num_threads):
    queue = Queue()
    p = Process(target=benchmark_worker, args=(tok_type, model_name, action, lang, num_threads, queue))
    p.start()
    p.join()
    if p.exitcode != 0:
        print(f"  Process failed with exit code {p.exitcode}")
        return None
    return queue.get()

def check_model_available(tok_type, model_name):
    if tok_type == "sentencepiece":
        if model_name == "t5-base":
            return os.path.exists("models/spiece.model")
        elif model_name == "gemma3":
            return os.path.exists("models/gemma3/tokenizer.model")
    elif tok_type == "huggingface":
        if model_name == "t5-base":
            return os.path.exists("models/tokenizer.json")
        elif model_name == "gemma3":
            return os.path.exists("models/gemma3/tokenizer.json")
    return True

def main():
    # Verify dataset exists
    for lang, path in DATASETS.items():
        if not os.path.exists(path):
            print(f"Error: Dataset file '{path}' not found.")
            print("Please run 'python prepare_balanced_raw_multilingual.py' first.")
            return

    results = []
    
    tasks = [
        ("sentencepiece", "t5-base", "unigram_t5"),
        ("huggingface", "t5-base", "unigram_t5"),
        ("tiktoken", "gpt2", "bpe_gpt2"),
        ("huggingface", "gpt2", "bpe_gpt2"),
        ("sentencepiece", "gemma3", "bpe_gemma3"),
        ("huggingface", "gemma3", "bpe_gemma3")
    ]
    
    # Filter tasks based on model availability
    available_tasks = []
    for tok_type, model_name, group in tasks:
        if check_model_available(tok_type, model_name):
            available_tasks.append((tok_type, model_name, group))
        else:
            print(f"Warning: Skipping task '{tok_type} ({model_name})' because model files are missing.")
            
    if not available_tasks:
        print("Error: No tasks available to run. Please run 'python download_models.py' first.")
        return
    
    for lang in DATASETS.keys():
        print(f"Running benchmark for language: {lang}")
        for tok_type, model_name, group in available_tasks:
            for action in ["encode"]:
                for t in THREADS:
                    print(f"  {tok_type} ({model_name}) | {action} | threads: {t} ... ", end="", flush=True)
                    
                    runs = []
                    failed = False
                    for run_idx in range(NUM_RUNS):
                        res = run_single_benchmark(tok_type, model_name, action, lang, t)
                        if res is None:
                            failed = True
                            break
                        runs.append(res)
                        time.sleep(0.5)
                        
                    if failed:
                        print("FAILED")
                        continue
                        
                    best_run = min(runs, key=lambda x: x["duration"])
                    avg_duration = sum(r["duration"] for r in runs) / len(runs)
                    avg_bytes_per_sec = sum(r["bytes_per_sec"] for r in runs) / len(runs)
                    avg_chars_per_sec = sum(r["chars_per_sec"] for r in runs) / len(runs)
                    avg_tokens_per_sec = sum(r["tokens_per_sec"] for r in runs) / len(runs)
                    
                    print(f"{best_run['bytes_per_sec'] / 1024 / 1024:.2f} MB/s (best) | {avg_bytes_per_sec / 1024 / 1024:.2f} MB/s (avg)")
                    
                    results.append({
                        "language": lang,
                        "tokenizer_type": tok_type,
                        "model_name": model_name,
                        "group": group,
                        "action": action,
                        "threads": t,
                        "best": best_run,
                        "avg": {
                            "duration": avg_duration,
                            "bytes_per_sec": avg_bytes_per_sec,
                            "chars_per_sec": avg_chars_per_sec,
                            "tokens_per_sec": avg_tokens_per_sec
                        },
                        "runs": runs
                    })
                    
    with open("results.json", "w") as f:
        json.dump(results, f, indent=2)
    print("Results saved to results.json")

if __name__ == "__main__":
    main()
