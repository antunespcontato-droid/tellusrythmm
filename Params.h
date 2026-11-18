#pragma once
// Tabela única de parâmetros (X-macro): usada pelo motor, pelo APVTS do plugin e pelos testes.
#include <cstdio>
#include "Types.h"

namespace ps {

enum ParamKind { KFloat = 0, KInt = 1, KBool = 2, KChoice = 3 };

// X(enum, id, nome, tipo, min, max, default, escolhas "A|B|C")
#define PS_GLOBAL_PARAMS(X) \
 X(Mode,        "mode",        "Mode",           KChoice, 0, 1, 0, "Mono|Poly") \
 X(Source,      "source",      "Rhythm Source",  KChoice, 0, 1, 0, "Groove|Sequencer") \
 X(MainOn,      "mainOn",      "Generator On",   KBool,   0, 1, 1, "") \
 X(OutChannel,  "outCh",       "Out Channel",    KInt,    1, 16, 1, "") \
 X(Res,         "res",         "Resolution",     KChoice, 0, 5, 2, "1/4|1/8|1/16|1/32|1/8T|1/16T") \
 X(Speed,       "speed",       "Speed",          KChoice, 0, 2, 1, "0.5x|1x|2x") \
 X(Triplet,     "triplet",     "Triplets",       KBool,   0, 1, 0, "") \
 X(Swing,       "swing",       "Shuffle",        KFloat,  0, 1, 0, "") \
 X(Human,       "human",       "Humanize",       KFloat,  0, 1, 0, "") \
 X(FreeRun,     "freeRun",     "Free Run",       KBool,   0, 1, 0, "") \
 X(Latch,       "latch",       "Latch",          KBool,   0, 1, 0, "") \
 X(Scale,       "scale",       "Scale",          KChoice, 0, 32, 2, "") /* nomes vindos de Scales.h */ \
 X(Root,        "root",        "Root",           KChoice, 0, 11, 0, "C|C#|D|D#|E|F|F#|G|G#|A|A#|B") \
 X(CustomMask,  "customMask",  "Custom Scale",   KInt,    1, 4095, 1365, "") \
 X(KeyFreeze,   "keyFreeze",   "Key Freeze",     KBool,   0, 1, 1, "") \
 X(MelodyProb,  "melodyProb",  "Melody Prob",    KFloat,  0, 1, 0.5, "") \
 X(Steps,       "steps",       "Steps",          KInt,    1, 12, 4, "") \
 X(Shape,       "shape",       "Scale Shape",    KChoice, 0, 4, 0, "Straight|Branch|Cascade|Thumb|Pinky") \
 X(Direction,   "direction",   "Direction",      KChoice, 0, 3, 2, "Up|Down|Both|Random") \
 X(OctRange,    "octRange",    "Oct Range",      KInt,    0, 3, 1, "") \
 X(OctChance,   "octChance",   "Oct Chance",     KFloat,  0, 1, 0.3, "") \
 X(Chance,      "chance",      "Chance",         KFloat,  0, 1, 0.7, "") \
 X(Extend,      "extend",      "Extend",         KFloat,  0, 1, 0.2, "") \
 X(ChordBlend,  "chordBlend",  "Fusion Fader",   KFloat,  0, 1, 0, "") \
 X(Flux,        "flux",        "Chord Flux",     KFloat,  0, 1, 0.2, "") \
 X(Spread,      "spread",      "Spread",         KFloat,  0, 1, 0.2, "") \
 X(Voices,      "voices",      "Voices",         KInt,    1, 6, 3, "") \
 X(StackProb,   "stackProb",   "Poly Stack",     KFloat,  0, 1, 0, "") \
 X(Density,     "density",     "Density",        KFloat,  0, 1, 1, "") \
 X(Groove8,     "groove8",     "Groove 1/8",     KFloat,  0, 1, 0, "") \
 X(Groove4,     "groove4",     "Groove 1/4",     KFloat,  0, 1, 0, "") \
 X(Groove2,     "groove2",     "Groove 1/2",     KFloat,  0, 1, 0, "") \
 X(Glitch,      "glitch",      "Glitch",         KFloat,  0, 1, 0, "") \
 X(Velocity,    "velocity",    "Velocity",       KInt,    1, 127, 100, "") \
 X(VelRand,     "velRand",     "Vel Random",     KFloat,  0, 1, 0.2, "") \
 X(AccentAmt,   "accentAmt",   "Accent",         KFloat,  0, 1, 0.4, "") \
 X(Gate,        "gate",        "Gate",           KFloat,  0.05, 2, 0.8, "") \
 X(GateRand,    "gateRand",    "Gate Random",    KFloat,  0, 1, 0, "") \
 X(PbRand,      "pbRand",      "Pitch-Bend Rnd", KFloat,  0, 1, 0, "") \
 X(Intensity,   "intensity",   "Intensity",      KFloat,  0, 2, 1, "") \
 X(LoopOn,      "loopOn",      "Loop",           KBool,   0, 1, 0, "") \
 X(LoopLen,     "loopLen",     "Loop Length",    KChoice, 0, 7, 3, "1/32|1/16|1/8|1/4|1/2|1 bar|2 bars|4 bars") \
 X(Cc1Num,      "cc1Num",      "CC1 Number",     KInt,    0, 127, 0, "") \
 X(Cc1Src,      "cc1Src",      "CC1 Source",     KChoice, 0, 4, 0, "Random|Velocity|Pitch|Gate|Step") \
 X(Cc1Amt,      "cc1Amt",      "CC1 Amount",     KFloat,  0, 1, 1, "") \
 X(Cc2Num,      "cc2Num",      "CC2 Number",     KInt,    0, 127, 0, "") \
 X(Cc2Src,      "cc2Src",      "CC2 Source",     KChoice, 0, 4, 0, "Random|Velocity|Pitch|Gate|Step") \
 X(Cc2Amt,      "cc2Amt",      "CC2 Amount",     KFloat,  0, 1, 1, "") \
 X(Cc3Num,      "cc3Num",      "CC3 Number",     KInt,    0, 127, 0, "") \
 X(Cc3Src,      "cc3Src",      "CC3 Source",     KChoice, 0, 4, 0, "Random|Velocity|Pitch|Gate|Step") \
 X(Cc3Amt,      "cc3Amt",      "CC3 Amount",     KFloat,  0, 1, 1, "") \
 X(Cc4Num,      "cc4Num",      "CC4 Number",     KInt,    0, 127, 0, "") \
 X(Cc4Src,      "cc4Src",      "CC4 Source",     KChoice, 0, 4, 0, "Random|Velocity|Pitch|Gate|Step") \
 X(Cc4Amt,      "cc4Amt",      "CC4 Amount",     KFloat,  0, 1, 1, "") \
 X(SeqLen,      "seqLen",      "Seq Length",     KInt,    1, 32, 16, "") \
 X(SeqStart,    "seqStart",    "Seq Loop Start", KInt,    1, 32, 1, "") \
 X(SeqEnd,      "seqEnd",      "Seq Loop End",   KInt,    1, 32, 16, "") \
 X(SeqDir,      "seqDir",      "Seq Direction",  KChoice, 0, 4, 0, "Forward|Reverse|Ping-Pong|Random|Drunk") \
 X(SeqResetOnPlay,"seqResetPlay","Reset On Play", KBool,  0, 1, 1, "") \
 X(PercOn,      "percOn",      "PERC On",        KBool,   0, 1, 1, "") \
 X(PercMidi,    "percMidi",    "PERC MIDI Out",  KBool,   0, 1, 1, "") \
 X(PercAudio,   "percAudio",   "PERC Audio",     KBool,   0, 1, 1, "") \
 X(PercCh,      "percCh",      "PERC Channel",   KInt,    1, 16, 10, "") \
 X(PercSwing,   "percSwing",   "PERC Swing",     KFloat,  0, 1, 0, "") \
 X(PercInt,     "percInt",     "PERC Intensity", KFloat,  0, 2, 1, "") \
 X(PercRandom,  "percRandom",  "PERC Random",    KFloat,  0, 1, 0, "") \
 X(PercVar,     "percVar",     "PERC Variation", KFloat,  0, 1, 0, "") \
 X(PercFill,    "percFill",    "Fill Every",     KChoice, 0, 3, 1, "1 bar|2 bars|4 bars|8 bars") \
 X(PercLevel,   "percLevel",   "PERC Level",     KFloat,  0, 1.5, 0.8, "") \
 X(PercPitch,   "percPitch",   "PERC Pitch",     KFloat, -12, 12, 0, "") \
 X(BrOn,        "brOn",        "Beat Repeat",    KBool,   0, 1, 0, "") \
 X(BrMain,      "brMain",      "BR on Generator",KBool,   0, 1, 1, "") \
 X(BrPerc,      "brPerc",      "BR on PERC",     KBool,   0, 1, 1, "") \
 X(BrFreeze,    "brFreeze",    "BR Freeze",      KBool,   0, 1, 0, "") \
 X(BrSize,      "brSize",      "BR Size",        KChoice, 0, 6, 3, "1/32|1/16|1/16T|1/8|1/8T|1/4|1/2") \
 X(BrInterval,  "brInterval",  "BR Interval",    KChoice, 0, 4, 2, "1/4|1/2|1 bar|2 bars|4 bars") \
 X(BrChance,    "brChance",    "BR Frequency",   KFloat,  0, 1, 0.5, "") \
 X(BrDuration,  "brDuration",  "BR Duration",    KChoice, 0, 4, 1, "1/4|1/2|1 bar|2 bars|4 bars") \
 X(BrStutter,   "brStutter",   "BR Stutter",     KFloat, -1, 1, 0, "") \
 X(BrGate,      "brGate",      "BR Gate",        KFloat,  0.1, 1, 1, "") \
 X(BrVar,       "brVar",       "BR Variation",   KFloat,  0, 1, 0, "") \
 X(BrIntensity, "brIntensity", "BR Intensity",   KFloat,  0, 1, 1, "") \
 X(BrRandom,    "brRandom",    "BR Random",      KFloat,  0, 1, 0, "") \
 X(BrMix,       "brMix",       "BR Live Mute",   KFloat,  0, 1, 1, "")

enum GlobalParam {
#define X(e, id, nm, kind, mn, mx, df, ch) p##e,
    PS_GLOBAL_PARAMS(X)
#undef X
    pGlobalCount
};

// Parâmetros por lane do PERC (8 lanes)
#define PS_LANE_PARAMS(Y) \
 Y(Mute,  "mute",  "Mute",  KBool,   0, 1, 0, "") \
 Y(Level, "level", "Level", KFloat,  0, 1.5, 1, "") \
 Y(Pitch, "pitch", "Pitch", KFloat, -12, 12, 0, "") \
 Y(Pan,   "pan",   "Pan",   KFloat, -1, 1, 0, "") \
 Y(Prob,  "prob",  "Prob",  KFloat,  0, 1, 1, "") \
 Y(Gate,  "gate",  "Gate",  KFloat,  0.05, 2, 1, "") \
 Y(Len,   "len",   "Steps", KInt,    1, 32, 16, "") \
 Y(Res,   "res",   "Res",   KChoice, 0, 5, 2, "1/4|1/8|1/16|1/32|1/8T|1/16T")

enum LaneParam {
#define Y(e, id, nm, kind, mn, mx, df, ch) lp##e,
    PS_LANE_PARAMS(Y)
#undef Y
    kLaneParamCount
};

constexpr int kNumParams = pGlobalCount + kNumLanes * kLaneParamCount;
inline constexpr int laneParam(int lane, int which) { return pGlobalCount + lane * kLaneParamCount + which; }

struct ParamDef {
    char id[40]; char name[48];
    int kind; float mn, mx, def;
    const char* choices;
};

// Preenche 'out' (tamanho kNumParams) – chamado uma vez, fora do tempo real.
inline void buildParamDefs(ParamDef* out) {
    int i = 0;
    auto put = [&](const char* id, const char* nm, int kind, float mn, float mx, float df, const char* ch) {
        ParamDef& d = out[i++];
        std::snprintf(d.id, sizeof d.id, "%s", id);
        std::snprintf(d.name, sizeof d.name, "%s", nm);
        d.kind = kind; d.mn = mn; d.mx = mx; d.def = df; d.choices = ch;
    };
#define X(e, id, nm, kind, mn, mx, df, ch) put(id, nm, kind, (float)(mn), (float)(mx), (float)(df), ch);
    PS_GLOBAL_PARAMS(X)
#undef X
    static const char* laneNames[kNumLanes] = {"kick","clap","chat","ohat","shaker","rim","tom","perc"};
    for (int l = 0; l < kNumLanes; ++l) {
        char idb[48], nb[64];
#define Y(e, id, nm, kind, mn, mx, df, ch) \
        std::snprintf(idb, sizeof idb, "l%d_%s", l, id); \
        std::snprintf(nb, sizeof nb, "%s %s", laneNames[l], nm); \
        put(idb, nb, kind, (float)(mn), (float)(mx), (float)(df), ch);
        PS_LANE_PARAMS(Y)
#undef Y
    }
}

} // namespace ps
