# Assets Directory

This directory should contain the trained ML models and data files required for swipe prediction.

## Required Files

### Core ML Models

1. **swipe_encoder.onnx** (~10-50 MB)
   - Neural network that encodes swipe gesture paths into embeddings
   - Input: Sequence of (x, y) coordinates
   - Output: 128-dimensional embedding vector
   - Source: `HeyEyeTracker/assets/swipe_encoder.onnx`

2. **index.faiss** (~50-500 MB depending on vocabulary size)
   - FAISS vector similarity index for vocabulary lookup
   - Maps word embeddings to word IDs
   - Source: `HeyEyeTracker/assets/index.faiss`

3. **vocab.msgpck** (~1-10 MB)
   - MessagePack-encoded vocabulary file
   - Maps word IDs to actual words/strings
   - Source: `HeyEyeTracker/assets/vocab.msgpck`

### Optional Models

4. **lightgbm_ranker.txt** (~1-5 MB)
   - LightGBM ranking model
   - Ranks candidate words based on 70+ features
   - Source: Train using `HeyEyeTracker/training/07-train_LightGBM.py`

5. **kenlm_model.arpa** (size varies, typically 100MB - 2GB)
   - KenLM n-gram language model
   - Provides language model scores for word sequences
   - Train using KenLM tools on your corpus

## Copying Assets

From the HeyEyeTracker project:

```bash
# Copy from HeyEyeTracker to HeyEyeUnified
cp ../to_combined/HeyEyeTracker/assets/swipe_encoder.onnx .
cp ../to_combined/HeyEyeTracker/assets/index.faiss .
cp ../to_combined/HeyEyeTracker/assets/vocab.msgpck .
```

## File Formats

### swipe_encoder.onnx
- Format: ONNX (Open Neural Network Exchange)
- Architecture: Transformer-based sequence encoder
- Input shape: [batch_size, sequence_length, 2]
- Output shape: [batch_size, embedding_dim]

### index.faiss
- Format: FAISS binary index file
- Index type: IndexFlatL2 (Euclidean distance)
- Dimension: 128 (must match encoder output)
- Number of vectors: Depends on vocabulary size

### vocab.msgpck
- Format: MessagePack binary serialization
- Structure: Map<int, vector<string>>
  - Key: FAISS index ID
  - Value: List of word variants

## Generating Your Own Models

### 1. Swipe Encoder

Train using the data in `HeyEyeTracker/training/`:

```python
# See HeyEyeTracker/training/my_translator/ for training code
# The model should learn to encode swipe paths into embeddings
```

### 2. FAISS Index

```python
import faiss
import numpy as np

# Encode all vocabulary words
word_embeddings = encode_all_words(vocabulary)  # Shape: [vocab_size, 128]

# Create index
index = faiss.IndexFlatL2(128)
index.add(word_embeddings)

# Save
faiss.write_index(index, "index.faiss")
```

### 3. Vocabulary

```python
import msgpack

# Create vocabulary mapping
vocab = {
    0: ["hello", "helo", "hllo"],  # Word variants
    1: ["world", "wrld"],
    # ...
}

# Save
with open("vocab.msgpck", "wb") as f:
    msgpack.pack(vocab, f)
```

### 4. LightGBM Ranker

Use the training script:

```bash
cd ../to_combined/HeyEyeTracker/training
python 07-train_LightGBM.py
```

This requires:
- Training data with swipe paths and ground truth words
- Feature computation (DTW, LM scores, etc.)
- LightGBM Python package

## Verifying Assets

To verify your assets are correct:

1. Check file sizes match expected ranges
2. Test loading in Python:

```python
import onnxruntime as ort
import faiss
import msgpack

# Test ONNX
session = ort.InferenceSession("swipe_encoder.onnx")
print("ONNX inputs:", [i.name for i in session.get_inputs()])
print("ONNX outputs:", [o.name for o in session.get_outputs()])

# Test FAISS
index = faiss.read_index("index.faiss")
print(f"FAISS dimension: {index.d}, size: {index.ntotal}")

# Test vocabulary
with open("vocab.msgpck", "rb") as f:
    vocab = msgpack.unpack(f)
print(f"Vocabulary size: {len(vocab)}")
```

## Troubleshooting

### "File not found" errors
- Ensure assets directory is in the build output folder
- Check that paths in `TextInputEngine::initialize()` are correct

### "Dimension mismatch" errors
- FAISS index dimension must match swipe encoder output (typically 128)
- Rebuild index if encoder model changes

### "Invalid model format" errors
- Ensure ONNX model is saved with compatible opset version
- ONNX Runtime version must support the model's operators

## Storage and Distribution

Due to large file sizes, consider:
- Git LFS for version control
- Separate storage (cloud bucket, shared drive)
- Model compression techniques
- On-demand download during installation

## License and Attribution

These assets are derived from the HeyEyeTracker project. Ensure compliance with:
- Original project licenses
- Dataset licenses (for training data)
- Third-party model licenses (if using pre-trained components)
