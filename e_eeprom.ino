//=========================================================================================//
//=>                         J-DRUMS v8.0.19 - CONTROLADOR MIDI BATERIA                    <= 
//=>                        Sistema de Bateria Eletrônica Arduino                          <=
//=>                     Copyright (c) 2026 Joanaldo Jhon Leonez de Melo                   <=
//=>                             Licensed under the MIT License.                           <=
//=>         See LICENSE.txt file in the project root for full license information.        <=
//=>                      DESENVOLVIDO POR JOANALDO JHON LEONEZ DE MELO                    <=
//=>                                   Janeiro/2026                                        <=
//=>                                                                                       <=
//=>  FUNCIONALIDADES:                                                                     <=
//=>  • Salvamento de configurações na EEPROM                                              <=
//=>  • Sistema de cache (salva a cada 60s)                                                <=
//=>  • 3 slots de backup/presets completos                                                <=
//=>  • Proteção contra corrupção de dados                                                 <=
//=>  • Recuperação automática em caso de falha                                            <=
//=========================================================================================//

//==============================
//    EEPROM - 16 PINOS FIXOS + VST PRESET + SAÍDAS MIDI (14 PARÂMETROS ADVANCED)
//    *** COM SUPORTE A PERSONA (NOTAS SALVAM) ***
//==============================

#if defined(__arm__) 
//TODO: https://github.com/sebnil/DueFlashStorage
#endif

// *** MODIFICAÇÃO 2: rimSub definido em h_menu.ino (compilado depois) — declaração externa ***
extern byte rimSub;

void LoadAllEEPROM()
{
  #if defined(__AVR__)
  byte Version=EEPROM.read(1);
  
  if(Version==41)  // *** v41: inclui XCanCost (param 0x0F) no load/save ***
  {
    LoadGeneralEEPROM(0);
    LoadGeneralEEPROM(2);
    LoadGeneralEEPROM(3);
    
    for(int a=0; a<14; a++)
      LoadAdvancedEEPROM(a);
    
    for(int h=0; h<12; h++)
      LoadHHEEPROM(h);
    
    for(int i=0; i<16; i++)
      for(int j=0; j<16; j++)  // *** CORRIGIDO: j<16 inclui param 14 e 15 (ANTICROSSTALK=0x0F) ***
        LoadEEPROM(i,j);
    
    LoadBacklightFromEEPROM();
    LoadBuzzerFromEEPROM();
    LoadMaxNoteAgeEEPROM();  // *** NOVO ***
    LoadNamesFromEEPROM();
    LoadVelMinimoEEPROM();       // *** VEL MINIMO POR PAD (A0-A15) ***
    LoadInvertSensorEEPROM();    // *** INVERT SENSOR POR PAD (A0-A15) ***
    
    // *** NOVO: Carrega estado Pads Auxiliar ***
    LoadPadsAuxiliarEEPROM();
    
    // *** NOVO: Carrega modo dos Botões ***
    LoadModoBotoesEEPROM();
    
    // *** NOVO: Carrega notas Pads Digitais ***
    LoadDigitalNotesEEPROM();
    
    // *** NOVO: Carrega notas Choke ***
    LoadChokeNotesEEPROM();
    
    // Reaplicar preset para sincronizar estruturas internas com notas Addictive
    applyVSTPreset();
    
    // *** RESTAURA notas dos pads A2-A15 salvas na EEPROM (applyVSTPreset sobrescreve) ***
    for(int i = 2; i < 16; i++)
      LoadEEPROM(i, 0);  // param 0 = Note
    
    // *** RESTAURA CUSTOM_NOTE_40 e CUSTOM_RIMSHOT_FORCE_NOTE salvos na EEPROM ***
    LoadAdvancedEEPROM(9);   // case 9 = CUSTOM_NOTE_40
    LoadAdvancedEEPROM(10);  // case 10 = CUSTOM_RIMSHOT_FORCE_NOTE

    // *** RESTAURA digitalPadNotes e sincroniza zoneDual_NoteAux e NoteBlock ***
    LoadDigitalNotesEEPROM();
    for(byte p = 0; p < ZONE_DUAL_PAIRS; p++) {
      // NoteAux segue digitalPadNotes (índice p coincide com Dig 1-6)
      zoneDual_NoteAux[p]   = digitalPadNotes[p];
      // NoteBlock segue a nota atual do pad piezo associado
      zoneDual_NoteBlock[p] = Pin[zoneDual_PadIdx[p]].Note;
    }

    // *** NOVO: Carrega tabela XCancel da EEPROM ***
    LoadXCancelEEPROM();
    
    // *** Sincroniza CUSTOM_NOTE_38 com nota atual do pad A2 restaurada ***
    if(Pin[2].Note > 0 && Pin[2].Note <= 127) {
      CUSTOM_NOTE_38 = Pin[2].Note;
    }

    // *** Sincroniza CUSTOM_NOTE_37 com nota atual do pad A3 restaurada ***
    if(Pin[3].Note > 0 && Pin[3].Note <= 127) {
      CUSTOM_NOTE_37 = Pin[3].Note;
    }
    
    // *** Aplica modo Comp/Forc/Des salvo (rimSub) aos enables ***
    if(rimSub == 0) {
      ENABLE_NOTE_37_38_TO_40 = 1;
      ENABLE_RIMSHOT_38_TO_40 = 0;
    } else if(rimSub == 1) {
      ENABLE_NOTE_37_38_TO_40 = 0;
      ENABLE_RIMSHOT_38_TO_40 = 1;
      CUSTOM_RIMSHOT_FORCE_NOTE = CUSTOM_NOTE_40; // Forc: sincroniza nota forcada com RishotNote
    } else {
      // Des(2): desativa os dois rimshots
      ENABLE_NOTE_37_38_TO_40 = 0;
      ENABLE_RIMSHOT_38_TO_40 = 0;
    }
  }
  else
  {
    SaveGeneralEEPROM(0);
    SaveGeneralEEPROM(2);
    SaveGeneralEEPROM(3);
    
    for(int a=0; a<14; a++)
      SaveAdvancedEEPROM(a);
    
    for(int h=0; h<12; h++)
      SaveHHEEPROM(h);
    
    for(int i=0; i<16; i++)
      for(int j=0; j<16; j++)  // *** CORRIGIDO: j<16 inclui param 14 e 15 (ANTICROSSTALK=0x0F) ***
        SaveEEPROM(i,j);
    
    SaveBacklightEEPROM();
    SaveBuzzerEEPROM();
    
    for(int i=0; i<16; i++)
      SaveNameEEPROM(i);
    
    // *** NOVO: Grava padrões dos Pads Digitais e Chokes na primeira inicialização ***
    SaveDigitalNotesEEPROM();
    SaveChokeNotesEEPROM();
        
    EEPROM.write(1, 41);  // *** v41 ***
  }
  #endif
}

void LoadEEPROM(byte Pin, byte Param)
{
  #if defined(__AVR__)
  if(Pin >= 16) return;
  
  byte Value=EEPROM.read(100+(Pin*16)+Param);
  // RETRIGGER (0x04): permite 255 = Aut. Demais parâmetros: clampeia em 127.
  if(Param != RETRIGGER) {
    if(Value > 127) Value = 127;
  }
  ExecCommand(0x03,Pin,Param,Value);
  #endif
}

void SaveEEPROM(byte Pin, byte Param, byte Value)
{
  #if defined(__AVR__)
  if(Pin >= 16) return;
  
  ExecCommand(0x03,Pin,Param,Value);
  EEPROM.write(100+(Pin*16)+Param, Value);
  #endif
}

void SaveEEPROM(byte Pin, byte Param)
{
  #if defined(__AVR__)
  if(Pin >= 16) return;
  
  EEPROM.write(100+(Pin*16)+Param, GetPinSetting(Pin,Param));
  #endif
}

void SaveGeneralEEPROM(byte Param, byte Value)
{
  #if defined(__AVR__)
  ExecCommand(0x03,0x7E,Param,Value);
  EEPROM.write(Param,Value);
  #endif
}

void SaveGeneralEEPROM(byte Param)
{
  #if defined(__AVR__)
  byte Value=0;
  switch(Param)
  {
    case 0x00:
      Value=delayTime/2;
    break;

    case 0x02:
      Value=NSensor;
      break;
    
    case 0x03:
      Value=GeneralXtalk;
     break;
  }
  if(Value>127) Value=127;
  EEPROM.write(Param,Value);
  #endif
}

void SaveHHEEPROM(byte Param, byte Value)
{
  #if defined(__AVR__)
  ExecCommand(0x03,0x4C,Param,Value);
  EEPROM.write(50+Param,Value); 
  #endif
}

void SaveHHEEPROM(byte Param)
{
  #if defined(__AVR__)
  byte Value=0;
  
  if(Param<4)Value=HHNoteSensor[Param];
   else if(Param<8)Value=HHThresoldSensor[Param-4];
   else if(Param<10) Value=HHFootNoteSensor[Param-8];
   else Value=HHFootThresoldSensor[Param-10];
   
  if(Value>127) Value=127;
  EEPROM.write(50+Param,Value); 
  #endif
}

void SaveAdvancedEEPROM(byte Param, byte Value)
{
  #if defined(__AVR__)
  ExecCommand(0x03,0x7D,Param,Value);
  EEPROM.write(70+Param,Value); 
  #endif
}

void SaveAdvancedEEPROM(byte Param)
{
  #if defined(__AVR__)
  byte Value=0;
  
  switch(Param)
  {
    case 0: Value=ENABLE_NOTE_37_38_TO_40; break;
    case 1: Value=ENABLE_VELOCITY_FILTER; break;
    case 2: Value=ENABLE_RIMSHOT_38_TO_40; break;
    case 3: Value=ENABLE_NOTE_101_TO_102; break;
    case 4: Value=ENABLE_NOTE_103_TO_104; break;
    case 5: Value=(byte)(BLOCK_WINDOW_MS/10); break;
    case 6: Value=VELOCITY_THRESHOLD_37_38; break;
    case 7: Value=(byte)DETECTION_WINDOW_MS; break;
    case 8: Value=CUSTOM_NOTE_37; break;
    case 9: Value=CUSTOM_NOTE_40; break;
    case 10: Value=CUSTOM_RIMSHOT_FORCE_NOTE; break;
    case 11: Value=rimSub; break;   // *** MODIFICAÇÃO 2: salva rimSub (Comp=0/Forc=1) ***
    case 12: Value=MIDI_USB_ENABLED; break;
    case 13: Value=MIDI_TX1_ENABLED; break;
  }
  
  if(Value>127) Value=127;
  EEPROM.write(70+Param,Value); 
  #endif
}

void LoadAdvancedEEPROM(byte Param)
{
  #if defined(__AVR__)
  byte Value=EEPROM.read(70+Param);
  
  // *** Se EEPROM virgem (255), nao aplica - mantém valor definido no código ***
  if(Value == 255) return;
  
  if(Value>127) Value=127;
  
  byte paramCode = 0;
  switch(Param)
  {
    case 0: paramCode = ADV_ENABLE_37_38_TO_40; break;
    case 1: paramCode = ADV_ENABLE_VEL_FILTER; break;
    case 2: paramCode = ADV_ENABLE_RIMSHOT; break;
    case 3: paramCode = ADV_ENABLE_101_TO_102; break;
    case 4: paramCode = ADV_ENABLE_103_TO_104; break;
    case 5: paramCode = ADV_BLOCK_WINDOW; break;
    case 6: paramCode = ADV_VEL_THRESHOLD_37_38; break;
    case 7: paramCode = ADV_DETECTION_WINDOW; break;
    case 8: paramCode = RIMSHOT_NOTE_37; break;
    case 9: paramCode = RIMSHOT_NOTE_40; break;
    case 10: paramCode = RIMSHOT_FORCE_NOTE_40; break;
    case 11:  // *** MODIFICAÇÃO 2: carrega rimSub (Comp/Forc/Des do A2) ***
      rimSub = (Value <= 2) ? Value : 0;
      return;  // não usa ExecCommand para rimSub
    case 12: paramCode = MIDI_OUTPUT_USB; break;
    case 13: paramCode = MIDI_OUTPUT_TX1; break;
  }
  
  ExecCommand(0x03,0x7D,paramCode,Value);
  #endif
}

void LoadGeneralEEPROM(byte Param)
{
  #if defined(__AVR__)
  byte Value=EEPROM.read(Param);
  if(Value>127) Value=127;
  ExecCommand(0x03,0x7E,Param,Value);
  #endif
}

void LoadHHEEPROM(byte Param)
{
  #if defined(__AVR__)
  byte Value=EEPROM.read(50+Param);
  if(Value>127) Value=127;
  ExecCommand(0x03,0x4C,Param,Value);
  #endif
}

void SaveBacklightEEPROM()
{
  #if defined(__AVR__)
  EEPROM.write(520, backlightMode);
  #endif
}

void LoadBacklightFromEEPROM()
{
  #if defined(__AVR__)
  backlightMode = EEPROM.read(520);
  if(backlightMode > 1) backlightMode = 0;
  #endif
}

void SaveBuzzerEEPROM()
{
  #if defined(__AVR__)
  #if BUZZER
  EEPROM.write(521, buzzerEnabled ? 1 : 0);
  #else
  EEPROM.write(521, 0);
  #endif
  #endif
}

void LoadBuzzerFromEEPROM()
{
  #if defined(__AVR__)
  #if BUZZER
  byte buzzerValue = EEPROM.read(521);
  buzzerEnabled = (buzzerValue == 1);
  #else
  buzzerEnabled = false;
  #endif
  #endif
}

void SaveNameEEPROM(byte pin)
{
  #if defined(__AVR__)
  if(pin < 16) {
    // *** CORRIGIDO: aceita todos os 35 nomes disponíveis (0-34).
    // O limite antigo "> 24" era um resíduo de quando havia menos nomes
    // e bloqueava índices 25-34 (SURDO 1, SURDO 2, CAIXA, ARO CAIXA, etc.).
    if(selected_names[pin] >= NUM_AVAILABLE_NAMES) {
      selected_names[pin] = 0;
    }
    
    int address = 500 + pin;
    EEPROM.write(address, selected_names[pin]);
  }
  #endif
}

void LoadNamesFromEEPROM()
{
  #if defined(__AVR__)
  for(byte i = 0; i < 16; i++) {
    int address = 500 + i;
    
    selected_names[i] = EEPROM.read(address);
    
    // *** CORRIGIDO: aceita todos os 35 nomes disponíveis (0-34).
    // O limite antigo "> 24" bloqueava SURDO 1(25), SURDO 2(26), PAD EFET 1(31) etc.
    if(selected_names[i] >= NUM_AVAILABLE_NAMES || 
       selected_names[i] == 255) {
      
      selected_names[i] = (i < NUM_AVAILABLE_NAMES) ? i : 0;
      SaveNameEEPROM(i);
    }
  }
  
  bool foundCorruption = false;
  for(byte i = 0; i < 16; i++) {
    if(selected_names[i] >= NUM_AVAILABLE_NAMES) {
      selected_names[i] = 0;
      foundCorruption = true;
    }
  }
  
  if(foundCorruption) {
    for(byte i = 0; i < 16; i++) {
      SaveNameEEPROM(i);
    }
  }
  #endif
}

void SaveAllToEEPROMImmediate()
{
  #if defined(__AVR__)
  
  SaveGeneralEEPROM(0x02);
  SaveGeneralEEPROM(0x03);
  
  for(int h = 0; h < 12; h++) {
    SaveHHEEPROM(h);
  }
  
  for(int a = 0; a < 14; a++) {
    SaveAdvancedEEPROM(a);
  }
  
  SaveBacklightEEPROM();
  SaveBuzzerEEPROM();
  SaveMaxNoteAgeEEPROM();  // *** NOVO ***
  SaveVelMinimoEEPROM();   // *** VEL MINIMO POR PAD ***
  SaveInvertSensorEEPROM(); // *** INVERT SENSOR POR PAD ***
  
  for(int i = 0; i < 16; i++) {
    SaveNameEEPROM(i);
  }
  
  for(int i = 0; i < 16; i++) {
    for(int j = 0; j < 14; j++) {
      SaveEEPROM(i, j);
    }
  }
  
  for(int i = 0; i < 5; i++) {
    digitalWrite(13, HIGH);
    delay(50);
    digitalWrite(13, LOW);
    delay(50);
  }
  
  #endif
}

// *** SAVE/LOAD MAX_NOTE_AGE_MS — endereço EEPROM 525 ***
void SaveMaxNoteAgeEEPROM() {
  #if defined(__AVR__)
  EEPROM.write(525, (byte)MAX_NOTE_AGE_MS);
  #endif
}

void LoadMaxNoteAgeEEPROM() {
  #if defined(__AVR__)
  byte val = EEPROM.read(525);
  if(val >= 1 && val <= 30) {
    MAX_NOTE_AGE_MS = val;  // Usa valor salvo na EEPROM
  }
  // Se EEPROM virgem (255) ou inválida: mantém o valor definido no código
  #endif
}

// *** NOMES PERSONALIZADOS DOS PRESETS DE BACKUP ***
//  Endereços 537-549 → Preset 1 (13 bytes: 12 chars + '\0')
//  Endereços 550-562 → Preset 2
//  Endereços 563-575 → Preset 3
//  Endereços 576-578 → Flags de nome customizado (1 = customizado)
//
//  Nomes padrão: "Preset 1", "Preset 2", "Preset 3"
//  (idêntico ao sistema de nomes dos pads principais)

#define PRESET_NAME_BASE   537   // 3 × 13 bytes = 39 bytes (537..575)
#define PRESET_FLAG_BASE   576   // 3 bytes de flag (576, 577, 578)

// Retorna true se o preset tem nome customizado
bool hasPresetCustomName(byte presetNum) {
  if(presetNum < 1 || presetNum > 3) return false;
  #if defined(__AVR__)
  return EEPROM.read(PRESET_FLAG_BASE + (presetNum - 1)) == 1;
  #else
  return false;
  #endif
}

// Salva nome customizado para o preset (1..3)
void savePresetCustomName(byte presetNum, char* name) {
  if(presetNum < 1 || presetNum > 3) return;
  #if defined(__AVR__)
  int addr = PRESET_NAME_BASE + (presetNum - 1) * 13;
  for(int i = 0; i < 12; i++) {
    char c = (name[i] != '\0') ? name[i] : ' ';
    EEPROM.write(addr + i, c);
  }
  EEPROM.write(addr + 12, '\0');
  EEPROM.write(PRESET_FLAG_BASE + (presetNum - 1), 1);
  #endif
}

// Carrega nome do preset para buffer[13]
// Se não tem nome customizado, usa "Preset X"
void loadPresetName(byte presetNum, char* buffer) {
  if(presetNum < 1 || presetNum > 3) {
    strcpy(buffer, "Preset ?");
    return;
  }
  #if defined(__AVR__)
  if(EEPROM.read(PRESET_FLAG_BASE + (presetNum - 1)) == 1) {
    int addr = PRESET_NAME_BASE + (presetNum - 1) * 13;
    for(int i = 0; i < 12; i++) {
      buffer[i] = EEPROM.read(addr + i);
    }
    buffer[12] = '\0';
    return;
  }
  #endif
  // Nome padrão
  strcpy(buffer, "Preset ");
  buffer[7] = '0' + presetNum;
  buffer[8] = '\0';
}

// Apaga nome customizado e volta ao padrão
void clearPresetCustomName(byte presetNum) {
  if(presetNum < 1 || presetNum > 3) return;
  #if defined(__AVR__)
  EEPROM.write(PRESET_FLAG_BASE + (presetNum - 1), 0);
  #endif
}

// *** VEL_MINIMO POR PAD — endereços 652 a 667 (16 bytes, um por porta A0-A15) ***
#define EEPROM_VEL_MINIMO_BASE  652
#define EEPROM_VEL_MINIMO_FLAG  668  // flag de validade: 0xBB = dados gravados

// *** INVERT_SENSOR POR PAD — endereços 669 a 684 (16 bytes, um por porta A0-A15) ***
#define EEPROM_INVERT_SENSOR_BASE  669
#define EEPROM_INVERT_SENSOR_FLAG  685  // flag de validade: 0xCC = dados gravados

void SaveInvertSensorEEPROM() {
  #if defined(__AVR__)
  for(byte i = 0; i < 16; i++) {
    EEPROM.write(EEPROM_INVERT_SENSOR_BASE + i, Pin[i].InvertSensor ? 1 : 0);
  }
  EEPROM.write(EEPROM_INVERT_SENSOR_FLAG, 0xCC);
  #endif
}

void LoadInvertSensorEEPROM() {
  #if defined(__AVR__)
  if(EEPROM.read(EEPROM_INVERT_SENSOR_FLAG) != 0xCC) return; // EEPROM virgem
  for(byte i = 0; i < 16; i++) {
    byte v = EEPROM.read(EEPROM_INVERT_SENSOR_BASE + i);
    Pin[i].InvertSensor = (v == 1) ? 1 : 0;
  }
  #endif
}
void SaveVelMinimoEEPROM() {
  #if defined(__AVR__)
  for(byte i = 0; i < 16; i++) {
    byte v = Pin[i].VelMinimo;
    if(v > 60) v = 60;
    EEPROM.write(EEPROM_VEL_MINIMO_BASE + i, v);
  }
  EEPROM.write(EEPROM_VEL_MINIMO_FLAG, 0xBB);
  #endif
}

void LoadVelMinimoEEPROM() {
  #if defined(__AVR__)
  if(EEPROM.read(EEPROM_VEL_MINIMO_FLAG) != 0xBB) return; // EEPROM virgem
  for(byte i = 0; i < 16; i++) {
    byte v = EEPROM.read(EEPROM_VEL_MINIMO_BASE + i);
    if(v <= 60) Pin[i].VelMinimo = v;
    else        Pin[i].VelMinimo = 0;
  }
  #endif
}

// *** XCANCELL — endereços 587 a 650 (16 pares × 4 bytes = 64 bytes) ***
// Cada par: [source, target, windowMs, ghostVel] — 4 bytes consecutivos
// Endereço base do par N: 587 + (N * 4)
#define EEPROM_XCANCEL_BASE  587
#define EEPROM_XCANCEL_FLAG  651  // flag de validade: 0xAC = dados gravados

void SaveXCancelEEPROM() {
  #if defined(__AVR__)
  for(byte i = 0; i < XPAIR_COUNT; i++) {
    if(i == 0) continue;  // *** Par 1/16 e somente leitura — nao salva na EEPROM ***
    int addr = EEPROM_XCANCEL_BASE + (i * 4);
    EEPROM.write(addr + 0, xpairRam[i].source);
    EEPROM.write(addr + 1, xpairRam[i].target);
    EEPROM.write(addr + 2, xpairRam[i].windowMs);
    EEPROM.write(addr + 3, xpairRam[i].ghostVel);
  }
  EEPROM.write(EEPROM_XCANCEL_FLAG, 0xAC);  // marca como válido
  #endif
}

void LoadXCancelEEPROM() {
  #if defined(__AVR__)
  // Só carrega se a flag de validade estiver presente
  if(EEPROM.read(EEPROM_XCANCEL_FLAG) != 0xAC) return;
  for(byte i = 0; i < XPAIR_COUNT; i++) {
    if(i == 0) continue;  // *** Par 1/16 e somente leitura — sempre usa o padrao do c_pin.ino ***
    int addr = EEPROM_XCANCEL_BASE + (i * 4);
    byte src = EEPROM.read(addr + 0);
    byte tgt = EEPROM.read(addr + 1);
    byte win = EEPROM.read(addr + 2);
    byte vel = EEPROM.read(addr + 3);
    // Valida: source/target devem ser 0-15 ou 255 (desativado)
    if(src <= 15 || src == 255) xpairRam[i].source  = src;
    if(tgt <= 15 || tgt == 255) xpairRam[i].target  = tgt;
    if(win >= 10)               xpairRam[i].windowMs = win;
    if(vel >= 1 && vel <= 50)   xpairRam[i].ghostVel = vel;
  }
  #endif
}