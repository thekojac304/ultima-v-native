#pragma once
// Ultima V Native for Windows: the audio output.
//
// The T-Deck's TdeckAudioBackend (native/targets/tdeck/main/tdeck_audio.cpp)
// owns a FreeRTOS task that drives the portable openu5::AudioRingPump -- the
// PC-speaker SFX synth and, with the music-patched pack, the OPL2 music synth
// -- into an I2S DMA ring at 16 kHz mono. This is the same design on SDL3:
// SDL's audio thread asks for samples, and its callback runs the SAME pump
// (unchanged core code) into an SDL audio stream opened at the same 16 kHz
// signed 16-bit mono; SDL resamples to the output device. The game thread
// only posts commands (never waits), exactly the AudioBackend contract.
//
// A device that cannot be opened latches the backend off: every call then
// returns false cheaply and the game runs silent, as on a T-Deck whose I2S
// bring-up failed. There is no music without the music-patched audio pack;
// nothing is synthesised for an unpatched installation.
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/audio_stream.h"

struct SDL_AudioStream;

namespace uv {

class SdlAudioBackend final : public openu5::AudioBackend, public openu5::AudioPerfSource {
  public:
    SdlAudioBackend();
    ~SdlAudioBackend() override;
    SdlAudioBackend(const SdlAudioBackend &) = delete;
    SdlAudioBackend &operator=(const SdlAudioBackend &) = delete;

    /** Boot, before anything plays: the pack's songs (null = no music). */
    void set_music_library(const openu5::AudioPackPayload *library);
    /** Open the output now (otherwise the first sound opens it). False = silent from here on. */
    bool start();
    /** Close the output; safe to call twice and from the destructor. */
    void shutdown();
    bool running() const { return stream_ != nullptr; }
    bool failed() const { return failed_; }
    const std::string &error() const { return error_; }

    // openu5::AudioBackend
    bool play_sfx(const openu5::SfxRequest &) override;
    void stop_sfx() override;
    bool start_music(openu5::MusicSong, uint16_t gain_q15) override;
    void stop_music() override;
    void set_gain(openu5::AudioChannel, uint16_t gain_q15) override;
    uint32_t music_length_ms(openu5::MusicSong song) const override;

    // openu5::AudioPerfSource
    bool perf_snapshot(openu5::AudioPerfSnapshot &) const override;
    void perf_reset() override;
    bool set_music_bypass(bool on) override;

    /** Samples handed to SDL since start (tests and diagnostics). */
    uint64_t frames_delivered() const { return frames_delivered_.load(); }
    uint64_t sfx_queue_full() const { return queue_full_.load(); }

    /**
     * Renders exactly what the SDL callback would hand the device for
     * `frames` samples, without a device (tests). Commands posted before
     * are drained first, as in the callback.
     */
    void render_offline(int16_t *out, size_t frames);
    /** Tests: never open a device; render_offline() is the only consumer. */
    void use_offline_output() { offline_mode_ = true; }

  private:
    struct Command {
        openu5::SfxRequest request{};
        uint32_t epoch = 0;
    };
    struct MusicCommand {
        openu5::MusicSong song = openu5::MusicSong::None;
        bool stop = false;
        bool pending = false;
    };
    // The pump's PcmRingSink, appending each block to the SDL stream (or the
    // offline buffer). It reports "played" as `ring` behind "written", so the
    // pump sees a full ring and paces itself on SDL's demand alone.
    class StreamSink final : public openu5::PcmRingSink {
      public:
        bool preload(const int16_t *block) override { return deliver(block); }
        bool enable() override { return true; }
        void disable() override {}
        bool write(const int16_t *block) override { return deliver(block); }
        uint32_t blocks_played() const override {
            return delivered_ >= openu5::kAudioRingBlocks ? delivered_ - uint32_t(openu5::kAudioRingBlocks) : 0u;
        }
        uint32_t underrun_events() const override { return 0; }
        bool deliver(const int16_t *block);
        SDL_AudioStream *stream = nullptr;
        int16_t *offline = nullptr; // when set, blocks are copied here instead
        size_t offline_frames = 0, offline_used = 0;
        uint32_t delivered_ = 0;
    };

    static void on_audio(void *user, SDL_AudioStream *stream, int additional_amount, int total_amount);
    static uint64_t clock_us(void *);
    bool ensure_started();
    void fail(const char *step);
    /** Drain the game thread's commands into the pump (audio thread). */
    void drain_commands();
    /** Produce at least `bytes` of output into the sink (audio thread). */
    void produce(size_t bytes);
    void publish_perf();

    openu5::AudioRingPump pump_{};
    openu5::MusicLibrary library_{};
    StreamSink sink_{};
    SDL_AudioStream *stream_ = nullptr;
    bool failed_ = false;
    bool offline_mode_ = false;
    bool offline_started_ = false;
    std::string error_;

    std::mutex commands_mutex_;
    std::array<Command, 16> commands_{};
    size_t command_count_ = 0;
    MusicCommand music_command_{};
    std::atomic<uint32_t> epoch_{0};
    std::atomic<uint16_t> sfx_gain_{0}, music_gain_{0};
    std::atomic<bool> perf_reset_requested_{false};
    std::atomic<bool> bypass_requested_{false};
    std::atomic<uint64_t> frames_delivered_{0};
    std::atomic<uint64_t> queue_full_{0};

    mutable std::mutex perf_mutex_;
    openu5::AudioPerfSnapshot published_{};
    bool published_valid_ = false;
    uint32_t blocks_since_publish_ = 0;
    static constexpr uint32_t kPublishEveryBlocks = 16;
};

} // namespace uv
