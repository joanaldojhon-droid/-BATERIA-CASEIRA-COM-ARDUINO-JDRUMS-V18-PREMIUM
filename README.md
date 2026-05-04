# 🥁 J-DRUMS v5.0 — Controlador MIDI de Bateria Eletrônica para Arduino Mega 2560

Firmware open-source que transforma piezos e sensores em sinais MIDI profissionais, com menu completo em display LCD.

## ✅ Recursos

- 🎛️ **16 Pads Analógicos (A0–A15)** — nota MIDI, threshold, gain, curva de velocity e crosstalk configuráveis individualmente
- 🎚️ **Hi-Hat Inteligente (TCRT5000)** — sensor óptico com auto-calibração em 8 segundos, suporte a modo Normal e Invertido (InvSensor)
- 🥁 **Rimshot em 2 modos** — Rimshot completo (aro + caixa) e Rimshot de força (velocity 127 na caixa)
- 🎵 **Tri-Zone e Dual Pad** — Chimbal (3 zonas), Ride (3 zonas) e 2× Crash (2 zonas cada) com 1 piezo por prato
- ⚡ **Latch temporal 80ms** — compatível com membranas caseiras de fita de cobre, EVA ou metal
- 🔌 **7 Pads Digitais (D33–D45)** + expansão via Pro Micro (4 pads) ou Leonardo (6 pads) por SysEx
- 📡 **Saída MIDI Dupla** — USB e Serial TX1 simultâneos (PC + módulo de som ao mesmo tempo)
- 🎹 **4 Mapas MIDI prontos** — Persona (editável), SupEzd1, SupEzd2 e Addictive Drums
- 💾 **3 Slots de Preset na EEPROM** — salva e restaura configurações completas do kit
- 🎯 **XCancel CrossTalk** — elimina ghost notes com até 16 pares Source→Target configuráveis
- ⚡ **Ajuste Rápido de Pad** — segure encoder 3s, bata no pad, acessa diretamente o ajuste
- 🔊 **Monitor MIDI em tempo real** — porta, nota e velocity no LCD
- 👁️ **Monitor Hi-Hat** — barra visual de 16 posições no LCD
- 🔄 **7 Curvas de Velocity por pad** — Linear, Soft1/2/3, Hard1/2 e Fixed
- 🎵 **4 Chokes de Pratos** (pinos D47–D53) — contato metálico direto, sem componentes extras
- 🔕 **Note OFF implementado corretamente** — notas não ficam presas no VST
- 🎛️ **ID MIDI editável** — personalize o nome do dispositivo no sistema operacional e DAW
- 💡 **Menu completo no LCD** — navegação por encoder com luz ajustável
- 🔄 **Inverte botões** e reset de módulo pelo menu
- 🔀 **InvSensor** — suporte a sensor TCRT5000 manual ou módulo LM393 pronto

Código completo com código de limpeza do Arduino suporte Pro Micro e Leonado Com manual ilustrado completo passo a passo com resumo de cada função do código no link abaixo.
https://www.mediafire.com/folder/are76n6h5zycm/CODIGO+JDRUMS+5.0
IT License — free to use, modify, and distribute.
Developed by **Joanaldo Jhon Leonez de Melo** · © 2026
