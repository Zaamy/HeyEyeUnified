#ifndef TEXTINPUTENGINE_H
#define TEXTINPUTENGINE_H

#include <wx/wx.h>
#include <vector>
#include <map>
#include <utility>
#include <string>
#include <functional>

// Forward declarations for ML components
namespace Ort {
    class Session;
    struct MemoryInfo;
}

namespace faiss {
    class Index;
    typedef long long idx_t;
}

class LightGBMRanker;

/**
 * @brief Manages text prediction using ML models and letter-by-letter input
 *
 * Integrates:
 * - ONNX swipe encoder for gesture embeddings
 * - FAISS vector search for candidate retrieval
 * - KenLM language model for scoring
 * - LightGBM ranker for final prediction
 * - Letter-by-letter direct input
 */
class TextInputEngine
{
public:
    explicit TextInputEngine();
    ~TextInputEngine();

    // Initialization
    bool Initialize(const wxString& assetsPath);
    bool IsInitialized() const { return m_initialized; }

    // Current text management
    wxString GetCurrentText() const { return m_currentText; }
    void AppendCharacter(wxChar c);
    void AppendText(const wxString& text);
    void DeleteLastCharacter();
    void DeleteLastWord();
    void Clear();

    // Swipe prediction
    wxString PredictFromSwipe(const std::vector<std::pair<float, float>>& swipePath);
    std::vector<wxString> PredictTopKFromSwipe(const std::vector<std::pair<float, float>>& swipePath, int k = 5);

    // Language model access for external use
    float EvaluateSequence(const std::vector<wxString>& words);

    // Incremental evaluation with state (for ranking candidates)
    struct KenLMResult {
        float logProb;
        void* state;  // Opaque pointer to lm::ngram::State
    };
    KenLMResult EvaluateIncremental(const std::vector<wxString>& words, float initialLogProb = 0.0f, void* initialState = nullptr);
    void* GetBeginSentenceState();

    // Callbacks (replace Qt signals)
    std::function<void(const wxString&)> OnTextChanged;
    std::function<void(const wxString&)> OnPredictionReady;
    std::function<void(const std::vector<wxString>&)> OnTopKPredictionsReady;

private:
    // Initialization helpers
    bool LoadSwipeEncoder(const wxString& modelPath);
    bool LoadVocabulary(const wxString& vocabPath);
    bool LoadFaissIndex(const wxString& indexPath);
    bool LoadKenLM(const wxString& modelPath);
    bool LoadLightGBM(const wxString& modelPath);

    // Swipe encoding
    std::vector<float> EncodeSwipe(const std::vector<std::pair<float, float>>& swipePath);

    // Vocabulary search
    std::map<faiss::idx_t, float> SearchVocabulary(const std::vector<float>& embedding, int topK = 100);

    // Ranking
    std::string RankCandidates(
        const std::vector<std::pair<float, float>>& swipePath,
        const std::map<faiss::idx_t, float>& candidates
    );

    bool m_initialized;
    wxString m_currentText;
    std::vector<wxString> m_wordHistory;

    // ML Components (using pointers to avoid header dependencies)
    Ort::Session* m_swipeEncoder;
    Ort::MemoryInfo* m_memoryInfo;
    faiss::Index* m_faissIndex;
    void* m_kenLM;  // Opaque pointer to lm::ngram::Model
    LightGBMRanker* m_lightGBM;

    // Vocabulary
    std::map<int, std::vector<std::string>>* m_vocab;
    std::vector<std::string>* m_vocabKeys;
};

#endif // TEXTINPUTENGINE_H
