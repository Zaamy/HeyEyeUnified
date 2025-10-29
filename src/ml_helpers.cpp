#include "ml_helpers.h"

#ifdef USE_FAISS
#include "faiss/IndexFlat.h"
#include <faiss/index_io.h>
#endif

#include <fstream>
#include <iostream>

// msgpack-c is header-only, so we can include it directly
#ifdef USE_MSGPACK
#include <msgpack.hpp>
#endif

std::string normalize_string(const std::string& s) {
    return s;
}

std::pair<std::map<int, std::vector<std::string>>*, std::vector<std::string>*>
load_vocab(const std::string& filepath) {
#ifdef USE_MSGPACK
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return {nullptr, nullptr};
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    msgpack::object_handle oh = msgpack::unpack(buffer.data(), buffer.size());

    std::map<int, std::vector<std::string>>* deserialized_vocab = new std::map<int, std::vector<std::string>>;
    oh.get().convert(*deserialized_vocab);

    // Build vocabKeys by taking the first element from each vocab entry
    std::vector<std::string>* vocabKeys = new std::vector<std::string>(deserialized_vocab->size());

    for (const auto& pair : *deserialized_vocab) {
        int idx = pair.first;
        const std::vector<std::string>& words = pair.second;
        if (!words.empty() && idx >= 0 && idx < (int)vocabKeys->size()) {
            (*vocabKeys)[idx] = words[0];  // Take the first word as canonical form
        }
    }

    return {deserialized_vocab, vocabKeys};
#else
    (void)filepath;
    std::cerr << "msgpack support not compiled" << std::endl;
    return {nullptr, nullptr};
#endif
}

faiss::Index* load_faiss_index(const std::string& filepath) {
#ifdef USE_FAISS
    faiss::Index* loadedIndex = faiss::read_index(filepath.c_str());

    // Optional: Use dynamic_cast if you know the type and want to cast safely
    auto* flatIndex = dynamic_cast<faiss::IndexFlatIP*>(loadedIndex);
    if (flatIndex) {
        std::cout << "Index loaded with " << flatIndex->ntotal << " vectors in it." << std::endl;
        return flatIndex;
    } else {
        return nullptr;
    }
#else
    (void)filepath;
    std::cerr << "FAISS support not compiled" << std::endl;
    return nullptr;
#endif
}

std::map<faiss::idx_t, float> search_faiss_index(
    std::vector<float>* query,
    faiss::Index* index,
    int k_nearest
) {
#ifdef USE_FAISS
    // Search k vectors
    std::vector<faiss::idx_t> indices(k_nearest);
    std::vector<float> distances(k_nearest);
    index->search(1, query->data(), k_nearest, distances.data(), indices.data());

    std::map<faiss::idx_t, float> results;
    for (int i = 0; i < k_nearest; i++) {
        if (indices[i] != -1)
            results[indices[i]] = distances[i];
    }

    return results;
#else
    (void)query;
    (void)index;
    (void)k_nearest;
    return std::map<faiss::idx_t, float>();
#endif
}
