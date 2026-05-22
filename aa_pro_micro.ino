//=========================================================================================//
//=>                         J-DRUMS v8.0.16 - CONTROLADOR MIDI BATERIA                    <= 
//=>                        Sistema de Bateria Eletrônica Arduino                          <=
//=>                     Copyright (c) 2026 Joanaldo Jhon Leonez de Melo                   <=
//=>                             Licensed under the MIT License.                           <=
//=>         See LICENSE.txt file in the project root for full license information.        <=
//=>                      DESENVOLVIDO POR JOANALDO JHON LEONEZ DE MELO                    <=
//=>                                   Janeiro/2026                                        <=
//=>                                                                                       <=
//=>  FUNCIONALIDADES:                                                                     <=
//=>  • Expansão de 4 pads via Pro Micro                                                   <=
//=>  • Comunicação SysEx entre Mega e Pro Micro                                           <=
//=>  • Salvamento automático na EEPROM do Mega                                            <=
//=>  • Nomes personalizados para pads Pro Micro                                           <=
//=>  • Todos os parâmetros configuráveis via menu                                         <=
//=>  • Anti-crosstalk independente                                                        <=
//=========================================================================================//

#ifndef PRO_MICRO_PADS_H
#define PRO_MICRO_PADS_H

#define NUM_PRO_MICRO_PADS 11

// Códigos dos parâmetros (SINCRONIZADO COM PRO MICRO)
#define PARAM_NOTE       0x00
#define PARAM_THRESHOLD  0x01
#define PARAM_SCANTIME   0x02
#define PARAM_MASKTIME   0x03
#define PARAM_RETRIGGER  0x04
#define PARAM_CURVE      0x05
#define PARAM_CURVEFORM  0x06
#define PARAM_GAIN       0x07
#define PARAM_TYPE       0x08
#define PARAM_CHANNEL    0x09
#define PARAM_ANTICROSSTALK 0x0A  // *** NOVO ***

// Estrutura do pad Pro Micro
struct ProMicroPad {
  byte note;
  byte threshold;
  byte scanTime;
  byte maskTime;
  byte retrigger;
  byte curve;
  byte curveForm;
  byte gain;
  byte type;  // 0=Disabled, 1=Piezo
  byte channel;
  byte antiCrosstalk;  // 0-127
  byte nameIndex;  // *** Índice do nome da lista available_names (0-24) ***
};

// Arrays globais
ProMicroPad proMicroPads[NUM_PRO_MICRO_PADS];

// Nomes dos pads (NÃO USADO MAIS - USA available_names)
const char* proMicroPadNames[NUM_PRO_MICRO_PADS] = {
  "Pad 0",
  "Pad 1",
  "Pad 2",
  "Pad 3",
  "Pad 4",
  "Pad 5",
  "Pad 6",
  "Pad 7",
  "Pad 8",
  "Pad 9",
  "Pad 10"
};

#endif

//==============================
//    FUNÇÕES DE COMUNICAÇÃO
//==============================

void sendProMicroCommand(byte cmd, byte padIndex, byte param, byte value) {
  // Formato: F0 7D [CMD] [PAD] [PARAM] [VALUE] F7
  Serial1.write(0xF0);
  Serial1.write(0x7D);  // Manufacturer ID
  Serial1.write(cmd);
  Serial1.write(padIndex);
  Serial1.write(param);
  Serial1.write(value);
  Serial1.write(0xF7);
  Serial1.flush();
}

//==============================
//    INICIALIZAÇÃO
//==============================

void initProMicroPads() {
  // Valores padrão
  for(int i = 0; i < NUM_PRO_MICRO_PADS; i++) {
    proMicroPads[i].note = 36 + i;
    proMicroPads[i].threshold = 12;
    proMicroPads[i].scanTime = 10;
    proMicroPads[i].maskTime = 16;
    proMicroPads[i].retrigger = 94;
    proMicroPads[i].curve = 0; // Linear
    proMicroPads[i].curveForm = 68;
    proMicroPads[i].gain = 28;
    proMicroPads[i].type = 0; // Piezo Inativo
    proMicroPads[i].channel = 9;
    proMicroPads[i].antiCrosstalk = 26; // Valor médio (0-127)
    proMicroPads[i].nameIndex = 15; // *** Nome padrão = PAD EFE 1 ***
  }
  
  // Carrega da EEPROM se disponível
  loadProMicroPadsFromEEPROM();
}

//==============================
//    EEPROM
//==============================

void saveProMicroPadToEEPROM(byte padIndex) {
  #if defined(__AVR__)
  if(padIndex >= NUM_PRO_MICRO_PADS) return;
  
  int baseAddr = 4500 + (padIndex * 12);  // 12 bytes por pad
  EEPROM.write(baseAddr + 0, proMicroPads[padIndex].note);
  EEPROM.write(baseAddr + 1, proMicroPads[padIndex].threshold);
  EEPROM.write(baseAddr + 2, proMicroPads[padIndex].scanTime);
  EEPROM.write(baseAddr + 3, proMicroPads[padIndex].maskTime);
  EEPROM.write(baseAddr + 4, proMicroPads[padIndex].retrigger);
  EEPROM.write(baseAddr + 5, proMicroPads[padIndex].curve);
  EEPROM.write(baseAddr + 6, proMicroPads[padIndex].curveForm);
  EEPROM.write(baseAddr + 7, proMicroPads[padIndex].gain);
  EEPROM.write(baseAddr + 8, proMicroPads[padIndex].type);
  EEPROM.write(baseAddr + 9, proMicroPads[padIndex].channel);
  EEPROM.write(baseAddr + 10, proMicroPads[padIndex].antiCrosstalk);
  EEPROM.write(baseAddr + 11, proMicroPads[padIndex].nameIndex);  // *** SALVA ÍNDICE DO NOME ***
  #endif
}

// *** NOVA: Salva nome personalizado para pad Pro Micro ***
void saveProMicroCustomName(byte padIndex, char* name) {
  if(padIndex >= NUM_PRO_MICRO_PADS) return;
  int address = 4700 + (padIndex * 13);  // Área após os parâmetros (4500 + 11*12 = 4632, uso 4700 para segurança)
  #if defined(__AVR__)
  for(int i = 0; i < 13; i++) {
    EEPROM.write(address + i, name[i]);
  }
  EEPROM.write(4700 + 143 + padIndex, 1);  // Flag de nome customizado
  #endif
  markNameChanged();
}

// *** NOVA: Carrega nome personalizado para pad Pro Micro ***
bool loadProMicroCustomName(byte padIndex, char* buffer) {
  if(padIndex >= NUM_PRO_MICRO_PADS) return false;
  #if defined(__AVR__)
  if(EEPROM.read(4700 + 143 + padIndex) != 1) {
    return false;
  }
  int address = 4700 + (padIndex * 13);
  for(int i = 0; i < 13; i++) {
    buffer[i] = EEPROM.read(address + i);
    if(buffer[i] == '\0') {
      for(int j = i; j < 12; j++) {
        buffer[j] = ' ';
      }
      buffer[12] = '\0';
      break;
    }
  }
  buffer[12] = '\0';
  return true;
  #else
  return false;
  #endif
}

// *** NOVA: Verifica se pad Pro Micro tem nome personalizado ***
bool hasProMicroCustomName(byte padIndex) {
  if(padIndex >= NUM_PRO_MICRO_PADS) return false;
  #if defined(__AVR__)
  return EEPROM.read(4700 + 143 + padIndex) == 1;
  #else
  return false;
  #endif
}

// *** NOVA: Limpa nome personalizado do pad Pro Micro ***
void clearProMicroCustomName(byte padIndex) {
  if(padIndex >= NUM_PRO_MICRO_PADS) return;
  #if defined(__AVR__)
  EEPROM.write(4700 + 143 + padIndex, 0);
  #endif
  markNameChanged();
}

void loadProMicroPadsFromEEPROM() {
  #if defined(__AVR__)
  for(int i = 0; i < NUM_PRO_MICRO_PADS; i++) {
    int baseAddr = 4500 + (i * 12);
    
    byte storedNote = EEPROM.read(baseAddr + 0);
    // Se o primeiro byte for 255 (não inicializado), usa valores padrão
    if(storedNote == 255) {
      continue; // Mantém valores padrão do initProMicroPads
    }
    
    proMicroPads[i].note = storedNote;
    proMicroPads[i].threshold = EEPROM.read(baseAddr + 1);
    proMicroPads[i].scanTime = EEPROM.read(baseAddr + 2);
    proMicroPads[i].maskTime = EEPROM.read(baseAddr + 3);
    proMicroPads[i].retrigger = EEPROM.read(baseAddr + 4);
    proMicroPads[i].curve = EEPROM.read(baseAddr + 5);
    proMicroPads[i].curveForm = EEPROM.read(baseAddr + 6);
    proMicroPads[i].gain = EEPROM.read(baseAddr + 7);
    proMicroPads[i].type = EEPROM.read(baseAddr + 8);
    proMicroPads[i].channel = EEPROM.read(baseAddr + 9);
    proMicroPads[i].antiCrosstalk = EEPROM.read(baseAddr + 10);
    byte storedNameIndex = EEPROM.read(baseAddr + 11);
    // *** Se nunca foi salvo (255), inválido, ou era o antigo padrão (índice do pad), usa PAD EFE 1 (15) ***
    if(storedNameIndex == 255 || storedNameIndex >= NUM_AVAILABLE_NAMES || storedNameIndex == i) {
      proMicroPads[i].nameIndex = 15;
    } else {
      proMicroPads[i].nameIndex = storedNameIndex;
    }
    
    // Envia para o Pro Micro
    sendAllParametersToProMicro(i);
  }
  #endif
}

//==============================
//    ENVIO DE PARÂMETROS
//==============================

void sendAllParametersToProMicro(byte padIndex) {
  if(padIndex >= NUM_PRO_MICRO_PADS) return;
  
  sendProMicroCommand(0x03, padIndex, PARAM_NOTE, proMicroPads[padIndex].note);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_THRESHOLD, proMicroPads[padIndex].threshold);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_SCANTIME, proMicroPads[padIndex].scanTime);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_MASKTIME, proMicroPads[padIndex].maskTime);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_RETRIGGER, proMicroPads[padIndex].retrigger);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_CURVE, proMicroPads[padIndex].curve);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_CURVEFORM, proMicroPads[padIndex].curveForm);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_GAIN, proMicroPads[padIndex].gain);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_TYPE, proMicroPads[padIndex].type);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_CHANNEL, proMicroPads[padIndex].channel);
  delay(5);
  sendProMicroCommand(0x03, padIndex, PARAM_ANTICROSSTALK, proMicroPads[padIndex].antiCrosstalk);  // *** NOVO ***
  delay(5);
}

// ✅ ATUALIZA PARÂMETRO + SALVA NA EEPROM DO MEGA + ENVIA PARA PRO MICRO
void updateProMicroParameter(byte padIndex, byte param, byte value) {
  if(padIndex >= NUM_PRO_MICRO_PADS) return;
  
  switch(param) {
    case PARAM_NOTE:      proMicroPads[padIndex].note = value; break;
    case PARAM_THRESHOLD: proMicroPads[padIndex].threshold = value; break;
    case PARAM_SCANTIME:  proMicroPads[padIndex].scanTime = value; break;
    case PARAM_MASKTIME:  proMicroPads[padIndex].maskTime = value; break;
    case PARAM_RETRIGGER: proMicroPads[padIndex].retrigger = value; break;
    case PARAM_CURVE:     proMicroPads[padIndex].curve = value; break;
    case PARAM_CURVEFORM: proMicroPads[padIndex].curveForm = value; break;
    case PARAM_GAIN:      proMicroPads[padIndex].gain = value; break;
    case PARAM_TYPE:      proMicroPads[padIndex].type = value; break;
    case PARAM_CHANNEL:   proMicroPads[padIndex].channel = value; break;
    case PARAM_ANTICROSSTALK: proMicroPads[padIndex].antiCrosstalk = value; break;  // *** NOVO ***
  }
  
  // Envia comando para Pro Micro (que salva automaticamente na EEPROM dele)
  sendProMicroCommand(0x03, padIndex, param, value);
  
  // Salva na EEPROM do Mega
  saveProMicroPadToEEPROM(padIndex);
}