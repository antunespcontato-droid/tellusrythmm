# Relatório de testes (honesto)

Ambiente: container Linux (g++ 13, sem Windows, sem Ableton, sem GUI).

## Verificado de fato
| Área | Como | Resultado |
|---|---|---|
| Motor rítmico (mono/poly, groove, density, shuffle, humanize, glitch, escalas, key freeze, shapes, direção, oitavas, CC map, pitch-bend, latch) | `Tests/engine_tests.cpp`, g++ -O1 com AddressSanitizer + UBSan | 12.056 verificações, 0 falhas |
| Sequenciador (passos, prob, ratchet, accent, loop start/end, forward/reverse/ping-pong, modo Groove por passo) | idem | OK |
| PERC (8 lanes, polirritmia, swing, ratchet, pitch, fill/variação, random, templates) | idem | OK |
| Beat Repeat (repetição de fragmento, freeze em tempo real, stutter) | idem | OK |
| Looper (grava e repete idêntico; volta ao gerador) | idem | OK |
| Sincronização: BPM 60–200, SR 44.1–96 kHz, blocos 64–2000 amostras, posições idênticas, PPQ arbitrário, free run, Stop sem notas presas, salto de transporte | idem | erro de grade ≤ 1 amostra |
| Serialização de padrões | idem | roundtrip idêntico |
| Tempo real | contador global de `operator new` durante `process` | **0 alocações** |
| CPU (motor + sintetizador interno, carga pesada, 174 BPM, 48 kHz, bloco 512) | -O2 | ~2,7% de 1 núcleo |
| Código JUCE (Processor/Editor) | `g++ -fsyntax-only` contra JUCE 8.0.12 | compila sem erros (não linkado) |

## NÃO verificado (precisa ser feito por você / CI)
- Compilação e linkagem real do `.vst3` no Windows (workflow `.github/workflows/build-windows.yml` faz isso).
- Geração do instalador `.exe` (Inno Setup rodando no CI).
- Carregar no Ableton Live, automação de parâmetros, recuperação de projeto, save/load de presets pela GUI, aparência da GUI, estabilidade e CPU/RAM no Live.
- Logo oficial e imagem de referência da interface: **não foram recebidas**.
Por isso o plugin **não deve ser considerado concluído** até esses itens passarem.
