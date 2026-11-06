#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray& TR9090Processor::soundParamIds()
{
    static const juce::StringArray ids {
        "accent","bd_tune","bd_level","bd_attack","bd_decay","sd_tune","sd_level","sd_tone","sd_snappy",
        "lt_tune","lt_decay","lt_level","mt_tune","mt_decay","mt_level","ht_tune","ht_decay","ht_level",
        "rs_level","cp_level","hh_level","ch_decay","oh_decay","cr_level","cr_tune","rd_level","rd_tune" };
    return ids;
}

static float defaultFor (const juce::String& id) { return id.endsWith ("_level") ? 0.8f : 0.5f; }

juce::AudioProcessorValueTreeState::ParameterLayout TR9090Processor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    for (auto& id : soundParamIds())
        v.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, id.toUpperCase(), 0.0f, 1.0f, defaultFor (id)));
    v.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "volume", 1 }, "Volume", 0.0f, 1.0f, 0.8f));
    v.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tempo", 1 }, "Tempo",
                                                              juce::NormalisableRange<float> (40.f, 240.f, 0.1f), 125.f));
    v.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "shuffle", 1 }, "Shuffle", 0, 6, 0));
    return { v.begin(), v.end() };
}

TR9090Processor::TR9090Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TR9090", createLayout())
{
    for (int i = 0; i < 27; ++i) raw[(size_t) i] = apvts.getRawParameterValue (soundParamIds()[i]);
    pVolume = apvts.getRawParameterValue ("volume");
    pTempo = apvts.getRawParameterValue ("tempo");
    pShuffle = apvts.getRawParameterValue ("shuffle");
    for (auto& p : pattern) for (auto& r : p) for (auto& s : r) s.store (0);
    loadFactoryPatterns();
    loadUserKitsFromDisk();
}

bool TR9090Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto o = l.getMainOutputChannelSet();
    return o == juce::AudioChannelSet::stereo() || o == juce::AudioChannelSet::mono();
}

void TR9090Processor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    bd.prepare (sr); sd.prepare (sr); lt.prepare (sr); mt.prepare (sr); ht.prepare (sr);
    rs.prepare (sr); cp.prepare (sr); ch.prepare (sr); oh.prepare (sr); cr.prepare (sr); rd.prepare (sr);
}

void TR9090Processor::updateVoiceParams()
{
    auto P = [this] (int i) { return raw[(size_t) i]->load(); };
    bd.tune = P(1); bd.level = P(2); bd.attack = P(3); bd.decay = P(4);
    sd.tune = P(5); sd.level = P(6); sd.tone = P(7); sd.snappy = P(8);
    lt.tune = P(9);  lt.decay = P(10); lt.level = P(11);
    mt.tune = P(12); mt.decay = P(13); mt.level = P(14);
    ht.tune = P(15); ht.decay = P(16); ht.level = P(17);
    rs.level = P(18); cp.level = P(19);
    ch.level = oh.level = P(20); ch.decay = P(21); oh.decay = P(22);
    cr.level = P(23); cr.tune = P(24); rd.level = P(25); rd.tune = P(26);
}

int TR9090Processor::noteToVoice (int n)
{
    switch (n)
    {
        case 35: case 36: return 0;  case 38: case 40: return 1;
        case 41: case 43: return 2;  case 45: case 47: return 3;  case 48: case 50: return 4;
        case 37: return 5; case 39: return 6; case 42: case 44: return 7; case 46: return 8;
        case 49: case 57: return 9; case 51: case 59: return 10;
        default: return -1;
    }
}

void TR9090Processor::trigger (int v, float vel)
{
    switch (v)
    {
        case 0: bd.trigger (vel); break;  case 1: sd.trigger (vel); break;
        case 2: lt.trigger (vel); break;  case 3: mt.trigger (vel); break;  case 4: ht.trigger (vel); break;
        case 5: rs.trigger (vel); break;  case 6: cp.trigger (vel); break;
        case 7: oh.choke(); ch.trigger (vel); break;      // closed hat chokes open hat
        case 8: oh.trigger (vel); break;
        case 9: cr.trigger (vel); break;  case 10: rd.trigger (vel); break;
        default: break;
    }
}

void TR9090Processor::fireStep (int step)
{
    if (step == 0)
    {
        const int q = queuedPattern.exchange (-1);
        if (q >= 0) currentPattern = q;
    }
    playingStep = step;
    const int p = currentPattern.load();
    const bool acc = pattern[p][kAccentRow][step].load (std::memory_order_relaxed) != 0;
    const float vel = acc ? 0.7f * (1.0f + 0.6f * raw[0]->load()) : 0.7f;
    for (int r = 0; r < kVoices; ++r)
        if (pattern[p][r][step].load (std::memory_order_relaxed)) trigger (r, vel);
}

void TR9090Processor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buf.clear();
    const int n = buf.getNumSamples();
    updateVoiceParams();

    if (const int m = previewMask.exchange (0))
        for (int v = 0; v < kVoices; ++v) if (m & (1 << v)) trigger (v, 0.8f);

    // ---- clock: host transport (plugin) or internal (standalone / host stopped) ----
    double bpm = pTempo->load(), ppqStart = internalPpq;
    bool play = running.load(), hostDriven = false;
    if (! isStandalone())
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (pos->getIsPlaying())
                {
                    hostDriven = true; play = true;
                    if (auto b = pos->getBpm()) bpm = *b;
                    if (auto q = pos->getPpqPosition()) ppqStart = *q;
                }

    if (play && ! wasPlaying && ! hostDriven) { internalPpq = ppqStart = 0.0; }

    struct Ev { int off, step; } evs[64]; int ne = 0;
    const double ppqPerSample = bpm / 60.0 / sr, ppqEnd = ppqStart + n * ppqPerSample;
    if (play)
    {
        const double swing = (pShuffle->load() / 6.0) * 0.25 * 0.33; // odd 16ths delayed up to ~1/3 step
        const long first = (long) std::floor (ppqStart * 4.0) - 1, last = (long) std::floor (ppqEnd * 4.0);
        for (long k = std::max (0L, first); k <= last && ne < 64; ++k)
        {
            const double t = k * 0.25 + ((k & 1) ? swing : 0.0);
            if (t >= ppqStart && t < ppqEnd)
                evs[ne++] = { juce::jlimit (0, n - 1, (int) ((t - ppqStart) / ppqPerSample)), (int) (k % kSteps) };
        }
        if (! hostDriven) internalPpq = ppqEnd;
    }
    else playingStep = -1;
    wasPlaying = play;

    // ---- render ----
    const float vol = pVolume->load() * 1.4f;
    auto* L = buf.getWritePointer (0);
    auto* R = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : nullptr;
    auto mit = midi.cbegin(); int ei = 0;
    for (int i = 0; i < n; ++i)
    {
        while (ei < ne && evs[ei].off <= i) fireStep (evs[ei++].step);
        while (mit != midi.cend() && (*mit).samplePosition <= i)
        {
            const auto msg = (*mit).getMessage();
            if (msg.isNoteOn()) { const int v = noteToVoice (msg.getNoteNumber()); if (v >= 0) trigger (v, msg.getFloatVelocity()); }
            ++mit;
        }
        float s = bd.tick() + sd.tick() + lt.tick() + mt.tick() + ht.tick() + rs.tick() + cp.tick()
                + ch.tick() + oh.tick() + cr.tick() + rd.tick();
        s = std::tanh (s * vol);
        L[i] = s; if (R) R[i] = s;
    }
    midi.clear();
}

// ---------------- Patterns ----------------
juce::String TR9090Processor::encodePattern (int p) const
{
    juce::String s;
    for (int r = 0; r < kRows; ++r) for (int st = 0; st < kSteps; ++st) s << (pattern[p][r][st].load() ? '1' : '0');
    return s;
}

void TR9090Processor::decodePattern (int p, const juce::String& s)
{
    for (int r = 0; r < kRows; ++r) for (int st = 0; st < kSteps; ++st)
    {
        const int i = r * kSteps + st;
        pattern[p][r][st].store (i < s.length() && (s[i] == '1' || s[i] == 'x') ? 1 : 0);
    }
}

void TR9090Processor::loadFactoryPatterns()
{
    // rows: BD SD LT MT HT RS CP CH OH CR RD AC
    struct FP { std::vector<std::pair<int, const char*>> rows; };
    const std::vector<FP> f = {
        { { {0,"x...x...x...x..."},{6,"....x.......x..."},{8,"..x...x...x...x."},{11,"x...x...x...x..."} } },                // 1 House
        { { {0,"x...x...x...x..."},{1,"....x.......x..."},{7,"xxxxxxxxxxxxxxxx"},{8,"..x...x...x...x."},{10,"x.x.x.x.x.x.x.x."},{11,"..x...x...x...x."} } }, // 2 Techno
        { { {0,"x...x...x...x.x."},{1,"....x..x....x..."},{2,"..........x....."},{3,".............x.."},{4,"..............x."},{7,"x.xxx.xxx.xxx.xx"},{8,"..x...x...x...x."} } }, // 3 Rolling
        { { {0,"x......x..x....."},{1,"....x.......x..."},{6,"....x.......x..."},{5,"..x....x..x..x.."},{7,"x.x.x.x.x.x.x.x."},{11,"x...x...x...x..."} } }, // 4 Electro
        { { {0,"x.x.......x....."},{1,"....x.......x..x"},{7,"x.x.x.x.x.x.x.x."},{8,".............x.."} } },                // 5 Breaks
        { { {0,"x...x...x...x..."},{8,"..x...x...x...x."},{9,"x..............."},{11,"xxxxxxxxxxxxxxxx"} } },                // 6 Hard
        { { {1,"x.x.x.x.x.xxxxxx"},{2,"..........x.x..."},{3,"......x.x......."},{4,"..x.x..........."},{0,"x..............."},{11,"...x...x...x.xxx"} } }, // 7 Fill
    };
    for (int p = 0; p < (int) f.size(); ++p)
        for (auto& [row, str] : f[(size_t) p].rows)
            for (int s = 0; s < kSteps; ++s) pattern[p][row][s].store (str[s] == 'x' ? 1 : 0);
}

// ---------------- Kits (sound memory bank) ----------------
struct FactoryKit { const char* name; std::vector<std::pair<const char*, float>> o; };
static const std::vector<FactoryKit>& factoryKits()
{
    static const std::vector<FactoryKit> k = {
        { "909 Classic", {} },
        { "Hard Techno", { {"bd_decay",.75f},{"bd_attack",.8f},{"bd_tune",.35f},{"sd_snappy",.7f},{"oh_decay",.35f},{"ch_decay",.3f},{"accent",.8f} } },
        { "Deep House",  { {"bd_decay",.6f},{"bd_attack",.3f},{"bd_tune",.45f},{"sd_tone",.3f},{"sd_level",.6f},{"oh_decay",.6f},{"cp_level",.9f},{"hh_level",.6f} } },
        { "Gabber",      { {"bd_decay",.95f},{"bd_attack",1.f},{"bd_tune",.2f},{"bd_level",1.f},{"accent",1.f},{"sd_snappy",.9f} } },
        { "Electro Tight",{ {"bd_decay",.3f},{"sd_tune",.7f},{"sd_snappy",.4f},{"ch_decay",.1f},{"oh_decay",.25f},{"rs_level",.9f},{"lt_decay",.3f},{"mt_decay",.3f},{"ht_decay",.3f} } },
        { "Lo-Fi Dusty", { {"bd_tune",.3f},{"sd_tone",.15f},{"sd_tune",.3f},{"hh_level",.5f},{"cr_tune",.2f},{"rd_tune",.25f},{"accent",.3f} } },
    };
    return k;
}

int TR9090Processor::getNumFactoryKits() const { return (int) factoryKits().size(); }
juce::String TR9090Processor::getFactoryKitName (int i) const { return factoryKits()[(size_t) i].name; }

void TR9090Processor::setSoundParam (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void TR9090Processor::loadFactoryKit (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumFactoryKits())) return;
    for (auto& id : soundParamIds()) setSoundParam (id, defaultFor (id));
    for (auto& [id, v] : factoryKits()[(size_t) i].o) setSoundParam (id, v);
}

void TR9090Processor::storeUserKit (int slot, const juce::String& name)
{
    if (! juce::isPositiveAndBelow (slot, kUserKits)) return;
    userKit[slot].clear();
    for (int i = 0; i < 27; ++i) userKit[slot].push_back (raw[(size_t) i]->load());
    userKitName[slot] = name; userKitUsed[slot] = true;
    saveUserKitsToDisk();
}

void TR9090Processor::loadUserKit (int slot)
{
    if (! juce::isPositiveAndBelow (slot, kUserKits) || ! userKitUsed[slot]) return;
    for (int i = 0; i < 27 && i < (int) userKit[slot].size(); ++i) setSoundParam (soundParamIds()[i], userKit[slot][(size_t) i]);
}

juce::File TR9090Processor::userKitFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("TR9090").getChildFile ("UserKits.xml");
}

static void kitsToXml (juce::XmlElement& parent, const juce::String* names, const bool* used, const std::vector<float>* kits)
{
    for (int s = 0; s < kUserKits; ++s)
    {
        if (! used[s]) continue;
        auto* e = parent.createNewChildElement ("Kit");
        e->setAttribute ("slot", s); e->setAttribute ("name", names[s]);
        juce::StringArray vals; for (auto v : kits[s]) vals.add (juce::String (v, 4));
        e->setAttribute ("v", vals.joinIntoString (","));
    }
}

void TR9090Processor::loadUserKitsFromDisk()
{
    if (auto xml = juce::XmlDocument::parse (userKitFile()))
        for (auto* e : xml->getChildWithTagNameIterator ("Kit"))
        {
            const int s = e->getIntAttribute ("slot", -1);
            if (! juce::isPositiveAndBelow (s, kUserKits)) continue;
            userKit[s].clear();
            for (auto& t : juce::StringArray::fromTokens (e->getStringAttribute ("v"), ",", "")) userKit[s].push_back (t.getFloatValue());
            userKitName[s] = e->getStringAttribute ("name"); userKitUsed[s] = true;
        }
}

void TR9090Processor::saveUserKitsToDisk()
{
    juce::XmlElement root ("UserKits");
    kitsToXml (root, userKitName, userKitUsed, userKit);
    userKitFile().getParentDirectory().createDirectory();
    root.writeTo (userKitFile());
}

std::unique_ptr<juce::XmlElement> TR9090Processor::bankToXml() const
{
    auto x = std::make_unique<juce::XmlElement> ("TR9090Bank");
    for (int p = 0; p < kPatterns; ++p)
    {
        auto* e = x->createNewChildElement ("Pattern");
        e->setAttribute ("index", p); e->setAttribute ("data", encodePattern (p));
    }
    kitsToXml (*x, userKitName, userKitUsed, userKit);
    return x;
}

void TR9090Processor::bankFromXml (const juce::XmlElement& x)
{
    for (auto* e : x.getChildWithTagNameIterator ("Pattern"))
    {
        const int p = e->getIntAttribute ("index", -1);
        if (juce::isPositiveAndBelow (p, kPatterns)) decodePattern (p, e->getStringAttribute ("data"));
    }
    bool anyKit = false;
    for (auto* e : x.getChildWithTagNameIterator ("Kit"))
    {
        const int s = e->getIntAttribute ("slot", -1);
        if (! juce::isPositiveAndBelow (s, kUserKits)) continue;
        userKit[s].clear();
        for (auto& t : juce::StringArray::fromTokens (e->getStringAttribute ("v"), ",", "")) userKit[s].push_back (t.getFloatValue());
        userKitName[s] = e->getStringAttribute ("name"); userKitUsed[s] = true; anyKit = true;
    }
    if (anyKit) saveUserKitsToDisk();
}

// ---------------- State ----------------
void TR9090Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto xml = apvts.copyState().createXml();
    auto* pats = xml->createNewChildElement ("Patterns");
    pats->setAttribute ("current", currentPattern.load());
    for (int p = 0; p < kPatterns; ++p) pats->setAttribute ("p" + juce::String (p), encodePattern (p));
    copyXmlToBinary (*xml, dest);
}

void TR9090Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml) return;
    if (auto* pats = xml->getChildByName ("Patterns"))
    {
        for (int p = 0; p < kPatterns; ++p)
            if (pats->hasAttribute ("p" + juce::String (p))) decodePattern (p, pats->getStringAttribute ("p" + juce::String (p)));
        currentPattern = juce::jlimit (0, kPatterns - 1, pats->getIntAttribute ("current", 0));
    }
    xml->deleteAllChildElementsWithTagName ("Patterns");
    if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* TR9090Processor::createEditor() { return new TR9090Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TR9090Processor(); }
