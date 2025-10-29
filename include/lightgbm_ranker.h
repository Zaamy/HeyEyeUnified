#ifndef LIGHTGBM_RANKER_H
#define LIGHTGBM_RANKER_H

#include <string>
#include <vector>

// Forward declaration - no includes needed!
struct CandidateFeatures;

// Pimpl pattern to completely hide LightGBM implementation
class LightGBMRanker {
private:
    class Impl;  // Forward declaration of implementation
    Impl* pimpl; // Pointer to implementation

public:
    LightGBMRanker();
    ~LightGBMRanker();

    // Load the trained LightGBM model from file
    bool load_model(const std::string& model_path);

    // Predict scores for a vector of candidates
    // Returns a vector of scores (same length as candidates)
    std::vector<float> predict(const std::vector<CandidateFeatures>& candidates);

    // Convenience method: rank candidates and return the best word
    std::string get_best_candidate(const std::vector<CandidateFeatures>& candidates);

    // Rank candidates and return sorted indices (best first)
    std::vector<size_t> rank_candidates(const std::vector<CandidateFeatures>& candidates);

    // Check if model is loaded
    bool is_model_loaded() const;

    // Prevent copying
    LightGBMRanker(const LightGBMRanker&) = delete;
    LightGBMRanker& operator=(const LightGBMRanker&) = delete;
};

#endif // LIGHTGBM_RANKER_H
