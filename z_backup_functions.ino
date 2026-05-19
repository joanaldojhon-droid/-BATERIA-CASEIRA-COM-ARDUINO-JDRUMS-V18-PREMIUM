///=========================================================================================//
//=>                         J-DRUMS v8.0 - CONTROLADOR MIDI BATERIA                       <= 
//=>                        Sistema de Bateria Eletrônica Arduino                          <=
//=>                     Copyright (c) 2026 Joanaldo Jhon Leonez de Melo                   <=
//=>                             Licensed under the MIT License.                           <=
//=>         See LICENSE.txt file in the project root for full license information.        <=
//=>                      DESENVOLVIDO POR JOANALDO JHON LEONEZ DE MELO                    <=
//=>                                   Janeiro/2026                                        <=

//==========================================================================================
// SISTEMA DE BACKUP - 3 PRESETS COM VST PRESET + SAÍDAS MIDI + NOTAS PERSONA
//==========================================================================================

#define BACKUP_EEPROM_START_1 1000
#define BACKUP_EEPROM_START_2 2200
#define BACKUP_EEPROM_START_3 3400
#define BACKUP_SIZE_1 1200
#define BACKUP_SIZE_2 1200
#define BACKUP_SIZE_3 696
#define NUM_BACKUPS 3

//==========================================================================================
// PRESETS DE FÁBRICA — ARMAZENADOS NA MEMÓRIA FLASH (PROGMEM)
// Esses presets ficam gravados no firmware e NUNCA são apagados.
// O usuário pode APENAS RESTAURAR (não salvar).
// Ao restaurar, as notas e nomes são gravados na EEPROM normal.
// Ao editar notas ou nomes após restaurar, salva normalmente como qualquer preset.
//
// Preset 4 = "Addctive"  (índice fábrica 0)
// Preset 5 = "SupEzd1"   (índice fábrica 1)
// Preset 6 = "SupEzd2"   (índice fábrica 2)
//==========================================================================================

// --- Notas A0-A15 para cada preset de fábrica ---
// Preset 4 "Addctive": 1 4 8 38 42 36 71 69 67 60 61 77 79 81 65 47
//   RimshotNote = 37
//   Dig1 P45 = nota 61 (digitalPadNotes[0])
//   Dig1 P43 = nota 62 (digitalPadNotes[1])
const byte FACTORY_NOTES_P4[16] PROGMEM = {
   4,  // A0  - HHC
   8,  // A1  - CHIMBAL
  38,  // A2  - CAIXA
  42,  // A3  - ARO CAIXA
  36,  // A4  - BUMBO
  71,  // A5  - TOM 2
  69,  // A6  - TOM 3
  67,  // A7  - SURDO 1
  60,  // A8  - RIDE COND
  61,  // A9  - RIDE CUPULA
  77,  // A10 - CRASH 1
  79,  // A11 - CRASH 2
  81,  // A12 - CRASH 3
  65,  // A13 - SURDO 2
  47,  // A14 - TOM 1
  96   // A15 - PAD EFET 1
};
// RimshotNote (CUSTOM_NOTE_40) e notas digitais do Preset 4
#define FACTORY_P4_RIMSHOT_NOTE  37   // CUSTOM_NOTE_40
#define FACTORY_P4_DIG1_P45      61   // digitalPadNotes[0]
#define FACTORY_P4_DIG1_P43      62   // digitalPadNotes[1]

// selected_names (índices de available_names) para Preset 4 — sequência padrão 0..15
const byte FACTORY_NAMES_P4[16] PROGMEM = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

// --- Preset 5 "SupEzd1": 4 5 38 37 36 47 45 43 51 53 49 59 86 41 48 55 ---
//   RimshotNote = 40
//   Dig2 P45 = nota 53 (digitalPadNotes[0])
//   Dig1 P43 = nota 59 (digitalPadNotes[1])
//   Mapeamento de nomes:
//     A0(0)->idx 0, A1(1)->idx 1, A2(2)->idx 2, A3(3)->idx 3, A4(4)->idx 4
//     A5(5)->idx 6  (name_6 = "TOM 2")
//     A6(6)->idx 7  (name_7 = "TOM 3")
//     A7(7)->idx 25 (name_25 = "SURDO 1")
//     A8(8)->idx 8, A9(9)->idx 9
//     A10(10)->idx 20 (name_20 = "CRASH 1")
//     A11(11)->idx 21 (name_21 = "CRASH 2")
//     A12(12)->idx 22 (name_22 = "CRASH 3")
//     A13(13)->idx 26 (name_26 = "SURDO 2")
//     A14(14)->idx 5  (name_5  = "TOM 1")
//     A15(15)->idx 31 (name_31 = "PAD EFET 1")
const byte FACTORY_NOTES_P5[16] PROGMEM = {
   4,  // A0
   8,  // A1
  38,  // A2
  37,  // A3
  36,  // A4
  47,  // A5
  45,  // A6
  43,  // A7
  51,  // A8
  53,  // A9
  49,  // A10
  57,  // A11
  86,  // A12
  41,  // A13
  48,  // A14
  55   // A15
};
#define FACTORY_P5_RIMSHOT_NOTE  40
#define FACTORY_P5_DIG2_P45      53   // digitalPadNotes[0]
#define FACTORY_P5_DIG1_P43      59   // digitalPadNotes[1]

const byte FACTORY_NAMES_P5[16] PROGMEM = {
   30,  // A0  -> CONT CHIMBAL
   31,  // A1  -> CHIMBAL
   32,  // A2  -> CAIXA
   33,  // A3  -> ARO CAIXA
   34,  // A4  -> BUMBO
   6,   // A5  -> TOM 2
   7,   // A6  -> TOM 3
  24,   // A7  -> SURDO 1
   8,   // A8
   9,   // A9
  18,   // A10 -> CRASH 1
  19,   // A11 -> CRASH 2
  20,   // A12 -> CRASH 3
  25,   // A13 -> SURDO 2
   5,   // A14 -> TOM 1
  26    // A15 -> PAD EFET 1
};

// --- Preset 6 "SupEzd2": 4 5 38 37 36 48 47 43 51 53 49 59 86 41 69 55 ---
//   RimshotNote = 40
//   Dig2 P45 = nota 53 (digitalPadNotes[0])
//   Dig1 P43 = nota 59 (digitalPadNotes[1])
//   Mapeamento de nomes: igual ao Preset 5
const byte FACTORY_NOTES_P6[16] PROGMEM = {
   4,  // A0
   8,  // A1
  38,  // A2
  37,  // A3
  36,  // A4
  48,  // A5
  47,  // A6
  43,  // A7
  51,  // A8
  53,  // A9
  49,  // A10
  57,  // A11
  86,  // A12
  41,  // A13
  69,  // A14
  55   // A15
};
#define FACTORY_P6_RIMSHOT_NOTE  40
#define FACTORY_P6_DIG2_P45      53   // digitalPadNotes[0]
#define FACTORY_P6_DIG1_P43      59   // digitalPadNotes[1]

const byte FACTORY_NAMES_P6[16] PROGMEM = {
   30,  // A0  -> CONT CHIMBAL
   31,  // A1  -> CHIMBAL
   32,  // A2  -> CAIXA
   33,  // A3  -> ARO CAIXA
   34,  // A4  -> BUMBO
   6,   // A5  -> TOM 2
   7,   // A6  -> TOM 3
  24,   // A7  -> SURDO 1
   8,   // A8
   9,   // A9
  18,   // A10 -> CRASH 1
  19,   // A11 -> CRASH 2
  20,   // A12 -> CRASH 3
  25,   // A13 -> SURDO 2
   5,   // A14 -> TOM 1
  26    // A15 -> PAD EFET 1
};

// --- Nome de fábrica dos 3 presets fixos (exibido no LCD quando sem nome customizado) ---
const char FACTORY_PRESET_NAME_4[] PROGMEM = "Addctive    ";
const char FACTORY_PRESET_NAME_5[] PROGMEM = "SupEzd1     ";
const char FACTORY_PRESET_NAME_6[] PROGMEM = "SupEzd2     ";

// Retorna o nome de fábrica do preset fixo (4, 5 ou 6) para buffer[13]
void getFactoryPresetName(byte presetNum, char* buffer) {
  const char* src = NULL;
  if(presetNum == 4)      src = FACTORY_PRESET_NAME_4;
  else if(presetNum == 5) src = FACTORY_PRESET_NAME_5;
  else if(presetNum == 6) src = FACTORY_PRESET_NAME_6;
  else { strcpy(buffer, "Preset ?"); return; }
  for(byte i = 0; i < 12; i++) buffer[i] = pgm_read_byte(src + i);
  buffer[12] = '\0';
}

//==========================================================================================
// RestoreFactoryPreset — restaura preset de fábrica (4, 5 ou 6) da PROGMEM
// Aplica notas nos pinos A0-A15, nomes, notas digitais e rimshot.
// Depois grava tudo na EEPROM normal (igual ao fluxo existente).
//==========================================================================================
void RestoreFactoryPreset(byte presetNum) {
  #if defined(__AVR__)

  if(presetNum < 4 || presetNum > 6) return;

  // Seleciona as tabelas PROGMEM corretas
  const byte* notesTable = NULL;
  const byte* namesTable = NULL;
  byte rimshotNote = 37;
  byte dig0 = 61;  // digitalPadNotes[0]
  byte dig1 = 62;  // digitalPadNotes[1]

  if(presetNum == 4) {
    notesTable  = FACTORY_NOTES_P4;
    namesTable  = FACTORY_NAMES_P4;
    rimshotNote = FACTORY_P4_RIMSHOT_NOTE;
    dig0        = FACTORY_P4_DIG1_P45;
    dig1        = FACTORY_P4_DIG1_P43;
  } else if(presetNum == 5) {
    notesTable  = FACTORY_NOTES_P5;
    namesTable  = FACTORY_NAMES_P5;
    rimshotNote = FACTORY_P5_RIMSHOT_NOTE;
    dig0        = FACTORY_P5_DIG2_P45;
    dig1        = FACTORY_P5_DIG1_P43;
  } else {
    notesTable  = FACTORY_NOTES_P6;
    namesTable  = FACTORY_NAMES_P6;
    rimshotNote = FACTORY_P6_RIMSHOT_NOTE;
    dig0        = FACTORY_P6_DIG2_P45;
    dig1        = FACTORY_P6_DIG1_P43;
  }

  // --- Aplica notas A0-A15 ---
  for(byte i = 0; i < 16; i++) {
    byte nota = pgm_read_byte(notesTable + i);
    Pin[i].Note = nota;
  }

  // --- Aplica nomes dos pads ---
  for(byte i = 0; i < 16; i++) {
    byte nameIdx = pgm_read_byte(namesTable + i);
    if(nameIdx < NUM_AVAILABLE_NAMES) {
      selected_names[i] = nameIdx;
    } else {
      selected_names[i] = (i < NUM_AVAILABLE_NAMES) ? i : 0;
    }
    clearCustomName(i);  // remove nomes customizados anteriores
  }

  // --- Aplica notas digitais (mantém os demais, só muda [0] e [1]) ---
  digitalPadNotes[0] = dig0;
  digitalPadNotes[1] = dig1;

  // --- Aplica rimshot: CUSTOM_NOTE_40 = rimshotNote, sincroniza derivados ---
  CUSTOM_NOTE_40 = rimshotNote;
  CUSTOM_RIMSHOT_FORCE_NOTE = rimshotNote;
  CUSTOM_NOTE_38 = Pin[2].Note;  // caixa restaurada
  CUSTOM_NOTE_37 = Pin[3].Note;  // aro restaurado

  // --- Sincroniza ZoneDual com notas restauradas ---
  for(byte p = 0; p < ZONE_DUAL_PAIRS; p++) {
    zoneDual_NoteAux[p]   = digitalPadNotes[p];
    zoneDual_NoteBlock[p] = Pin[zoneDual_PadIdx[p]].Note;
  }

  // --- Salva tudo na EEPROM (notas, nomes, digitais, chokes, avançado) ---
  markAllPinsChanged();
  markNameChanged();
  markAdvancedChanged();
  SaveDigitalNotesEEPROM();
  SaveChokeNotesEEPROM();
  forceImmediateSaveToEEPROM();

  // --- Sinalização visual: 4 piscadas do LED ---
  forceLedsOff();
  for(int i = 0; i < 4; i++) {
    digitalWrite(13, HIGH);
    delay(100);
    digitalWrite(13, LOW);
    delay(100);
  }

  #endif
}

int getBackupStartAddress(byte backupNumber) {
  switch(backupNumber) {
    case 1: return BACKUP_EEPROM_START_1;
    case 2: return BACKUP_EEPROM_START_2;
    case 3: return BACKUP_EEPROM_START_3;
    default: return BACKUP_EEPROM_START_1;
  }
}

int getBackupSize(byte backupNumber) {
  switch(backupNumber) {
    case 1: return BACKUP_SIZE_1;
    case 2: return BACKUP_SIZE_2;
    case 3: return BACKUP_SIZE_3;
    default: return BACKUP_SIZE_1;
  }
}

void SaveBackup(byte backupNumber) {
  #if defined(__AVR__)
  
  if(backupNumber < 1 || backupNumber > 3) return;
  
  forceImmediateSaveToEEPROM();
  
  int startAddress = getBackupStartAddress(backupNumber);
  int maxSize = getBackupSize(backupNumber);
  int offset = 0;
  
  // === CABEÇALHO (6 bytes) ===
  EEPROM.write(startAddress + offset++, 0xAB);
  EEPROM.write(startAddress + offset++, backupNumber);
  EEPROM.write(startAddress + offset++, 0x55);
  EEPROM.write(startAddress + offset++, 0x40);
  EEPROM.write(startAddress + offset++, 0x20);
  EEPROM.write(startAddress + offset++, 0x25);
  
  // === QUANTIDADE DE PINOS FIXO (1 byte) ===
  EEPROM.write(startAddress + offset++, 16);
  
  // === CONFIGURAÇÕES GERAIS (3 bytes) ===
  EEPROM.write(startAddress + offset++, delayTime/2);
  EEPROM.write(startAddress + offset++, NSensor);
  EEPROM.write(startAddress + offset++, GeneralXtalk);
  
  // === CONFIGURAÇÕES HH (12 bytes) ===
  for(int h = 0; h < 4; h++) {
    EEPROM.write(startAddress + offset++, HHNoteSensor[h]);
  }
  for(int h = 0; h < 4; h++) {
    EEPROM.write(startAddress + offset++, HHThresoldSensor[h]);
  }
  for(int h = 0; h < 2; h++) {
    EEPROM.write(startAddress + offset++, HHFootNoteSensor[h]);
  }
  for(int h = 0; h < 2; h++) {
    EEPROM.write(startAddress + offset++, HHFootThresoldSensor[h]);
  }
  
  // === CONFIGURAÇÕES AVANÇADAS (14 bytes) ===
  EEPROM.write(startAddress + offset++, ENABLE_NOTE_37_38_TO_40);
  EEPROM.write(startAddress + offset++, ENABLE_VELOCITY_FILTER);
  EEPROM.write(startAddress + offset++, ENABLE_RIMSHOT_38_TO_40);
  EEPROM.write(startAddress + offset++, ENABLE_NOTE_101_TO_102);
  EEPROM.write(startAddress + offset++, ENABLE_NOTE_103_TO_104);
  EEPROM.write(startAddress + offset++, (byte)(BLOCK_WINDOW_MS/10));
  EEPROM.write(startAddress + offset++, VELOCITY_THRESHOLD_37_38);
  EEPROM.write(startAddress + offset++, (byte)DETECTION_WINDOW_MS);
  EEPROM.write(startAddress + offset++, CUSTOM_NOTE_37);
  EEPROM.write(startAddress + offset++, CUSTOM_NOTE_40);
  EEPROM.write(startAddress + offset++, CUSTOM_RIMSHOT_FORCE_NOTE);
  EEPROM.write(startAddress + offset++, CUSTOM_NOTE_38); // era reservado — agora salva nota A2
  EEPROM.write(startAddress + offset++, MIDI_USB_ENABLED);
  EEPROM.write(startAddress + offset++, MIDI_TX1_ENABLED);
  
  // === MAX_NOTE_AGE_MS / R>NotAgeMs (1 byte) ===
  EEPROM.write(startAddress + offset++, (byte)MAX_NOTE_AGE_MS);
  
  // === BACKLIGHT (1 byte) ===
  EEPROM.write(startAddress + offset++, backlightMode);
  
  // === BUZZER (1 byte) ===
  #if BUZZER
  EEPROM.write(startAddress + offset++, buzzerEnabled ? 1 : 0);
  #else
  EEPROM.write(startAddress + offset++, 0);
  #endif
  
  // *** RESERVADO: 16 bytes (era notas Persona) — grava zeros ***
  for(int i = 0; i < 16; i++) {
    EEPROM.write(startAddress + offset++, 0);
  }
  
  // === NOMES PADRÃO (16 bytes) ===
  for(int i = 0; i < 16; i++) {
    EEPROM.write(startAddress + offset++, selected_names[i]);
  }
  
  // === CONTAGEM DE NOMES CUSTOMIZADOS (1 byte) ===
  byte customCount = 0;
  for(int i = 0; i < 16; i++) {
    if(hasCustomName(i)) customCount++;
  }
  EEPROM.write(startAddress + offset++, customCount);
  
  // === NOMES CUSTOMIZADOS ===
  int spaceForCustomNames = maxSize - offset - (16 * 12) - 2;
  int maxCustomNamesAllowed = spaceForCustomNames / 13;
  
  byte savedCustomCount = 0;
  for(int i = 0; i < 16 && savedCustomCount < min(customCount, maxCustomNamesAllowed); i++) {
    if(hasCustomName(i)) {
      if(offset + 13 < (startAddress + maxSize - (16 * 12) - 2)) {
        EEPROM.write(startAddress + offset++, i);
        
        char customName[13];
        if(loadCustomName(i, customName)) {
          for(int j = 0; j < 12; j++) {
            EEPROM.write(startAddress + offset++, customName[j]);
          }
        } else {
          for(int j = 0; j < 12; j++) {
            EEPROM.write(startAddress + offset++, ' ');
          }
        }
        savedCustomCount++;
      } else {
        break;
      }
    }
  }
  
  EEPROM.write(startAddress + 71, savedCustomCount); // Posição ajustada (+16 bytes das notas Persona)
  
  // === DADOS DOS PINOS (12 bytes cada - 16 pinos fixos) ===
  for(int i = 0; i < 16; i++) {
    if(offset + 12 < (startAddress + maxSize - 1)) {
      EEPROM.write(startAddress + offset++, Pin[i].Note);
      EEPROM.write(startAddress + offset++, Pin[i].Thresold);
      EEPROM.write(startAddress + offset++, Pin[i].ScanTime);
      EEPROM.write(startAddress + offset++, Pin[i].MaskTime);
      EEPROM.write(startAddress + offset++, Pin[i].Retrigger);
      EEPROM.write(startAddress + offset++, Pin[i].Curve);
      EEPROM.write(startAddress + offset++, Pin[i].Xtalk);
      EEPROM.write(startAddress + offset++, Pin[i].XtalkGroup);
      EEPROM.write(startAddress + offset++, Pin[i].CurveForm);
      EEPROM.write(startAddress + offset++, Pin[i].ChokeNote);
      EEPROM.write(startAddress + offset++, Pin[i].Type);
      EEPROM.write(startAddress + offset++, Pin[i].Channel);
      EEPROM.write(startAddress + offset++, Pin[i].AntiCrosstalk);
    } else {
      break;
    }
  }
  
  // === MARCA FINAL ===
  EEPROM.write(startAddress + maxSize - 1, 0xCD);
  
  // === NOTAS PADS DIGITAIS (7 bytes) ===
  for(int i = 0; i < 7; i++) {
    EEPROM.write(startAddress + offset++, digitalPadNotes[i]);
  }
  
  // === NOTAS CHOKES (4 bytes) ===
  for(int i = 0; i < 4; i++) {
    EEPROM.write(startAddress + offset++, chokeNotes[i]);
  }

  // *** CORRECAO: salvar VelMinimo (16 bytes), InvertSensor (16 bytes), XCancel (60 bytes) ***

  // === VELOCITY MINIMO (16 bytes) ===
  for(int i = 0; i < 16; i++) {
    EEPROM.write(startAddress + offset++, Pin[i].VelMinimo);
  }

  // === INVERT SENSOR (16 bytes) ===
  for(int i = 0; i < 16; i++) {
    EEPROM.write(startAddress + offset++, Pin[i].InvertSensor ? 1 : 0);
  }

  // === XCANCEL (15 pares x 4 bytes = 60 bytes, pula par 0 que e somente leitura) ===
  for(int i = 1; i < XPAIR_COUNT; i++) {
    EEPROM.write(startAddress + offset++, xpairRam[i].source);
    EEPROM.write(startAddress + offset++, xpairRam[i].target);
    EEPROM.write(startAddress + offset++, xpairRam[i].windowMs);
    EEPROM.write(startAddress + offset++, xpairRam[i].ghostVel);
  }

  while(offset < (maxSize - 1)) {
    EEPROM.write(startAddress + offset++, 0x00);
  }
  
  forceLedsOff();
  for(int i = 0; i < backupNumber; i++) {
    digitalWrite(13, HIGH);
    delay(100);
    digitalWrite(13, LOW);
    delay(100);
  }
  
  #endif
}

void RestoreBackup(byte backupNumber) {
  #if defined(__AVR__)
  
  if(backupNumber < 1 || backupNumber > 3) return;
  
  int startAddress = getBackupStartAddress(backupNumber);
  int maxSize = getBackupSize(backupNumber);
  int offset = 0;
  
  // === VERIFICAÇÃO ===
  if(EEPROM.read(startAddress + offset++) != 0xAB) return;
  if(EEPROM.read(startAddress + offset++) != backupNumber) return;
  if(EEPROM.read(startAddress + offset++) != 0x55) return;
  offset += 3;
  
  if(EEPROM.read(startAddress + maxSize - 1) != 0xCD) return;
  
  // === LÊ QUANTIDADE DE PINOS (deve ser 16) ===
  byte backupPinCount = EEPROM.read(startAddress + offset++);
  if(backupPinCount != 16) return;
  
  // === CONFIGURAÇÕES GERAIS ===
  byte tempDelayTime = EEPROM.read(startAddress + offset++);
  NSensor = EEPROM.read(startAddress + offset++);
  GeneralXtalk = EEPROM.read(startAddress + offset++);
  
  // === CONFIGURAÇÕES HH ===
  for(int h = 0; h < 4; h++) {
    HHNoteSensor[h] = EEPROM.read(startAddress + offset++);
  }
  for(int h = 0; h < 4; h++) {
    HHThresoldSensor[h] = EEPROM.read(startAddress + offset++);
  }
  for(int h = 0; h < 2; h++) {
    HHFootNoteSensor[h] = EEPROM.read(startAddress + offset++);
  }
  for(int h = 0; h < 2; h++) {
    HHFootThresoldSensor[h] = EEPROM.read(startAddress + offset++);
  }
  
  // === CONFIGURAÇÕES AVANÇADAS (14 bytes) ===
  ENABLE_NOTE_37_38_TO_40 = EEPROM.read(startAddress + offset++);
  ENABLE_VELOCITY_FILTER = EEPROM.read(startAddress + offset++);
  ENABLE_RIMSHOT_38_TO_40 = EEPROM.read(startAddress + offset++);
  ENABLE_NOTE_101_TO_102 = EEPROM.read(startAddress + offset++);
  ENABLE_NOTE_103_TO_104 = EEPROM.read(startAddress + offset++);
  BLOCK_WINDOW_MS = (unsigned long)EEPROM.read(startAddress + offset++) * 10;
  VELOCITY_THRESHOLD_37_38 = EEPROM.read(startAddress + offset++);
  DETECTION_WINDOW_MS = (unsigned long)EEPROM.read(startAddress + offset++);
  CUSTOM_NOTE_37 = EEPROM.read(startAddress + offset++);
  CUSTOM_NOTE_40 = EEPROM.read(startAddress + offset++);
  CUSTOM_RIMSHOT_FORCE_NOTE = EEPROM.read(startAddress + offset++);
  { // era reservado — agora lê CUSTOM_NOTE_38 salvo
    byte v = EEPROM.read(startAddress + offset++);
    if(v > 0 && v <= 127) CUSTOM_NOTE_38 = v;
  }
  
  MIDI_USB_ENABLED = EEPROM.read(startAddress + offset++);
  MIDI_TX1_ENABLED = EEPROM.read(startAddress + offset++);
  
  // === MAX_NOTE_AGE_MS / R>NotAgeMs (1 byte) ===
  {
    byte restoredAge = EEPROM.read(startAddress + offset++);
    if(restoredAge >= 1 && restoredAge <= 30) MAX_NOTE_AGE_MS = restoredAge;
    else MAX_NOTE_AGE_MS = 15;
  }
  
  // === BACKLIGHT ===
  backlightMode = EEPROM.read(startAddress + offset++);
  if(backlightMode > 1) backlightMode = 0;
  
  // === BUZZER ===
  #if BUZZER
  byte buzzerValue = EEPROM.read(startAddress + offset++);
  buzzerEnabled = (buzzerValue == 1);
  #else
  offset++;
  buzzerEnabled = false;
  #endif
  
  // *** RESERVADO: 16 bytes (era notas Persona) — pula ***
  offset += 16;
  
  // === NOMES PADRÃO (16 fixos) ===
  for(int i = 0; i < 16; i++) {
    selected_names[i] = EEPROM.read(startAddress + offset++);
    if(selected_names[i] >= NUM_AVAILABLE_NAMES || selected_names[i] == 255) {
      selected_names[i] = (i < NUM_AVAILABLE_NAMES) ? i : 0;
    }
  }
  
  // === LIMPA NOMES CUSTOMIZADOS ===
  for(int i = 0; i < 16; i++) {
    clearCustomName(i);
  }
  
  // === RESTAURA NOMES CUSTOMIZADOS ===
  byte savedCustomCount = EEPROM.read(startAddress + offset++);
  
  for(int c = 0; c < savedCustomCount && c < 16; c++) {
    byte pinIndex = EEPROM.read(startAddress + offset++);
    
    if(pinIndex < 16) {
      char customName[13];
      for(int j = 0; j < 12; j++) {
        customName[j] = EEPROM.read(startAddress + offset++);
      }
      customName[12] = '\0';
      
      bool isEmpty = true;
      for(int j = 0; j < 12; j++) {
        if(customName[j] != ' ' && customName[j] != '\0') {
          isEmpty = false;
          break;
        }
      }
      
      if(!isEmpty) {
        saveCustomName(pinIndex, customName);
      }
    }
  }
  
  // === DADOS DOS PINOS (16 fixos) ===
  for(int i = 0; i < 16; i++) {
    Pin[i].Note = EEPROM.read(startAddress + offset++);
    Pin[i].Thresold = EEPROM.read(startAddress + offset++);
    Pin[i].ScanTime = EEPROM.read(startAddress + offset++);
    Pin[i].MaskTime = EEPROM.read(startAddress + offset++);
    Pin[i].Retrigger = EEPROM.read(startAddress + offset++);
    Pin[i].Curve = (curve)EEPROM.read(startAddress + offset++);
    Pin[i].Xtalk = EEPROM.read(startAddress + offset++);
    Pin[i].XtalkGroup = EEPROM.read(startAddress + offset++);
    Pin[i].CurveForm = EEPROM.read(startAddress + offset++);
    Pin[i].ChokeNote = EEPROM.read(startAddress + offset++);
    Pin[i].Type = (type)EEPROM.read(startAddress + offset++);
    Pin[i].Channel = EEPROM.read(startAddress + offset++);
    Pin[i].AntiCrosstalk = EEPROM.read(startAddress + offset++);
  }
  
  // === NOTAS PADS DIGITAIS (7 bytes) ===
  byte defaultDigital[7] = {53, 59,  7,  9, 27, 31, 90};
  for(int i = 0; i < 7; i++) {
    byte val = EEPROM.read(startAddress + offset++);
    digitalPadNotes[i] = (val <= 127) ? val : defaultDigital[i];
  }
  
  // === NOTAS CHOKES (4 bytes) ===
  byte defaultChoke[4] = {78, 80, 82, 63};
  for(int i = 0; i < 4; i++) {
    byte val = EEPROM.read(startAddress + offset++);
    chokeNotes[i] = (val <= 127) ? val : defaultChoke[i];
  }

  // *** CORRECAO: restaurar VelMinimo, InvertSensor e XCancel do backup ***
  // Backups antigos sem esses dados: os ifs de range impedem leitura fora do intervalo.

  // === VELOCITY MINIMO (16 bytes) ===
  if(offset + 16 <= maxSize - 1) {
    for(int i = 0; i < 16; i++) {
      byte v = EEPROM.read(startAddress + offset++);
      Pin[i].VelMinimo = (v <= 60) ? v : 0;
    }
  }

  // === INVERT SENSOR (16 bytes) ===
  if(offset + 16 <= maxSize - 1) {
    for(int i = 0; i < 16; i++) {
      byte v = EEPROM.read(startAddress + offset++);
      Pin[i].InvertSensor = (v == 1) ? 1 : 0;
    }
  }

  // === XCANCEL (15 pares x 4 bytes = 60 bytes, pula par 0) ===
  if(offset + 60 <= maxSize - 1) {
    for(int i = 1; i < XPAIR_COUNT; i++) {
      byte src = EEPROM.read(startAddress + offset++);
      byte tgt = EEPROM.read(startAddress + offset++);
      byte win = EEPROM.read(startAddress + offset++);
      byte vel = EEPROM.read(startAddress + offset++);
      if(src <= 15 || src == 255) xpairRam[i].source   = src;
      if(tgt <= 15 || tgt == 255) xpairRam[i].target   = tgt;
      if(win >= 10)               xpairRam[i].windowMs = win;
      if(vel >= 1 && vel <= 50)   xpairRam[i].ghostVel = vel;
    }
  }

  markGeneralChanged();
  markHHChanged();
  markAdvancedChanged();
  markBacklightChanged();
  markBuzzerChanged();
  markNameChanged();
  markMidiOutputChanged();
  markAllPinsChanged();
  
  // *** SINCRONIZA CUSTOM_NOTE_38/37 COM AS NOTAS DOS PADS RESTAURADOS ***
  // Garante que o buffer rimshot use a nota correta de A2 (caixa) e A3 (aro)
  // mesmo se o backup foi salvo numa versão antiga que não gravava CUSTOM_NOTE_38.
  CUSTOM_NOTE_38 = Pin[2].Note;
  CUSTOM_NOTE_37 = Pin[3].Note;
  SaveDigitalNotesEEPROM();  // grava notas digitais restauradas na EEPROM principal
  SaveChokeNotesEEPROM();    // grava notas choke restauradas na EEPROM principal
  
  forceImmediateSaveToEEPROM();
  
  forceLedsOff();
  for(int i = 0; i < 3; i++) {
    digitalWrite(13, HIGH);
    delay(150);
    digitalWrite(13, LOW);
    delay(150);
  }
  
  #endif
}

bool BackupExists(byte backupNumber) {
  #if defined(__AVR__)
  
  if(backupNumber < 1 || backupNumber > 3) return false;
  
  int startAddress = getBackupStartAddress(backupNumber);
  int maxSize = getBackupSize(backupNumber);
  
  if(EEPROM.read(startAddress) != 0xAB) return false;
  if(EEPROM.read(startAddress + 1) != backupNumber) return false;
  if(EEPROM.read(startAddress + 2) != 0x55) return false;
  if(EEPROM.read(startAddress + maxSize - 1) != 0xCD) return false;
  
  return true;
  
  #else
  return false;
  #endif
}

void ClearBackup(byte backupNumber) {
  #if defined(__AVR__)
  
  if(backupNumber < 1 || backupNumber > 3) return;
  
  int startAddress = getBackupStartAddress(backupNumber);
  int maxSize = getBackupSize(backupNumber);
  
  EEPROM.write(startAddress, 0x00);
  EEPROM.write(startAddress + 1, 0x00);
  EEPROM.write(startAddress + 2, 0x00);
  EEPROM.write(startAddress + maxSize - 1, 0x00);
  
  forceLedsOff();
  for(int i = 0; i < 2; i++) {
    digitalWrite(13, HIGH);
    delay(50);
    digitalWrite(13, LOW);
    delay(50);
  }
  
  #endif
}

void CheckAllBackupsIntegrity() {
  #if defined(__AVR__)
  
  bool corruptionFound = false;
  
  for(byte i = 1; i <= 3; i++) {
    if(!BackupExists(i)) continue;
    
    int startAddress = getBackupStartAddress(i);
    
    byte testNSensor = EEPROM.read(startAddress + 7);
    if(testNSensor > 6) {
      ClearBackup(i);
      corruptionFound = true;
      continue;
    }
    
    byte testPinCount = EEPROM.read(startAddress + 6);
    if(testPinCount != 16) {
      ClearBackup(i);
      corruptionFound = true;
    }
  }
  
  if(corruptionFound) {
    forceLedsOff();
    for(int i = 0; i < 10; i++) {
      digitalWrite(13, HIGH);
      delay(30);
      digitalWrite(13, LOW);
      delay(30);
    }
  }
  
  #endif
}

void InitBackupSystem() {
  CheckAllBackupsIntegrity();
  
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);
  delay(100);
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);
}