///=========================================================================================//
//=>                         J-DRUMS v5.0 - CONTROLADOR MIDI BATERIA                       <= 
//=>                        Sistema de Bateria Eletrônica Arduino                          <=
//=>                     Copyright (c) 2026 Joanaldo Jhon Leonez de Melo                   <=
//=>                             Licensed under the MIT License.                           <=
//=>         See LICENSE.txt file in the project root for full license information.        <=
//=>                      DESENVOLVIDO POR JOANALDO JHON LEONEZ DE MELO                    <=
//=>                                   Janeiro/2026                                        <=
//=>---------------------------------------------------------------------------------------<=
unsigned long lastInteractionTime = 0;

// *** FILTRO TEMPORAL CONTÍNUO PARA CHOKES ***
//
// PROBLEMA: cabo de 1,20m na base do BC548 age como antena.
// Ruído elétrico induzido dura dezenas de ms → passa por qualquer
// debounce simples ou leitura múltipla rápida.
//
// SOLUÇÃO: o pino SÓ é considerado acionado se ficar LOW de forma
// CONTÍNUA por pelo menos CHOKE_HOLD_MS milissegundos seguidos.
// Qualquer HIGH no meio zera o contador → pico de ruído rejeitado.
//
// Ajuste fino:
//   CHOKE_HOLD_MS  → aumente se ainda disparar (tente 8, 12, 15, 20)
//   CHOKE_DEBOUNCE_MS → tempo mínimo entre dois acionamentos seguidos
//
#define CHOKE1_HOLD_MS     3    // Choke1 pino 51 — OK com 3ms
#define CHOKE2_HOLD_MS     3    // Choke2 pino 49 nota 82 — mais ruído, precisa mais
#define CHOKE3_HOLD_MS     3    // Choke3 pino 47
#define CHOKERIDE_HOLD_MS  3    // ChokeRide pino 53
#define CHOKE_DEBOUNCE_MS 50    // ms mínimos entre dois disparos do mesmo choke

// Quando chokeHoldAtivo==1, usa valores do menu (chokeHoldMs[]) em vez dos defines acima
#define CHOKE_HOLD_VAL(idx, def) (chokeHoldAtivo ? (unsigned long)chokeHoldMs[idx] : (unsigned long)(def))

// Timestamps do início do LOW contínuo de cada choke
unsigned long Choke1_LowSince    = 0;
unsigned long Choke2_LowSince    = 0;
unsigned long Choke3_LowSince    = 0;
unsigned long ChokeRide_LowSince = 0;

// Timestamps do último disparo de cada choke (debounce entre disparos)
unsigned long Choke1_LastTrigger    = 0;
unsigned long Choke2_LastTrigger    = 0;
unsigned long Choke3_LastTrigger    = 0;
unsigned long ChokeRide_LastTrigger = 0;

// *** BLOQUEIO DE PAD ANALÓGICO POR CHOKE DIGITAL ***
// Quando o Choke2 (pino 49, nota 82) aciona, o piezo do pad na mesma
// estrutura física capta a vibração mecânica e dispara baixinho (nota 86).
// Solução: ao acionar o Choke2, bloqueia o pad A12 por CHOKE2_PAD_BLOCK_MS.
// Ajuste CHOKE2_PAD_BLOCK_MS se a nota fantasma ainda aparecer (aumente)
// ou se o pad real demorar a responder depois do choke (diminua).
#define CHOKE2_PAD_BLOCK_MS  150  // ms de bloqueio do pad após choke acionar
#define CHOKE2_PAD_IDX        12  // índice analógico bloqueado (A12=12, nota 86)
unsigned long Choke2_PadBlockUntil = 0;

// *** DEBOUNCE TEMPORAL PADS DIGITAIS (Aux1-7) ***
#define AUX_DEBOUNCE_MS 0
unsigned long Aux1_LastTrigger = 0;
unsigned long Aux2_LastTrigger = 0;
unsigned long Aux3_LastTrigger = 0;
unsigned long Aux4_LastTrigger = 0;
unsigned long Aux5_LastTrigger = 0;
unsigned long Aux6_LastTrigger = 0;
unsigned long Aux7_LastTrigger = 0;

// *** ZONA DUPLA VIA PAD AUX (ZoneDual) ***
// Quando o switch Aux estiver fechado (LOW), o piezo associado bloqueia
// sua nota normal e envia a nota do Aux com a velocity do piezo.
#define ZONE_DUAL_PAIRS 6

// Par | Aux nota | Bloqueia | Envia | Piezo
//  1  |    53    |    51    |   53  |  A8
//  2  |    59    |    51    |   59  |  A8
//  3  |     7    |     8    |    7  |  A1
//  4  |     9    |     8    |    9  |  A1
//  5  |    27    |    49    |   27  | A10
//  6  |    31    |    57    |   31  | A11
byte zoneDual_NoteAux[ZONE_DUAL_PAIRS]   = { 53, 59,  7,  9, 27, 31 };
byte zoneDual_NoteBlock[ZONE_DUAL_PAIRS] = { 51, 51,  8,  8, 49, 57 };
byte zoneDual_PadIdx[ZONE_DUAL_PAIRS]    = {  8,  8,  1,  1, 10, 11 };
bool zoneDual_Active[ZONE_DUAL_PAIRS]    = { false, false, false, false, false, false };

#define ZONE_DUAL_LATCH_MS 80
unsigned long zoneDual_LatchTime[ZONE_DUAL_PAIRS] = { 0, 0, 0, 0, 0, 0 };

const byte zoneDual_AuxPins[7] = { Aux1_Pin, Aux2_Pin, Aux3_Pin, Aux4_Pin, Aux5_Pin, Aux6_Pin, Aux7_Pin };

// *** LEITURA SENSÍVEL DE PINO AUX ***
// AUX usa cabo curto — qualquer LOW já confirma (baqueta toca rápido).
inline int readAuxSensitive(byte pin) {
  if(digitalRead(pin) == LOW) return LOW;
  if(digitalRead(pin) == LOW) return LOW;
  if(digitalRead(pin) == LOW) return LOW;
  return HIGH;
}

// *** HELPER — atualiza flag ZoneDual para um Aux específico ***
void updateZoneDual(byte auxIdx, int switchState)
{
  for(byte p = 0; p < ZONE_DUAL_PAIRS; p++) {
    if(digitalPadNotes[auxIdx] == zoneDual_NoteAux[p]) {
      if(switchState == LOW) {
        zoneDual_Active[p] = true;
        zoneDual_LatchTime[p] = millis();
      }
      return;
    }
  }
}

// =========================================================
// MACRO READ_CHOKE — FILTRO TEMPORAL CONTÍNUO
//
// Funciona assim:
//  LOW detectado:
//    → grava timestamp na primeira vez (LowSince)
//    → aguarda CHOKE_HOLD_MS ms contínuos de LOW
//    → só dispara se tempo OK E debounce OK
//    → qualquer HIGH reinicia LowSince = 0 (rejeita ruído)
//  HIGH detectado:
//    → se estava LOW (acionado) → envia Note Off
//    → zera LowSince e muda State para HIGH
//
// Parâmetros:
//   _pin       = pino físico
//   _state     = variável de estado (int)
//   _lowsince  = unsigned long timestamp início LOW contínuo
//   _lasttrig  = unsigned long timestamp último disparo
//   _note      = nota MIDI (byte)
// =========================================================
#define READ_CHOKE(_pin, _state, _lowsince, _lasttrig, _note, _holdms) \
{ \
  unsigned long _now = millis(); \
  int _raw = digitalRead(_pin); \
  if (_raw == LOW) { \
    if (_lowsince == 0) _lowsince = _now; \
    if (_state == HIGH) { \
      if ((_now - _lowsince) >= _holdms) { \
        if ((_now - _lasttrig) >= CHOKE_DEBOUNCE_MS) { \
          sendMidiNote(0x09, _note, 127); \
          _lasttrig = _now; \
          _state = LOW; \
          resetBacklightTimer(); \
        } \
      } \
    } \
  } else { \
    if (_state == LOW) { \
      sendMidiNoteOff(0x09, _note); \
      resetBacklightTimer(); \
    } \
    _lowsince = 0; \
    _state = HIGH; \
  } \
}

//==============================
//    FASTSCAN - 16 PINOS DIRETOS
//==============================
#define fastScan(sensor,count) { byte pin=sensor; if(Pin[pin].Type!=Disabled) { Pin[pin].scan(sensor,count); if(Pin[pin].State==Scan_Time) { Pin[pin].scan(sensor,count); Pin[pin].scan(sensor,count); Pin[pin].scan(sensor,count);}}}

//==============================
//     LOOP - VERSÃO 4.0 SEM MULTIPLEXAÇÃO (16 PINOS DIRETOS A0-A15)
//==============================
void loop()
{
  // *** CRÍTICO: DEVE SER A PRIMEIRA LINHA DO LOOP ***
  processInvisibleMidiSystem();
  
  #if BUZZER
  updateBuzzer();
  #endif
  
  saveToEEPROMIfNeeded();
  
  #if ENCODER
  static unsigned long lastEncoderCheck = 0;
  if(millis() - lastEncoderCheck >= 2) {
    for(int i = 0; i < 2; i++) {
      int encoderActivity = readEncoder();
      bool encoderButtonActivity = readEncoderButton();
      if (encoderActivity != 0 || encoderButtonActivity) {
        resetBacklightTimer();
        lastInteractionTime = millis();
        break;
      }
    }
    lastEncoderCheck = millis();
  }
  #endif
  
  if (digitalRead(7) == LOW || digitalRead(6) == LOW) {
    resetBacklightTimer();
    lastInteractionTime = millis();
  }

  updateBacklight();
  
  #if USE_LCD
  if(proMicroNameEditMode) {
    ProMicroNameEditMenu();
    return;
  }
  if(proMicroConfirmationMode) {
    ProMicroConfirmationMenu();
    return;
  }
  #endif
  
  Input();
  #if USE_LCD
  checkPadShortcutTimeout();
  Menu();
  #endif
  
  updateNoteBlockControl();
  
  if(Mode==Off)
  {
    delay(100);
    return;
  }

  // *** WATCHDOG ZoneDual — reseta somente se pino HIGH *e* latch expirado ***
  {
    unsigned long now = millis();
    for(byte p = 0; p < ZONE_DUAL_PAIRS; p++) {
      if(zoneDual_Active[p]) {
        if((now - zoneDual_LatchTime[p]) >= ZONE_DUAL_LATCH_MS) {
          for(byte a = 0; a < 7; a++) {
            if(digitalPadNotes[a] == zoneDual_NoteAux[p]) {
              if(readAuxSensitive(zoneDual_AuxPins[a]) == HIGH) {
                zoneDual_Active[p] = false;
              }
              break;
            }
          }
        }
      }
    }
  }

  // ============================================================
  // CHOKES — FILTRO TEMPORAL CONTÍNUO (anti-ruído cabo longo)
  //
  // Se ainda disparar com CHOKE_HOLD_MS 8:
  //   → mude para 12, recompile e teste
  //   → se resolver mas choke real demorar: use 10
  //   → máximo recomendado: 20ms (acima disso o músico percebe atraso)
  // ============================================================
  READ_CHOKE(Choke1_Pin,    Choke1_State,    Choke1_LowSince,    Choke1_LastTrigger,    chokeNotes[1], CHOKE_HOLD_VAL(1, CHOKE1_HOLD_MS))
  // Choke2 — ao disparar, bloqueia pad A12 por CHOKE2_PAD_BLOCK_MS
  {
    unsigned long _now = millis();
    int _raw = digitalRead(Choke2_Pin);
    if (_raw == LOW) {
      if (Choke2_LowSince == 0) Choke2_LowSince = _now;
      if (Choke2_State == HIGH) {
        if ((_now - Choke2_LowSince) >= CHOKE_HOLD_VAL(2, CHOKE2_HOLD_MS)) {
          if ((_now - Choke2_LastTrigger) >= CHOKE_DEBOUNCE_MS) {
            sendMidiNote(0x09, chokeNotes[2], 127);
            Choke2_LastTrigger = _now;
            Choke2_State = LOW;
            // *** BLOQUEIA pad A12 para suprimir nota fantasma 86 ***
            Choke2_PadBlockUntil = _now + CHOKE2_PAD_BLOCK_MS;
            resetBacklightTimer();
          }
        }
      }
    } else {
      if (Choke2_State == LOW) {
        sendMidiNoteOff(0x09, chokeNotes[2]);
        resetBacklightTimer();
      }
      Choke2_LowSince = 0;
      Choke2_State = HIGH;
    }
  }
  READ_CHOKE(Choke3_Pin,    Choke3_State,    Choke3_LowSince,    Choke3_LastTrigger,    chokeNotes[3], CHOKE_HOLD_VAL(3, CHOKE3_HOLD_MS))
  READ_CHOKE(ChokeRide_Pin, ChokeRide_State, ChokeRide_LowSince, ChokeRide_LastTrigger, chokeNotes[0], CHOKE_HOLD_VAL(0, CHOKERIDE_HOLD_MS))

  // AUX 1 (Digital 1)
  currentSwitchState = readAuxSensitive(Aux1_Pin);
  updateZoneDual(0, currentSwitchState);
  if( currentSwitchState == LOW && Aux1_State == HIGH ) {
    if(millis() - Aux1_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[0]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[0], 127);
      Aux1_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux1_State == LOW ) {
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[0]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[0]);
    resetBacklightTimer();
  }
  Aux1_State = currentSwitchState;

  // AUX 2 (Digital 2)
  currentSwitchState = readAuxSensitive(Aux2_Pin);
  updateZoneDual(1, currentSwitchState);
  if( currentSwitchState == LOW && Aux2_State == HIGH ) {
    if(millis() - Aux2_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[1]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[1], 127);
      Aux2_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux2_State == LOW ) {
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[1]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[1]);
    resetBacklightTimer();
  }
  Aux2_State = currentSwitchState;

  // AUX 3 (Digital 3)
  currentSwitchState = readAuxSensitive(Aux3_Pin);
  updateZoneDual(2, currentSwitchState);
  if( currentSwitchState == LOW && Aux3_State == HIGH ) {
    if(millis() - Aux3_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[2]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[2], 127);
      Aux3_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux3_State == LOW ) {
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[2]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[2]);
    resetBacklightTimer();
  }
  Aux3_State = currentSwitchState;

  // AUX 4 (Digital 4)
  currentSwitchState = readAuxSensitive(Aux4_Pin);
  updateZoneDual(3, currentSwitchState);
  if( currentSwitchState == LOW && Aux4_State == HIGH ) {
    if(millis() - Aux4_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[3]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[3], 127);
      Aux4_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux4_State == LOW ) {
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[3]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[3]);
    resetBacklightTimer();
  }
  Aux4_State = currentSwitchState;

  // AUX 5 (Digital 5)
  currentSwitchState = readAuxSensitive(Aux5_Pin);
  updateZoneDual(4, currentSwitchState);
  if( currentSwitchState == LOW && Aux5_State == HIGH ) {
    if(millis() - Aux5_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[4]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[4], 127);
      Aux5_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux5_State == LOW ) {
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[4]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[4]);
    resetBacklightTimer();
  }
  Aux5_State = currentSwitchState;

  // AUX 6 (Digital 6)
  currentSwitchState = readAuxSensitive(Aux6_Pin);
  updateZoneDual(5, currentSwitchState);
  if( currentSwitchState == LOW && Aux6_State == HIGH ) {
    if(millis() - Aux6_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[5]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[5], 127);
      Aux6_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux6_State == LOW ) {
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[5]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[5]);
    resetBacklightTimer();
  }
  Aux6_State = currentSwitchState;

  // AUX 7 (Digital 7)
  currentSwitchState = readAuxSensitive(Aux7_Pin);
  updateZoneDual(6, currentSwitchState);
  if( currentSwitchState == LOW && Aux7_State == HIGH ) {
    if(millis() - Aux7_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[6]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[6], 127);
      Aux7_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux7_State == LOW ) {
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[6]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[6]);
    resetBacklightTimer();
  }
  Aux7_State = currentSwitchState;

  // *** LÊ OS 16 PINOS ANALÓGICOS DIRETOS (A0-A15) ***
  for(byte Sensor=0; Sensor < 16; Sensor++)
  {
    // *** BLOQUEIO PAD: se este sensor está bloqueado por choke, pula ***
    if (Sensor == CHOKE2_PAD_IDX && millis() < Choke2_PadBlockUntil) continue;

    if (Pin[Sensor].Type != Disabled)
    {
      Pin[Sensor].scan(Sensor, 0);
      if (Pin[Sensor].State == Scan_Time)
      {
        Pin[Sensor].scan(Sensor, 0);
        Pin[Sensor].scan(Sensor, 0);
        Pin[Sensor].scan(Sensor, 0);
      }
    }

    #if USE_LCD
    checkPadShortcutHit(Sensor);
    #endif

    byte dualSensorIndex = DualSensor(Sensor);
    if(dualSensorIndex < 16) {
      Pin[Sensor].play(Sensor, &Pin[dualSensorIndex]);
    } else {
      Pin[Sensor].play(Sensor, &Pin[Sensor]);
    }
  }

  //RESET XTALK
  for(int i=0; i<NXtalkGroup; i++)
    MaxXtalkGroup[i]=-1;
}

//==============================
//    LOGTOOL - 16 PINOS DIRETOS
//==============================
void LogTool(int yn_0, byte Sensor)
{
  #if MENU_LOG
  if(log_state==0)
  {
    log_T1=TIMEFUNCTION;
    log_Nmax=yn_0;
    log_state=1;
    if(Sensor < 16) {
      Pin[Sensor].Gain=64;
      Pin[Sensor].CurveForm=32;
      Pin[Sensor].Curve=Linear;
    }
    d_hsum=0;
    d_tsum=0;
    d_tnum=0;
    d_vmax=0;
    d_vmin=1024;
    N=0;
    DrawLog(0);
  }
  else if(log_state==1)
  {
    if(yn_0>log_Nmax) { log_Nmax=yn_0*1.2; DrawLog(4);}
    if((log_T1+20000)<TIMEFUNCTION)
    {
      log_state=2;
      if(d_tnum<25) DrawLog(1);
      else DrawLog(2);
    }
  }
  else if(log_state==2)
  {
    if(yn_0>log_Nmax)
    {
      DrawLog(0);
      N++;
      log_Tmax=log_T1=TIMEFUNCTION;
      log_state=3;
      log_Vmax=yn_0;
    }
  }
  else if(log_state==3)
  {
    if(yn_0>log_Nmax)
    {
      N++;
      if(yn_0>log_Vmax)
      {
        log_Vmax=yn_0;
        log_Tmax=TIMEFUNCTION;
        log_T50=0;
      }
      else
      {
        if(yn_0>(int)((float)log_Vmax*0.5))
          log_T50=TIMEFUNCTION;
      }
    }
    if((TIMEFUNCTION-log_T1) > 1000)
    {
      log_state=4;
      d_hsum+=(log_T50==0?0:(log_T50-log_Tmax));
      d_tsum+=(log_Tmax-log_T1);
      d_tnum++;
      if(log_Vmax>d_vmax) d_vmax=log_Vmax;
      if(log_Vmax<d_vmin) d_vmin=log_Vmax;
      if(d_tnum<25) DrawLog(1);
      else if(d_tnum<50) DrawLog(2);
    }
  }
  else if(log_state==4)
  {
    if(d_tnum==25)
    {
      if(Sensor < 16) {
        Pin[Sensor].Gain=(16.0/(float)d_vmin)*64.0;
        if (Pin[Sensor].Gain <16) Pin[Sensor].Gain=16;
        Pin[Sensor].Thresold=((float)(d_vmin-log_Nmax))*((float)Pin[Sensor].Gain/64.0);
        Pin[Sensor].CurveForm=((32.0*64.0)/(float)Pin[Sensor].Gain)-1;
      }
      log_T1=TIMEFUNCTION;
      log_Nmax=0;
      log_state=1;
      d_hsum=0;
      d_tsum=0;
      d_vmax=0;
      N=0;
      DrawLog(0);
    }
    else if(d_tnum==50)
    {
      if(Sensor < 16) {
        Pin[Sensor].ScanTime=((float)d_tsum/25.0);
        Pin[Sensor].MaskTime=((float)d_hsum/25.0);
        Pin[Sensor].Retrigger= (d_vmax*8.0)/((float)d_hsum/25.0);
        Pin[Sensor].CurveForm= max(Pin[Sensor].CurveForm,(1024.0/(float)d_vmax)*32);
        Pin[Sensor].Curve=Linear;
        markPinChanged(Sensor);
      }
      DrawLog(3);
      log_state=5;
    }
    else if(yn_0>log_Nmax)
    {
      log_Tmax=log_T1=TIMEFUNCTION;
      log_state=3;
      log_Vmax=yn_0;
      N=0;
      DrawLog(0);
    }
  }
  #else
    N++;
    if(yn_0>=(LogThresold*2) && Sensor < 16)
    SendLog(Sensor,N,yn_0,Pin[Sensor].useCurve(),Pin[Sensor].MaxReading,Pin[Sensor].State);
  #endif  
}
