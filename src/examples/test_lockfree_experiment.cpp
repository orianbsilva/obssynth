#include "../app/common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

using namespace synth;
using namespace examples;

// Global variables for the experiment
static std::unique_ptr<PolyphonicOscillator> g_poly_oscillator = nullptr;
static std::atomic<bool> g_running{true};
[[maybe_unused]] static std::atomic<int> g_race_condition_count{0};
static std::atomic<int> g_audio_callback_count{0};
static std::atomic<int> g_note_events_count{0};

// Lock-free version of audio generation (EXPERIMENTAL - UNSAFE)
class LockFreePolyphonicOscillator
{
private:
    static constexpr int MAX_VOICES = 8;
    std::array<Voice, MAX_VOICES> voices_;
    [[maybe_unused]] float sample_rate_;
    [[maybe_unused]] WaveType wave_type_;
    // NO MUTEX!

public:
    LockFreePolyphonicOscillator(WaveType wave_type, float sample_rate)
        : sample_rate_(sample_rate), wave_type_(wave_type)
    {
        for (auto &voice : voices_)
        {
            voice.oscillator_ = std::make_unique<Oscillator>(440.0f, wave_type, sample_rate);
            voice.oscillator_->setAmplitude(0.0f);
            voice.is_active_ = false;
        }
    }

    // UNSAFE: No locks - race conditions possible
    void noteOn(uint8_t note_number, float velocity = 1.0f, float pan = 0.0f)
    {
        g_note_events_count++;

        // Find free voice (RACE CONDITION: audio thread might read while we modify)
        int voice_index = -1;
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (!voices_[i].is_active_)
            {
                voice_index = i;
                break;
            }
        }

        if (voice_index == -1)
        {
            voice_index = 0; // Simple voice stealing
        }

        Voice &voice = voices_[voice_index];
        voice.note_number_ = note_number;
        voice.frequency_ = PolyphonicOscillator::noteToFrequency(note_number);
        voice.velocity_ = velocity;
        voice.amplitude_ = velocity * 0.8f / MAX_VOICES;
        voice.pan_position_ = pan;

        // RACE CONDITION: Audio thread might read these while we modify
        voice.oscillator_->setFrequency(voice.frequency_);
        voice.oscillator_->setPan(voice.pan_position_);
        voice.oscillator_->setAmplitude(voice.amplitude_);
        voice.oscillator_->resetPhase();

        // CRITICAL RACE: This write might be torn/partially visible
        voice.is_active_ = true;
    }

    // UNSAFE: No locks
    void noteOff(uint8_t note_number)
    {
        g_note_events_count++;

        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (voices_[i].is_active_ && voices_[i].note_number_ == note_number)
            {
                voices_[i].is_active_ = false; // RACE CONDITION
                voices_[i].oscillator_->setAmplitude(0.0f); // RACE CONDITION
                break;
            }
        }
    }

    // UNSAFE: No locks - generates audio with potential race conditions
    void generateBufferLockFree(float *buffer, int size)
    {
        g_audio_callback_count++;

        if (!buffer || size <= 0)
            return;

        std::fill(buffer, buffer + size, 0.0f);

        for (int sample = 0; sample < size; ++sample)
        {
            float mixedSample = 0.0f;

            for (int v = 0; v < MAX_VOICES; ++v)
            {
                Voice &voice = voices_[v];

                // RACE CONDITION: is_active_ might change during this read
                // But as you suggest, it's just one sample - next sample will be correct
                if (voice.is_active_)
                {
                    // RACE CONDITION: oscillator state might be inconsistent
                    // but only for a brief moment
                    mixedSample += voice.oscillator_->getSample();
                }
            }

            buffer[sample] = mixedSample;
        }
    }

    int getActiveVoiceCount() const
    {
        int count = 0;
        for (const auto &voice : voices_)
        {
            if (voice.is_active_)
                count++;
        }
        return count;
    }

    bool isActive() const
    {
        for (const auto &voice : voices_)
        {
            if (voice.is_active_)
                return true;
        }
        return false;
    }
};

static std::unique_ptr<LockFreePolyphonicOscillator> g_lockfree_oscillator = nullptr;

// Audio callback using lock-free oscillator
void lockFreeAudioCallback(float *buffer, int bufferSize, int /*numChannels*/)
{
    if (g_lockfree_oscillator && g_running.load())
    {
        g_lockfree_oscillator->generateBufferLockFree(buffer, bufferSize);
    }
    else
    {
        std::fill(buffer, buffer + bufferSize, 0.0f);
    }
}

// Function to hammer the oscillator with note events from a separate thread
void noteEventThread()
{
    const std::vector<uint8_t> notes = {60, 64, 67, 72, 76, 79, 84}; // C major scale
    int note_index = 0;

    while (g_running.load())
    {
        if (g_lockfree_oscillator)
        {
            // Rapidly trigger note events to stress-test race conditions
            uint8_t note = notes[note_index % notes.size()];
            g_lockfree_oscillator->noteOn(note, 0.5f);

            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 notes/sec

            g_lockfree_oscillator->noteOff(note);
            note_index++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main()
{
    std::cout << "Lock-Free Polyphonic Oscillator Experiment\n";
    std::cout << "==========================================\n";
    std::cout << "This test explores the hypothesis that race conditions\n";
    std::cout << "in small audio buffers are transient and self-correcting.\n\n";

    // Initialize platform-specific features
    PlatformUtils::initialize();

    // Create lock-free oscillator
    g_lockfree_oscillator = std::make_unique<LockFreePolyphonicOscillator>(WaveType::SINE, SAMPLE_RATE);

    // Initialize audio sink
    AudioSink sink;
    if (sink.init() != 0)
    {
        std::cout << "Failed to initialize audio sink\n";
        PlatformUtils::cleanup();
        return -1;
    }

    // Set up audio callback
    sink.setAudioCallback(lockFreeAudioCallback);

    // Start audio
    if (sink.start() != 0)
    {
        std::cout << "Failed to start audio\n";
        PlatformUtils::cleanup();
        return -1;
    }

    // Start note event thread to create race conditions
    std::thread note_thread(noteEventThread);

    std::cout << "🎵 Lock-free audio synthesis running...\n";
    std::cout << "Listen for:\n";
    std::cout << "- Audio glitches (clicks, pops, dropouts)\n";
    std::cout << "- Stuck notes (notes that don't turn off)\n";
    std::cout << "- Volume inconsistencies\n";
    std::cout << "- Frequency drift\n\n";
    std::cout << "Press any key to stop...\n";

    // Monitor for race condition symptoms
    auto start_time = std::chrono::steady_clock::now();
    int last_audio_count = 0;
    int last_note_count = 0;

    while (!PlatformUtils::kbhit())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        int audio_count = g_audio_callback_count.load();
        int note_count = g_note_events_count.load();
        int active_voices = g_lockfree_oscillator->getActiveVoiceCount();

        std::cout << "Time: " << elapsed << "s | "
                  << "Audio callbacks: " << (audio_count - last_audio_count) << "/s | "
                  << "Note events: " << (note_count - last_note_count) << "/s | "
                  << "Active voices: " << active_voices << "/8\n";

        last_audio_count = audio_count;
        last_note_count = note_count;
    }

    // Cleanup
    g_running.store(false);
    note_thread.join();
    sink.stop();
    PlatformUtils::cleanup();

    std::cout << "\n📊 Experiment Results:\n";
    std::cout << "Total audio callbacks: " << g_audio_callback_count.load() << "\n";
    std::cout << "Total note events: " << g_note_events_count.load() << "\n";
    std::cout << "Did you notice any audio artifacts?\n";
    std::cout << "Race conditions are theoretically present but may be\n";
    std::cout << "too brief to cause audible problems in practice.\n";

    return 0;
}
