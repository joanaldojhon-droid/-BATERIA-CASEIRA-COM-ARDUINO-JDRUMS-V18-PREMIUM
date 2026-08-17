# 🥁 J-DRUMS V18 PREMIUM — Arduino Mega 2560 MIDI Drum Controller
Código finalizado após mais de um ano de desenvolvimento e testes intensivos em bateria acústica convertida e baterias caseiras feitas de PVC e outros materiais, com músicos profissionais, bateristas e percussionistas com anos de
experiência, que aprovaram o J-DRUMS.

Code finalized after more than a year of development and intensive testing on converted acoustic drum kits and homemade drums made of PVC and other materials, with professional musicians, drummers, and percussionists with years of experience, who approved J-DRUMS.

Desmostração Rimshot - Trizone Ride e Midi Off note / Rimshot Demonstration - Trizone Ride and Midi Off Note
https://youtube.com/shorts/sp2UuVqdgNQ

**Open-source firmware** para bateria eletrônica DIY / for DIY electronic drums.  
Transforma piezos e sensores em sinais MIDI profissionais — converts piezo sensors into professional MIDI signals.  
Menu completo em display LCD · Full LCD menu · Auto-calibration · Multi-zone cymbals.

> 🇧🇷 Projeto brasileiro open-source — Brazilian open-source project

---

## ✅ Features / Recursos

* 🎛️ 16 Analog Pads — nota MIDI, threshold, gain, velocity curve e crosstalk cancel individuais
* 🎚️ Smart Hi-Hat (TCRT5000) — auto-calibração em 8s / 8-second auto-calibration · Normal e Invertido (InvSensor / LM393)
* 🥁 Rimshot — 2 modos: completo (Aro+Caixa, maior velocity entre os dois) e force rimshot (Caixa acima de velocity 125) / 2 modes: full (Rim+Head, highest velocity of the two) and force rimshot (Snare above velocity 125)
* 🔀 Dual Pad por Intensidade — nota trocada (101→102, 103→104) por velocity ≥125 / Intensity-based Dual Pad — note swap (101→102, 103→104) at velocity ≥125
* 🎵 Tri-Zone & Dual Pad — Hi-Hat (3 zones) · Ride (3 zones) · 2× Crash (2 zones) — 1 piezo each, velocity real do piezo / real piezo velocity
* ⚡ 80ms Temporal Latch — membranas caseiras / homemade membranes: copper tape, EVA, metal
* 🔌 7 Digital Pads (D33–D45) + expansão / expansion: Pro Micro (+4) ou Leonardo (+6) via SysEx
* 📡 Dual MIDI Output — USB + Serial TX1 simultâneos / simultaneous
* 💾 3 EEPROM Preset Slots + 3 Presets de Fábrica — salva e restaura config completa do kit / save and restore full kit config
* 🎯 XtakCancel — AntiCrosstalk Dinâmico Global, equivalente ao Crosstalk Cancel da Roland, janela de 120ms / Dynamic Global AntiCrosstalk, Roland Crosstalk Cancel equivalent, 120ms window — eliminates ghost notes across the whole kit
* 🚧 Filtro de Aro e Caixa — bater na Caixa bloqueia o Aro e vice-versa / Rim/Head filter — hitting the snare blocks the rim and vice-versa
* 🛡️ Velocity Mínimo — gate anti-interferência do piezo, separado para Caixa e demais pads / piezo anti-interference gate, separate for Snare and other pads
* 🌊 Filtro Ruído x Batida Real — ignora vibração de fundo (ventilador, TV, rede 60Hz) / Noise vs Real-Hit filter — ignores background vibration (fan, TV, 60Hz mains)
* ⚡ Quick Pad Adjust — segure encoder 3s + bata no pad / hold encoder 3s + hit pad
* 🔕 Note OFF Automático — 120ms, até 12 notas simultâneas, nunca fica presa no VST / Automatic Note OFF — 120ms, up to 12 simultaneous notes, never stuck in VST
* 💽 Cache EEPROM Não-Bloqueante — grava sem travar o MIDI / Non-blocking EEPROM cache — writes without stalling MIDI
* 🔊 Real-time MIDI Monitor — porta, nota e velocity no LCD
* 👁️ Hi-Hat Monitor — barra visual de 16 posições / 16-position bar on LCD
* 🔄 7 Velocity Curves por pad — Linear · Soft1/2/3 · Hard1/2 · Fixed
* 🎵 4 Cymbal Chokes (D47–D53) — contato metálico direto / direct metal contact
* 💡 Luz do Display — auto-desliga ou sempre acesa / Display backlight — auto-off or always on
* 🔀 Encoder Rotativo — 3 funções de botão + Buzzer / Rotary encoder — 3 button functions + Buzzer
* 🔁 Inverte Botões — troca a função A/B / Invert Buttons — swaps A/B function
* 🏷️ 35 Nomes de Pads pré-definidos / 35 predefined pad names
* 💡 LED Indicador — pisca ao bater nos pads (pino 22), acende a cada nota MIDI de A1–A15 / LED Indicator — blinks on pad hit (pin 22), lights on every MIDI note from A1–A15
* 🎛️ Editable MIDI Device ID — personalize o nome no OS e DAW / customize the device name in OS and DAW
* 💡 Full LCD Menu — encoder + luz ajustável / adjustable backlight
* 🔀 InvSensor — TCRT5000 manual ou módulo LM393 pronto / ready-made
* ♻️ Resetar Módulo — dupla confirmação (10s cada), restaura ao padrão de fábrica / Reset Module — double confirmation (10s each), restores factory defaults

- 🥁 Testado e Aprovado por uma Comunidade de Bateristas
O J-DRUMS foi desenvolvido e testado dentro de um grupo com mais de 54 membros, entre eles vários músicos profissionais, bateristas e percussionistas com anos de experiência, que testaram o módulo em baterias acústicas convertidas para eletrônicas e o utilizam na prática. O nível de refinamento alcançado — filtros de rimshot e crosstalk funcionando com precisão, resposta consistente em toques rápidos, sem falso disparo entre pads — chega a superar o desempenho e a quantidade de funções de vários módulos de entrada disponíveis no mercado, mesmo sendo construído sobre um Arduino Mega 2560. O resultado se aproxima do nível de módulos profissionais de ponta, como o TD-27 (Roland) e similares.
-🥁 Tested and Approved by a Community of Drummers
J-DRUMS was developed and tested within a group of over 54 members, including several professional musicians, drummers, and percussionists with years of experience, who tested the module on acoustic kits converted to electronic and use it in real practice. The level of refinement achieved — rimshot and crosstalk filters working with precision, consistent response on fast strokes, no false triggering between pads — even surpasses the performance and feature count of several entry-level modules available on the market, despite being built on an Arduino Mega 2560. The result gets close to the level of top-tier professional modules, such as Roland's TD-27 and similar.
---
## 🎮 Compatible with / Compatível com.
**DAWs:** Ableton · Reaper · FL Studio · Pro Tools · ASIO/ASIO4ALL  
**VSTs:** Superior Drummer · EZdrummer · Addictive Drums · any MIDI VST
---
## 🛠️ Hardware
Arduino Mega 2560 · Piezo sensors · TCRT5000 · LCD 16×2 · Rotary encoder  
Expansão opcional / Optional expansion: Pro Micro ou Leonardo
---
## 📦 Download
Código completo + Manual ilustrado HTML passo a passo / Full source code + illustrated HTML manual step by step.
🔗 [Download — MediaFire](https://www.mediafire.com/folder/are76n6h5zycm/CODIGO+JDRUMS+5.0)
 🥁 Apoie o Desenvolvedor com uma doação de qualquer valor Pix: d194052e-c2cc-43d9-9710-a4ab9b8bbc2b  
---
MIT License — livre para usar, modificar e distribuir / free to use, modify and distribute.
---
*Agradeço a Deus por me proporcionar esse projeto maravilhoso. 🙏*  
*I thank God for inspiring this amazing project.*  
**Developed by Joanaldo Jhon Leonez de Melo · © 2026 *
