///=========================================================================================//
//=>                         J-DRUMS v8.0.14 - CONTROLADOR MIDI BATERIA                    <= 
//=>                        Sistema de Bateria Eletronica Arduino                          <=
//=>                     Copyright (c) 2026 Joanaldo Jhon Leonez de Melo                   <=
//=>                             Licensed under the MIT License.                           <=
//=>         See LICENSE.txt file in the project root for full license information.        <=
//=>                      DESENVOLVIDO POR JOANALDO JHON LEONEZ DE MELO                    <=
//=>                                   Janeiro/2026                                        <=
//=>                                                                                       <=
//=>  FUNCIONALIDADES:                                                                     <=
//=>  . Fila nao-bloqueante de gravacao EEPROM                                             <=
//=>  . Grava 1 byte por ciclo do loop sem parar o audio MIDI                             <=
//=>  . Salvamento imediato ao alterar qualquer valor no menu                              <=
//=>  . Sem timer de 80 segundos - sem perda de dados ao desligar                         <=
//=>  . Suporte ao novo parametro MAX_NOTE_AGE_MS (endereco 525)                          <=
//=========================================================================================//

// -----------------------------------------------------------------------
//  FILA NAO-BLOQUEANTE
//  Cada entrada: endereco EEPROM + valor a gravar
//  O loop() processa 1 byte por chamada - audio nao trava
// -----------------------------------------------------------------------

// *** rimSub declarado em h_menu.ino (compilado depois) — declaracao externa ***
extern byte rimSub;

#define EEPROM_QUEUE_SIZE 320  // *** CORRECAO: aumentado de 64 para 320 (suporta save completo: ~303 bytes) ***

struct EepromJob {
  int  address;
  byte value;
};

EepromJob eepromQueue[EEPROM_QUEUE_SIZE];
// *** CORRECAO: byte nao suporta valores > 255, mas EEPROM_QUEUE_SIZE=320 ***
// Com byte, indices fazem overflow em 255 e sobrescrevem entradas da fila.
uint16_t queueHead  = 0;
uint16_t queueTail  = 0;
uint16_t queueCount = 0;

// Variaveis de controle da fila
unsigned long lastSaveTime  = 0;
bool pendingChanges         = false;

// -----------------------------------------------------------------------
//  Enfileira 1 byte - retorna imediatamente, nao bloqueia
// -----------------------------------------------------------------------
void _enqueueEEPROM(int address, byte value) {
  if(queueCount >= EEPROM_QUEUE_SIZE) return;
  eepromQueue[queueTail].address = address;
  eepromQueue[queueTail].value   = value;
  queueTail = (queueTail + 1) % EEPROM_QUEUE_SIZE;
  queueCount++;
  pendingChanges = true;
}

// -----------------------------------------------------------------------
//  saveToEEPROMIfNeeded() - chamada no loop()
//  Processa APENAS 1 byte por chamada - loop continua sem atraso
// -----------------------------------------------------------------------
void saveToEEPROMIfNeeded() {
  if(queueCount == 0) {
    pendingChanges = false;
    return;
  }
  #if defined(__AVR__)
  int  addr = eepromQueue[queueHead].address;
  byte val  = eepromQueue[queueHead].value;
  // Grava so se o valor mudou - protege desgaste da EEPROM
  if(EEPROM.read(addr) != val) {
    EEPROM.write(addr, val);
  }
  #endif
  queueHead = (queueHead + 1) % EEPROM_QUEUE_SIZE;
  queueCount--;
  if(queueCount == 0) {
    pendingChanges = false;
    lastSaveTime = millis();
  }
}

// -----------------------------------------------------------------------
//  Descarrega fila de forma sincrona (apenas para backup/restore)
// -----------------------------------------------------------------------
void _flushQueueSync() {
  #if defined(__AVR__)
  while(queueCount > 0) {
    int  addr = eepromQueue[queueHead].address;
    byte val  = eepromQueue[queueHead].value;
    if(EEPROM.read(addr) != val) EEPROM.write(addr, val);
    queueHead = (queueHead + 1) % EEPROM_QUEUE_SIZE;
    queueCount--;
  }
  #endif
  pendingChanges = false;
  lastSaveTime   = millis();
}

void initCacheSystem() {
  queueHead       = 0;
  queueTail       = 0;
  queueCount      = 0;
  lastSaveTime    = millis();
  pendingChanges  = false;
}

// -----------------------------------------------------------------------
//  Helpers de endereco EEPROM (espelham e_eeprom.ino)
// -----------------------------------------------------------------------
inline int _addrPin(byte pin, byte param) { return 100 + (pin * 16) + param; }
inline int _addrGeneral(byte param)       { return (int)param; }
inline int _addrHH(byte param)            { return 50  + param; }
inline int _addrAdvanced(byte param)      { return 70  + param; }
inline int _addrBacklight()               { return 520; }
inline int _addrBuzzer()                  { return 521; }
inline int _addrName(byte pin)            { return 500 + pin; }
inline int _addrMaxNoteAge()              { return 525; }  // *** NOVO ***

// -----------------------------------------------------------------------
//  Leitura dos valores atuais das variaveis
// -----------------------------------------------------------------------
byte _valHH(byte param) {
  if(param < 4)  return HHNoteSensor[param];
  if(param < 8)  return HHThresoldSensor[param - 4];
  if(param < 10) return HHFootNoteSensor[param - 8];
  return HHFootThresoldSensor[param - 10];
}

byte _valAdvanced(byte param) {
  switch(param) {
    case 0:  return ENABLE_NOTE_37_38_TO_40;
    case 1:  return ENABLE_VELOCITY_FILTER;
    case 2:  return ENABLE_RIMSHOT_38_TO_40;
    case 3:  return ENABLE_NOTE_101_TO_102;
    case 4:  return ENABLE_NOTE_103_TO_104;
    case 5:  return (byte)(BLOCK_WINDOW_MS / 10);
    case 6:  return VELOCITY_THRESHOLD_37_38;
    case 7:  return (byte)DETECTION_WINDOW_MS;
    case 8:  return CUSTOM_NOTE_37;
    case 9:  return CUSTOM_NOTE_40;
    case 10: return CUSTOM_RIMSHOT_FORCE_NOTE;
    case 11: return rimSub;  // *** rimSub (Comp=0 / Forc=1) ***
    case 12: return MIDI_USB_ENABLED;
    case 13: return MIDI_TX1_ENABLED;
    default: return 0;
  }
}

// -----------------------------------------------------------------------
//  mark*() - enfileia bytes e retorna imediatamente
//  O loop grava 1 byte por ciclo - SEM parar o audio
// -----------------------------------------------------------------------

void markGeneralChanged() {
  _enqueueEEPROM(_addrGeneral(0x02), NSensor);
  _enqueueEEPROM(_addrGeneral(0x03), GeneralXtalk);
}

void markHHChanged() {
  for(byte h = 0; h < 12; h++) {
    _enqueueEEPROM(_addrHH(h), _valHH(h));
  }
}

// markAdvancedChanged inclui MAX_NOTE_AGE_MS (endereco 525)
void markAdvancedChanged() {
  for(byte a = 0; a < 14; a++) {
    _enqueueEEPROM(_addrAdvanced(a), _valAdvanced(a));
  }
  // *** NOVO: MAX_NOTE_AGE_MS salva junto com Advanced ***
  _enqueueEEPROM(_addrMaxNoteAge(), (byte)MAX_NOTE_AGE_MS);
}

void markBacklightChanged() {
  _enqueueEEPROM(_addrBacklight(), backlightMode);
}

void markBuzzerChanged() {
  #if BUZZER
  _enqueueEEPROM(_addrBuzzer(), buzzerEnabled ? 1 : 0);
  #else
  _enqueueEEPROM(_addrBuzzer(), 0);
  #endif
}

void markNameChanged() {
  for(byte i = 0; i < 16; i++) {
    _enqueueEEPROM(_addrName(i), selected_names[i]);
  }
}

void markMidiOutputChanged() {
  _enqueueEEPROM(_addrAdvanced(12), MIDI_USB_ENABLED);
  _enqueueEEPROM(_addrAdvanced(13), MIDI_TX1_ENABLED);
}

void markPinChanged(byte pin) {
  if(pin < 16) {
    for(byte j = 0; j < 16; j++) {  // *** CORRIGIDO: j<16 inclui param 15 (ANTICROSSTALK=0x0F) ***
      _enqueueEEPROM(_addrPin(pin, j), GetPinSetting(pin, j));
    }
  }
}

void markAllPinsChanged() {
  for(byte i = 0; i < 16; i++) {
    markPinChanged(i);
  }
}

// -----------------------------------------------------------------------
//  forceBuzzerSave - gravacao sincrona pontual do buzzer
// -----------------------------------------------------------------------
void forceBuzzerSave() {
  #if BUZZER
  SaveBuzzerEEPROM();
  digitalWrite(13, HIGH);
  if(buzzerEnabled) playBeep();
  delay(200);
  digitalWrite(13, LOW);
  #endif
}

// -----------------------------------------------------------------------
//  forceImmediateSaveToEEPROM - gravacao sincrona completa DIRETA na EEPROM
//  *** CORRECAO: NAO usa a fila. Grava byte a byte direto via EEPROM.write()
//  para evitar overflow da fila (303 bytes > tamanho antigo de 64).
//  Usar apenas em backup/restore/reset (sem audio ativo neste momento)
// -----------------------------------------------------------------------
void forceImmediateSaveToEEPROM() {
  #if defined(__AVR__)
  // --- Limpa fila antes de gravar direto, evita residuos ---
  queueHead  = 0;
  queueTail  = 0;
  queueCount = 0;
  pendingChanges = false;

  // --- Geral ---
  EEPROM.write(_addrGeneral(0x02), NSensor);
  EEPROM.write(_addrGeneral(0x03), GeneralXtalk);

  // --- HiHat ---
  for(byte h = 0; h < 12; h++) {
    EEPROM.write(_addrHH(h), _valHH(h));
  }

  // --- Advanced (14 params + MAX_NOTE_AGE_MS) ---
  for(byte a = 0; a < 14; a++) {
    EEPROM.write(_addrAdvanced(a), _valAdvanced(a));
  }
  EEPROM.write(_addrMaxNoteAge(), (byte)MAX_NOTE_AGE_MS);

  // --- Backlight ---
  EEPROM.write(_addrBacklight(), backlightMode);

  // --- Buzzer ---
  #if BUZZER
  EEPROM.write(_addrBuzzer(), buzzerEnabled ? 1 : 0);
  #else
  EEPROM.write(_addrBuzzer(), 0);
  #endif

  // --- Nomes dos pads ---
  for(byte i = 0; i < 16; i++) {
    EEPROM.write(_addrName(i), selected_names[i]);
  }

  // --- Pads digitais Aux 1-7 (526-532) ---
  for(byte i = 0; i < 7; i++) {
    EEPROM.write(526 + i, digitalPadNotes[i]);
  }
  // *** CORRECAO BUG FLAG 579: grava 0xA3 (valor que LoadDigitalNotesEEPROM espera) ***
  // *** CORRECAO CONFLITO: flag movida de 537 para 579 (537 e PRESET_NAME_BASE) ***
  EEPROM.write(579, 0xA3);

  // --- Chokes (533-536) ---
  SaveChokeNotesEEPROM();

  // --- Todos os pinos (100 + pin*16 + param) ---
  for(byte i = 0; i < 16; i++) {
    for(byte j = 0; j < 16; j++) {
      EEPROM.write(_addrPin(i, j), GetPinSetting(i, j));
    }
  }

  // --- VelMinimo, InvertSensor, XCancel ---
  SaveVelMinimoEEPROM();
  SaveInvertSensorEEPROM();
  SaveXCancelEEPROM();

  // --- Notas Persona (endereços 799-815) ---
  pendingChanges = false;
  lastSaveTime   = millis();
  #endif
}

// -----------------------------------------------------------------------
//  Backup / Restore
// -----------------------------------------------------------------------
void saveBackupWithCache(byte backupNumber) {
  forceImmediateSaveToEEPROM();
  SaveBackup(backupNumber);
}

void restoreBackupWithCache(byte backupNumber) {
  RestoreBackup(backupNumber);
  markGeneralChanged();
  markHHChanged();
  markAdvancedChanged();
  markBacklightChanged();
  markBuzzerChanged();
  markNameChanged();
  markMidiOutputChanged();
  SaveDigitalNotesEEPROM();  // pads digitais 526-532
  SaveChokeNotesEEPROM();    // chokes 533-536
  markAllPinsChanged();
  _flushQueueSync();
  // *** CORRECAO: salvar VelMinimo, InvertSensor e XCancel restaurados do backup ***
  SaveVelMinimoEEPROM();
  SaveInvertSensorEEPROM();
  SaveXCancelEEPROM();
}

void printCacheStatus() {
  Serial.print("Fila EEPROM - bytes pendentes: ");
  Serial.println(queueCount);
}
