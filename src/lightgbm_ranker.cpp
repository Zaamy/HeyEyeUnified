#include "lightgbm_ranker.h"
#include "ranking_features.h"

#ifdef USE_LIGHTGBM
#include <LightGBM/c_api.h>
#endif

#include <algorithm>
#include <stdexcept>
#include <cstring>

// Pimpl implementation class - all heavy includes are HERE, not in header
class LightGBMRanker::Impl {
public:
#ifdef USE_LIGHTGBM
    void* booster;
#endif
    bool is_loaded;

    Impl() :
#ifdef USE_LIGHTGBM
        booster(nullptr),
#endif
        is_loaded(false) {}

    ~Impl() {
#ifdef USE_LIGHTGBM
        if (booster != nullptr) {
            LGBM_BoosterFree(booster);
        }
#endif
    }

    int get_num_features() const {
        return 39;  // Match feature_list.json
    }

    std::vector<double> features_to_array(const CandidateFeatures& features) {
        // CRITICAL: Order must match feature_list.json from training EXACTLY!
        std::vector<double> arr;
        arr.reserve(get_num_features());

        // Feature order from feature_list.json:
        arr.push_back(features.dtw_raw);                    // 0
        arr.push_back(features.dtw_normalized_by_max);      // 1
        arr.push_back(features.dtw_normalized_by_min);      // 2
        arr.push_back(features.dtw_normalized_by_sum);      // 3
        arr.push_back(features.len_swipe);                  // 4
        arr.push_back(features.len_word);                   // 5
        arr.push_back(features.path_length_ratio);          // 6
        arr.push_back(features.word_length);                // 7
        arr.push_back(features.lm_score);                   // 8
        arr.push_back(features.faiss_distance);             // 9
        arr.push_back(features.faiss_rank);                 // 10
        arr.push_back(features.dtw_distance);               // 11
        arr.push_back(features.dtw_rank);                   // 12
        arr.push_back(features.lm_normalized);              // 13
        arr.push_back(features.faiss_normalized);           // 14
        arr.push_back(features.dtw_normalized);             // 15
        arr.push_back(features.lm_zscore);                  // 16
        arr.push_back(features.faiss_zscore);               // 17
        arr.push_back(features.dtw_zscore);                 // 18
        arr.push_back(features.lm_gap_to_best);             // 19
        arr.push_back(features.faiss_gap_to_best);          // 20
        arr.push_back(features.dtw_gap_to_best);            // 21
        arr.push_back(features.lm_percentile);              // 22
        arr.push_back(features.faiss_percentile);           // 23
        arr.push_back(features.dtw_percentile);             // 24
        arr.push_back(features.rank_agreement);             // 25
        arr.push_back(features.min_rank);                   // 26
        arr.push_back(features.is_top_faiss);               // 27
        arr.push_back(features.is_top_dtw);                 // 28
        arr.push_back(features.is_top_in_both);             // 29
        arr.push_back(features.log_faiss_distance);         // 30
        arr.push_back(features.log_dtw_distance);           // 31
        arr.push_back(features.inv_faiss_distance);         // 32
        arr.push_back(features.inv_dtw_distance);           // 33
        arr.push_back(features.faiss_rank_reciprocal);      // 34
        arr.push_back(features.dtw_rank_reciprocal);        // 35
        arr.push_back(features.lm_faiss_interaction);       // 36
        arr.push_back(features.lm_dtw_interaction);         // 37
        arr.push_back(features.faiss_dtw_interaction);      // 38

        return arr;
    }
};

// Public interface implementation
LightGBMRanker::LightGBMRanker() : pimpl(new Impl()) {
}

LightGBMRanker::~LightGBMRanker() {
    delete pimpl;
}

bool LightGBMRanker::load_model(const std::string& model_path) {
#ifdef USE_LIGHTGBM
    int num_iterations = 0;
    int result = LGBM_BoosterCreateFromModelfile(model_path.c_str(), &num_iterations,
                                                   reinterpret_cast<BoosterHandle*>(&pimpl->booster));

    if (result != 0) {
        pimpl->is_loaded = false;
        return false;
    }

    pimpl->is_loaded = true;
    return true;
#else
    (void)model_path;
    pimpl->is_loaded = false;
    return false;
#endif
}

bool LightGBMRanker::is_model_loaded() const {
    return pimpl->is_loaded;
}

std::vector<float> LightGBMRanker::predict(const std::vector<CandidateFeatures>& candidates) {
#ifdef USE_LIGHTGBM
    if (!pimpl->is_loaded || pimpl->booster == nullptr) {
        throw std::runtime_error("LightGBM model not loaded");
    }

    if (candidates.empty()) {
        return std::vector<float>();
    }

    int num_features = pimpl->get_num_features();
    int num_candidates = candidates.size();

    // Prepare data in row-major format
    std::vector<double> data;
    data.reserve(num_candidates * num_features);

    for (const auto& cand : candidates) {
        std::vector<double> feature_array = pimpl->features_to_array(cand);
        data.insert(data.end(), feature_array.begin(), feature_array.end());
    }

    // Predict
    int64_t out_len;
    std::vector<double> out_result(num_candidates);

    int result = LGBM_BoosterPredictForMat(
        pimpl->booster,
        data.data(),
        C_API_DTYPE_FLOAT64,  // data type
        num_candidates,       // nrow
        num_features,         // ncol
        1,                    // is_row_major
        C_API_PREDICT_NORMAL, // predict_type
        0,                    // start_iteration (0 means from start)
        -1,                   // num_iteration (-1 means best iteration)
        "",                   // parameter
        &out_len,
        out_result.data()
    );

    if (result != 0) {
        throw std::runtime_error("LightGBM prediction failed");
    }

    // Convert double to float
    std::vector<float> scores(out_result.begin(), out_result.end());
    return scores;
#else
    (void)candidates;
    throw std::runtime_error("LightGBM support not compiled");
#endif
}

std::vector<size_t> LightGBMRanker::rank_candidates(const std::vector<CandidateFeatures>& candidates) {
    std::vector<float> scores = predict(candidates);

    // Create indices
    std::vector<size_t> indices(scores.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    // Sort indices by score (descending - higher score is better)
    std::sort(indices.begin(), indices.end(), [&scores](size_t a, size_t b) {
        return scores[a] > scores[b];
    });

    return indices;
}

std::string LightGBMRanker::get_best_candidate(const std::vector<CandidateFeatures>& candidates) {
    if (candidates.empty()) {
        return "";
    }

    std::vector<size_t> ranked = rank_candidates(candidates);
    return candidates[ranked[0]].word;
}
