#include "ranking_features.h"
#include <cmath>
#include <algorithm>

// Helper function to compute enhanced features for a single candidate
CandidateFeatures compute_enhanced_features(
    const std::string& word,
    float lm_score,
    float faiss_distance,
    int faiss_rank,
    float dtw_distance,
    int dtw_rank,
    const std::vector<float>& lm_scores_all,
    const std::vector<float>& faiss_distances_all,
    const std::vector<float>& dtw_distances_all,
    float dtw_raw,
    float dtw_normalized_by_max,
    float dtw_normalized_by_min,
    float dtw_normalized_by_sum,
    int len_swipe,
    int len_word,
    float path_length_ratio,
    int word_length
) {
    CandidateFeatures features;
    features.word = word;

    const float epsilon = 1e-6f;

    // Core features
    features.lm_score = lm_score;
    features.faiss_distance = faiss_distance;
    features.faiss_rank = faiss_rank;
    features.dtw_distance = dtw_distance;
    features.dtw_rank = dtw_rank;

    // Min-max normalization
    float lm_min = *std::min_element(lm_scores_all.begin(), lm_scores_all.end());
    float lm_max = *std::max_element(lm_scores_all.begin(), lm_scores_all.end());
    features.lm_normalized = (lm_score - lm_min) / (lm_max - lm_min + epsilon);

    float faiss_min = *std::min_element(faiss_distances_all.begin(), faiss_distances_all.end());
    float faiss_max = *std::max_element(faiss_distances_all.begin(), faiss_distances_all.end());
    features.faiss_normalized = (faiss_distance - faiss_min) / (faiss_max - faiss_min + epsilon);

    float dtw_min = *std::min_element(dtw_distances_all.begin(), dtw_distances_all.end());
    float dtw_max = *std::max_element(dtw_distances_all.begin(), dtw_distances_all.end());
    features.dtw_normalized = (dtw_distance - dtw_min) / (dtw_max - dtw_min + epsilon);

    // Z-score normalization
    float lm_sum = 0.0f, faiss_sum = 0.0f, dtw_sum = 0.0f;
    for (float v : lm_scores_all) lm_sum += v;
    for (float v : faiss_distances_all) faiss_sum += v;
    for (float v : dtw_distances_all) dtw_sum += v;

    float lm_mean = lm_sum / lm_scores_all.size();
    float faiss_mean = faiss_sum / faiss_distances_all.size();
    float dtw_mean = dtw_sum / dtw_distances_all.size();

    float lm_var = 0.0f, faiss_var = 0.0f, dtw_var = 0.0f;
    for (float v : lm_scores_all) lm_var += (v - lm_mean) * (v - lm_mean);
    for (float v : faiss_distances_all) faiss_var += (v - faiss_mean) * (v - faiss_mean);
    for (float v : dtw_distances_all) dtw_var += (v - dtw_mean) * (v - dtw_mean);

    float lm_std = std::sqrt(lm_var / lm_scores_all.size());
    float faiss_std = std::sqrt(faiss_var / faiss_distances_all.size());
    float dtw_std = std::sqrt(dtw_var / dtw_distances_all.size());

    features.lm_zscore = (lm_score - lm_mean) / (lm_std + epsilon);
    features.faiss_zscore = (faiss_distance - faiss_mean) / (faiss_std + epsilon);
    features.dtw_zscore = (dtw_distance - dtw_mean) / (dtw_std + epsilon);

    // Gap to best
    features.lm_gap_to_best = lm_max - lm_score;
    features.faiss_gap_to_best = faiss_distance - faiss_min;
    features.dtw_gap_to_best = dtw_distance - dtw_min;

    // Percentile features
    int lm_count = 0, faiss_count = 0, dtw_count = 0;
    for (float v : lm_scores_all) if (v < lm_score) lm_count++;
    for (float v : faiss_distances_all) if (v > faiss_distance) faiss_count++;
    for (float v : dtw_distances_all) if (v > dtw_distance) dtw_count++;

    features.lm_percentile = (float)lm_count / lm_scores_all.size();
    features.faiss_percentile = (float)faiss_count / faiss_distances_all.size();
    features.dtw_percentile = (float)dtw_count / dtw_distances_all.size();

    // Rank agreement features
    features.rank_agreement = std::abs(faiss_rank - dtw_rank);
    features.min_rank = std::min(faiss_rank, dtw_rank);
    features.is_top_faiss = (faiss_rank == 1) ? 1.0f : 0.0f;
    features.is_top_dtw = (dtw_rank == 1) ? 1.0f : 0.0f;
    features.is_top_in_both = (faiss_rank == 1 && dtw_rank == 1) ? 1.0f : 0.0f;

    // Log and inverse features
    features.log_faiss_distance = std::log(faiss_distance + epsilon);
    features.log_dtw_distance = std::log(dtw_distance + epsilon);
    features.inv_faiss_distance = 1.0f / (faiss_distance + epsilon);
    features.inv_dtw_distance = 1.0f / (dtw_distance + epsilon);

    // Rank reciprocals
    features.faiss_rank_reciprocal = 1.0f / faiss_rank;
    features.dtw_rank_reciprocal = 1.0f / dtw_rank;

    // Interaction features
    features.lm_faiss_interaction = lm_score * faiss_distance;
    features.lm_dtw_interaction = lm_score * dtw_distance;
    features.faiss_dtw_interaction = faiss_distance * dtw_distance;

    // Raw DTW metrics
    features.dtw_raw = dtw_raw;
    features.dtw_normalized_by_max = dtw_normalized_by_max;
    features.dtw_normalized_by_min = dtw_normalized_by_min;
    features.dtw_normalized_by_sum = dtw_normalized_by_sum;

    // Path metrics
    features.len_swipe = len_swipe;
    features.len_word = len_word;
    features.path_length_ratio = path_length_ratio;
    features.word_length = word_length;

    return features;
}
