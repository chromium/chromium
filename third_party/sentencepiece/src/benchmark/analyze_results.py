import json
import os
import sys
import pandas as pd

if not os.path.exists("results.json"):
    print("Error: 'results.json' not found.")
    print("Please run the benchmark first: 'python run_bench.py'")
    sys.exit(1)

with open("results.json", "r") as f:
    data = json.load(f)

if not data:
    print("Error: 'results.json' is empty.")
    sys.exit(1)

df = pd.DataFrame(data)

# Flatten best run metrics
df["best_bytes_per_sec"] = df["best"].apply(lambda x: x["bytes_per_sec"])
df["best_tokens_per_sec"] = df["best"].apply(lambda x: x["tokens_per_sec"])
df["best_duration"] = df["best"].apply(lambda x: x["duration"])

# We want to see: language, action, group, tokenizer_type, threads, best_bytes_per_sec (in MB/s)
df["best_mb_per_sec"] = df["best_bytes_per_sec"] / 1024 / 1024

pd.set_option('display.max_rows', None)
pd.set_option('display.max_columns', None)
pd.set_option('display.width', 1000)

for lang in df["language"].unique():
    for action in df["action"].unique():
        print(f"\n======================================")
        print(f" Language: {lang.upper()} | Action: {action.upper()} ")
        print(f"======================================")
        
        sub_df = df[(df["language"] == lang) & (df["action"] == action)]
        
        # Pivot to see threads as columns
        pivot_df = sub_df.pivot(index=["group", "tokenizer_type", "model_name"], columns="threads", values="best_mb_per_sec")
        print(pivot_df)
