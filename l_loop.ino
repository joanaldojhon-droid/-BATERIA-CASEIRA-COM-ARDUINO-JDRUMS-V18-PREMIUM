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

// Par | Aux nota | Bloqueia | Envia | Piezo
//  1  |    53    |    51    |   53  |  A8 (mesmo piezo do par 2 — par menor vence se ambos fechados)
//  2  |    59    |    51    |   59  |  A8
//  3  |     7    |     8    |    7  |  A1 (mesmo piezo do par 4 — par menor vence se ambos fechados)
//  4  |     9    |     8    |    9  |  A1
//  5  |    27    |    49    |   27  | A10
//  6  |    31    |    57    |   31  | A11
byte zoneDual_NoteAux[ZONE_DUAL_PAIRS]   = { 53, 59,  7,  9, 27, 31 }; // nota Aux que ativa
byte zoneDual_NoteBlock[ZONE_DUAL_PAIRS] = { 51, 51,  8,  8, 49, 57 }; // nota piezo bloqueada
byte zoneDual_PadIdx[ZONE_DUAL_PAIRS]    = {  8,  8,  1,  1, 10, 11 }; // índice piezo (A0=0..A15=15)
bool       zoneDual_Active[ZONE_DUAL_PAIRS]    = { false, false, false, false, false, false };

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
void updateZoneDual(byte auxIdx, int switchState)
{
  for(byte p = 0; p < ZONE_DUAL_PAIRS; p++) {
    if(digitalPadNotes[auxIdx] == zoneDual_NoteAux[p]) {
      if(switchState == LOW) {
        // Fecha: ativa e renova o latch
        zoneDual_Active[p] = true;
        zoneDual_LatchTime[p] = millis();
      }
      // Se HIGH: não reseta aqui — o watchdog abaixo reseta após o latch expirar
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
                zoneDual_Active[p] = false;
              }
              break;
            }
          }
        }
        // Latch ainda vigente: mantém Active = true independente do pino
      }
    }
  }

  // Lê os pinos digitais (Arduino Mega - TODOS RESTAURADOS)
  
  // CHOKE PRATO 01 (Crash 2 - Choke1_Pin 51)
  currentSwitchState = digitalRead(Choke1_Pin);
  if( currentSwitchState == LOW && Choke1_State == HIGH ) // push
  MIDI_TX(0x90 | 144, chokeNotes[1], 127);
  if( currentSwitchState == HIGH && Choke1_State == LOW ) // release
  MIDI_TX(0x90 | 128, chokeNotes[1], 127);
  Choke1_State = currentSwitchState;
  
  // CHOKE PRATO 02 (Crash 3 - Choke2_Pin 49)
  currentSwitchState = digitalRead(Choke2_Pin);
  if( currentSwitchState == LOW && Choke2_State == HIGH ) // push
  MIDI_TX(0x90 | 144, chokeNotes[2], 127);
  if( currentSwitchState == HIGH && Choke2_State == LOW ) // release
  MIDI_TX(0x90 | 128, chokeNotes[2], 127);
  Choke2_State = currentSwitchState;
  
  // CHOKE PRATO 03 (Ride - Choke3_Pin 47)
  currentSwitchState = digitalRead(Choke3_Pin);
  if( currentSwitchState == LOW && Choke3_State == HIGH ) // push
  MIDI_TX(0x90 | 144, chokeNotes[3], 127);
  if( currentSwitchState == HIGH && Choke3_State == LOW ) // release
  MIDI_TX(0x90 | 128, chokeNotes[3], 127);
  Choke3_State = currentSwitchState;
  
  // CHOKE RIDE (Crash 1 - ChokeRide_Pin 53)
  currentSwitchState = digitalRead(ChokeRide_Pin);
  if( currentSwitchState == LOW && ChokeRide_State == HIGH ) // push
  MIDI_TX(0x90 | 144, chokeNotes[0], 127);
  if( currentSwitchState == HIGH && ChokeRide_State == LOW ) // release
  MIDI_TX(0x90 | 128, chokeNotes[0], 127);
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

  // AUX 2 (Digital 2)
  currentSwitchState = readAuxSensitive(Aux2_Pin);
  updateZoneDual(1, currentSwitchState);
  if( currentSwitchState == LOW && Aux2_State == HIGH ) { // push
    if(millis() - Aux2_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[1]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[1], 127);
      Aux2_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux2_State == LOW ) { // release
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

  // AUX 5 (Digital 5)
  currentSwitchState = readAuxSensitive(Aux5_Pin);
  updateZoneDual(4, currentSwitchState);
  if( currentSwitchState == LOW && Aux5_State == HIGH ) { // push
    if(millis() - Aux5_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[4]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[4], 127);
      Aux5_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux5_State == LOW ) { // release
    bool isZone = false;
    for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[4]==zoneDual_NoteAux[p]) { isZone=true; break; }
    if(!isZone) sendMidiNoteOff(0x09, digitalPadNotes[4]);
    resetBacklightTimer();
  }
  Aux5_State = currentSwitchState;

  // AUX 6 (Digital 6)
  currentSwitchState = readAuxSensitive(Aux6_Pin);
  updateZoneDual(5, currentSwitchState);
  if( currentSwitchState == LOW && Aux6_State == HIGH ) { // push
    if(millis() - Aux6_LastTrigger > AUX_DEBOUNCE_MS) {
      bool isZone = false;
      for(byte p=0;p<ZONE_DUAL_PAIRS;p++) if(digitalPadNotes[5]==zoneDual_NoteAux[p]) { isZone=true; break; }
      if(!isZone) sendMidiNote(0x09, digitalPadNotes[5], 127);
      Aux6_LastTrigger = millis();
      resetBacklightTimer();
    }
  }
  if( currentSwitchState == HIGH && Aux6_State == LOW ) { // release
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