#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ps;

// ---------------------------------------------------------------------------------------------
static const std::vector<ParamDef>& allDefs()
{
    static std::vector<ParamDef> d = [] { std::vector<ParamDef> v ((size_t) kNumParams); buildParamDefs (v.data()); return v; }();
    return d;
}
const ParamDef& PsyProcessor::def (int i) { return allDefs()[(size_t) i]; }

juce::AudioProcessorValueTreeState::ParameterLayout PsyProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = def (i);
        juce::ParameterID pid { juce::String (d.id), 1 };
        juce::String name (d.name);
        switch (d.kind)
        {
            case KFloat:  layout.add (std::make_unique<juce::AudioParameterFloat> (pid, name, juce::NormalisableRange<float> (d.mn, d.mx, 0.0f), d.def)); break;
            case KInt:    layout.add (std::make_unique<juce::AudioParameterInt> (pid, name, (int) d.mn, (int) d.mx, (int) d.def)); break;
            case KBool:   layout.add (std::make_unique<juce::AudioParameterBool> (pid, name, d.def > 0.5f)); break;
            default:
            {
                juce::StringArray items;
                if (i == pScale)
                {
                    int c = 0; const ScaleDef* t = scaleTable (c);
                    for (int k = 0; k < c; ++k) items.add (t[k].name);
                    items.add ("Custom");
                }
                else items = juce::StringArray::fromTokens (juce::String (d.choices), "|", "");
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, name, items, (int) d.def));
                break;
            }
        }
    }
    return layout;
}

// ---------------------------------------------------------------------------------------------
PsyProcessor::PsyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (def (i).id);
        jassert (raw[(size_t) i] != nullptr);
        P[i] = def (i).def;
    }
    for (auto& a : uiLaneStep) a.store (-1);

    Rng r (2024);
    editBank.main.clear();
    for (int i : { 0, 3, 6, 8, 11, 14 }) editBank.main.s[i].mode = 1;
    applyPsyGroove (editBank, 0, r);

    engineOwner = std::make_unique<Engine>();
    engine = engineOwner.get();
    synth = std::make_unique<DrumSynth>();
    outBuf = std::make_unique<OutBuffer>();
    engine->bank = editBank;
}

bool PsyProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PsyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    synth->prepare (sampleRate);
    engine->prepare();
    engine->seed ((uint64_t) juce::Time::currentTimeMillis());
    midiOut.ensureSize (16384);
    juce::ignoreUnused (samplesPerBlock);
    prepared = true;
}

// ---------------------------------------------------------------------------------------------
void PsyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (! prepared || buffer.getNumChannels() < 2) return;

    for (int i = 0; i < kNumParams; ++i) P[i] = raw[(size_t) i]->load();

    if (bankDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType l (bankLock);
        if (l.isLocked()) { engine->bank = editBank; bankDirty.store (false); }
    }
    if (seqResetRequested.exchange (false)) engine->requestSeqReset();

    // MIDI de entrada -> notas seguradas
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())       engine->noteOn (m.getNoteNumber(), P);
        else if (m.isNoteOff()) engine->noteOff (m.getNoteNumber(), P);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine->allNotesOff();
    }

    // transporte do host
    BlockInfo bi; bi.sampleRate = sr; bi.numSamples = n; bi.bpm = 120.0; bi.ppqStart = 0.0; bi.playing = false; bi.beatsPerBar = 4;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            bi.bpm = pos->getBpm().orFallback (120.0);
            bi.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) bi.ppqStart = *ppq;
            else if (auto secs = pos->getTimeInSeconds()) bi.ppqStart = *secs * bi.bpm / 60.0;
            if (auto ts = pos->getTimeSignature())
                bi.beatsPerBar = std::max (1, (int) std::lround (ts->numerator * 4.0 / std::max (1, ts->denominator)));
        }
    }

    engine->process (bi, P, *outBuf);

    // saída MIDI + áudio interno do PERC (sample-accurate)
    midiOut.clear();
    const bool percMidi = P[pPercMidi] > 0.5f, percAudio = P[pPercAudio] > 0.5f;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int pos = 0; const float gain = P[pPercLevel];
    for (int i = 0; i < outBuf->count; ++i)
    {
        const OutEvent& e = outBuf->e[i];
        if (percAudio && e.type == EvType::NoteOn && e.lane >= 0)
        {
            if (e.sample > pos) { synth->render (L + pos, R + pos, e.sample - pos, gain); pos = e.sample; }
            synth->trigger (e.lane, (float) e.d2, e.pan, e.pitchSemi, e.lenSec);
        }
        if (e.lane >= 0 && ! percMidi) continue;
        switch (e.type)
        {
            case EvType::NoteOn:     midiOut.addEvent (juce::MidiMessage::noteOn (e.ch + 1, e.d1, (juce::uint8) e.d2), e.sample); break;
            case EvType::NoteOff:    midiOut.addEvent (juce::MidiMessage::noteOff (e.ch + 1, e.d1), e.sample); break;
            case EvType::CC:         midiOut.addEvent (juce::MidiMessage::controllerEvent (e.ch + 1, e.d1, e.d2), e.sample); break;
            case EvType::PitchBend:  midiOut.addEvent (juce::MidiMessage::pitchWheel (e.ch + 1, e.d1 | (e.d2 << 7)), e.sample); break;
        }
    }
    if (pos < n) synth->render (L + pos, R + pos, n - pos, gain);
    midi.swapWith (midiOut);

    // feedback UI
    uiRunning.store (bi.playing || P[pFreeRun] > 0.5f);
    uiMainStep.store ((int) engine->rhythm.lastStepIndex());
    for (int l = 0; l < kNumLanes; ++l) uiLaneStep[(size_t) l].store ((int) engine->perc.lastStep[l]);
    uiLoopState.store (engine->rhythm.loopStateValue());
    uiBpm.store ((float) bi.bpm);
}

// ---------------------------------------------------------------------------------------------
void PsyProcessor::setParamReal (int index, float v)
{
    if (auto* p = apvts.getParameter (def (index).id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}
void PsyProcessor::resetAllParamsToDefault()
{
    for (int i = 0; i < kNumParams; ++i) setParamReal (i, def (i).def);
}

ps::Pattern PsyProcessor::copyPattern (int which)
{
    const juce::SpinLock::ScopedLockType l (bankLock);
    return which < 0 ? editBank.main : editBank.lane[clampi (which, 0, kNumLanes - 1)];
}
void PsyProcessor::editPattern (int which, const std::function<void (ps::Pattern&)>& fn)
{
    { const juce::SpinLock::ScopedLockType l (bankLock);
      fn (which < 0 ? editBank.main : editBank.lane[clampi (which, 0, kNumLanes - 1)]); }
    bankDirty.store (true);
}
ps::PatternBank PsyProcessor::copyBank() { const juce::SpinLock::ScopedLockType l (bankLock); return editBank; }
void PsyProcessor::setBank (const ps::PatternBank& b) { { const juce::SpinLock::ScopedLockType l (bankLock); editBank = b; } bankDirty.store (true); }

// ---------------------------------------------------------------------------------------------
void PsyProcessor::writeBankToState (juce::ValueTree& st) const
{
    auto old = st.getChildWithName ("PATTERNS"); if (old.isValid()) st.removeChild (old, nullptr);
    juce::ValueTree pt ("PATTERNS");
    PatternBank b; { auto* self = const_cast<PsyProcessor*> (this); b = self->copyBank(); }
    pt.setProperty ("main", juce::String (b.main.toString()), nullptr);
    for (int l = 0; l < kNumLanes; ++l) pt.setProperty ("lane" + juce::String (l), juce::String (b.lane[l].toString()), nullptr);
    st.appendChild (pt, nullptr);
}
bool PsyProcessor::readBankFromState (const juce::ValueTree& st)
{
    auto pt = st.getChildWithName ("PATTERNS"); if (! pt.isValid()) return false;
    PatternBank b = const_cast<PsyProcessor*> (this)->copyBank(); bool ok = true;
    ok = b.main.fromString (pt.getProperty ("main").toString().toStdString()) && ok;
    for (int l = 0; l < kNumLanes; ++l) ok = b.lane[l].fromString (pt.getProperty ("lane" + juce::String (l)).toString().toStdString()) && ok;
    setBank (b); return ok;
}

void PsyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("presetName", currentPresetName, nullptr);
    writeBankToState (st);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void PsyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            readBankFromState (st);
            currentPresetName = st.getProperty ("presetName", "Init").toString();
            auto copy = st.createCopy(); auto pt = copy.getChildWithName ("PATTERNS"); if (pt.isValid()) copy.removeChild (pt, nullptr);
            apvts.replaceState (copy);
        }
}

// ---------------------------------------------------------------------------------------------
juce::File PsyProcessor::presetDirectory()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("PsyRhythm").getChildFile ("Presets");
    d.createDirectory(); return d;
}
juce::StringArray PsyProcessor::listUserPresets() const
{
    juce::StringArray names;
    for (auto& f : presetDirectory().findChildFiles (juce::File::findFiles, false, "*.psypreset")) names.add (f.getFileNameWithoutExtension());
    names.sort (true); return names;
}
bool PsyProcessor::savePreset (const juce::String& name)
{
    auto clean = juce::File::createLegalFileName (name.trim()); if (clean.isEmpty()) return false;
    auto st = apvts.copyState(); st.setProperty ("presetName", clean, nullptr); writeBankToState (st);
    auto xml = st.createXml(); if (! xml) return false;
    auto f = presetDirectory().getChildFile (clean + ".psypreset");
    if (! xml->writeTo (f)) return false;
    currentPresetName = clean; return true;
}
bool PsyProcessor::loadPresetFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return false;
    auto st = juce::ValueTree::fromXml (*xml);
    readBankFromState (st);
    auto copy = st.createCopy(); auto pt = copy.getChildWithName ("PATTERNS"); if (pt.isValid()) copy.removeChild (pt, nullptr);
    apvts.replaceState (copy);
    currentPresetName = f.getFileNameWithoutExtension(); return true;
}

juce::StringArray PsyProcessor::factoryPresetNames()
{
    return { "Init", "Psy Rolling Arp", "Progressive Pulse", "Full-On Perc Layer", "Glitch Stutter Lead", "Hirajoshi Dark Poly", "Minimal Psy Texture" };
}
void PsyProcessor::loadFactoryPreset (int idx)
{
    resetAllParamsToDefault();
    Rng r ((uint64_t) (idx + 1) * 7919);
    PatternBank b = copyBank();
    auto S = [this] (int p, float v) { setParamReal (p, v); };
    switch (idx)
    {
        case 1: // arpejo rolante em escala frígia dominante
            S (pMode, 1); S (pScale, 10); S (pRes, 2); S (pDensity, 0.92f); S (pGroove8, 0.25f); S (pDirection, 2); S (pChance, 0.8f); S (pExtend, 0.25f);
            S (pOctRange, 2); S (pOctChance, 0.35f); S (pVelRand, 0.3f); S (pGate, 0.6f); S (pHuman, 0.15f); S (pPercOn, 0); applyPsyGroove (b, 0, r); break;
        case 2: // progressive: baixo mono com melodia curta + percussão
            S (pMode, 0); S (pScale, 2); S (pMelodyProb, 0.35f); S (pSteps, 3); S (pShape, 1); S (pDensity, 0.85f); S (pGroove4, 0.2f); S (pGate, 0.5f);
            S (pSwing, 0.1f); S (pVelRand, 0.25f); applyPsyGroove (b, 1, r); break;
        case 3: // só camada de percussão full-on
            S (pMainOn, 0); S (pPercVar, 0.5f); S (pPercFill, 1); S (pPercSwing, 0.05f); S (pPercRandom, 0.15f); applyPsyGroove (b, 0, r); break;
        case 4: // glitch + beat repeat
            S (pMode, 0); S (pScale, 5); S (pMelodyProb, 0.8f); S (pSteps, 6); S (pShape, 2); S (pGlitch, 0.5f); S (pRes, 3); S (pDensity, 0.7f);
            S (pBrOn, 1); S (pBrChance, 0.4f); S (pBrSize, 1); S (pBrInterval, 1); S (pBrDuration, 0); S (pBrStutter, -0.4f); S (pPercOn, 0); break;
        case 5: // Hirajoshi poly
            S (pMode, 1); S (pScale, 25); S (pChordBlend, 0.35f); S (pVoices, 3); S (pFlux, 0.4f); S (pSpread, 0.4f); S (pRes, 1); S (pGate, 1.2f);
            S (pStackProb, 0.2f); S (pPercOn, 0); break;
        case 6: // textura minimal
            S (pMainOn, 0); S (pPercRandom, 0.3f); S (pPercVar, 0.3f); S (pPercSwing, 0.12f); applyPsyGroove (b, 2, r); break;
        default: break;
    }
    setBank (b);
    currentPresetName = factoryPresetNames()[idx];
}

juce::AudioProcessorEditor* PsyProcessor::createEditor() { return new PsyEditor (*this); }

// Ponto de entrada exigido pelo JUCE (escopo global).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PsyProcessor();
}
