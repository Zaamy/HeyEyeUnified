#include "textinputengine.h"
#include "lightgbm_ranker.h"
#include "ranking_features.h"
#include "ml_helpers.h"
#include <wx/filename.h>
#include <set>

// Include ML library headers when enabled
#ifdef USE_ONNX
#include "onnxruntime_cxx_api.h"
#endif

#ifdef USE_FAISS
#include "faiss/IndexFlat.h"
#include <faiss/index_io.h>
#endif

#ifdef USE_KENLM
#include "lm/model.hh"
#endif

#define MAX_LENGTH_SWIPE 520

TextInputEngine::TextInputEngine()
    : m_initialized(false)
    , OnTextChanged(nullptr)
    , OnPredictionReady(nullptr)
    , OnTopKPredictionsReady(nullptr)
    , m_swipeEncoder(nullptr)
    , m_memoryInfo(nullptr)
    , m_faissIndex(nullptr)
    , m_kenLM(nullptr)
    , m_lightGBM(nullptr)
    , m_vocab(nullptr)
    , m_vocabKeys(nullptr)
{
}

TextInputEngine::~TextInputEngine()
{
    // Clean up ML components
#ifdef USE_ONNX
    delete m_swipeEncoder;
    delete m_memoryInfo;
#endif

#ifdef USE_FAISS
    delete m_faissIndex;
#endif

#ifdef USE_KENLM
    if (m_kenLM) {
        delete static_cast<lm::ngram::Model*>(m_kenLM);
    }
#endif

    delete m_lightGBM;
    delete m_vocab;
    delete m_vocabKeys;
}

bool TextInputEngine::Initialize(const wxString& assetsPath)
{
    wxLogMessage("TextInputEngine: Initializing with assets path: %s", assetsPath);

    // Initialize keyboard coordinates for DTW
    init_keyboard_coords();

    // Initialize ONNX Runtime for swipe encoding
    wxString swipeModelPath = assetsPath + wxT("/swipe_encoder.onnx");
    if (!LoadSwipeEncoder(swipeModelPath)) {
        wxLogWarning("Failed to load swipe encoder");
        return false;
    }

    // Load vocabulary
    wxString vocabPath = assetsPath + wxT("/vocab.msgpck");
    if (!LoadVocabulary(vocabPath)) {
        wxLogWarning("Failed to load vocabulary");
        return false;
    }

    // Load FAISS index
    wxString indexPath = assetsPath + wxT("/index.faiss");
    if (!LoadFaissIndex(indexPath)) {
        wxLogWarning("Failed to load FAISS index");
        return false;
    }

    // Load KenLM
    wxString kenlmPath = assetsPath + wxT("/kenlm_model.arpa");
    if (wxFileName::Exists(kenlmPath) && !LoadKenLM(kenlmPath)) {
        wxLogWarning("Failed to load KenLM");
        return false;
    }

    // Load LightGBM (optional)
    wxString lgbmPath = assetsPath + wxT("/lightgbm_ranker.txt");
    if (wxFileName::Exists(lgbmPath)) {
        if (!LoadLightGBM(lgbmPath)) {
            wxLogWarning("Failed to load LightGBM model");
            // Don't fail initialization - LightGBM is optional
        }
    } else {
        wxLogMessage("LightGBM model not found (optional): %s", lgbmPath);
        wxLogMessage("Will use fallback scoring for word prediction");
    }

    m_initialized = true;
    wxLogMessage("TextInputEngine: Initialization complete");
    return true;
}

void TextInputEngine::AppendCharacter(wxChar c)
{
    m_currentText << c;

    // Update word history when space is added (for consistency)
    if (c == wxT(' ')) {
        // Extract the last completed word
        wxString text_without_trailing_space = m_currentText;
        if (text_without_trailing_space.EndsWith(wxT(" "))) {
            text_without_trailing_space.RemoveLast();
        }

        int lastSpace = text_without_trailing_space.Find(' ', true);  // Find from end
        wxString lastWord;
        if (lastSpace != wxNOT_FOUND) {
            lastWord = text_without_trailing_space.Mid(lastSpace + 1);
        } else {
            lastWord = text_without_trailing_space;
        }

        if (!lastWord.IsEmpty()) {
            m_wordHistory.push_back(lastWord);
        }
    }

    if (OnTextChanged) {
        OnTextChanged(m_currentText);
    }
}

void TextInputEngine::AppendText(const wxString& text)
{
    m_currentText << text;

    // Update word history
    if (text.Contains(wxT(" "))) {
        wxArrayString words = wxSplit(text, ' ', '\0');
        for (const wxString& word : words) {
            if (!word.IsEmpty()) {
                m_wordHistory.push_back(word);
            }
        }
    }

    if (OnTextChanged) {
        OnTextChanged(m_currentText);
    }
}

void TextInputEngine::DeleteLastCharacter()
{
    if (!m_currentText.IsEmpty()) {
        m_currentText.RemoveLast();
        if (OnTextChanged) {
            OnTextChanged(m_currentText);
        }
    }
}

void TextInputEngine::DeleteLastWord()
{
    // Find last space or beginning
    int lastSpace = m_currentText.Find(' ', true);  // true = from end

    if (lastSpace != wxNOT_FOUND) {
        m_currentText = m_currentText.Left(lastSpace + 1);
    } else {
        m_currentText.Clear();
    }

    if (!m_wordHistory.empty()) {
        m_wordHistory.pop_back();
    }

    if (OnTextChanged) {
        OnTextChanged(m_currentText);
    }
}

void TextInputEngine::Clear()
{
    m_currentText.Clear();
    m_wordHistory.clear();
    if (OnTextChanged) {
        OnTextChanged(m_currentText);
    }
}

wxString TextInputEngine::PredictFromSwipe(const std::vector<std::pair<float, float>>& swipePath)
{
    if (!m_initialized) {
        wxLogWarning("TextInputEngine not initialized");
        return wxEmptyString;
    }

    if (swipePath.empty()) {
        wxLogWarning("Empty swipe path");
        return wxEmptyString;
    }

    // Step 1: Encode swipe path to embedding
    std::vector<float> embedding = EncodeSwipe(swipePath);

    // Step 2: Search vocabulary for candidates
    std::map<faiss::idx_t, float> candidates = SearchVocabulary(embedding, 100);

    // Step 3: Rank candidates using LightGBM
    std::string prediction = RankCandidates(swipePath, candidates);

    wxString result = wxString::FromUTF8(prediction.c_str());
    if (OnPredictionReady) {
        OnPredictionReady(result);
    }

    return result;
}

std::vector<wxString> TextInputEngine::PredictTopKFromSwipe(const std::vector<std::pair<float, float>>& swipePath, int k)
{
    // TODO: Implement top-K prediction
    // For now, return single prediction
    std::vector<wxString> results;
    wxString prediction = PredictFromSwipe(swipePath);
    if (!prediction.IsEmpty()) {
        results.push_back(prediction);
    }

    if (OnTopKPredictionsReady) {
        OnTopKPredictionsReady(results);
    }
    return results;
}

float TextInputEngine::EvaluateSequence(const std::vector<wxString>& words)
{
#ifdef USE_KENLM
    if (!m_kenLM) {
        wxLogWarning("KenLM not initialized");
        return 0.0f;
    }

    try {
        lm::ngram::Model* model = static_cast<lm::ngram::Model*>(m_kenLM);
        lm::ngram::State in_state = model->BeginSentenceState();
        lm::ngram::State out_state;
        float total_log_prob = 0.0f;

        // Score each word in sequence
        for (const wxString& word : words) {
            if (word.IsEmpty()) {
                continue;
            }

            std::string stdWord = word.ToStdString();
            lm::WordIndex wordIndex = model->GetVocabulary().Index(stdWord);
            lm::FullScoreReturn ret = model->FullScore(in_state, wordIndex, out_state);
            total_log_prob += ret.prob;
            in_state = out_state;
        }

        // Add end of sentence score
        lm::WordIndex endSentence = model->GetVocabulary().EndSentence();
        lm::FullScoreReturn ret = model->FullScore(in_state, endSentence, out_state);
        total_log_prob += ret.prob;

        return total_log_prob;
    } catch (const std::exception& e) {
        wxLogError("Error evaluating sequence: %s", e.what());
        return 0.0f;
    }
#else
    wxUnusedVar(words);
    wxLogWarning("KenLM support not compiled");
    return 0.0f;
#endif
}

TextInputEngine::KenLMResult TextInputEngine::EvaluateIncremental(const std::vector<wxString>& words, float initialLogProb, void* initialState)
{
    KenLMResult result;
    result.logProb = initialLogProb;
    result.state = nullptr;

#ifdef USE_KENLM
    if (!m_kenLM) {
        wxLogWarning("KenLM not initialized");
        return result;
    }

    try {
        lm::ngram::Model* model = static_cast<lm::ngram::Model*>(m_kenLM);
        lm::ngram::State in_state;
        lm::ngram::State* out_state = new lm::ngram::State();

        // Use provided state or begin sentence state
        if (initialState != nullptr) {
            in_state = *static_cast<lm::ngram::State*>(initialState);
        } else {
            in_state = model->BeginSentenceState();
        }

        float total_log_prob = initialLogProb;

        // Score each word in sequence
        for (const wxString& word : words) {
            if (word.IsEmpty()) {
                continue;
            }

            std::string stdWord = word.ToStdString();
            lm::WordIndex wordIndex = model->GetVocabulary().Index(stdWord);
            lm::FullScoreReturn ret = model->FullScore(in_state, wordIndex, *out_state);
            total_log_prob += ret.prob;
            in_state = *out_state;
        }

        // Add end of sentence score
        lm::WordIndex endSentence = model->GetVocabulary().EndSentence();
        lm::FullScoreReturn ret = model->FullScore(in_state, endSentence, *out_state);
        total_log_prob += ret.prob;

        result.logProb = total_log_prob;
        result.state = out_state;
        return result;
    } catch (const std::exception& e) {
        wxLogError("Error in incremental evaluation: %s", e.what());
        return result;
    }
#else
    wxUnusedVar(words);
    wxUnusedVar(initialLogProb);
    wxUnusedVar(initialState);
    return result;
#endif
}

void* TextInputEngine::GetBeginSentenceState()
{
#ifdef USE_KENLM
    if (!m_kenLM) {
        wxLogWarning("KenLM not initialized");
        return nullptr;
    }

    try {
        lm::ngram::Model* model = static_cast<lm::ngram::Model*>(m_kenLM);
        lm::ngram::State* state = new lm::ngram::State();
        *state = model->BeginSentenceState();
        return state;
    } catch (const std::exception& e) {
        wxLogError("Error getting begin sentence state: %s", e.what());
        return nullptr;
    }
#else
    return nullptr;
#endif
}

// Private implementation

bool TextInputEngine::LoadSwipeEncoder(const wxString& modelPath)
{
    wxLogMessage("Loading swipe encoder from %s", modelPath);

#ifdef USE_ONNX
    if (!wxFileName::Exists(modelPath)) {
        wxLogError("Swipe encoder model file not found: %s", modelPath);
        return false;
    }

    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "HeyEyeUnified");
        Ort::SessionOptions sess_opt;
        sess_opt.SetLogSeverityLevel(4); // Minimal logging
        sess_opt.DisableCpuMemArena(); // Avoiding crashes

        // Convert wxString to wstring for ONNX
        std::wstring modelPathW = modelPath.ToStdWstring();
        m_swipeEncoder = new Ort::Session(env, modelPathW.c_str(), sess_opt);
        m_memoryInfo = new Ort::MemoryInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault));

        wxLogMessage("Swipe encoder loaded successfully");
        return true;
    } catch (const Ort::Exception& e) {
        wxLogError("ONNX exception during swipe encoder loading: %s", e.what());
        return false;
    }
#else
    wxLogWarning("ONNX support not compiled");
    return false;
#endif
}

bool TextInputEngine::LoadVocabulary(const wxString& vocabPath)
{
    wxLogMessage("Loading vocabulary from %s", vocabPath);

    if (!wxFileName::Exists(vocabPath)) {
        wxLogError("Vocabulary file not found: %s", vocabPath);
        return false;
    }

    std::string vocabPathStr = vocabPath.ToStdString();
    auto vocabPair = load_vocab(vocabPathStr);
    m_vocab = vocabPair.first;
    m_vocabKeys = vocabPair.second;

    if (m_vocab && m_vocabKeys) {
        wxLogMessage("Vocabulary loaded successfully with %zu entries", m_vocab->size());
        return true;
    } else {
        wxLogError("Failed to load vocabulary");
        return false;
    }
}

bool TextInputEngine::LoadFaissIndex(const wxString& indexPath)
{
    wxLogMessage("Loading FAISS index from %s", indexPath);

    if (!wxFileName::Exists(indexPath)) {
        wxLogError("FAISS index file not found: %s", indexPath);
        return false;
    }

    std::string indexPathStr = indexPath.ToStdString();
    m_faissIndex = load_faiss_index(indexPathStr);

    if (m_faissIndex) {
        wxLogMessage("FAISS index loaded successfully");
        return true;
    } else {
        wxLogError("Failed to load FAISS index");
        return false;
    }
}

bool TextInputEngine::LoadKenLM(const wxString& modelPath)
{
    wxLogMessage("Loading KenLM from %s", modelPath);
#ifdef USE_KENLM
    try {
        std::string path = modelPath.ToStdString();
        lm::ngram::Model* model = new lm::ngram::Model(path.c_str());
        m_kenLM = static_cast<void*>(model);
        wxLogMessage("KenLM model loaded successfully");
        return true;
    } catch (const std::exception& e) {
        wxLogError("Failed to load KenLM model: %s", e.what());
        return false;
    }
#else
    wxLogWarning("KenLM support not compiled");
    return false;
#endif
}

bool TextInputEngine::LoadLightGBM(const wxString& modelPath)
{
    wxLogMessage("Loading LightGBM from %s", modelPath);

    if (!wxFileName::Exists(modelPath)) {
        wxLogWarning("LightGBM model file not found: %s", modelPath);
        return false;
    }

    m_lightGBM = new LightGBMRanker();
    std::string modelPathStr = modelPath.ToStdString();

    if (m_lightGBM->load_model(modelPathStr)) {
        wxLogMessage("LightGBM model loaded successfully");
        return true;
    } else {
        wxLogError("Failed to load LightGBM model");
        delete m_lightGBM;
        m_lightGBM = nullptr;
        return false;
    }
}

std::vector<float> TextInputEngine::EncodeSwipe(const std::vector<std::pair<float, float>>& swipePath)
{
#ifdef USE_ONNX
    if (!m_swipeEncoder || !m_memoryInfo) {
        wxLogError("Swipe encoder not initialized");
        return std::vector<float>();
    }

    if (swipePath.empty()) {
        wxLogWarning("Empty swipe path");
        return std::vector<float>();
    }

    try {
        // Convert swipe path to flat vector
        std::vector<float> all_points;
        all_points.reserve(swipePath.size() * 2);
        for (const auto& point : swipePath) {
            all_points.push_back(point.first);
            all_points.push_back(point.second);
        }

        // Prepare positions
        std::vector<int64_t> positions;
        positions.reserve(MAX_LENGTH_SWIPE);
        for (int i = 0; i < MAX_LENGTH_SWIPE; ++i) {
            positions.push_back(i);
        }

        // Prepare mask
        std::vector<uint8_t> mask;
        size_t keep_last = 2 * MAX_LENGTH_SWIPE;

        if (all_points.size() > keep_last) {
            // Crop to keep last MAX_LENGTH_SWIPE points
            all_points.erase(all_points.begin(), all_points.end() - keep_last);
            mask.resize(all_points.size() / 2, 0);
        } else if (all_points.size() < keep_last) {
            // Pad with -200
            mask.resize(all_points.size() / 2, 0);
            all_points.resize(keep_last, -200.0f);
            mask.resize(all_points.size() / 2, 1);
        } else {
            mask.resize(all_points.size() / 2, 0);
        }

        // Create input tensors
        std::array<int64_t, 3> input_shape{1, MAX_LENGTH_SWIPE, 2};
        std::array<int64_t, 2> positions_shape{1, MAX_LENGTH_SWIPE};
        std::array<int64_t, 2> mask_shape{1, MAX_LENGTH_SWIPE};

        std::vector<const char*> input_node_names = {"input", "positions", "mask"};
        std::vector<const char*> output_node_names = {"output"};

        std::vector<Ort::Value> inputTensors;
        inputTensors.emplace_back(Ort::Value::CreateTensor<float>(*m_memoryInfo, all_points.data(), all_points.size(), input_shape.data(), input_shape.size()));
        inputTensors.emplace_back(Ort::Value::CreateTensor<int64_t>(*m_memoryInfo, positions.data(), positions.size(), positions_shape.data(), positions_shape.size()));
        inputTensors.emplace_back(Ort::Value::CreateTensor<bool>(*m_memoryInfo, reinterpret_cast<bool*>(mask.data()), mask.size(), mask_shape.data(), mask_shape.size()));

        // Run inference
        std::vector<Ort::Value> outputTensor = m_swipeEncoder->Run(Ort::RunOptions{nullptr}, input_node_names.data(), inputTensors.data(), inputTensors.size(), output_node_names.data(), 1);

        if (outputTensor.size() > 0) {
            int nb_embedding = outputTensor[0].GetTensorTypeAndShapeInfo().GetShape()[1];
            const float* output_data = outputTensor[0].GetTensorData<float>();

            std::vector<float> embedding(output_data, output_data + nb_embedding);
            return embedding;
        }
    } catch (const Ort::Exception& e) {
        wxLogError("ONNX exception during swipe encoding: %s", e.what());
    }
#else
    wxUnusedVar(swipePath);
    wxLogWarning("ONNX support not compiled");
#endif

    return std::vector<float>();
}

std::map<faiss::idx_t, float> TextInputEngine::SearchVocabulary(const std::vector<float>& embedding, int topK)
{
    if (!m_faissIndex) {
        wxLogError("FAISS index not initialized");
        return std::map<faiss::idx_t, float>();
    }

    if (embedding.empty()) {
        wxLogWarning("Empty embedding");
        return std::map<faiss::idx_t, float>();
    }

    std::vector<float> embedding_copy = embedding;
    return search_faiss_index(&embedding_copy, m_faissIndex, topK);
}

std::string TextInputEngine::RankCandidates(
    const std::vector<std::pair<float, float>>& swipePath,
    const std::map<faiss::idx_t, float>& candidates)
{
    if (!m_vocab || !m_vocabKeys) {
        wxLogError("Vocabulary not initialized");
        return "";
    }

    if (candidates.empty()) {
        wxLogWarning("No candidates to rank");
        return "";
    }

    // Step 1: Pre-compute initial LM state from current text
    // This matches HeyEyeTracker behavior (gaze_track.cpp:169-183)
    float initial_log_prob = 0.0f;
    lm::ngram::State initial_state;
    std::vector<wxString> context_words;

#ifdef USE_KENLM
    if (m_kenLM) {
        lm::ngram::Model* model = static_cast<lm::ngram::Model*>(m_kenLM);
        initial_state = model->BeginSentenceState();

        // Extract word context from current text (split by spaces)
        if (!m_currentText.IsEmpty()) {
            wxArrayString all_words = wxSplit(m_currentText, ' ', '\0');
            for (const wxString& word : all_words) {
                if (!word.IsEmpty()) {
                    context_words.push_back(word);
                }
            }

            // Keep only last 4 words for context (same as HeyEyeTracker)
            if (context_words.size() > 4) {
                context_words.erase(context_words.begin(), context_words.end() - 4);
            }

            // Pre-compute LM state for context words (optimization)
            try {
                lm::ngram::State out_state;
                for (const wxString& context_word : context_words) {
                    std::string stdWord = context_word.ToStdString();
                    lm::WordIndex wordIndex = model->GetVocabulary().Index(stdWord);
                    lm::FullScoreReturn ret = model->FullScore(initial_state, wordIndex, out_state);
                    initial_log_prob += ret.prob;
                    initial_state = out_state;
                }
            } catch (const std::exception& e) {
                wxLogWarning("Error pre-computing LM context: %s", e.what());
                // Reset to begin state on error
                initial_state = model->BeginSentenceState();
                initial_log_prob = 0.0f;
            }
        }
    }
#endif

    // Step 2: Collect all candidate words and compute LM scores
    std::vector<std::string> candidate_words;
    std::vector<float> lm_scores;
    std::set<std::string> seen_words;

    for (const auto& faiss_pair : candidates) {
        int vocab_idx = faiss_pair.first;

        if (m_vocab->find(vocab_idx) == m_vocab->end()) {
            continue;
        }

        const std::vector<std::string>& words = m_vocab->at(vocab_idx);

        for (const std::string& word : words) {
            if (seen_words.count(word) > 0) {
                continue;
            }
            seen_words.insert(word);

            // Compute LM score starting from pre-computed context state
            float lm_score = initial_log_prob;
#ifdef USE_KENLM
            if (m_kenLM) {
                try {
                    lm::ngram::Model* model = static_cast<lm::ngram::Model*>(m_kenLM);
                    lm::ngram::State out_state;

                    // Score the candidate word (starting from context state)
                    lm::WordIndex wordIndex = model->GetVocabulary().Index(word);
                    lm::FullScoreReturn ret = model->FullScore(initial_state, wordIndex, out_state);
                    lm_score += ret.prob;

                    // Add end of sentence score
                    lm::WordIndex endSentence = model->GetVocabulary().EndSentence();
                    ret = model->FullScore(out_state, endSentence, out_state);
                    lm_score += ret.prob;
                } catch (const std::exception& e) {
                    wxLogWarning("Error evaluating LM for word '%s': %s", word.c_str(), e.what());
                }
            }
#endif

            candidate_words.push_back(word);
            lm_scores.push_back(lm_score);
        }
    }

    if (candidate_words.empty()) {
        wxLogWarning("No valid candidate words after processing");
        return "";
    }

    // Step 2: Use LightGBM ranker if available
    std::string selected_word = "";

    if (m_lightGBM && m_lightGBM->is_model_loaded()) {
        try {
            // Compute all features
            std::vector<CandidateFeatures> features = compute_all_features(
                swipePath,
                candidates,
                m_vocab,
                m_vocabKeys,
                lm_scores,
                candidate_words
            );

            // Rank candidates
            std::vector<size_t> ranked_indices = m_lightGBM->rank_candidates(features);

            if (!ranked_indices.empty()) {
                selected_word = features[ranked_indices[0]].word;

                // Debug: Show top 5 predictions
                wxLogMessage("LightGBM Top 5 predictions:");
                for (size_t i = 0; i < std::min(size_t(5), ranked_indices.size()); ++i) {
                    wxString word = wxString::FromUTF8(features[ranked_indices[i]].word.c_str());
                    wxLogMessage("  %zu. %s", i + 1, word);
                }
            }
        } catch (const std::exception& e) {
            wxLogError("Error during LightGBM ranking: %s", e.what());
        }
    } else {
        // Fallback: simple scoring (LM score - 0.5 * FAISS distance)
        wxLogMessage("Using fallback scoring (LightGBM not available)");
        float max_score = -std::numeric_limits<float>::infinity();

        for (size_t i = 0; i < candidate_words.size(); ++i) {
            // Find corresponding FAISS distance
            float faiss_distance = 0.0f;
            for (const auto& faiss_pair : candidates) {
                const auto& words = m_vocab->at(faiss_pair.first);
                if (std::find(words.begin(), words.end(), candidate_words[i]) != words.end()) {
                    faiss_distance = faiss_pair.second;
                    break;
                }
            }

            float score = lm_scores[i] - 0.5f * faiss_distance;

            if (score > max_score) {
                max_score = score;
                selected_word = candidate_words[i];
            }
        }
    }

    return selected_word;
}
