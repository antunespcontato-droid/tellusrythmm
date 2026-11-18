# PsyRhythm (nome de trabalho) – gerador MIDI rítmico/melódico VST3

Recriação independente, inspirada nas funções públicas do Futurephonic Rhythmizer 5 (ver `RHYTHMIZER5_ANALYSIS.md`). Não afiliado à Futurephonic. Troque o nome do produto no `CMakeLists.txt`.

## Uso no Ableton Live
O plugin é um instrumento VST3 com saída MIDI. Coloque-o numa trilha MIDI, toque notas/acordes nele e crie outra trilha MIDI com o sintetizador, com *MIDI From* = a trilha do plugin. O módulo PERC também toca por um mini-sintetizador interno (saída de áudio).

## Módulos
- **Generator** (Mono/Poly, escalas, Key Freeze, shapes, direção, oitavas, Fusion/Flux/Spread, Poly Stack, dinâmica, Looper, CC Map).
- **Rhythm & Sequencer** (groove probabilístico, shuffle, humanize, glitch; sequenciador de 32 passos com vel/gate/prob/pitch/ratchet/accent, loop start/end, direções, random fill, mutate, rotate, reverse).
- **PERC** (extra, própria): 8 lanes polirrítmicas, templates Psytrance, fills/variação.
- **Beat Repeat** (extra, própria): fragmentos sincronizados, stutter, freeze em tempo real.

## Build
Windows (VS 2022 + CMake): `cmake -S . -B build && cmake --build build --config Release --target PsyRhythm_VST3`
Instalador: `iscc installer\PsyRhythm.iss`. Ou use o workflow do GitHub Actions incluído.
Testes do motor (qualquer SO, sem JUCE): `cmake -S . -B bt -DPSY_BUILD_PLUGIN=OFF && cmake --build bt && ctest --test-dir bt`

## Logo
Coloque **sua** logo em `assets/logo.png` (e `assets/logo.ico` / `assets/logo.bmp` para o instalador) e recompile. Ela é exibida sem redesenho nem distorção (só reduz mantendo proporção).
