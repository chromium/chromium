import os
from huggingface_hub import hf_hub_download

os.makedirs("models", exist_ok=True)

# 1. Download T5 (public)
try:
    print("Downloading T5 spiece.model...")
    sp_path = hf_hub_download(repo_id="t5-base", filename="spiece.model", local_dir="models")
    print(f"Downloaded to {sp_path}")
except Exception as e:
    print(f"Failed to download spiece.model: {e}")

try:
    print("Downloading T5 tokenizer.json...")
    hf_path = hf_hub_download(repo_id="t5-base", filename="tokenizer.json", local_dir="models")
    print(f"Downloaded to {hf_path}")
except Exception as e:
    print(f"Failed to download tokenizer.json: {e}")

# 2. Download Gemma 3 (gated)
os.makedirs("models/gemma3", exist_ok=True)
try:
    print("Downloading Gemma 3 tokenizer.model (for SentencePiece)...")
    sp_path = hf_hub_download(repo_id="google/gemma-3-4b", filename="tokenizer.model", local_dir="models/gemma3")
    print(f"Downloaded to {sp_path}")
except Exception as e:
    print(f"Failed to download Gemma 3 tokenizer.model: {e}")
    print("Note: google/gemma-3-4b is a gated model. You may need to run 'huggingface-cli login' or set HF_TOKEN env var.")

try:
    print("Downloading Gemma 3 tokenizer.json (for Hugging Face)...")
    hf_path = hf_hub_download(repo_id="google/gemma-3-4b", filename="tokenizer.json", local_dir="models/gemma3")
    print(f"Downloaded to {hf_path}")
except Exception as e:
    print(f"Failed to download Gemma 3 tokenizer.json: {e}")
