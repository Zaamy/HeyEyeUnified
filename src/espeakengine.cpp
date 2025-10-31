#include "espeakengine.h"
#include <wx/log.h>

#ifdef USE_ESPEAK
#include "espeak-ng/speak_lib.h"
#include <wx/sound.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#endif

ESpeakEngine::ESpeakEngine()
    : m_initialized(false)
    , m_sampleRate(22050)
{
}

ESpeakEngine::~ESpeakEngine()
{
    Shutdown();
}

bool ESpeakEngine::Initialize(const wxString& espeakDataPath)
{
#ifdef USE_ESPEAK
    std::lock_guard<std::mutex> lock(m_espeakLock);

    if (m_initialized) {
        wxLogWarning("ESpeakEngine: Already initialized");
        return true;
    }

    // Convert wxString to C string
    wxCharBuffer pathBuffer = espeakDataPath.ToUTF8();
    const char* dataPath = pathBuffer.data();

    // Initialize espeak-ng
    // AUDIO_OUTPUT_RETRIEVAL: We handle audio output ourselves
    // buflength=0: Use default buffer
    // options: 1 = espeakINITIALIZE_PHONEME_EVENTS (not used but safe)
    unsigned int options = 0;
    int samplingRate = espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, 0, dataPath, options);

    if (samplingRate < 0) {
        wxLogError("ESpeakEngine: Failed to initialize espeak (error %d)", samplingRate);
        return false;
    }

    m_sampleRate = samplingRate;
    wxLogMessage("ESpeakEngine: Initialized with sample rate %d Hz", m_sampleRate);

    // Set synthesis callback
    // Cast to the correct function pointer type expected by espeak
    espeak_SetSynthCallback(reinterpret_cast<t_espeak_callback*>(ESpeakEngine::SynthCallback));

    // Set French voice
    int result = espeak_SetVoiceByName("fr");
    if (result != EE_OK) {
        wxLogWarning("ESpeakEngine: Failed to set French voice, trying 'French (France)'");
        result = espeak_SetVoiceByName("French (France)");
        if (result != EE_OK) {
            wxLogWarning("ESpeakEngine: Could not set French voice");
        }
    }

    // Set default parameters
    espeak_SetParameter(espeakRATE, 100, 0);  // 100 WPM
    espeak_SetParameter(espeakPITCH, 50, 0);  // Default pitch
    espeak_SetParameter(espeakVOLUME, 100, 0); // Default volume

    m_initialized = true;
    return true;
#else
    wxLogWarning("ESpeakEngine: Not compiled with USE_ESPEAK support");
    return false;
#endif
}

void ESpeakEngine::Shutdown()
{
#ifdef USE_ESPEAK
    std::lock_guard<std::mutex> lock(m_espeakLock);

    if (m_initialized) {
        espeak_Cancel();
        espeak_Terminate();
        m_initialized = false;
        wxLogMessage("ESpeakEngine: Shut down");
    }
#endif
}

void ESpeakEngine::Speak(const wxString& text)
{
#ifdef USE_ESPEAK
    if (!m_initialized) {
        wxLogWarning("ESpeakEngine: Not initialized");
        return;
    }

    if (text.IsEmpty()) {
        wxLogWarning("ESpeakEngine: Empty text provided");
        return;
    }

    std::lock_guard<std::mutex> lock(m_espeakLock);

    // Clear previous audio data
    {
        std::lock_guard<std::mutex> audioLock(m_audioLock);
        m_audioData.clear();
    }

    // Convert text to UTF-8 encoding
    wxCharBuffer textBuffer = text.ToUTF8();
    const char* utf8Text = textBuffer.data();
    size_t textLength = strlen(utf8Text);

    // Synthesize speech
    int result = espeak_Synth(
        utf8Text,
        textLength + 1,
        0,
        POS_SENTENCE,
        0,
        espeakCHARS_UTF8,
        NULL,
        this
    );

    if (result != EE_OK) {
        wxLogError("ESpeakEngine: espeak_Synth failed with error %d", result);
        return;
    }

    // Wait for synthesis to complete
    espeak_Synchronize();
#endif
}

void ESpeakEngine::Stop()
{
#ifdef USE_ESPEAK
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_espeakLock);
    espeak_Cancel();

    // Clear audio buffer
    std::lock_guard<std::mutex> audioLock(m_audioLock);
    m_audioData.clear();
#endif
}

void ESpeakEngine::SetVoice(const wxString& voiceName)
{
#ifdef USE_ESPEAK
    if (!m_initialized) {
        wxLogWarning("ESpeakEngine: Not initialized");
        return;
    }

    std::lock_guard<std::mutex> lock(m_espeakLock);

    wxCharBuffer nameBuffer = voiceName.ToUTF8();
    int result = espeak_SetVoiceByName(nameBuffer.data());

    if (result != EE_OK) {
        wxLogWarning("ESpeakEngine: Failed to set voice '%s'", voiceName);
    }
#endif
}

void ESpeakEngine::SetRate(int rate)
{
#ifdef USE_ESPEAK
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_espeakLock);
    espeak_SetParameter(espeakRATE, rate, 0);
#endif
}

void ESpeakEngine::SetPitch(int pitch)
{
#ifdef USE_ESPEAK
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_espeakLock);
    espeak_SetParameter(espeakPITCH, pitch, 0);
#endif
}

void ESpeakEngine::SetVolume(int volume)
{
#ifdef USE_ESPEAK
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_espeakLock);
    espeak_SetParameter(espeakVOLUME, volume, 0);
#endif
}

int ESpeakEngine::SynthCallback(short *wav, int numsamples, void* eventsPtr)
{
#ifdef USE_ESPEAK
    espeak_EVENT* events = static_cast<espeak_EVENT*>(eventsPtr);

    if (events && events->user_data) {
        ESpeakEngine* engine = static_cast<ESpeakEngine*>(events->user_data);
        engine->HandleSynthCallback(wav, numsamples);
    }
#endif

    return 0;
}

void ESpeakEngine::HandleSynthCallback(short *wav, int numsamples)
{
#ifdef USE_ESPEAK
    if (wav == nullptr || numsamples == 0) {
        // Synthesis complete - play accumulated audio
        PlayAudio();
        return;
    }

    // Append audio samples to buffer
    std::lock_guard<std::mutex> lock(m_audioLock);
    const char* wavBytes = reinterpret_cast<const char*>(wav);
    size_t bytesToAdd = numsamples * 2;
    m_audioData.insert(m_audioData.end(), wavBytes, wavBytes + bytesToAdd);
#endif
}

void ESpeakEngine::PlayAudio()
{
#ifdef USE_ESPEAK
    std::lock_guard<std::mutex> lock(m_audioLock);

    if (m_audioData.empty()) {
        return;
    }

    // Create WAV header for PCM audio data
    struct WAVHeader {
        char riff[4];           // "RIFF"
        uint32_t fileSize;      // File size - 8 bytes
        char wave[4];           // "WAVE"
        char fmt[4];            // "fmt "
        uint32_t fmtSize;       // Size of fmt chunk (16 for PCM)
        uint16_t audioFormat;   // 1 = PCM
        uint16_t numChannels;   // 1 = mono
        uint32_t sampleRate;    // Sample rate
        uint32_t byteRate;      // sampleRate * numChannels * bitsPerSample/8
        uint16_t blockAlign;    // numChannels * bitsPerSample/8
        uint16_t bitsPerSample; // 16 bits
        char data[4];           // "data"
        uint32_t dataSize;      // Size of audio data
    };

    WAVHeader header;
    memcpy(header.riff, "RIFF", 4);
    header.fileSize = 36 + m_audioData.size();
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    header.fmtSize = 16;
    header.audioFormat = 1;
    header.numChannels = 1;
    header.sampleRate = m_sampleRate;
    header.bitsPerSample = 16;
    header.byteRate = m_sampleRate * header.numChannels * header.bitsPerSample / 8;
    header.blockAlign = header.numChannels * header.bitsPerSample / 8;
    memcpy(header.data, "data", 4);
    header.dataSize = m_audioData.size();

    // Combine header and audio data
    m_wavData.clear();
    m_wavData.reserve(sizeof(WAVHeader) + m_audioData.size());
    const char* headerBytes = reinterpret_cast<const char*>(&header);
    m_wavData.insert(m_wavData.end(), headerBytes, headerBytes + sizeof(WAVHeader));
    m_wavData.insert(m_wavData.end(), m_audioData.begin(), m_audioData.end());

    // Write to temporary file for async playback
    // wxSound on Windows requires a file path for async playback
    wxString tempDir = wxStandardPaths::Get().GetTempDir();
    wxString tempFile = tempDir + wxFILE_SEP_PATH + "espeak_temp.wav";

    wxFile file;
    if (file.Create(tempFile, true)) {
        file.Write(m_wavData.data(), m_wavData.size());
        file.Close();

        // Play asynchronously (non-blocking)
        wxSound sound(tempFile);
        if (sound.IsOk()) {
            sound.Play(wxSOUND_ASYNC);
        }
    }
#endif
}
