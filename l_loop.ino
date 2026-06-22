///=========================================================================================//
//=>                         J-DRUMS v8.0.19 - CONTROLADOR MIDI BATERIA                    <= 
//=>                        Sistema de Bateria Eletrônica Arduino                          <=
//=>                     Copyright (c) 2026 Joanaldo Jhon Leonez de Melo                   <=
//=>                             Licensed under the MIT License.                           <=
//=>         See LICENSE.txt file in the project root for full license information.        <=
//=>                      DESENVOLVIDO POR JOANALDO JHON LEONEZ DE MELO                    <=
//=>                                   Janeiro/2026                                        <=
//=>---------------------------------------------------------------------------------------<=
unsigned long lastInteractionTime = 0;

// *** HOLD-MEMBRANA CHOKES (Ride/Crash1/Crash2) ***
// Segura LOW por CHOKE_HOLD_MS → dispara nota hold (63/78/80) com vel=127
// Solta → NoteOff da nota hold. Toque rápido → nota normal do choke.
// *** CHOKE_HOLD_MS: tempo de hold para choke (ms) — ajustável via menu (padrão 8ms, faixa 5-30ms) ***
// Acesso: segure encoder 20s com pad ativo → R>ChkHoldMs (4º item do menu timing)
// Endereço EEPROM: 689
uint16_t CHOKE_HOLD_MS = 8;
unsigned long chokeRide_HoldStart  = 0;
unsigned long choke1_HoldStart     = 0;
unsigned long choke3_HoldStart     = 0;
bool          chokeRide_HoldFired  = false;
bool          choke1_HoldFired     = false;
bool          choke3_HoldFired     = false;
// *** HOLD-MEMBRANA AUX2/5/6 (ZoneDual Ride/Crash1/Crash2) ***
// Aux2(P43)→chokeNotes[3]=63  Aux5(P37)→chokeNotes[0]=78  Aux6(P35)→chokeNotes[1]=80
unsigned long aux2_HoldStart = 0;
unsigned long aux5_HoldStart = 0;
unsigned long aux6_HoldStart = 0;
bool          aux2_HoldFired = false;
bool          aux5_HoldFired = false;
bool          aux6_HoldFired = false;
// chokeMode e triZoneMode declarados em h_menu.ino — visíveis no mesmo projeto Arduino
// chokeMode: 0=Simples(só chokes) 1=TriZone(só holds Aux) 2=Sim+Triz(ambos) 3=Desativado
// triZoneMode: 0=Desativado 1=Ativado
// *** DEBOUNCE TEMPORAL PADS DIGITAIS (Aux1-7) ***
#define AUX_DEBOUNCE_MS 0  // Tempo mínimo entre disparos (ms) - ajustável
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
//
// Par 1: Aux configurado com nota 53 (cúpula Ride)
//   → bloqueia nota 51 (Ride condução, piezo A8)
//   → envia nota 53 com velocity do piezo A8
//
// Par 2: Aux configurado com nota 59 (Ride campana extra)
//   → bloqueia nota 51 (Ride condução, piezo A8)
//   → envia nota 59 com velocity do piezo A8
//
// Par 3: Aux configurado com nota 7 (zona extra Chimbal)
//   → bloqueia nota 8 (Chimbal, piezo A1)
//   → envia nota 7 com velocity do piezo A1
//
// Par 4: Aux configurado com nota 9 (cúpula Chimbal)
//   → bloqueia nota 8 (Chimbal, piezo A1)
//   → envia nota 9 com velocity do piezo A1
//
// zoneDual_NoteAux   = nota configurada no Aux que ativa o redirecionamento
// zoneDual_NoteBlock = nota do piezo que será bloqueada quando Aux fechado
// zoneDual_PadIdx    = índice do piezo (0-15) cujo play será redirecionado
// zoneDual_Active    = flag: true enquanto o switch Aux estiver LOW

#define ZONE_DUAL_PAIRS 6

// Par | Aux nota | Bloqueia | Envia | Piezo | Hold (segura)
//  1  |    61    |    60    |   61  |  A8 (Ride cond → cup)
//  2  |    62    |    60    |   62  |  A8 (Ride cond → campana)  segura→63
//  3  |     7    |     8    |    7  |  A1 (Chimbal zona extra)
//  4  |     9    |     8    |    9  |  A1 (Chimbal cúpula)
//  5  |    27    |    77    |   27  | A10 (Crash 1 zona)          segura→78
//  6  |    31    |    79    |   31  | A11 (Crash 2 zona)          segura→80
byte zoneDual_NoteAux[ZONE_DUAL_PAIRS]   = { 61, 62,  7,  9, 27, 31 }; // nota Aux que ativa (Addictive)
byte zoneDual_NoteBlock[ZONE_DUAL_PAIRS] = { 60, 60,  8,  8, 77, 79 }; // nota piezo bloqueada (Addictive)
byte zoneDual_PadIdx[ZONE_DUAL_PAIRS]    = {  8,  8,  1,  1, 10, 11 }; // índice piezo (A0=0..A15=15)
bool       zoneDual_Active[ZONE_DUAL_PAIRS]    = { false, false, false, false, false, false };

// *** HOLD-CHOKE ZoneDual ***
// Membrana com borda + choke na mesma zona (pares 1, 4, 5):
//   Par 1 (Aux2/pino43): toque→62  segurar→63  (choke Ride condução)
//   Par 4 (Aux5/pino37): toque→27  segurar→78  (choke Crash 1)
//   Par 5 (Aux6/pino35): toque→31  segurar→80  (choke Crash 2)
// Pares sem hold-choke usam 0 (desativado).
// ZONE_DUAL_HOLD_MS usa a mesma variável CHOKE_HOLD_MS (ajustável no menu)
#define ZONE_DUAL_HOLD_MS CHOKE_HOLD_MS
//                                          p0  p1  p2  p3  p4  p5
// *** HoldNote NÃO é const — o menu pode atualizar quando a nota ZoneDual muda ***
// Par 1 (Aux2/P43 Crash Ride):  segura→63, bate→62 normal
// Par 4 (Aux5/P37 Crash 1):     segura→78, bate→27 normal
// Par 5 (Aux6/P35 Crash 2):     segura→80, bate→31 normal
byte   zoneDual_HoldNote[ZONE_DUAL_PAIRS] = {  0, 63,  0,  0, 78, 80 };
unsigned long zoneDual_HoldStart[ZONE_DUAL_PAIRS] = { 0,  0,  0,  0,  0,  0 };
bool         zoneDual_HoldFired[ZONE_DUAL_PAIRS]  = { false,false,false,false,false,false };
bool         zoneDual_EdgeFired[ZONE_DUAL_PAIRS]  = { false,false,false,false,false,false };

// *** LATCH TEMPORAL ZoneDual ***
// Quando o pino fecha (LOW), grava o timestamp.
// zoneDual_Active permanece true por ZONE_DUAL_LATCH_MS mesmo após o pino abrir.
// Isso garante que batidas de baqueta (contato brevíssimo ~3-10ms) sejam capturadas,
// pois o piezo pode chegar em Piezo_Time só 10-30ms depois do contato.
// Valor 80ms: cobre até os pianos mais lentos sem atrasar o reset entre batidas.
#define ZONE_DUAL_LATCH_MS 80
unsigned long zoneDual_LatchTime[ZONE_DUAL_PAIRS] = { 0, 0, 0, 0, 0, 0 };

// *** WATCHDOG ZoneDual — mapeamento Aux→pino para reset imediato ***
// Agora reseta somente se o pino estiver HIGH *e* o latch já tiver expirado.
const byte zoneDual_AuxPins[7] = { Aux1_Pin, Aux2_Pin, Aux3_Pin, Aux4_Pin, Aux5_Pin, Aux6_Pin, Aux7_Pin };

// *** LEITURA SENSÍVEL DE PINO AUX ***
// Faz 3 leituras consecutivas — se qualquer uma retornar LOW considera fechado.
// Pega contatos leves e rápidos que uma leitura única poderia perder.
// Custo: < 3µs por chamada — invisível pro desempenho dos pads analógicos.
inline int readAuxSensitive(byte pin) {
  if(digitalRead(pin) == LOW) return LOW;
  if(digitalRead(pin) == LOW) return LOW;
  if(digitalRead(pin) == LOW) return LOW;
  return HIGH;
}

// *** HELPER — atualiza flag ZoneDual para um Aux específico ***
// Chamado a cada leitura do switch Aux.
// Se a nota configurada no Aux (digitalPadNotes[auxIdx]) bate com
// zoneDual_NoteAux[pair], atualiza zoneDual_Active[pair] com o estado do switch.
// *** MAPEAMENTO DIRETO Aux→Par ZoneDual para hold-choke ***
// Independente da nota configurada no menu — baseado no pino físico.
// 255 = Aux sem hold-choke.
//       Aux0 Aux1 Aux2 Aux3 Aux4 Aux5 Aux6
const byte zoneDual_AuxHoldPair[7] = { 255,  1, 255, 255,  4,  5, 255 };

void updateZoneDual(byte auxIdx, int switchState)
{
  // *** HOLD-CHOKE DIRETO POR PINO — não depende do match de nota ***
  byte hp = zoneDual_AuxHoldPair[auxIdx];
  if(hp != 255 && zoneDual_HoldNote[hp] != 0) {
    if(switchState == LOW) {
      if(!zoneDual_HoldFired[hp]) {
        zoneDual_HoldStart[hp] = millis();
      }
    } else {
      // Soltou: envia NoteOff se choke disparou
      if(zoneDual_HoldFired[hp]) {
        sendMidiNoteOff(0x09, zoneDual_HoldNote[hp]);
        // mantém HoldFired=true até watchdog limpar (evita borda espúria)
      } else {
        zoneDual_HoldFired[hp] = false;
        zoneDual_EdgeFired[hp] = false;
      }
    }
  }

  // *** ZONA DUPLA NORMAL — match por nota (redireciona piezo) ***
  for(byte p = 0; p < ZONE_DUAL_PAIRS; p++) {
    if(digitalPadNotes[auxIdx] == zoneDual_NoteAux[p]) {
      if(switchState == LOW) {
        // Borda de descida: reseta edge se não disparou choke ainda
        if(!zoneDual_HoldFired[p]) {
          zoneDual_EdgeFired[p] = false;
        }
        // Fecha: ativa e renova o latch
        zoneDual_Active[p] = true;
        zoneDual_LatchTime[p] = millis();
      } else {
        if(!zoneDual_HoldFired[p]) {
          zoneDual_EdgeFired[p] = false;
        }
        // Se HIGH: não reseta Active aqui — o watchdog abaixo reseta após o latch expirar
      }
      return;
    }
  }
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
  // Processa o sistema MIDI automático (Note OFF + RX1)
  processInvisibleMidiSystem();
  
  // *** ATUALIZA BUZZER ***
  #if BUZZER
  updateBuzzer();
  #endif
  
  // *** VERIFICA E SALVA CACHE A CADA 60 SEGUNDOS ***
  saveToEEPROMIfNeeded();
  
  // *** DETECÇÃO DE ATIVIDADE COM ENCODER OTIMIZADA ***
  #if ENCODER
  // Detecta atividade no encoder - MÚLTIPLAS LEITURAS
  static unsigned long lastEncoderCheck = 0;
  if(millis() - lastEncoderCheck >= 2) { // Verifica a cada 2ms
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
  
  // Detecta atividade nos botões e reativa a luz de fundo
  if (digitalRead(7) == LOW || digitalRead(6) == LOW) {
    resetBacklightTimer();
    lastInteractionTime = millis();
  }

  // Controle integrado da luz de fundo baseado no menu
  updateBacklight();
  
  // *** MENUS ESPECIAIS PRO MICRO - DEVEM VIR ANTES DO Menu() PRINCIPAL ***
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
  checkPadShortcutTimeout();  // *** ATALHO PAD: cancela se passar 5s sem bater ***
  Menu();
  #endif
  
  // Adiciona a chamada para manter o controle do bloqueio das notas 37/38
  updateNoteBlockControl();
  
  if(Mode==Off)
  {
    delay(100);
    return;
  }

  // *** WATCHDOG ZoneDual — reseta somente se pino HIGH *e* latch expirado ***
  // O latch garante que batidas rápidas de baqueta (contato ~3-10ms) sejam
  // capturadas mesmo quando o piezo ainda não chegou em Piezo_Time.
  {
    unsigned long now = millis();
    for(byte p = 0; p < ZONE_DUAL_PAIRS; p++) {
      if(zoneDual_Active[p]) {
        // Verifica se o latch já expirou
        if((now - zoneDual_LatchTime[p]) >= ZONE_DUAL_LATCH_MS) {
          // Latch expirado: só mantém se o pino ainda estiver LOW
          for(byte a = 0; a < 7; a++) {
            if(digitalPadNotes[a] == zoneDual_NoteAux[p]) {
              if(readAuxSensitive(zoneDual_AuxPins[a]) == HIGH) {
                zoneDual_Active[p]    = false;
                // *** FIX CHOKE-RELEASE: agora sim libera flags ***
                // Latch expirou + pino HIGH → ciclo completo encerrado
                zoneDual_HoldFired[p] = false;
                zoneDual_EdgeFired[p] = false;
                zoneDual_HoldStart[p] = 0;  // reseta contagem hold
              }
              break;
            }
          }
        }
        // Latch ainda vigente: mantém Active = true independente do pino
      }
    }
  }

  // *** POLLING HOLD-CHOKE ZoneDual ***
  // Verifica diretamente o estado do pino físico — não depende de zoneDual_Active
  // para pares com hold-choke (Aux2→par1, Aux5→par4, Aux6→par5).
  // *** CORREÇÃO: só roda em triZoneMode==1 E chokeMode==1(TrZn) ou 2(S+Tz) ***
  // (mesma condição "holdOk" usada nos blocos Aux2/Aux5/Aux6 abaixo)
  if(triZoneMode == 1 && (chokeMode == 1 || chokeMode == 2)) {
    unsigned long now = millis();
    for(byte a = 0; a < 7; a++) {
      byte p = zoneDual_AuxHoldPair[a];
      if(p == 255) continue;
      if(!zoneDual_HoldNote[p]) continue;
      if(zoneDual_HoldFired[p]) continue;
      // Pino ainda LOW?
      if(readAuxSensitive(zoneDual_AuxPins[a]) != LOW) continue;
      if(zoneDual_HoldStart[p] == 0) continue;
      if((now - zoneDual_HoldStart[p]) >= ZONE_DUAL_HOLD_MS) {
        sendMidiNote(0x09, zoneDual_HoldNote[p], 127);
        zoneDual_HoldFired[p] = true;
      }
    }
  }

  // *** CHOKES SIMPLES — só executam se chokeMode==0(Simples) ou ==2(Sim+Triz) ***
  // CHOKE PRATO 01 (Crash 2 - Choke1_Pin 51)
  currentSwitchState = digitalRead(Choke1_Pin);
  if(chokeMode == 0 || chokeMode == 2) {
    if( currentSwitchState == HIGH ) {
      if(choke1_HoldFired) { sendMidiNoteOff(0x09, chokeNotes[1]); }
      choke1_HoldFired = false; choke1_HoldStart = 0;
      if(Choke1_State == LOW) resetBacklightTimer();
    } else {
      if(choke1_HoldStart == 0) { choke1_HoldStart = millis(); choke1_HoldFired = false; resetBacklightTimer(); }
      if(!choke1_HoldFired && (millis() - choke1_HoldStart) >= CHOKE_HOLD_MS) {
        sendMidiNote(0x09, chokeNotes[1], 127); choke1_HoldFired = true;
      }
    }
  } else {
    // Modo desabilitado: garante reset das flags se pino soltar
    if(currentSwitchState == HIGH) { choke1_HoldFired = false; choke1_HoldStart = 0; }
  }
  Choke1_State = currentSwitchState;

  // CHOKE PRATO 02 (Crash 3 - Choke2_Pin 49)
  currentSwitchState = digitalRead(Choke2_Pin);
  if(chokeMode == 0 || chokeMode == 2) {
    if( currentSwitchState == LOW && Choke2_State == HIGH ) {
      sendMidiNote(0x09, chokeNotes[2], 127); resetBacklightTimer();
    }
    if( currentSwitchState == HIGH && Choke2_State == LOW ) {
      sendMidiNoteOff(0x09, chokeNotes[2]); resetBacklightTimer();
    }
  }
  Choke2_State = currentSwitchState;

  // CHOKE PRATO 03 (Ride - Choke3_Pin 47)
  currentSwitchState = digitalRead(Choke3_Pin);
  if(chokeMode == 0 || chokeMode == 2) {
    if( currentSwitchState == HIGH ) {
      if(choke3_HoldFired) { sendMidiNoteOff(0x09, chokeNotes[3]); }
      choke3_HoldFired = false; choke3_HoldStart = 0;
      if(Choke3_State == LOW) resetBacklightTimer();
    } else {
      if(choke3_HoldStart == 0) { choke3_HoldStart = millis(); choke3_HoldFired = false; resetBacklightTimer(); }
      if(!choke3_HoldFired && (millis() - choke3_HoldStart) >= CHOKE_HOLD_MS) {
        sendMidiNote(0x09, chokeNotes[3], 127); choke3_HoldFired = true;
      }
    }
  } else {
    if(currentSwitchState == HIGH) { choke3_HoldFired = false; choke3_HoldStart = 0; }
  }
  Choke3_State = currentSwitchState;

  // CHOKE RIDE (Crash 1 - ChokeRide_Pin 53)
  currentSwitchState = digitalRead(ChokeRide_Pin);
  if(chokeMode == 0 || chokeMode == 2) {
    if( currentSwitchState == HIGH ) {
      if(chokeRide_HoldFired) { sendMidiNoteOff(0x09, chokeNotes[0]); }
      chokeRide_HoldFired = false; chokeRide_HoldStart = 0;
      if(ChokeRide_State == LOW) resetBacklightTimer();
    } else {
      if(chokeRide_HoldStart == 0) { chokeRide_HoldStart = millis(); chokeRide_HoldFired = false; resetBacklightTimer(); }
      if(!chokeRide_HoldFired && (millis() - chokeRide_HoldStart) >= CHOKE_HOLD_MS) {
        sendMidiNote(0x09, chokeNotes[0], 127); chokeRide_HoldFired = true;
      }
    }
  } else {
    if(currentSwitchState == HIGH) { chokeRide_HoldFired = false; chokeRide_HoldStart = 0; }
  }
  ChokeRide_State = currentSwitchState;
  
  // AUX 1 (Digital 1)
  currentSwitchState = readAuxSensitive(Aux1_Pin);
  updateZoneDual(0, currentSwitchState);
  if( currentSwitchState == LOW && Aux1_State == HIGH ) { // push
    if(millis() - Aux1_LastTrigger > AUX_DEBOUNCE_MS) {
      // Só dispara nota própria se NÃO for um par ZoneDual
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[0]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[0], 127);
      Aux1_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux1_State == LOW ) { // release
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[0]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[0]);
    resetBacklightTimer();
  }
  Aux1_State = currentSwitchState;

  // AUX 2 (Digital 2) — P43 Ride membrana: segura→chokeNotes[3]=63
  // Hold ativo se triZoneMode==1 E (chokeMode==1 ou chokeMode==2)
  currentSwitchState = readAuxSensitive(Aux2_Pin);
  updateZoneDual(1, currentSwitchState);
  {
    bool holdOk = (triZoneMode == 1) && (chokeMode == 1 || chokeMode == 2);
    if( currentSwitchState == HIGH ) {
      if(aux2_HoldFired) { sendMidiNoteOff(0x09, chokeNotes[3]); }
      aux2_HoldFired = false; aux2_HoldStart = 0;
    } else if(holdOk) {
      if(aux2_HoldStart == 0) { aux2_HoldStart = millis(); aux2_HoldFired = false; }
      if(!aux2_HoldFired && (millis() - aux2_HoldStart) >= CHOKE_HOLD_MS) {
        sendMidiNote(0x09, chokeNotes[3], 127); aux2_HoldFired = true;
      }
    } else {
      aux2_HoldFired = false; aux2_HoldStart = 0;
    }
  }
  if( currentSwitchState == LOW && Aux2_State == HIGH ) {
    if(millis() - Aux2_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[1]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[1], 127);
      Aux2_LastTrigger = millis(); resetBacklightTimer();
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
  if( currentSwitchState == LOW && Aux3_State == HIGH ) { // push
    if(millis() - Aux3_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[2]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[2], 127);
      Aux3_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux3_State == LOW ) { // release
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[2]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[2]);
    resetBacklightTimer();
  }
  Aux3_State = currentSwitchState;

  // AUX 4 (Digital 4)
  currentSwitchState = readAuxSensitive(Aux4_Pin);
  updateZoneDual(3, currentSwitchState);
  if( currentSwitchState == LOW && Aux4_State == HIGH ) { // push
    if(millis() - Aux4_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[3]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[3], 127);
      Aux4_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux4_State == LOW ) { // release
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[3]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[3]);
    resetBacklightTimer();
  }
  Aux4_State = currentSwitchState;

  // AUX 5 (Digital 5) — P37 Crash1 membrana: segura→chokeNotes[0]=78
  currentSwitchState = readAuxSensitive(Aux5_Pin);
  updateZoneDual(4, currentSwitchState);
  {
    bool holdOk = (triZoneMode == 1) && (chokeMode == 1 || chokeMode == 2);
    if( currentSwitchState == HIGH ) {
      if(aux5_HoldFired) { sendMidiNoteOff(0x09, chokeNotes[0]); }
      aux5_HoldFired = false; aux5_HoldStart = 0;
    } else if(holdOk) {
      if(aux5_HoldStart == 0) { aux5_HoldStart = millis(); aux5_HoldFired = false; }
      if(!aux5_HoldFired && (millis() - aux5_HoldStart) >= CHOKE_HOLD_MS) {
        sendMidiNote(0x09, chokeNotes[0], 127); aux5_HoldFired = true;
      }
    } else {
      aux5_HoldFired = false; aux5_HoldStart = 0;
    }
  }
  if( currentSwitchState == LOW && Aux5_State == HIGH ) {
    if(millis() - Aux5_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[4]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[4], 127);
      Aux5_LastTrigger = millis(); resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux5_State == LOW ) {
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[4]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[4]);
    resetBacklightTimer();
  }
  Aux5_State = currentSwitchState;

  // AUX 6 (Digital 6) — P35 Crash2 membrana: segura→chokeNotes[1]=80
  currentSwitchState = readAuxSensitive(Aux6_Pin);
  updateZoneDual(5, currentSwitchState);
  {
    bool holdOk = (triZoneMode == 1) && (chokeMode == 1 || chokeMode == 2);
    if( currentSwitchState == HIGH ) {
      if(aux6_HoldFired) { sendMidiNoteOff(0x09, chokeNotes[1]); }
      aux6_HoldFired = false; aux6_HoldStart = 0;
    } else if(holdOk) {
      if(aux6_HoldStart == 0) { aux6_HoldStart = millis(); aux6_HoldFired = false; }
      if(!aux6_HoldFired && (millis() - aux6_HoldStart) >= CHOKE_HOLD_MS) {
        sendMidiNote(0x09, chokeNotes[1], 127); aux6_HoldFired = true;
      }
    } else {
      aux6_HoldFired = false; aux6_HoldStart = 0;
    }
  }
  if( currentSwitchState == LOW && Aux6_State == HIGH ) {
    if(millis() - Aux6_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[5]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[5], 127);
      Aux6_LastTrigger = millis(); resetBacklightTimer();
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
  if( currentSwitchState == LOW && Aux7_State == HIGH ) { // push
    if(millis() - Aux7_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[6]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[6], 127);
      Aux7_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux7_State == LOW ) { // release
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[6]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[6]);
    resetBacklightTimer();
  }
  Aux7_State = currentSwitchState;

  // *** LÊ OS 16 PINOS ANALÓGICOS DIRETOS (A0-A15) ***
  for(byte Sensor=0; Sensor < 16; Sensor++)
  {
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

    // *** ATALHO AJUSTE DE PAD: detecta batida forte antes do play ***
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

  // *** A8/A9 STRONGEST: processa hits pendentes (ativo quando TriZone=Des) ***
  processA8A9Strongest();
}

//==============================
//    LOGTOOL - 16 PINOS DIRETOS
//==============================
void LogTool(int yn_0, byte Sensor)
{
  #if MENU_LOG
  //FASE 1: autodeterminazione del rumore massimo Nmax in un intervallo fisso di 20s
  if(log_state==0)//FASE 1.a: AVVIO
  {
    log_T1=TIMEFUNCTION;
    log_Nmax=yn_0;
    log_state=1;

    //V2
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
      
    //WAIT
    DrawLog(0);

  }
  else if(log_state==1)//FASE 1.b: ANALISI
  {
    if(yn_0>log_Nmax) { log_Nmax=yn_0*1.2; DrawLog(4);}
    if((log_T1+20000)<TIMEFUNCTION) //20sec
    {
      log_state=2;
      if(d_tnum<25) DrawLog(1);
      else DrawLog(2);
    }
  }
  else if(log_state==2) //FASE 1.c: FINE
  {
    if(yn_0>log_Nmax)
    {
      DrawLog(0);//Wait
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
      
    //1 sec
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
      //V2
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
      DrawLog(0);//WAIT
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
      log_state=5; //END
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