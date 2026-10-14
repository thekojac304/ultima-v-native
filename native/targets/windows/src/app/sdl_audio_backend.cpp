#include "app/sdl_audio_backend.h"

#include <cstring>

#include <SDL3/SDL.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "openu5/sfx_synth.h"

namespace uv {
namespace {
constexpr char kTag[] = "SdlAudio";
constexpr int kBlockBytes = int(openu5::kAudioBlockFrames * sizeof(int16_t));
} // namespace

SdlAudioBackend::SdlAudioBackend() = default;

SdlAudioBackend::~SdlAudioBackend() { shutdown(); }

void SdlAudioBackend::set_music_library(const openu5::AudioPackPayload *library) {
    const int64_t t0 = esp_timer_get_time();
    const size_t playable = library_.load(library);
    const int64_t t1 = esp_timer_get_time();
    pump_.set_music_library(&library_);
    if (library && !library_.loaded())
        ESP_LOGE(kTag, "AUDIO_BACKEND music library bank failed to parse: songs will stay silent");
    else if (library)
        ESP_LOGI(kTag, "AUDIO_BACKEND music library songs=%u/%u events=%u parse_us=%lld", unsigned(playable),
                 unsigned(openu5::kMusicSongCount), unsigned(library_.event_count()), (long long)(t1 - t0));
}

uint64_t SdlAudioBackend::clock_us(void *) { return uint64_t(esp_timer_get_time()); }

void SdlAudioBackend::fail(const char *step) {
    error_ = std::string(step) + ": " + SDL_GetError();
    failed_ = true;
    ESP_LOGE(kTag, "AUDIO_BACKEND bring-up failed at %s; the game runs silent", error_.c_str());
}

bool SdlAudioBackend::ensure_started() {
    if (stream_) return true;
    if (offline_mode_) {
        if (!offline_started_) {
            pump_.set_clock({this, &SdlAudioBackend::clock_us});
            pump_.reset_perf();
            offline_started_ = true;
        }
        return true;
    }
    if (failed_) return false;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fail("SDL_InitSubSystem(AUDIO)");
        return false;
    }
    pump_.set_clock({this, &SdlAudioBackend::clock_us});
    pump_.reset_perf();
    // The device's own PCM format: the SFX synth's filters are 16 kHz
    // constants (sfx_synth.h), so SDL converts, never the synth.
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 1, int(openu5::kSfxOutputRateHz)};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &SdlAudioBackend::on_audio, this);
    if (!stream_) {
        fail("SDL_OpenAudioDeviceStream");
        return false;
    }
    sink_.stream = stream_;
    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
        sink_.stream = nullptr;
        fail("SDL_ResumeAudioStreamDevice");
        return false;
    }
    ESP_LOGI(kTag, "AUDIO_BACKEND open: %d Hz s16 mono via %s", spec.freq, SDL_GetCurrentAudioDriver());
    return true;
}

bool SdlAudioBackend::start() { return ensure_started(); }

void SdlAudioBackend::shutdown() {
    if (!stream_) return;
    // After SDL_DestroyAudioStream returns, the callback is no longer running.
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    sink_.stream = nullptr;
}

bool SdlAudioBackend::play_sfx(const openu5::SfxRequest &request) {
    // A cue with no synth program is declined here, as on the device.
    if (!openu5::sfx_supported(request.id)) return false;
    if (!ensure_started()) return false;
    std::lock_guard<std::mutex> lock(commands_mutex_);
    if (command_count_ >= commands_.size()) {
        queue_full_.fetch_add(1);
        return false;
    }
    commands_[command_count_++] = Command{request, epoch_.load()};
    return true;
}

void SdlAudioBackend::stop_sfx() { epoch_.fetch_add(1); }

bool SdlAudioBackend::start_music(openu5::MusicSong song, uint16_t gain_q15) {
    if (!ensure_started()) return false;
    music_gain_.store(gain_q15);
    std::lock_guard<std::mutex> lock(commands_mutex_);
    music_command_ = MusicCommand{song, false, true}; // latest wins
    return true;
}

void SdlAudioBackend::stop_music() {
    if (!stream_ && !offline_started_) return; // never started: nothing is playing
    std::lock_guard<std::mutex> lock(commands_mutex_);
    music_command_ = MusicCommand{openu5::MusicSong::None, true, true};
}

void SdlAudioBackend::set_gain(openu5::AudioChannel channel, uint16_t gain_q15) {
    if (channel == openu5::AudioChannel::Sfx) sfx_gain_.store(gain_q15);
    else music_gain_.store(gain_q15);
}

uint32_t SdlAudioBackend::music_length_ms(openu5::MusicSong song) const {
    const auto *track = library_.track(song);
    return track ? uint32_t(uint64_t(track->end_tick) * 1000u / openu5::kXmiTicksPerSecond) : 0;
}

bool SdlAudioBackend::perf_snapshot(openu5::AudioPerfSnapshot &out) const {
    std::lock_guard<std::mutex> lock(perf_mutex_);
    out = published_;
    return published_valid_;
}

void SdlAudioBackend::perf_reset() { perf_reset_requested_.store(true); }

bool SdlAudioBackend::set_music_bypass(bool on) {
    bypass_requested_.store(on);
    return true;
}

bool SdlAudioBackend::StreamSink::deliver(const int16_t *block) {
    ++delivered_;
    if (offline) {
        const size_t take = std::min<size_t>(openu5::kAudioBlockFrames, offline_frames - offline_used);
        std::memcpy(offline + offline_used, block, take * sizeof(int16_t));
        offline_used += take;
        // What did not fit is the next call's first samples, as SDL's stream
        // would keep it: a split render equals one long render.
        carry_count = openu5::kAudioBlockFrames - take;
        if (carry_count) std::memcpy(carry, block + take, carry_count * sizeof(int16_t));
        return true;
    }
    return stream && SDL_PutAudioStreamData(stream, block, kBlockBytes);
}

void SdlAudioBackend::drain_commands() {
    std::array<Command, 16> commands{};
    size_t count = 0;
    MusicCommand music{};
    {
        std::lock_guard<std::mutex> lock(commands_mutex_);
        count = command_count_;
        for (size_t i = 0; i < count; ++i) commands[i] = commands_[i];
        command_count_ = 0;
        music = music_command_;
        music_command_.pending = false;
    }
    pump_.note_sfx_queue_depth(uint32_t(count));
    // The epoch first: a flush that happened while these were in flight makes them stale.
    pump_.sync_epoch(epoch_.load());
    for (size_t i = 0; i < count; ++i) pump_.submit_sfx(commands[i].request, commands[i].epoch);
    if (music.pending) {
        if (music.stop) pump_.stop_music();
        else pump_.play_music(music.song);
    }
    if (perf_reset_requested_.exchange(false)) {
        pump_.reset_perf();
        blocks_since_publish_ = kPublishEveryBlocks;
    }
    pump_.set_music_bypass(bypass_requested_.load());
}

void SdlAudioBackend::publish_perf() {
    blocks_since_publish_ = 0;
    openu5::AudioPerfSnapshot snapshot{};
    pump_.perf(snapshot);
    std::lock_guard<std::mutex> lock(perf_mutex_);
    published_ = snapshot;
    published_valid_ = true;
}

void SdlAudioBackend::produce(size_t bytes) {
    static const int16_t kSilence[openu5::kAudioBlockFrames]{};
    size_t produced = 0;
    int guard = 0;
    while (produced < bytes && guard++ < 64) {
        drain_commands();
        if (pump_.sleeping()) {
            sink_.deliver(kSilence);
            --sink_.delivered_; // silence the pump never saw does not count as its block
        } else {
            const uint32_t before = sink_.delivered_;
            pump_.step(sink_, sfx_gain_.load(), music_gain_.load());
            if (sink_.delivered_ == before) continue; // the Running->Off edge delivers nothing
        }
        produced += size_t(kBlockBytes);
        frames_delivered_.fetch_add(openu5::kAudioBlockFrames);
        if (++blocks_since_publish_ >= kPublishEveryBlocks) publish_perf();
    }
}

void SDLCALL SdlAudioBackend::on_audio(void *user, SDL_AudioStream *, int additional_amount, int) {
    auto *self = static_cast<SdlAudioBackend *>(user);
    if (additional_amount > 0) self->produce(size_t(additional_amount));
}

void SdlAudioBackend::render_offline(int16_t *out, size_t frames) {
    sink_.offline = out;
    sink_.offline_frames = frames;
    sink_.offline_used = 0;
    ensure_started();
    if (sink_.carry_count) {
        const size_t take = std::min(sink_.carry_count, frames);
        std::memcpy(out, sink_.carry, take * sizeof(int16_t));
        std::memmove(sink_.carry, sink_.carry + take, (sink_.carry_count - take) * sizeof(int16_t));
        sink_.carry_count -= take;
        sink_.offline_used = take;
    }
    while (sink_.offline_used < frames) {
        const size_t before = sink_.offline_used;
        produce(size_t(kBlockBytes));
        if (sink_.offline_used == before) break;
    }
    sink_.offline = nullptr;
}

} // namespace uv
