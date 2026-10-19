#pragma once
#include <memory>

// Owns a small voice pool so simultaneous shots do not cut each other off.
class GunshotAudio
{
public:
    GunshotAudio();
    ~GunshotAudio();
    GunshotAudio(const GunshotAudio&) = delete;
    GunshotAudio& operator=(const GunshotAudio&) = delete;
    void Init();
    void Reset();
    void Play(float volume = 0.65f, float pan = 0.0f);
private:
    struct Data;
    std::unique_ptr<Data> data;
};
