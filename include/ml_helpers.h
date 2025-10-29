#ifndef ML_HELPERS_H
#define ML_HELPERS_H

#include <string>
#include <map>
#include <vector>
#include <utility>

// Forward declarations to avoid header dependencies
namespace faiss {
    class Index;
    typedef long long idx_t;
}

// Vocabulary loading from MessagePack format
// Returns pair of: (vocab map, vocab keys array)
std::pair<std::map<int, std::vector<std::string>>*, std::vector<std::string>*>
load_vocab(const std::string& filepath);

// FAISS index loading
faiss::Index* load_faiss_index(const std::string& filepath);

// FAISS search
std::map<faiss::idx_t, float> search_faiss_index(
    std::vector<float>* query,
    faiss::Index* index,
    int k_nearest
);

// String normalization (currently identity function, kept for compatibility)
std::string normalize_string(const std::string& s);

#endif // ML_HELPERS_H
