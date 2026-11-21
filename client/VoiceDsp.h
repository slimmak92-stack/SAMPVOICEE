/*
    Voice DSP (added): noise gate -> voice effect -> soft limiter

    Pure C++ (no BASS / Windows dependencies), so it can be unit tested on any platform.
    Process() is called from the BASS recording thread; the Set* functions may be called
    from any other thread.
*/

#pragma once

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <algorithm>

namespace voice_dsp {

enum Effect : int
{
    EffectNone = 0, // untouched voice
    EffectDeep,     // lower pitch
    EffectHigh,     // higher pitch
    EffectRobot,    // ring modulator
    EffectRadio,    // band limited + light saturation
    // --- added in the "voice lab" update (order of the first five is kept: old configs stay valid) ---
    EffectGiant,    // very low pitch
    EffectChipmunk, // very high pitch
    EffectMegaphone,// loud-hailer: narrow band + hard drive
    EffectTelephone,// 300 .. 3400 Hz phone line
    EffectEcho,     // repeating echo
    EffectCave,     // small reverb room
    EffectAlien,    // pitch up + metallic ring modulation
    EffectUnderwater,// muffled + wobbling pitch
    EffectCustom,   // free pitch shift, -12 .. +12 semitones

    EffectCount
};

// Tunables
// --------------------------------------------

constexpr bool  kGateEnabled   = true;   // default, adjustable at runtime (SetGate)
constexpr float kGateThreshold = 0.004F; // default RMS threshold of one frame (~ -48 dBFS)
constexpr float kGateFloor     = 0.10F;  // gain while gate is closed (not a hard mute)
constexpr float kGateHoldSec   = 0.25F;  // keep open this long after the last loud frame
constexpr float kGateOpenSec   = 0.005F; // ramp time closed -> open
constexpr float kGateCloseSec  = 0.060F; // ramp time open -> closed
constexpr float kGateNoiseMul  = 3.0F;   // gate opens at least this far above the measured noise floor
constexpr float kGateHystMul   = 0.60F;  // stays open until the level falls below open * this

constexpr float kAgcTargetRms  = 0.12F;  // desired speech level (~ -18 dBFS RMS)
constexpr float kAgcMaxGain    = 4.0F;   // +12 dB
constexpr float kAgcMinGain    = 0.35F;  // -9 dB
constexpr float kAgcAttack     = 0.50F;  // per frame, when the voice is too loud (fast)
constexpr float kAgcRelease    = 0.15F;  // per frame, when the voice is too quiet (slow)

constexpr float kLimiterKnee   = 0.85F;  // soft limiter starts here, output never exceeds 1.0

constexpr float kDeepRatio     = 0.80F;  // ~ -3.9 semitones
constexpr float kHighRatio     = 1.30F;  // ~ +4.5 semitones
constexpr float kRobotFreqHz   = 50.0F;
constexpr float kRobotDry      = 0.25F;

constexpr float kGiantRatio    = 0.62F;
constexpr float kChipmunkRatio = 1.60F;
constexpr float kAlienRatio    = 1.18F;
constexpr float kAlienFreqHz   = 90.0F;
constexpr float kAlienDry      = 0.45F;
constexpr float kEchoDelaySec  = 0.28F;
constexpr float kEchoFeedback  = 0.38F;
constexpr float kEchoWet       = 0.45F;
constexpr float kWaterLowPass  = 1000.0F;
constexpr float kWaterLfoHz    = 4.5F;
constexpr float kWaterDepth    = 0.025F;

constexpr float kLowCutHz      = 90.0F;  // removes rumble / desk bumps / plosive thumps
constexpr float kCompThreshold = 0.22F;  // ~ -13 dBFS
constexpr float kCompRatio     = 3.0F;
constexpr float kCompAttackSec = 0.005F;
constexpr float kCompReleaseSec= 0.120F;
constexpr float kCompMakeup    = 1.5F;   // ~ +3.5 dB

constexpr int   kCustomPitchMin = -12;
constexpr int   kCustomPitchMax =  12;

// Building blocks
// --------------------------------------------

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void Reset() noexcept { z1 = z2 = 0; }

    void SetLowPass(const float sr, const float f, const float q = 0.7071F) noexcept
    {
        const float w = 2 * 3.14159265358979F * f / sr, c = std::cos(w), s = std::sin(w), al = s / (2 * q);
        const float a0 = 1 + al;
        b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0;
        a1 = -2 * c / a0;      a2 = (1 - al) / a0;
    }

    void SetHighPass(const float sr, const float f, const float q = 0.7071F) noexcept
    {
        const float w = 2 * 3.14159265358979F * f / sr, c = std::cos(w), s = std::sin(w), al = s / (2 * q);
        const float a0 = 1 + al;
        b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0;
        a1 = -2 * c / a0;      a2 = (1 - al) / a0;
    }

    float Process(const float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Two-tap crossfaded delay line pitch shifter (low latency, fine for small shifts)
struct PitchShifter
{
    static constexpr size_t kSize = 8192; // power of two, > 2 * window

    float  buffer[kSize];
    size_t write  = 0;
    float  phase  = 0;
    float  window = 1440; // samples (30 ms @ 48 kHz)

    void Reset() noexcept
    {
        std::memset(buffer, 0, sizeof(buffer));
        write = 0;
        phase = 0;
    }

    float Process(const float x, const float ratio) noexcept
    {
        buffer[write] = x;

        phase += (1.0F - ratio) / window;
        phase -= std::floor(phase);

        float out = 0;

        for (int tap = 0; tap != 2; ++tap)
        {
            float p = phase + 0.5F * static_cast<float>(tap);
            if (p >= 1.0F) p -= 1.0F;

            const float delay = p * window;
            const float gain  = 0.5F - 0.5F * std::cos(6.28318530717959F * p);

            float position = static_cast<float>(write) - delay;
            if (position < 0) position += static_cast<float>(kSize);

            const size_t i0 = static_cast<size_t>(position) & (kSize - 1);
            const size_t i1 = (i0 + 1) & (kSize - 1);
            const float  fr = position - std::floor(position);

            out += gain * (buffer[i0] + (buffer[i1] - buffer[i0]) * fr);
        }

        write = (write + 1) & (kSize - 1);

        return out;
    }
};

// Feedback comb (reverb) with damping, and a Schroeder all-pass
struct Comb
{
    static constexpr size_t kSize = 8192;

    float  buffer[kSize];
    size_t write = 0;
    size_t delay = 1000;
    float  damp  = 0;

    void Reset(const size_t length) noexcept
    {
        std::memset(buffer, 0, sizeof(buffer));
        write = 0;
        damp  = 0;
        delay = length < 1 ? 1 : (length >= kSize ? kSize - 1 : length);
    }

    float Process(const float x, const float feedback, const float damping) noexcept
    {
        const float y = buffer[(write + kSize - delay) & (kSize - 1)];
        damp = y * (1.0F - damping) + damp * damping;
        buffer[write] = x + damp * feedback;
        write = (write + 1) & (kSize - 1);
        return y;
    }
};

struct AllPass
{
    static constexpr size_t kSize = 2048;

    float  buffer[kSize];
    size_t write = 0;
    size_t delay = 200;

    void Reset(const size_t length) noexcept
    {
        std::memset(buffer, 0, sizeof(buffer));
        write = 0;
        delay = length < 1 ? 1 : (length >= kSize ? kSize - 1 : length);
    }

    float Process(const float x) noexcept
    {
        const float b = buffer[(write + kSize - delay) & (kSize - 1)];
        buffer[write] = x + b * 0.5F;
        write = (write + 1) & (kSize - 1);
        return b - x;
    }
};

// Plain delay line (echo)
struct DelayLine
{
    static constexpr size_t kSize = 32768;

    float  buffer[kSize];
    size_t write = 0;

    void Reset() noexcept
    {
        std::memset(buffer, 0, sizeof(buffer));
        write = 0;
    }

    float Read(const size_t distance) const noexcept
    {
        return buffer[(write + kSize - distance) & (kSize - 1)];
    }

    void Push(const float x) noexcept
    {
        buffer[write] = x;
        write = (write + 1) & (kSize - 1);
    }
};

inline float SoftLimit(const float x) noexcept
{
    if (!std::isfinite(x)) return 0;

    const float a = std::fabs(x);
    if (a <= kLimiterKnee) return x;

    const float room = 1.0F - kLimiterKnee;
    const float y = kLimiterKnee + room * std::tanh((a - kLimiterKnee) / room);

    return x < 0 ? -y : y;
}

// Processor
// --------------------------------------------

struct Processor
{
    void Initialize(const float sample_rate) noexcept
    {
        _rate = sample_rate;
        _shifter.window = 0.030F * sample_rate;
        _reset.store(true);
    }

    void SetEffect(const int effect) noexcept
    {
        _requested.store(effect >= 0 && effect < EffectCount ? effect : EffectNone);
    }

    int GetEffect() const noexcept
    {
        return _requested.load();
    }

    void RequestReset() noexcept
    {
        _reset.store(true);
    }

    void SetGate(const bool enabled, const float threshold) noexcept
    {
        _gate_enabled.store(enabled);
        _gate_threshold.store(std::clamp(threshold, 0.0002F, 0.2F));
    }

    void SetAutoGain(const bool enabled) noexcept
    {
        _agc_enabled.store(enabled);
    }

    void SetLowCut(const bool enabled) noexcept
    {
        _lowcut_enabled.store(enabled);
    }

    void SetCompressor(const bool enabled) noexcept
    {
        _comp_enabled.store(enabled);
    }

    void SetCustomPitch(const int semitones) noexcept
    {
        _custom_pitch.store(std::clamp(semitones, kCustomPitchMin, kCustomPitchMax));
    }

    int GetCustomPitch() const noexcept
    {
        return _custom_pitch.load();
    }

    // peak of the last processed frame before gate / effects, 0 .. 1 (for the level meter)
    float GetInputLevel() const noexcept
    {
        return _level_in.load();
    }

    // peak of the last processed frame after everything (what is really sent), 0 .. 1
    float GetOutputLevel() const noexcept
    {
        return _level_out.load();
    }

    // in and out may alias
    void Process(const float* const in, float* const out, const size_t count) noexcept
    {
        if (count == 0) return;

        const int requested = _requested.load();

        if (_reset.exchange(false) || requested != _active)
        {
            _active = requested;
            ResetState();
        }

        // Low cut (optional, before everything else so the gate does not react to rumble)
        // --------------------------------------------

        const float* src = in;

        if (_lowcut_enabled.load())
        {
            for (size_t i = 0; i != count; ++i)
                out[i] = _lowcut.Process(in[i]);
            src = out;
        }
        else _lowcut.Reset();

        // Gate + auto gain (one decision per frame, smooth gain per sample)
        // --------------------------------------------

        double energy = 0;
        float peak_in = 0;
        for (size_t i = 0; i != count; ++i)
        {
            energy += static_cast<double>(src[i]) * src[i];
            peak_in = (std::max)(peak_in, std::fabs(src[i]));
        }

        _level_in.store((std::min)(peak_in, 1.0F));

        const float rms = static_cast<float>(std::sqrt(energy / static_cast<double>(count)));
        const float user_threshold = _gate_threshold.load();

        // adaptive noise floor: follows quiet frames quickly, drifts up only slowly and never
        // chases speech, so the gate also works in a noisy room
        if (rms < _noise) _noise += (rms - _noise) * 0.5F;
        else if (rms < _noise * 2.5F) _noise += (rms - _noise) * 0.05F;
        _noise = std::clamp(_noise, 0.0005F, 0.05F);

        const float noise_level = _noise * kGateNoiseMul;
        const float open_level  = user_threshold > noise_level ? user_threshold : noise_level;
        const float close_level = open_level * kGateHystMul;

        bool speech;
        if (rms >= open_level || (_open && rms >= close_level))
        {
            speech = true;
            _hold = static_cast<long>(kGateHoldSec * _rate);
        }
        else
        {
            _hold -= static_cast<long>(count);
            speech = _hold > 0;
        }
        _open = speech;

        const bool gate_on = _gate_enabled.load();
        const float target = (!gate_on || speech) ? 1.0F : kGateFloor;
        const float up     = (1.0F - kGateFloor) / (kGateOpenSec  * _rate);
        const float down   = (1.0F - kGateFloor) / (kGateCloseSec * _rate);

        // auto gain only learns while somebody is actually speaking
        const float agc_prev = _agc;
        if (_agc_enabled.load())
        {
            if (rms >= open_level && speech)
            {
                const float wanted = std::clamp(kAgcTargetRms / rms, kAgcMinGain, kAgcMaxGain);
                _agc += (wanted - _agc) * (wanted < _agc ? kAgcAttack : kAgcRelease);
            }
        }
        else _agc += (1.0F - _agc) * 0.5F;

        for (size_t i = 0; i != count; ++i)
        {
            _gain += std::clamp(target - _gain, -down, up);
            const float agc = agc_prev + (_agc - agc_prev) * (static_cast<float>(i) / static_cast<float>(count));
            out[i] = src[i] * _gain * agc;
        }

        // Compressor (optional): evens out loud and quiet parts, with make-up gain
        // --------------------------------------------

        if (_comp_enabled.load())
        {
            const float atk = 1.0F - std::exp(-1.0F / (kCompAttackSec  * _rate));
            const float rel = 1.0F - std::exp(-1.0F / (kCompReleaseSec * _rate));
            const float exponent = 1.0F / kCompRatio - 1.0F;

            for (size_t i = 0; i != count; ++i)
            {
                const float a = std::fabs(out[i]);
                _comp_env += (a - _comp_env) * (a > _comp_env ? atk : rel);

                float g = 1.0F;
                if (_comp_env > kCompThreshold)
                    g = std::pow(_comp_env / kCompThreshold, exponent);

                out[i] *= g * kCompMakeup;
            }
        }
        else _comp_env = 0;

        // Effect
        // --------------------------------------------

        switch (_active)
        {
            case EffectDeep:
                for (size_t i = 0; i != count; ++i) out[i] = _shifter.Process(out[i], kDeepRatio);
                break;
            case EffectHigh:
                for (size_t i = 0; i != count; ++i) out[i] = _shifter.Process(out[i], kHighRatio);
                break;
            case EffectGiant:
                for (size_t i = 0; i != count; ++i) out[i] = _shifter.Process(out[i], kGiantRatio);
                break;
            case EffectChipmunk:
                for (size_t i = 0; i != count; ++i) out[i] = _shifter.Process(out[i], kChipmunkRatio);
                break;
            case EffectCustom:
            {
                const int st = _custom_pitch.load();
                if (st != 0)
                {
                    const float ratio = std::pow(2.0F, static_cast<float>(st) / 12.0F);
                    for (size_t i = 0; i != count; ++i) out[i] = _shifter.Process(out[i], ratio);
                }
                break;
            }
            case EffectRobot:
                for (size_t i = 0; i != count; ++i)
                {
                    const float carrier = std::sin(_robot_phase);
                    _robot_phase += 6.28318530717959F * kRobotFreqHz / _rate;
                    if (_robot_phase > 6.28318530717959F) _robot_phase -= 6.28318530717959F;
                    out[i] = out[i] * (kRobotDry + (1.0F - kRobotDry) * carrier);
                }
                break;
            case EffectAlien:
                for (size_t i = 0; i != count; ++i)
                {
                    const float v = _shifter.Process(out[i], kAlienRatio);
                    const float carrier = std::sin(_robot_phase);
                    _robot_phase += 6.28318530717959F * kAlienFreqHz / _rate;
                    if (_robot_phase > 6.28318530717959F) _robot_phase -= 6.28318530717959F;
                    out[i] = v * (kAlienDry + (1.0F - kAlienDry) * carrier);
                }
                break;
            case EffectRadio:
                for (size_t i = 0; i != count; ++i)
                {
                    float v = _bp_hp.Process(out[i]);
                    v = _bp_lp.Process(v);
                    out[i] = std::tanh(2.2F * v) * 0.75F;
                }
                break;
            case EffectMegaphone:
                for (size_t i = 0; i != count; ++i)
                {
                    float v = _bp_hp.Process(out[i]);
                    v = _bp_lp2.Process(_bp_lp.Process(v));
                    out[i] = std::tanh(5.0F * v) * 0.7F;
                }
                break;
            case EffectTelephone:
                for (size_t i = 0; i != count; ++i)
                {
                    float v = _bp_hp.Process(out[i]);
                    v = _bp_lp2.Process(_bp_lp.Process(v));
                    out[i] = std::tanh(1.6F * v) * 0.9F;
                }
                break;
            case EffectEcho:
            {
                const size_t d = (std::min)(static_cast<size_t>(kEchoDelaySec * _rate), DelayLine::kSize - 1);
                for (size_t i = 0; i != count; ++i)
                {
                    const float x = out[i];
                    const float wet = _echo.Read(d);
                    _echo.Push(x + wet * kEchoFeedback);
                    out[i] = x * 0.85F + wet * kEchoWet;
                }
                break;
            }
            case EffectCave:
                for (size_t i = 0; i != count; ++i)
                {
                    const float x = out[i];
                    float sum = 0;
                    for (int c = 0; c != 4; ++c) sum += _comb[c].Process(x, 0.78F, 0.30F);
                    const float wet = _allpass.Process(sum * 0.25F);
                    out[i] = x * 0.7F + wet * 0.55F;
                }
                break;
            case EffectUnderwater:
                for (size_t i = 0; i != count; ++i)
                {
                    const float lfo = std::sin(_lfo_phase);
                    _lfo_phase += 6.28318530717959F * kWaterLfoHz / _rate;
                    if (_lfo_phase > 6.28318530717959F) _lfo_phase -= 6.28318530717959F;
                    const float v = _shifter.Process(out[i], 1.0F + kWaterDepth * lfo);
                    out[i] = _bp_lp.Process(v) * 1.6F;
                }
                break;
            default: break;
        }

        // Limiter
        // --------------------------------------------

        float peak_out = 0;
        for (size_t i = 0; i != count; ++i)
        {
            out[i] = SoftLimit(out[i]);
            peak_out = (std::max)(peak_out, std::fabs(out[i]));
        }

        _level_out.store((std::min)(peak_out, 1.0F));
    }

private:

    void ResetState() noexcept
    {
        _shifter.Reset();
        _echo.Reset();
        _lowcut.SetHighPass(_rate, kLowCutHz);
        _lowcut.Reset();

        // band filters for the radio-like effects (one set, configured for the active effect)
        float hp = 350.0F, lp = 3200.0F, lp2 = 3200.0F;
        switch (_active)
        {
            case EffectMegaphone: hp = 450.0F; lp = 3800.0F; lp2 = 3800.0F; break;
            case EffectTelephone: hp = 300.0F; lp = 3400.0F; lp2 = 3400.0F; break;
            case EffectUnderwater: lp = kWaterLowPass; break;
            default: break;
        }
        _bp_hp.SetHighPass(_rate, hp);
        _bp_lp.SetLowPass(_rate, lp);
        _bp_lp2.SetLowPass(_rate, lp2);
        _bp_hp.Reset();
        _bp_lp.Reset();
        _bp_lp2.Reset();

        // small room: four comb delays (ms) + one all-pass, scaled to the sample rate
        constexpr float kCombMs[4] = { 29.7F, 37.1F, 41.1F, 43.7F };
        for (int c = 0; c != 4; ++c)
            _comb[c].Reset(static_cast<size_t>(kCombMs[c] * 0.001F * _rate));
        _allpass.Reset(static_cast<size_t>(0.005F * _rate));

        _robot_phase = 0;
        _lfo_phase = 0;
        _comp_env = 0;
        _gain = 1.0F;
        _agc = 1.0F;
        _noise = 0.002F;
        _open = false;
        _hold = 0;
    }

private:

    float _rate = 48000;

    std::atomic<int>  _requested { EffectNone };
    std::atomic<bool> _reset     { true };
    std::atomic<bool>  _gate_enabled   { kGateEnabled };
    std::atomic<float> _gate_threshold { kGateThreshold };
    std::atomic<bool>  _agc_enabled    { false };
    std::atomic<bool>  _lowcut_enabled { false };
    std::atomic<bool>  _comp_enabled   { false };
    std::atomic<int>   _custom_pitch   { 0 };
    std::atomic<float> _level_in       { 0 };
    std::atomic<float> _level_out      { 0 };

    // Touched only by Process()
    int          _active = EffectNone;
    float        _gain = 1.0F;
    float        _agc = 1.0F;
    float        _noise = 0.002F;
    bool         _open = false;
    long         _hold = 0;
    float        _robot_phase = 0;
    float        _lfo_phase = 0;
    float        _comp_env = 0;
    PitchShifter _shifter;
    DelayLine    _echo;
    Comb         _comb[4];
    AllPass      _allpass;
    Biquad       _lowcut;
    Biquad       _bp_hp, _bp_lp, _bp_lp2;
};

} // namespace voice_dsp
