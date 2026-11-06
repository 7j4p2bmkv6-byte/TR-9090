#pragma once
// TR-9090 voice models: analog-style synthesis of the 909 voices
// (BD/SD/Toms/Rim/Clap) plus 6-oscillator metallic models for hats & cymbals.
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace tr
{
constexpr float twoPi = 6.28318530718f;

inline float decayCoef (double sr, float seconds) // time to -60 dB
{
    return std::pow (0.001f, 1.0f / (std::max (seconds, 0.001f) * (float) sr));
}

struct Noise
{
    uint32_t s = 0x9E3779B9u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.0f / 2147483648.0f); }
};

struct SVF // Cytomic / Simper state-variable filter
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set (double sr, float fc, float q)
    {
        fc = std::clamp (fc, 20.0f, (float) sr * 0.45f);
        const float g = std::tan (3.14159265f * fc / (float) sr);
        k = 1.0f / q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void tick (float v0, float& l, float& b, float& h)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        l = v2; b = v1; h = v0 - k * v1 - v2;
    }
    float lp (float x) { float l, b, h; tick (x, l, b, h); return l; }
    float bp (float x) { float l, b, h; tick (x, l, b, h); return b; }
    float hp (float x) { float l, b, h; tick (x, l, b, h); return h; }
};

inline float wrap (float p) { return p - std::floor (p); }

struct Kick
{
    double sr = 44100; float tune = .5f, attack = .5f, decay = .5f, level = .8f;
    float ph = 0, amp = 0, ampC = 0, pe = 0, peC = 0, cl = 0, clC = 0, vel = 0; bool on = false;
    Noise nz; SVF f;
    void prepare (double s) { sr = s; f.set (sr, 2500.f, 0.8f); }
    void trigger (float v)
    {
        vel = v; ph = 0; amp = pe = cl = 1; on = true;
        ampC = decayCoef (sr, 0.12f + decay * 1.1f);
        peC  = decayCoef (sr, 0.045f + 0.02f * tune);
        clC  = decayCoef (sr, 0.006f);
    }
    float tick()
    {
        if (! on) return 0;
        const float base = 44.f + tune * 24.f;
        ph = wrap (ph + base * (1.f + 3.2f * pe) / (float) sr);
        const float body  = std::tanh (2.0f * std::sin (twoPi * ph)) * 0.8f;
        const float click = (f.bp (nz.next()) * 1.5f + 0.8f) * cl * attack;
        const float o = (body * amp + click) * vel * level;
        amp *= ampC; pe *= peC; cl *= clC;
        if (amp < 1e-4f) on = false;
        return o;
    }
};

struct Snare
{
    double sr = 44100; float tune = .5f, tone = .5f, snappy = .5f, level = .8f;
    float p1 = 0, p2 = 0, aT = 0, cT = 0, aN = 0, cN = 0, pe = 0, peC = 0, vel = 0; bool on = false;
    Noise nz; SVF lp, hp;
    void prepare (double s) { sr = s; hp.set (sr, 1200.f, 0.7f); }
    void trigger (float v)
    {
        vel = v; aT = aN = pe = 1; on = true; p1 = p2 = 0;
        lp.set (sr, 2500.f + tone * 9000.f, 0.6f);
        cT = decayCoef (sr, 0.16f);
        cN = decayCoef (sr, 0.15f + 0.2f * tone);
        peC = decayCoef (sr, 0.03f);
    }
    float tick()
    {
        if (! on) return 0;
        const float f1 = (165.f + tune * 90.f) * (1.f + 0.25f * pe), f2 = f1 * 1.68f;
        p1 = wrap (p1 + f1 / (float) sr); p2 = wrap (p2 + f2 / (float) sr);
        const float body  = (std::sin (twoPi * p1) * 0.65f + std::sin (twoPi * p2) * 0.35f) * aT;
        const float noise = hp.hp (lp.lp (nz.next())) * aN * (0.2f + snappy * 1.3f);
        const float o = (body * 0.9f + noise) * vel * level;
        aT *= cT; aN *= cN; pe *= peC;
        if (aT < 1e-4f && aN < 1e-4f) on = false;
        return o;
    }
};

struct Tom
{
    double sr = 44100; float lo, hi, tune = .5f, decay = .5f, level = .8f;
    float ph = 0, amp = 0, ampC = 0, pe = 0, peC = 0, aN = 0, cN = 0, vel = 0; bool on = false;
    Noise nz; SVF f;
    Tom (float l, float h) : lo (l), hi (h) {}
    void prepare (double s) { sr = s; f.set (sr, lo * 6.f, 0.7f); }
    void trigger (float v)
    {
        vel = v; ph = 0; amp = pe = aN = 1; on = true;
        ampC = decayCoef (sr, 0.15f + decay * 0.9f);
        peC = decayCoef (sr, 0.12f); cN = decayCoef (sr, 0.05f);
    }
    float tick()
    {
        if (! on) return 0;
        const float base = lo + (hi - lo) * tune;
        ph = wrap (ph + base * (1.f + 0.5f * pe) / (float) sr);
        const float s = std::sin (twoPi * ph) * amp + f.lp (nz.next()) * aN * 0.25f;
        amp *= ampC; pe *= peC; aN *= cN;
        if (amp < 1e-4f) on = false;
        return std::tanh (s * 1.2f) * vel * level;
    }
};

struct Rim
{
    double sr = 44100; float level = .8f, p1 = 0, p2 = 0, amp = 0, ampC = 0, vel = 0; bool on = false; SVF f;
    void prepare (double s) { sr = s; f.set (sr, 1800.f, 1.5f); }
    void trigger (float v) { vel = v; amp = 1; on = true; p1 = p2 = 0; ampC = decayCoef (sr, 0.035f); }
    float tick()
    {
        if (! on) return 0;
        p1 = wrap (p1 + 500.f / (float) sr); p2 = wrap (p2 + 1650.f / (float) sr);
        const float s = std::sin (twoPi * p1) * 0.5f + std::sin (twoPi * p2) * 0.7f;
        const float o = std::tanh (f.bp (s) * 3.f) * amp * vel * level;
        amp *= ampC; if (amp < 1e-4f) on = false;
        return o;
    }
};

struct Clap
{
    double sr = 44100; float level = .8f, t = 0, vel = 0; bool on = false; Noise nz; SVF f;
    void prepare (double s) { sr = s; f.set (sr, 1150.f, 1.8f); }
    void trigger (float v) { vel = v; t = 0; on = true; }
    float tick()
    {
        if (! on) return 0;
        const float ts = t / (float) sr; t += 1;
        float e;
        if (ts < 0.033f) e = std::exp (-std::fmod (ts, 0.011f) * 300.f);   // 3 fast "hands"
        else             e = 0.8f * std::exp (-(ts - 0.033f) * 14.f);       // reverb-ish tail
        if (ts > 0.6f) on = false;
        return f.bp (nz.next()) * e * 1.6f * vel * level;
    }
};

struct Metal // six detuned square oscillators (808/909-style metallic source)
{
    float ph[6] = {};
    static constexpr float base[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
    float tick (double sr, float scale)
    {
        float s = 0;
        for (int i = 0; i < 6; ++i) { ph[i] = wrap (ph[i] + base[i] * scale / (float) sr); s += ph[i] < 0.5f ? 1.f : -1.f; }
        return s / 6.f;
    }
};

struct Hat
{
    double sr = 44100; float decay = .5f, level = .8f, minT, maxT;
    float amp = 0, ampC = 0, vel = 0; bool on = false; Metal m; Noise nz; SVF b, h;
    Hat (float mn, float mx) : minT (mn), maxT (mx) {}
    void prepare (double s) { sr = s; b.set (sr, 10000.f, 1.2f); h.set (sr, 7000.f, 0.7f); }
    void trigger (float v) { vel = v; amp = 1; on = true; ampC = decayCoef (sr, minT + decay * (maxT - minT)); }
    void choke() { if (on) ampC = decayCoef (sr, 0.01f); }
    float tick()
    {
        if (! on) return 0;
        float s = m.tick (sr, 1.6f) * 0.7f + nz.next() * 0.3f;
        s = h.hp (b.bp (s));
        const float o = s * amp * vel * level * 1.6f;
        amp *= ampC; if (amp < 1e-4f) on = false;
        return o;
    }
};

struct Cymbal
{
    double sr = 44100; float tune = .5f, level = .8f, baseT, fc, scaleBase;
    float amp = 0, ampC = 0, vel = 0; bool on = false; Metal m; Noise nz; SVF b, h;
    Cymbal (float t, float f, float sc) : baseT (t), fc (f), scaleBase (sc) {}
    void prepare (double s) { sr = s; b.set (sr, fc, 0.9f); h.set (sr, fc * 0.6f, 0.7f); }
    void trigger (float v) { vel = v; amp = 1; on = true; ampC = decayCoef (sr, baseT * (0.6f + tune * 0.8f)); }
    float tick()
    {
        if (! on) return 0;
        float s = m.tick (sr, scaleBase * (0.8f + tune * 0.4f)) * 0.75f + nz.next() * 0.25f;
        s = h.hp (b.bp (s));
        const float o = s * amp * vel * level * 1.5f;
        amp *= ampC; if (amp < 1e-4f) on = false;
        return o;
    }
};
} // namespace tr
