# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

O formato é baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.0.0/),
e este projeto adere ao [Versionamento Semântico](https://semver.org/lang/pt-BR/).

## [Unreleased]

### Adicionado
- ObsSynth4D: o motor **SUPER** (ENGINE → SUP), o do ObsSynth FM-1: sete vozes da onda do oscilador (SAW, SQR, TRI ou SIN; o knob WAVE), a do centro e seis desafinadas em volta até ±60 cents. DETUNE e SIDE nos knobs do granular (a posição e o spray, o mesmo destino da mod matrix: um LFO move o DETUNE); as sete com a energia de uma. Cada forma com o seu caráter: os quadrados com larguras espalhadas pelo DETUNE, os triângulos espalhados a 3/5 e dobrados (um shaper senoidal) conforme o DETUNE sobe. Presets Super Chords, Super Squares e Super Glass. Estados salvos antes (4 motores no ENGINE) abrem como eram
- Editores dos plugins ObsSynth4D e ObsDrums (CLAP/VST3): o painel do produto (`obssynth4d`/`obsdrums --remote`), dentro da janela do host no Windows, como o do ObsSynthCT1; o estado inteiro vai ao painel ao abrir, num programa e numa sessão; o painel ocioso não redesenha
- Pacote `.deb` (`packaging/deb/build-deb.sh`): os quatro painéis e os seus CLAPs em `/usr/lib/clap/ObsSynth` (cada plugin com o seu painel e o `pcm/` ao lado), atalhos em `/usr/bin` e no menu; SDL3 do sistema (Debian 13 / Carbonian 13). O CI faz no runner do PC (`deb.yml`): artefato a cada push, e no release de uma tag v*
- `obssynth4d` no SDK dos painéis, com a mesma cara: o laço, a barra, SETUP, o CC overlay, os presets e as cópias de `panel_kit` saem do painel (3105 → 2553 linhas). Parado no MAIN não desenha nada; o que se move (halos, scope, a matriz) redesenha só a sua área. O SDK ganha seletores, LEDs, pad XY, valores pelo produto, camada sobre os controles, estado inicial próprio, recorte no host e Shift F2 (próximo mapa) para todos
- SDK dos painéis (`src/app/common/panel_app.hpp`): um só programa em volta de cada painel (janela, barra, SETUP, CC overlay, presets do usuário, teclado, link com o plugin, desenho otimizado); o produto diz só o que é seu. O `obssynthone` é o primeiro: ganha a barra comum, SETUP (áudio, MIDI, paleta), CC overlay com mapas para Launchkey e Launchpad X, presets do usuário em `<data>/obssynthone/presets`, teclado tocando notas, 1600 × 900 centralizado. No Pi 2, um quadro de arrasto cai de 9,8 para 3,7 ms
- `obssynthone` sozinho, como o `obssynth4d`: sem `--remote` o painel tem som próprio (motor, saída de áudio, MIDI em `ObsSynthOne:input`) e os presets de fábrica em ‹ ›. Os valores, os presets e a ligação parâmetro → motor saem do plugin para `app/obssynthone/one_driver.hpp`, que os dois usam
- Perfil lite no ObsSynth One (painel e plugin): 6 vozes, modulação a cada 32 amostras, 1x num ARM 32-bit (`OBS_PROFILE=lite|full`). No Raspberry Pi 2 o perfil completo atrasava 58% dos blocos de 5,33 ms; o lite toca sem underrun, com o painel a 28% de CPU (o 4D, 110%)
- Painel do ObsSynth One em máquina pequena (`docs/PANEL_DRAWING.md`): um fundo por motor, rasterizado uma vez e os outros numa thread (trocar de motor no Pi 2: de 2,4 s para menos de 0,13 s); o painel guardado numa textura e só a área que mudou redesenhada; nada desenhado quando o ponteiro só passa. `OBS_PANEL_TIMES`, `OBS_PANEL_DRAG` para medir
- Linux: a thread de áudio do ALSA pede `SCHED_FIFO` (ou o nice permitido) e diz qual conseguiu; precisa do `rtprio` do grupo `audio` (`/etc/security/limits.d`)
- Gravações padrão em `pcm/` (VCSL, CC0; `pcm/SAMPLES.md`): órgão e cowbell. Os grãos do ObsSynth4D e os pads USER 3/4 do ObsDrums procuram um nome na pasta de samples do usuário e depois no `pcm/` que acompanha o plugin ou o painel; em um PC limpo não ficam mais mudos


### Removido
- Remove all NanoVG code from synth_view - window now renders simple GL triangle using immediate mode
### Em Desenvolvimento
- Melhorias contínuas no sistema de síntese FM
- Expansão do suporte a controladores MIDI

---

## [0.1.0] - 2026-05-18

### Adicionado
- Add chaos parameter to TubeSaturation (physical tube drift), CC29 mapping, mod matrix destination, preset I/O, prefer Launchpad/LaunchKey MIDI device
- Presets Stacked Fifths Bass e Minor Ninth Chord, teclas 1-9 selecionam presets
- Master Effect Slot (MFX) to ObsSynth4D signal chain
- Tube Drive/Asymmetry as mod matrix destinations with ModWheel routing in all presets
- Master stereo ping-pong delay FX, OSC4 sub-octave coarse tuning, pitch mod amount fixes
- Debug features: WAV recording and auto-screenshots in vst_gui_host
- MIDI input from Launchpad X fully working with ASIO audio output
- Real ASIO driver support - detects Focusrite, FL Studio, Voicemeeter, etc.
- VST Host with ASIO audio output and real MIDI input
- Add VST host test and improvement roadmap
- Sistema de checkpoint e changelog implementado
- **Visual Studio 2026**: Atualização de referências e scripts de build
- **FM Synthesis**: Implementação completa com 4 operadores e múltiplos algoritmos
- **VST3 Plugin (ObsSynth)**: Plugin VST3 funcional com interface VSTGUI
- **ModularSistemas**: Parâmetros de wave table, LFO, filtro e efeitos
- **PCM Waveforms**: Suporte a formas de onda PCM com interpolação

### Corrigido
- Edição do VSTGUI
- Edição do VSTGUI
- VST Host with working ASIO audio output (VB-Matrix VASIO-32) and MIDI monitoring
- Warnings de variáveis não inicializadas no switch de algoritmo FM
- Formatação na inicialização do backend PulseAudio
- Anotações de variáveis não utilizadas em múltiplos arquivos

### Alterado
- ModMatrix refactored to generic int-based API, created src/products/ with ObsSynth/ObsDrum/ObsBass mod enums
- Organize test files into structured directories: tests/ with doctest framework (33 tests), benchmarks/ for cross-platform performance tests
- Waveform dropdown: convertido para StringListParameter e COptionMenu, removido WaveformRow2 redundante
- Pre-configured Launchpad X as MIDI and VB-Matrix VASIO-32 as ASIO driver
- Refatoração da estrutura de código para melhor legibilidade
- Inicialização de backend de áudio específica por plataforma (Windows/Linux)

---

## [0.0.2] - 2026-04

### Adicionado
- Add chaos parameter to TubeSaturation (physical tube drift), CC29 mapping, mod matrix destination, preset I/O, prefer Launchpad/LaunchKey MIDI device
- Presets Stacked Fifths Bass e Minor Ninth Chord, teclas 1-9 selecionam presets
- Master Effect Slot (MFX) to ObsSynth4D signal chain
- Tube Drive/Asymmetry as mod matrix destinations with ModWheel routing in all presets
- Master stereo ping-pong delay FX, OSC4 sub-octave coarse tuning, pitch mod amount fixes
- Debug features: WAV recording and auto-screenshots in vst_gui_host
- MIDI input from Launchpad X fully working with ASIO audio output
- Real ASIO driver support - detects Focusrite, FL Studio, Voicemeeter, etc.
- VST Host with ASIO audio output and real MIDI input
- Add VST host test and improvement roadmap
- Sistema de checkpoint e changelog implementado
- **Stereo Audio**: Suporte completo a áudio estéreo com panning
- **DirectSound Backend**: Backend de áudio DirectSound para Windows
- **SIMD Optimizations**: Instruções SSE2 para processamento de efeitos
- **Lock-Free Oscillator**: Implementação lock-free para melhor performance
- **Crossfade Voice Transitions**: Transições suaves entre vozes

### Melhorado
- **Polyphonic Oscillator**: Aumento de 8 para 32 vozes máximas
- **ADSR Envelope**: Controle de envelope ADSR com dry/wet mix
- **Performance**: Testes de comparação de performance entre implementações

### Corrigido
- Edição do VSTGUI
- Edição do VSTGUI
- VST Host with working ASIO audio output (VB-Matrix VASIO-32) and MIDI monitoring
- Sample rate padronizado para 48000 Hz
- Timing de buffer em análise de áudio

---

## [0.0.1] - 2026-03

### Adicionado
- Add chaos parameter to TubeSaturation (physical tube drift), CC29 mapping, mod matrix destination, preset I/O, prefer Launchpad/LaunchKey MIDI device
- Presets Stacked Fifths Bass e Minor Ninth Chord, teclas 1-9 selecionam presets
- Master Effect Slot (MFX) to ObsSynth4D signal chain
- Tube Drive/Asymmetry as mod matrix destinations with ModWheel routing in all presets
- Master stereo ping-pong delay FX, OSC4 sub-octave coarse tuning, pitch mod amount fixes
- Debug features: WAV recording and auto-screenshots in vst_gui_host
- MIDI input from Launchpad X fully working with ASIO audio output
- Real ASIO driver support - detects Focusrite, FL Studio, Voicemeeter, etc.
- VST Host with ASIO audio output and real MIDI input
- Add VST host test and improvement roadmap
- Sistema de checkpoint e changelog implementado
- **MIDI Controllers**:
  - Launchkey MK4 com probing SysEx
  - Launchpad X com modo sessão e controle de logo
  - APC Key 25 Mk1 e Mk2 com grid pads e controles
- **ASIO Backend**: Suporte a ASIO SDK para baixa latência
- **Raspberry Pi**: Suporte a ARMv7 com configuração ALSA otimizada
- **LFO**: Controle de frequência e amplitude via MIDI
- **Effects**:
  - Chorus multi-voice
  - Overdrive com documentação Doxygen

### Infraestrutura
- **GitHub Actions**: Processo de release automatizado
- **CI/CD**: Workflow com Clang como compilador padrão
- **Documentação**: Doxygen completo para headers de efeitos

### Corrigido
- Edição do VSTGUI
- Edição do VSTGUI
- VST Host with working ASIO audio output (VB-Matrix VASIO-32) and MIDI monitoring
- Projeção ortográfica GL type casting
- Funções matemáticas inline para precisão float-only
- Conflitos de funções intrínsecas no MSVC

---

## [0.0.0] - 2026-02 (Início do Projeto)

### Fundação
- **Core Synthesizer**: Osciladores (sine, sawtooth, square, triangle)
- **Audio Backends**: WinMM (WaveOut), ALSA, PulseAudio
- **MIDI Input**: Leitura de entrada MIDI multiplataforma
- **WAV Writer**: Geração de arquivos WAV
- **OpenGL Visualization**: Visualização de formas de onda em tempo real
- **Build System**: CMake com suporte Windows/Linux

### Arquitetura
- C++17 com precisão float-only (nunca double)
- Namespace `synth` em minúsculas
- Sem dependências de áudio externas

---

## Convenções

### Tipos de Mudança
- **Adicionado**: Novas funcionalidades
- **Alterado**: Mudanças em funcionalidades existentes
- **Obsoleto**: Funcionalidades que serão removidas
- **Removido**: Funcionalidades removidas
- **Corrigido**: Correções de bugs
- **Segurança**: Correções de vulnerabilidades

### Versionamento
- **MAJOR**: Mudanças incompatíveis na API
- **MINOR**: Novas funcionalidades retrocompatíveis
- **PATCH**: Correções retrocompatíveis

