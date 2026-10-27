#!/bin/bash
set -e

# Get the directory of the script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Create venv if not exists
if [ ! -d "venv" ]; then
  python3 -m venv venv
fi

# Activate venv
source venv/bin/activate

INDEX_URL="https://pypi.org/simple"

# Upgrade pip
pip install --upgrade pip -i "$INDEX_URL"

# Install build requirements for sentencepiece python wrapper
pip install "setuptools>=61.0" wheel "pybind11>=2.12" -i "$INDEX_URL"

# Install other requirements
pip install build tiktoken tokenizers pandas datasets -i "$INDEX_URL"

# Build sentencepiece C++ from the parent directory (repo root)
SP_ROOT_DIR="$(cd .. && pwd)"
echo "Building SentencePiece from source at: $SP_ROOT_DIR"

cd "$SP_ROOT_DIR"
rm -rf build
mkdir -p build
cd build
cmake .. -DSPM_ENABLE_SHARED=OFF -DCMAKE_INSTALL_PREFIX=./root -DSPM_DISABLE_EMBEDDED_DATA=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON
make -j$(nproc) install
cd ..

# Build sentencepiece Python using --no-isolation
cd python
rm -rf build dist
python -m build --wheel --no-isolation

# Install the built wheel to venv
cd "$SCRIPT_DIR"
pip install "$SP_ROOT_DIR/python/dist"/sentencepiece*.whl

echo "Setup completed successfully!"
