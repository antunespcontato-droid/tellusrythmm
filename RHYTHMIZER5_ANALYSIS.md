# RHYTHMIZER5_ANALYSIS.md

Análise funcional do Futurephonic Rhythmizer 5 e plano de implementação independente.

**Método:** somente fontes públicas (página oficial do produto, FAQ oficial, reviews na imprensa especializada). Nenhum binário foi aberto, nenhum código ou arquivo interno foi extraído, nenhuma proteção foi contornada. Manual completo e vídeos **não** foram acessíveis nesta sessão; tudo que não aparece nas fontes está marcado como **NÃO CONFIRMADO** e foi tratado como decisão de projeto própria.

Fontes:
- Página oficial Rhythmizer 5 Suite — https://www.futurephonic.co.uk/products/rhythmizer-5-suite
- FAQ oficial (Fusion/Ultra) — https://www.futurephonic.co.uk/pages/rhythmizer-faq
- Sound On Sound, review do Rhythmizer Ultra — https://www.soundonsound.com/reviews/futurephonic-rhythmizer-ultra
- Magnetic Magazine, review do Rhythmizer Fusion — https://magneticmag.com/2025/05/futurephonic-rhythmizer-fusion-review-a-forward-thinking-arpeggiator-that-goes-crazy-with-randomization/

## 0. Descoberta mais importante: é um GERADOR DE MIDI

O Rhythmizer **não produz áudio** (exceto o sampler RK-Cell, produto à parte). Ele recebe notas MIDI e **emite** notas MIDI geradas, que vão para um sintetizador. Consequências para esta recriação:

- O plugin é declarado como instrumento VST3 com **entrada e saída MIDI** (Ableton Live não aceita VST3 como "MIDI Effect"; o fluxo é: plugin numa trilha MIDI → saída MIDI roteada para outra trilha com o sintetizador — o mesmo fluxo usado pelo original).
- O módulo PERC tem um **mini-sampler/sintetizador de percussão interno** (adição própria) para que seja audível sem roteamento, além de emitir MIDI.

## 1. Recursos CONFIRMADOS no original

| # | Recurso | Descrição pública | Implementação planejada (própria) | Módulo |
|---|---------|-------------------|-----------------------------------|--------|
| 1 | Modo Mono | Uma nota segurada gera fluxo contínuo de melodias/grooves segundo as configurações | `RhythmEngine` com nota-base única | Engine |
| 2 | Modo Poly | Acorde segurado vira arpejador randômico; "Chord Flux" e "Spread" remodelam o voicing; "Fusion Fader" mistura arpejo ↔ acorde cheio | Pool de notas seguradas; chance de inserir acorde; flux/spread/voices | Engine |
| 3 | Escalas | 38 escalas internas + escalas customizadas | Tabela de escalas (conjunto próprio, ver §3) + máscara de 12 bits customizada | Scales |
| 4 | Key Freeze | Ajusta notas à escala escolhida | Quantização de notas à escala/tônica | Scales |
| 5 | Melody: Prob / Steps | Prob = chance da nota gerada diferir da entrada; Steps = máximo de graus de escala de distância | Idem | Engine |
| 6 | Scale Shapes | Straight, Branch, Cascade, Thumb, Pinky, com direção e randomização | Cinco geradores de contorno de graus | Engine |
| 7 | Direção | Sobe/desce/ambos; botão randomiza direção; faixa de oitavas e frequência de cada oitava | Direção + oct range + oct chance | Engine |
| 8 | Chance / Extend | Quantas notas além da raiz soam; se notas fora do acorde segurado podem entrar | Parâmetros `chance`, `extend` | Engine |
| 9 | Poly Stack | Harmonias ocasionais, acordes em tonalidade | Empilha graus da escala sobre a nota | Engine |
| 10 | Groove generator | Probabilidade por subdivisão (colcheia, semínima, mínima; 32avos; tercinas), velocity e gate randomizados | Probabilidades por subdivisão, toggle de tercinas | Engine |
| 11 | Density | <100% remove notas | Idem | Engine |
| 12 | Shuffle | Valor positivo adiciona swing | Swing em passos pares | Clock |
| 13 | Humanize ("Human") | Pequenos deslocamentos de tempo fora da grade | Offset de timing aleatório limitado (≤ ~1/32) | Engine |
| 14 | Glitch | Repetição quase instantânea de notas; níveis baixos presos à grade, altos mais experimentais | Ratchets/repetições de subdivisão | Engine |
| 15 | Randomização de gate, velocity, pitch, ritmo, pitch-bend | Grau e estilo controláveis | Amount + estilo por alvo (pitch-bend: ver §4) | Engine |
| 16 | Step sequencer | Padrão por passo, probabilidade por passo, "groove override", Random Fill reescreve o padrão, Shift/Alt+Click atalhos | Sequenciador completo (ver `Source/Engine/Pattern.h`) | Seq |
| 17 | Loop / Looper | Captura trecho da saída e repete; de 32avos (glitch) a vários compassos | `LoopCapture`: grava N passos de eventos e repete enquanto ativo | Engine |
| 18 | CC Mapping | 4 slots enviam a aleatoriedade como MIDI CC para parâmetros do sintetizador | 4 slots (CC#, fonte, amount, intervalo) | Engine |
| 19 | Gravação / Drag-to-MIDI | Arrastar frase para trilha MIDI | **Fora do escopo v1** (depende de DnD de arquivo .mid; planejado: exportar .mid por botão) | UI |
| 20 | Presets | Biblioteca de presets; Key Freeze mestre travável durante troca de preset | `PresetManager` em arquivos XML | Presets |
| 21 | Sync | Ableton 11+ listado; sincronização com tempo do host | `AudioPlayHead` PPQ, BPM, play/stop | Clock |

## 2. NÃO CONFIRMADO / NÃO existe no original (informado nas fontes)

- Lista completa de parâmetros, faixas numéricas e curvas: não publicada nas fontes acessadas.
- Modulação interna além de CC Map e "arpeggiation as a modulation source" (FAQ Fusion): detalhes não publicados.
- Direções de reprodução do **step sequencer** (forward/reverse/pingpong…), Accent, controles de início/fim de ciclo: **não confirmados**.
- **PERC** como módulo: **não confirmado**. O RK-Cell (sampler de 128 células) é produto separado e fora do escopo; aqui o PERC é módulo **adicional de implementação própria**.
- **Beat Repeat** como módulo separado: **não confirmado**. O mais próximo é o Looper e o Glitch. Aqui é **módulo adicional de implementação própria**.
- Layout visual exato: nenhuma imagem de referência chegou nesta sessão (ver §6).

## 3. Escalas

Tabela própria com escalas de uso comum (maior, menores, modos, pentatônicas, blues, harmônica/melódica, Hirajoshi, Pelog aproximada, Phrygian dominante, Double Harmonic, etc.). **Não** é cópia da lista do original (que não é publicada em ordem completa) e não afirma ter 38 escalas idênticas. Máscara customizada de 12 notas disponível.

## 4. Limitação: Pitch Bend

A fonte confirma randomização de pitch-bend. Será emitido como mensagem MIDI pitch-wheel aleatória limitada (amount), com retorno a centro no fim da nota.

## 5. Arquitetura planejada

```
Source/
  Engine/        C++ puro (sem JUCE) -> testável em qualquer SO
    Types.h        eventos MIDI sem alocação, constantes
    Rng.h          xorshift determinístico
    Scales.h       tabela de escalas + quantização
    Pattern.h      passos (active, vel, accent, gate, prob, pitch, pan, ratchet)
    StepClock.h    PPQ -> passo, resolução, swing, direção, loop start/end
    RhythmEngine.h geradores Mono/Poly, groove, melodia, humanize, glitch, looper, CC map
    PercEngine.h   lanes de percussão + geradores de groove psytrance
    BeatRepeat.h   captura/repetição/stutter sobre o fluxo de eventos
    Engine.h       agrega módulos, processa um bloco
  Plugin/        JUCE
    PluginProcessor  APVTS, estado completo (parâmetros + padrões), host sync
    PluginEditor     UI
    DrumVoices       voz de percussão interna (áudio)
    PresetManager    presets em disco
Tests/           testes do motor (g++/ctest)
installer/       Inno Setup (.iss)
.github/workflows  build Windows (VST3 + instalador .exe)
```

Regras de tempo real: sem `new`/`malloc`/locks no `processBlock`; fila de eventos de capacidade fixa; edição de padrões pela UI via cópia atômica (double-buffer com `std::atomic<int>` + `SpinLock` curto só para troca de ponteiro, sem bloquear o áudio — ver `PluginProcessor`).

## 6. Logo e imagem de referência

O pedido cita logo oficial e imagem de interface "fornecidas anteriormente". **Nenhum dos dois chegou nesta sessão** (só o arquivo de texto). A build usa `assets/logo.png` automaticamente quando existir; sem ele, mostra um espaço reservado explícito. A logo nunca é redesenhada.
