#ifndef RANKING_FEATURES_H
#define RANKING_FEATURES_H

#include <vector>
#include <map>
#include <string>

// Structure to hold a candidate word with all its computed features
struct CandidateFeatures {
    std::string word;

    // Core features
    float lm_score;
    float faiss_distance;
    int faiss_rank;
    float dtw_distance;
    int dtw_rank;

    // Normalized features (min-max)
    float lm_normalized;
    float faiss_normalized;
    float dtw_normalized;

    // Normalized features (z-score)
    float lm_zscore;
    float faiss_zscore;
    float dtw_zscore;

    // Gap features (distance from best)
    float lm_gap_to_best;
    float faiss_gap_to_best;
    float dtw_gap_to_best;

    // Percentile features
    float lm_percentile;
    float faiss_percentile;
    float dtw_percentile;

    // Rank agreement features
    int rank_agreement;
    int min_rank;
    float is_top_faiss;
    float is_top_dtw;
    float is_top_in_both;

    // Log and inverse features
    float log_faiss_distance;
    float log_dtw_distance;
    float inv_faiss_distance;
    float inv_dtw_distance;

    // Rank reciprocals
    float faiss_rank_reciprocal;
    float dtw_rank_reciprocal;

    // Interaction features
    float lm_faiss_interaction;
    float lm_dtw_interaction;
    float faiss_dtw_interaction;

    // Raw DTW metrics
    float dtw_raw;
    float dtw_normalized_by_max;
    float dtw_normalized_by_min;
    float dtw_normalized_by_sum;

    // Path metrics
    int len_swipe;
    int len_word;
    float path_length_ratio;
    int word_length;
};

// Keyboard coordinate mapping - matches Python keyboard.py
extern std::map<char, std::pair<float, float>> keyboard_coord;

// Initialize keyboard coordinates
void init_keyboard_coords();

// DTW distance computation for multivariate sequences
float dtw_multivariate(const std::vector<std::pair<float, float>>& A,
                       const std::vector<std::pair<float, float>>& B,
                       int window = -1);

// Get the ideal keyboard path for a word
std::vector<std::pair<float, float>> get_word_path(const std::string& word);

// Forward declare faiss::idx_t to avoid including FAISS headers
namespace faiss {
    typedef long long idx_t;
}

// Compute all features for candidates given the raw inputs
std::vector<CandidateFeatures> compute_all_features(
    const std::vector<std::pair<float, float>>& swipe_path,
    const std::map<faiss::idx_t, float>& faiss_results,  // vocab_idx -> distance
    const std::map<int, std::vector<std::string>>* vocab,
    const std::vector<std::string>* vocabKeys,
    const std::vector<float>& lm_scores,  // one score per candidate word
    const std::vector<std::string>& candidate_words
);

#endif // RANKING_FEATURES_H
