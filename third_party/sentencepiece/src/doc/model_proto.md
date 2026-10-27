# Model Metadata and Proto Extensions

SentencePiece model files (typically `.model`) are self-contained binary files. Under the hood, they are simply serialized **Protocol Buffer** messages of type `sentencepiece.ModelProto`. All training parameters and options used to train the model are encoded directly inside this file (within `TrainerSpec`), making the model fully self-contained.

Because they are standard protobuf messages, you can easily read and extend them using standard protobuf tools in Python or C++. This is useful for:
*   Inspecting model configurations and training parameters programmatically.
*   Embedding custom metadata (e.g., dataset version, author info) directly inside the model file via extensions.

---

## 1. Reading Standard Metadata (Python)

You can use the auto-generated Python protobuf bindings (`sentencepiece_model_pb2`) to parse the model file.

First, ensure you have `protobuf` installed. The `sentencepiece` Python package includes `sentencepiece_model_pb2` for convenience.

```python
import sentencepiece as spm
import sentencepiece_model_pb2 as spm_pb2

# Load the serialized proto bytes from a file
with open("path/to/model.model", "rb") as f:
    proto_bytes = f.read()

# Parse the proto
model_proto = spm_pb2.ModelProto()
model_proto.ParseFromString(proto_bytes)

# Inspect standard fields
print(f"Vocab size: {model_proto.trainer_spec.vocab_size}")
print(f"Model type: {spm_pb2.TrainerSpec.ModelType.Name(model_proto.trainer_spec.model_type)}")
print(f"Character coverage: {model_proto.trainer_spec.character_coverage}")

# You can also extract the serialized proto directly from a loaded processor:
sp = spm.SentencePieceProcessor(model_file="path/to/model.model")
model_proto_2 = spm_pb2.ModelProto()
model_proto_2.ParseFromString(sp.serialized_model_proto())

# Dump the training parameters (TrainerSpec) to a human-readable text format
from google.protobuf import text_format
print(text_format.MessageToString(model_proto.trainer_spec))
```

---

## 2. Storing Custom Metadata via Proto Extensions

All major messages in `sentencepiece_model.proto` have defined **extension ranges** (fields 200 to max) to allow third-party developers to embed custom metadata.

Supported messages for extensions:
*   `ModelProto` (global model level)
*   `TrainerSpec` (training configuration level)
*   `NormalizerSpec` (normalization configuration level)
*   `ModelProto.SentencePiece` (individual token level)

### Step 1: Define your extension

Create a `.proto` file (e.g., `my_metadata.proto`) that extends `sentencepiece.ModelProto`:

```protobuf
syntax = "proto2";

import "sentencepiece_model.proto"; // You may need to copy this from sentencepiece repository

package my_project;

extend sentencepiece.ModelProto {
  optional string model_author = 200;
  optional string training_dataset_version = 201;
}
```

### Step 2: Compile your proto

Compile it using `protoc` to generate Python bindings:
```bash
protoc --python_out=. my_metadata.proto
```
This will generate `my_metadata_pb2.py`.

### Step 3: Read/Write Custom Metadata in Python

Now you can import your extension and use it to read/write metadata:

```python
import sentencepiece as spm
import sentencepiece_model_pb2 as spm_pb2
import my_metadata_pb2

# 1. Load model and write custom metadata using extensions
model_proto = spm_pb2.ModelProto()
with open("m.model", "rb") as f:
    model_proto.ParseFromString(f.read())

model_proto.Extensions[my_metadata_pb2.model_author] = "John Doe"
model_proto.Extensions[my_metadata_pb2.training_dataset_version] = "v2.1-clean"

# Save modified model back to disk
with open("m.model", "wb") as f:
    f.write(model_proto.SerializeToString())

# 2. Load the processor (either from in-memory serialized proto or from file)
# The processor preserves custom extension fields in its serialized proto.

# Option A: Load directly from in-memory proto instance (automatically serialized)
sp_mem = spm.SentencePieceProcessor(model_proto=model_proto)

# Option B: Load from the saved file later
sp_file = spm.SentencePieceProcessor(model_file="m.model")

# 3. Read metadata back from the processor instance
model_proto_loaded = spm_pb2.ModelProto()
model_proto_loaded.ParseFromString(sp_mem.serialized_model_proto()) # or sp_file.serialized_model_proto()

print(model_proto_loaded.Extensions[my_metadata_pb2.model_author])  # Output: John Doe
print(model_proto_loaded.Extensions[my_metadata_pb2.training_dataset_version])  # Output: v2.1-clean
```

The C++ `SentencePieceProcessor` (and the standard Python wrapper) will ignore these custom extension fields when loading the model for tokenization, ensuring full backward compatibility.
