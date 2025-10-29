#include "ranking_features.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <set>
#include <cstring>


// Keyboard coordinate mapping - matches Python keyboard.py
std::map<char, std::pair<float, float>> keyboard_coord;

void init_keyboard_coords() {
    const char* lines_down[] = {
        "&é\"'(-è_çà)=",
        "azertyuiop^$",
        "qsdfghjklmù*",
        "<wxcvbn,;:!"
    };

    const float SIZE = 20.0f;

    for (int l = 0; l < 4; ++l) {
        const char* line = lines_down[l];
        int len = strlen(line);
        for (int i = 0; i < len; ++i) {
            float x = (1.0f * i + fmod(0.5f * l, 1.5f)) * SIZE;
            float y = 90.0f - SIZE * l;
            keyboard_coord[line[i]] = std::make_pair(x, y);
        }
    }

    // Space bar
    keyboard_coord[' '] = std::make_pair(100.0f, 90.0f - SIZE * 4);
}

// DTW distance computation for multivariate sequences
// Optimized version using two rows instead of full matrix to save memory
float dtw_multivariate(const std::vector<std::pair<float, float>>& A,
                       const std::vector<std::pair<float, float>>& B,
                       int window) {
    int n = A.size();
    int m = B.size();

    if (n == 0 || m == 0) return 0.0f;

    if (window < 0) {
        window = std::max(n, m);
    }
    window = std::max(window, abs(n - m));

    const float INF = 1e12f;

    // Use only two rows to save memory
    std::vector<float> prev_row(m + 1, INF);
    std::vector<float> curr_row(m + 1, INF);
    prev_row[0] = 0.0f;

    for (int i = 1; i <= n; ++i) {
        curr_row[0] = INF;
        int jstart = std::max(1, i - window);
        int jend = std::min(m, i + window);

        for (int j = jstart; j <= jend; ++j) {
            // Euclidean distance in 2D
            float dx = A[i-1].first - B[j-1].first;
            float dy = A[i-1].second - B[j-1].second;
            float cost = std::sqrt(dx*dx + dy*dy);

            float min_val = std::min(prev_row[j], curr_row[j-1]);
            min_val = std::min(min_val, prev_row[j-1]);
            curr_row[j] = cost + min_val;
        }

        std::swap(prev_row, curr_row);
    }

    return prev_row[m];
}

// Get the ideal keyboard path for a word
std::vector<std::pair<float, float>> get_word_path(const std::string& word) {
    std::vector<std::pair<float, float>> path;

    for (char c : word) {
        auto it = keyboard_coord.find(c);
        if (it != keyboard_coord.end()) {
            path.push_back(it->second);
        }
    }

    return path;
}

// Forward declaration - implemented in ranking_features_helper.cpp
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
);

// Compute all features for candidates given the raw inputs
std::vector<CandidateFeatures> compute_all_features(
    const std::vector<std::pair<float, float>>& swipe_path,
    const std::map<faiss::idx_t, float>& faiss_results,
    const std::map<int, std::vector<std::string>>* vocab,
    const std::vector<std::string>* vocabKeys,
    const std::vector<float>& lm_scores,
    const std::vector<std::string>& candidate_words
) {
    // First pass: collect all candidate data for normalization
    struct TempCandidateData {
        std::string word;
        float lm_score;
        float faiss_distance;
        int faiss_rank;
        float dtw_raw;
        float dtw_normalized_by_max;
        float dtw_normalized_by_min;
        float dtw_normalized_by_sum;
        int len_swipe;
        int len_word;
        float path_length_ratio;
        int word_length;
    };

    std::vector<TempCandidateData> temp_candidates;
    std::set<std::string> seen_words;

    int len_swipe = swipe_path.size();
    int faiss_rank = 1;
    size_t lm_idx = 0;

    for (const auto& faiss_pair : faiss_results) {
        int vocab_idx = faiss_pair.first;
        float faiss_distance = faiss_pair.second;

        const auto& words_for_idx = vocab->at(vocab_idx);

        for (const std::string& word : words_for_idx) {
            if (seen_words.count(word) > 0) {
                continue;
            }
            seen_words.insert(word);

            // Get word path and compute DTW
            std::vector<std::pair<float, float>> word_path = get_word_path(vocabKeys->at(vocab_idx));
            int len_word = word_path.size();

            float dtw_raw = dtw_multivariate(swipe_path, word_path);

            int max_len = std::max(len_swipe, len_word);
            int min_len = std::min(len_swipe, len_word);
            int sum_len = len_swipe + len_word;

            float dtw_normalized_by_max = (max_len > 0) ? (dtw_raw / max_len) : 0.0f;
            float dtw_normalized_by_min = (min_len > 0) ? (dtw_raw / min_len) : 0.0f;
            float dtw_normalized_by_sum = (sum_len > 0) ? (dtw_raw / sum_len) : 0.0f;

            float path_length_ratio = (len_word > 0) ? ((float)len_swipe / len_word) : 0.0f;
            int word_length = word.length();

            float lm_score = (lm_idx < lm_scores.size()) ? lm_scores[lm_idx++] : 0.0f;

            TempCandidateData temp;
            temp.word = word;
            temp.lm_score = lm_score;
            temp.faiss_distance = faiss_distance;
            temp.faiss_rank = faiss_rank;
            temp.dtw_raw = dtw_raw;
            temp.dtw_normalized_by_max = dtw_normalized_by_max;
            temp.dtw_normalized_by_min = dtw_normalized_by_min;
            temp.dtw_normalized_by_sum = dtw_normalized_by_sum;
            temp.len_swipe = len_swipe;
            temp.len_word = len_word;
            temp.path_length_ratio = path_length_ratio;
            temp.word_length = word_length;

            temp_candidates.push_back(temp);
        }

        faiss_rank++;
    }

    // Sort by DTW to assign DTW ranks
    std::vector<size_t> dtw_indices(temp_candidates.size());
    for (size_t i = 0; i < dtw_indices.size(); ++i) {
        dtw_indices[i] = i;
    }
    std::sort(dtw_indices.begin(), dtw_indices.end(), [&](size_t a, size_t b) {
        return temp_candidates[a].dtw_normalized_by_max < temp_candidates[b].dtw_normalized_by_max;
    });

    std::vector<int> dtw_ranks(temp_candidates.size());
    for (size_t i = 0; i < dtw_indices.size(); ++i) {
        dtw_ranks[dtw_indices[i]] = i + 1;
    }

    // Extract arrays for normalization
    std::vector<float> lm_scores_all, faiss_distances_all, dtw_distances_all;
    for (const auto& cand : temp_candidates) {
        lm_scores_all.push_back(cand.lm_score);
        faiss_distances_all.push_back(cand.faiss_distance);
        dtw_distances_all.push_back(cand.dtw_normalized_by_max);
    }

    // Second pass: compute full features
    std::vector<CandidateFeatures> result;
    for (size_t i = 0; i < temp_candidates.size(); ++i) {
        const auto& temp = temp_candidates[i];

        CandidateFeatures features = compute_enhanced_features(
            temp.word,
            temp.lm_score,
            temp.faiss_distance,
            temp.faiss_rank,
            temp.dtw_normalized_by_max,
            dtw_ranks[i],
            lm_scores_all,
            faiss_distances_all,
            dtw_distances_all,
            temp.dtw_raw,
            temp.dtw_normalized_by_max,
            temp.dtw_normalized_by_min,
            temp.dtw_normalized_by_sum,
            temp.len_swipe,
            temp.len_word,
            temp.path_length_ratio,
            temp.word_length
        );

        result.push_back(features);
    }

    return result;
}
