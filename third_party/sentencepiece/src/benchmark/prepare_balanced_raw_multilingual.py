from datasets import load_dataset
import os

def main():
    print("Loading FLORES-200 datasets ...")
    ds_zh = load_dataset('haoranxu/FLORES-200', 'en-zh')
    ds_ja = load_dataset('haoranxu/FLORES-200', 'en-ja')
    ds_th = load_dataset('haoranxu/FLORES-200', 'en-th')
    
    split = 'test'
    
    print("Extracting sentences ...")
    en_sentences = [row['en-zh']['en'].strip() for row in ds_zh[split]]
    zh_sentences = [row['en-zh']['zh'].strip() for row in ds_zh[split]]
    ja_sentences = [row['en-ja']['ja'].strip() for row in ds_ja[split]]
    th_sentences = [row['en-th']['th'].strip() for row in ds_th[split]]
    
    N = len(en_sentences)
    assert len(zh_sentences) == N
    assert len(ja_sentences) == N
    assert len(th_sentences) == N
    
    print(f"Loaded {N} parallel sentences for EN, ZH, JA, TH.")
    
    mixed_lines = []
    for i in range(N):
        mixed_lines.append(en_sentences[i])
        mixed_lines.append(zh_sentences[i])
        mixed_lines.append(ja_sentences[i])
        mixed_lines.append(th_sentences[i])
        
    temp_content = "\n".join(mixed_lines)
    temp_bytes = len(temp_content.encode("utf-8"))
    print(f"One iteration size: {temp_bytes / 1024 / 1024:.2f} MB ({temp_bytes} bytes)")
    
    target_bytes = 11 * 1024 * 1024
    replications = max(1, round(target_bytes / temp_bytes))
    print(f"Replicating {replications} times to target ~11MB ...")
    
    final_lines = []
    for _ in range(replications):
        final_lines.extend(mixed_lines)
        
    final_content = "\n".join(final_lines)
    final_bytes = len(final_content.encode("utf-8"))
    
    output_path = "data/multilingual_raw_balanced.txt"
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(final_content)
        
    print(f"Saved balanced raw multilingual dataset to {output_path}")
    print(f"Final size: {final_bytes / 1024 / 1024:.2f} MB ({final_bytes} bytes)")
    print(f"Total lines: {len(final_lines)}")

if __name__ == "__main__":
    main()
