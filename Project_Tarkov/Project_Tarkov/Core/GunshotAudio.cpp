#include "GunshotAudio.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xaudio2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

struct GunshotAudio::Data
{
    IXAudio2* engine = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    std::array<IXAudio2SourceVoice*, 16> voices{};
    std::vector<std::int16_t> samples;
    bool comInitialized = false;

    ~Data()
    {
        // Destroy voices before releasing the sample storage they reference.
        for (auto* voice : voices) if (voice) voice->DestroyVoice();
        if (master) master->DestroyVoice();
        if (engine) engine->Release();
        if (comInitialized) CoUninitialize();
    }
};

GunshotAudio::GunshotAudio() = default;
GunshotAudio::~GunshotAudio() = default;
void GunshotAudio::Reset() { data.reset(); }

void GunshotAudio::Init()
{
    Reset();
    auto next = std::make_unique<Data>();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    next->comInitialized = SUCCEEDED(com);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) return;
    if (FAILED(XAudio2Create(&next->engine, 0, XAUDIO2_DEFAULT_PROCESSOR))) return;
    if (FAILED(next->engine->CreateMasteringVoice(&next->master, 2, 48000))) return;

    // Original synthesized rifle report: sharp crack, low blast and decaying tail.
    // Generated once; no network assets or file I/O on the firing path.
    constexpr int rate = 48000;
    next->samples.resize(rate / 2);
    std::uint32_t random = 0x7134abcd;
    float lowNoise = 0;
    for (size_t i = 0; i < next->samples.size(); ++i) {
        random = random * 1664525u + 1013904223u;
        const float noise = static_cast<float>(random >> 8) / 8388607.5f - 1.f;
        const float t = static_cast<float>(i) / rate;
        lowNoise += .12f * (noise - lowNoise);
        const float crack = (noise - lowNoise) * std::exp(-t * 150.f);
        const float blast = std::sin(6.2831853f * (110.f * t + 1.8f * (1.f - std::exp(-t * 35.f)))) * std::exp(-t * 32.f);
        const float tail = lowNoise * std::exp(-t * 13.f);
        const float envelope = (std::min)(1.f, t * 6000.f) * (std::min)(1.f, (.5f - t) * 100.f);
        const float sample = std::tanh(1.8f * crack + .65f * blast + 1.2f * tail) * envelope;
        next->samples[i] = static_cast<std::int16_t>(sample * 26000.f);
    }
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = rate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = rate * 2;
    for (auto& voice : next->voices)
        if (FAILED(next->engine->CreateSourceVoice(&voice, &format))) return;
    next->master->SetVolume(.7f);
    data = std::move(next);
}

void GunshotAudio::Play(float volume, float pan)
{
    if (!data || !std::isfinite(volume) || !std::isfinite(pan)) return;
    for (auto* voice : data->voices) {
        XAUDIO2_VOICE_STATE state{};
        voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued != 0) continue;
        pan = std::clamp(pan, -1.f, 1.f);
        const float matrix[] = {std::sqrt((1.f - pan) * .5f), std::sqrt((1.f + pan) * .5f)};
        voice->SetOutputMatrix(data->master, 1, 2, matrix);
        voice->SetVolume(std::clamp(volume, 0.f, 1.f));
        XAUDIO2_BUFFER buffer{};
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.AudioBytes = static_cast<UINT32>(data->samples.size() * sizeof(std::int16_t));
        buffer.pAudioData = reinterpret_cast<const BYTE*>(data->samples.data());
        if (SUCCEEDED(voice->SubmitSourceBuffer(&buffer))) voice->Start();
        return;
    }
    // A saturated pool drops the new sound instead of interrupting another shot.
}
