///=========================================================================================//
//=>                         J-DRUMS v5.0 - CONTROLADOR MIDI BATERIA                       <= 
//=>                        Sistema de Bateria Eletrônica Arduino                          <=
//=>                     Copyright (c) 2026 Joanaldo Jhon Leonez de Melo                   <=
//=>                             Licensed under the MIT License.                           <=
//=>         See LICENSE.txt file in the project root for full license information.        <=
//=>                      DESENVOLVIDO POR JOANALDO JHON LEONEZ DE MELO                    <=
//=>                                   Janeiro/2026                                        <=
//=>                                                                                       <=
//=>  CARACTERÍSTICAS PRINCIPAIS:                                                          <=
//=>  • 16 pads principais (A0-A15) com entrada analógica                                  <=
//=>  • 4 ou 6 pads expansíveis via Pro Micro Atmega34u2 ou Leonardo R3 (comunicação SysEx)<=
//=>  • Sistema inteligente de rimshot (37+38→40)                                          <=
//=>  • Anti-crosstalk avançado entre pads                                                 <=
//=>  • Menu completo em LCD 16x2 I2C                                                      <=
//=>  • Navegação via encoder rotativo + 2 botões                                          <=
//=>  • Buzzer de feedback sonoro                                                          <=
//=>  • Nomes personalizados para pads (25 nomes pré-definidos )                           <=
//=>  • Sistema de cache EEPROM (salva a cada 60s)                                         <=
//=>  • 3 slots de backup/presets completos                                                <=
//=>  • 4 presets VST (Persona, EZDrummer, Superior, Addictive)                            <=
//=>  • Sistema MIDI Note Off automático para sustain                                      <=
//=>  • Saídas MIDI: USB + TX1 (habilitáveis independentemente)                            <=
//=>  • Backlight inteligente (30s timeout ou sempre aceso)                                <=
//=>  • Choke de pratos via switches digitais                                              <=
//=>  • Curvas de velocidade: Linear, Exponencial, Log, Sigmoidal, Flat                    <=
//=>  • Dual pad support (notas 101→102, 103→104)                                          <=
//=>  • Configurações avançadas de scan time, mask time, retrigger                         <=
//=>  • Suporte a HiHat controller e pedal                                                 <=
//=>                                                                                       <=
//=>  COLABORADORES NO DESENVOLVIMENTO E TESTES:                                           <=
//=>  • Silvio - Testes de hardware e MIDI                                                 <=
//=>  • Lucas - Testes de sistema Note Off                                                 <=
//=>                                                                                       <=
//=>  HARDWARE:                                                                            <=
//=>  • Arduino Mega 2560                                                                  <=
//=>  • LCD I2C 16x2 (endereço 0x27)                                                       <=
//=>  • Encoder rotativo (pinos 9, 10, 11)                                                 <=
//=>  • Botões A e B (pinos 6, 7) Inverte no menu                                          <=
//=>  • Buzzer (pino 8)                                                                    <=
//=>  • 16 entradas analógicas para pads (A0-A15)                                          <=
//=>  • 4 switches de choke (pinos 47, 49, 51, 53)                                         <=
//=>  • Pro Micro 4 Pdas e Leonaro 6 Pads via comunicação serial SysEx Seleciona no menu   <=
//=========================================================================================//


//========CONFIGURE=============
#define USE_LCD           1     
#define FASTADC           1     
#define SERIALSPEED       0     
#define USE_DEFAULT_NAME  2     
#define ENABLE_CHANNEL    1     
#define MENU_LOG          1     
#define MEGA              1     
#define ENCODER           1     
#define BUZZER            1     

//========ENCODER E BUZZER - CONFIGURAÇÃO=============
#if ENCODER
  #define ENCODER_PIN_A    9    
  #define ENCODER_PIN_B    10   
  #define ENCODER_BTN      11   
  #define RESETDELAY       3000 
  
  byte encoder0PinALast = LOW;
  byte encoder0Pos = 0;
  bool encoderButtonPressed = false;
  unsigned long encoderButtonTime = 0;
  byte encoderButtonState = 0;
#endif

#if BUZZER
  #define BUZZER_PIN       8    
  #define BEEP_DURATION    50   
  #define BEEP_FREQUENCY   2000 
  
  bool buzzerEnabled = true;
  unsigned long buzzerOffTime = 0;
  bool buzzerActive = false;
#endif

//========FUNÇÕES ATIVAR/DESATIVAR==========
byte ENABLE_NOTE_37_38_TO_40 = 1;
byte ENABLE_VELOCITY_FILTER = 1;
byte ENABLE_RIMSHOT_38_TO_40 = 0;
byte ENABLE_NOTE_101_TO_102 = 0;
byte ENABLE_NOTE_103_TO_104 = 0;

// *** NOVO: Notas personalizadas para rimshot ***
byte CUSTOM_NOTE_37 = 37;           // Nota ARO (padrão 37)
byte CUSTOM_NOTE_40 = 40;           // Nota resultado 37+38 (padrão 40)
byte CUSTOM_RIMSHOT_FORCE_NOTE = 40; // Nota rimshot forçado vel>125 (padrão 40)

//========NOVAS VARIÁVEIS PARA SISTEMA DE BACKUP 3 PRESETS==============
#define BACKUP_EEPROM_START_1 1000
#define BACKUP_EEPROM_START_2 2200
#define BACKUP_EEPROM_START_3 3400
#define BACKUP_SIZE 1200

byte eMenuBackup = 0;

//========VARIÁVEIS GLOBAIS PARA MENU E NOMES====================
byte backlightMode = 0;
unsigned long backlightTimer = 0;
const unsigned long BACKLIGHT_TIMEOUT = 30000;
bool backlightState = true;

#define NUM_AVAILABLE_NAMES 25
#define MAX_PINS 16

const char name_0[] PROGMEM = "CON CHIMBAL";
const char name_1[] PROGMEM = "CHIMBAL    ";
const char name_2[] PROGMEM = "CAIXA      ";
const char name_3[] PROGMEM = "ARO CAIXA  ";
const char name_4[] PROGMEM = "BUMBO      ";
const char name_5[] PROGMEM = "TOM 2      ";
const char name_6[] PROGMEM = "TOM 3      ";
const char name_7[] PROGMEM = "SURDO 1    ";
const char name_8[] PROGMEM = "RIDE COND  ";
const char name_9[] PROGMEM = "RIDE CUPULA";
const char name_10[] PROGMEM = "CRASH 1   ";
const char name_11[] PROGMEM = "CRASH 2    ";
const char name_12[] PROGMEM = "CRASH 3    ";
const char name_13[] PROGMEM = "SURDO 2    ";
const char name_14[] PROGMEM = "TOM 1      ";
const char name_15[] PROGMEM = "PAD EFET 1 ";
const char name_16[] PROGMEM = "CRASH 4    ";
const char name_17[] PROGMEM = "CHINA      ";
const char name_18[] PROGMEM = "SPLASH     ";
const char name_19[] PROGMEM = "ROTONTOM 1 ";
const char name_20[] PROGMEM = "ROTONTOM 2 ";
const char name_21[] PROGMEM = "GOWBELL    ";
const char name_22[] PROGMEM = "PANDEROLA  ";
const char name_23[] PROGMEM = "CAIXA REVB ";
const char name_24[] PROGMEM = "PAD EFET 2 ";

const char* const available_names[NUM_AVAILABLE_NAMES] PROGMEM = {
  name_0, name_1, name_2, name_3, name_4, name_5, name_6, name_7, name_8, name_9,
  name_10, name_11, name_12, name_13, name_14, name_15, name_16, name_17, name_18, name_19,
  name_20, name_21, name_22, name_23, name_24
};

byte selected_names[MAX_PINS];

//==========================================

#if defined(__AVR__) 
  #include <EEPROM.h>
#endif

#if USE_LCD
  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
#endif

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define TIMEFUNCTION millis()

#if defined(__arm__) 
  #define fastWrite(_pin_, _state_) digitalWrite(_pin_, _state_);
#elif defined(__AVR__) 
  #define fastWrite(_pin_, _state_) (_state_ ?  PORTD |= 1 << _pin_ : PORTD &= ~(1 << _pin_ ))
#endif

// ====== BUFFER RIMSHOT COM NOTAS PERSONALIZADAS ======
struct SmartDrumBuffer {
  bool note37_active;
  bool note38_active;
  byte note37_velocity;
  byte note38_velocity;
  byte note37_channel;
  byte note38_channel;
  unsigned long note37_time;
  unsigned long note38_time;
  unsigned long window_start;
  bool window_open;
};

SmartDrumBuffer drumBuffer = {false, false, 0, 0, 0, 0, 0, 0, 0, false};

unsigned long DETECTION_WINDOW_MS = 12;  // Janela maior para rimshot mais fácil
unsigned long MAX_NOTE_AGE_MS = 30;      // SEMPRE > DETECTION_WINDOW_MS * 2
unsigned long BLOCK_WINDOW_MS = 60;  
byte VELOCITY_THRESHOLD_37_38 = 82;
// Valor 9 elimina crosstalk (velocity 1-7-15-19) sem cortar ghost notes reais (~20+)
// *** VELOCITY_MIN_NOTE38 mantida apenas para o buffer de rimshot da caixa (notas 37+38) ***
byte VELOCITY_MIN_NOTE38 = 0;

// *** VELOCITY_FLOOR GLOBAL REMOVIDA: agora cada pad tem seu próprio VelMinimo (0-26) ***
// Configurável individualmente no menu de cada pad (após Gain)

#if BUZZER
void playBeep() {
  if(!buzzerEnabled) return;
  tone(BUZZER_PIN, BEEP_FREQUENCY, BEEP_DURATION);
  buzzerActive = true;
  buzzerOffTime = millis() + BEEP_DURATION;
}

void updateBuzzer() {
  if(buzzerActive && millis() >= buzzerOffTime) {
    noTone(BUZZER_PIN);
    buzzerActive = false;
  }
}

void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  buzzerActive = false;
  buzzerOffTime = 0;
}
#endif

#if ENCODER
void initEncoder() {
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  encoder0PinALast = digitalRead(ENCODER_PIN_A);
  encoderButtonPressed = false;
  encoderButtonTime = 0;
  encoderButtonState = 0;
}

int readEncoder() {
  static unsigned long lastEncoderTime = 0;
  unsigned long currentTime = millis();
  if(currentTime - lastEncoderTime < 5) {
    return 0;
  }
  byte n = digitalRead(ENCODER_PIN_A);
  int result = 0;
  if ((encoder0PinALast == LOW) && (n == HIGH)) {
    if (digitalRead(ENCODER_PIN_B) == LOW) {
      result = -1;
    } else {
      result = 1;
    }
    lastEncoderTime = currentTime;
  }
  encoder0PinALast = n;
  return result;
}

bool readEncoderButton() {
  static unsigned long lastButtonTime = 0;
  unsigned long currentTime = millis();
  if(currentTime - lastButtonTime < 20) {
    return (digitalRead(ENCODER_BTN) == LOW);
  }
  lastButtonTime = currentTime;
  return (digitalRead(ENCODER_BTN) == LOW);
}
#endif

// *** FUNÇÃO RIMSHOT COM NOTAS PERSONALIZADAS ***
void processSmartDrumBuffer() {
  unsigned long currentTime = millis();
  
  // *** CORREÇÃO BUG DUPLA NOTA ***
  // Os guards de MAX_NOTE_AGE só podem liberar notas avulsas APÓS a janela fechar.
  // Se ficarem fora do !window_open, a nota 38 (ou 37) vaza como avulsa no mesmo
  // ciclo em que a janela expira e dispara o rimshot → resultado: 38 + 40 juntas.
  if (!drumBuffer.window_open) {
    if (drumBuffer.note37_active && (currentTime - drumBuffer.note37_time) > MAX_NOTE_AGE_MS) {
      sendMidiNote(drumBuffer.note37_channel, CUSTOM_NOTE_37, drumBuffer.note37_velocity);
      drumBuffer.note37_active = false;
    }
    if (drumBuffer.note38_active && (currentTime - drumBuffer.note38_time) > MAX_NOTE_AGE_MS) {
      // *** CORREÇÃO BUG CROSSTALK: só dispara se velocity >= mínimo ***
      // Crosstalk do chimbal gera velocity 1-7 na nota 38 → descarta
      // Ghost notes reais chegam com velocity >= 20 → passam normalmente
      if (drumBuffer.note38_velocity >= VELOCITY_MIN_NOTE38) {
        sendMidiNote(drumBuffer.note38_channel, 38, drumBuffer.note38_velocity);
      }
      drumBuffer.note38_active = false;
    }
  }
  
  if (drumBuffer.window_open && (currentTime - drumBuffer.window_start) > DETECTION_WINDOW_MS) {
    drumBuffer.window_open = false;
    
    if (drumBuffer.note37_active && drumBuffer.note38_active) {
      byte vel37 = drumBuffer.note37_velocity;
      byte vel38 = drumBuffer.note38_velocity;
      
      if (vel37 >= VELOCITY_THRESHOLD_37_38 && vel38 >= VELOCITY_THRESHOLD_37_38) {
        // *** USA NOTA PERSONALIZADA PARA RIMSHOT 37+38 ***
        sendMidiNote(drumBuffer.note37_channel, CUSTOM_NOTE_40, 127);
        drumBuffer.note37_active = false;
        drumBuffer.note38_active = false;
        return;
      }
    }
    
    if (drumBuffer.note37_active && drumBuffer.note38_active) {
      if (ENABLE_VELOCITY_FILTER) {
        if (drumBuffer.note37_velocity > drumBuffer.note38_velocity) {
          sendMidiNote(drumBuffer.note37_channel, CUSTOM_NOTE_37, drumBuffer.note37_velocity);
        } else {
          sendMidiNote(drumBuffer.note38_channel, 38, drumBuffer.note38_velocity);
        }
      } else {
        sendMidiNote(drumBuffer.note37_channel, CUSTOM_NOTE_37, drumBuffer.note37_velocity);
        sendMidiNote(drumBuffer.note38_channel, 38, drumBuffer.note38_velocity);
      }
      
      drumBuffer.note37_active = false;
      drumBuffer.note38_active = false;
    } 
    else if (drumBuffer.note37_active) {
      sendMidiNote(drumBuffer.note37_channel, CUSTOM_NOTE_37, drumBuffer.note37_velocity);
      drumBuffer.note37_active = false;
    }
    else if (drumBuffer.note38_active) {
      // *** CORREÇÃO BUG CROSSTALK: só dispara se velocity >= mínimo ***
      if (drumBuffer.note38_velocity >= VELOCITY_MIN_NOTE38) {
        sendMidiNote(drumBuffer.note38_channel, 38, drumBuffer.note38_velocity);
      }
      drumBuffer.note38_active = false;
    }
  }
}

bool addToSmartDrumBuffer(byte channel, byte note, byte velocity) {
  if (!ENABLE_NOTE_37_38_TO_40) return false;
  
  unsigned long currentTime = millis();
  
  if (!drumBuffer.window_open) {
    drumBuffer.window_open = true;
    drumBuffer.window_start = currentTime;
  }
  
  if (note == 37) {
    drumBuffer.note37_active = true;
    drumBuffer.note37_velocity = velocity;
    drumBuffer.note37_channel = channel;
    drumBuffer.note37_time = currentTime;
    return true;
  } 
  else if (note == 38) {
    drumBuffer.note38_active = true;
    drumBuffer.note38_velocity = velocity;
    drumBuffer.note38_channel = channel;
    drumBuffer.note38_time = currentTime;
    return true;
  }
  
  return false;
}

void updateNoteBlockControl() {
  processSmartDrumBuffer();
}

void SaveBackup(byte backupNumber);
void RestoreBackup(byte backupNumber);
bool BackupExists(byte backupNumber);

// *** MACROS COM NOTAS PERSONALIZADAS ***
#if ENABLE_CHANNEL
  #define fastNoteOn(_channel,_note,_velocity) { \
    if (ENABLE_RIMSHOT_38_TO_40 && _note == 38 && _velocity > 125) { \
      sendMidiNote(_channel, CUSTOM_RIMSHOT_FORCE_NOTE, _velocity); \
    } \
    else if (ENABLE_NOTE_37_38_TO_40 && (_note == 37 || _note == 38)) { \
      if (!addToSmartDrumBuffer(_channel, _note, _velocity)) { \
        if(_note == 37) { \
          sendMidiNote(_channel, CUSTOM_NOTE_37, _velocity); \
        } else { \
          sendMidiNote(_channel, _note, _velocity); \
        } \
      } \
    } \
    else if (ENABLE_NOTE_101_TO_102 && _note == 101 && _velocity > 125) { \
      sendMidiNote(_channel, 102, 125); \
    } \
    else if (ENABLE_NOTE_103_TO_104 && _note == 103 && _velocity > 125) { \
      sendMidiNote(_channel, 104, 125); \
    } \
    else { \
      sendMidiNote(_channel, _note, _velocity); \
    } \
  }
  #define fastMidiCC(_channel,_number,_value) { sendMidiCC(_channel, _number, _value); }
#else
  #define fastNoteOn(_channel,_note,_velocity) { \
    if (ENABLE_RIMSHOT_38_TO_40 && _note == 38 && _velocity > 125) { \
      sendMidiNote(0x09, CUSTOM_RIMSHOT_FORCE_NOTE, _velocity); \
    } \
    else if (ENABLE_NOTE_37_38_TO_40 && (_note == 37 || _note == 38)) { \
      if (!addToSmartDrumBuffer(0x09, _note, _velocity)) { \
        if(_note == 37) { \
          sendMidiNote(0x09, CUSTOM_NOTE_37, _velocity); \
        } else { \
          sendMidiNote(0x09, _note, _velocity); \
        } \
      } \
    } \
    else if (ENABLE_NOTE_101_TO_102 && _note == 101 && _velocity > 125) { \
      sendMidiNote(0x09, 102, 125); \
    } \
    else if (ENABLE_NOTE_103_TO_104 && _note == 103 && _velocity > 125) { \
      sendMidiNote(0x09, 104, 125); \
    } \
    else { \
      sendMidiNote(0x09, _note, _velocity); \
    } \
  }
  #define fastMidiCC(_channel,_number,_value) { sendMidiCC(0x09, _number, _value); }
#endif

enum mode:byte {
  Off = 0,
  Standby = 1,
  MIDI = 2,
  Tool = 3
};

mode Mode=MIDI;
unsigned long GlobalTime;

const int delayTime=10;
byte GeneralXtalk=0;
byte NSensor=2;

byte HHNoteSensor[] = {20,50,80,100};
byte HHThresoldSensor[] = {48,36,24,12};
byte HHFootNoteSensor[] = {0,19};
byte HHFootThresoldSensor[] = {127,127};

const byte NXtalkGroup=4;
int MaxXtalkGroup[NXtalkGroup] = {-1};

#define choke1 80    // Choke Crash 2 Nota
#define choke2 82    // Choke Crash 3 Nota
#define choke3 63    // Choke Ride  Nota
#define chokeRide 78 // Choke Crash 1 Nota
#define Aux1 84 // Pad Aux 1 Nota
#define Aux2 85 // Pad Aux 2 Nota
#define Aux3 86 // Pad Aux 3 Nota
#define Aux4 87 // Pad Aux 4 Nota
#define Aux5 88 // Pad Aux 5 Nota
#define Aux6 89 // Pad Aux 6 Nota
#define Aux7 90 // Pad Aux 7 Nota                                 

#define Choke1_Pin 51    // Choke Crash 2 Pino
#define Choke2_Pin 49    // Choke Crash 3 Pino
#define Choke3_Pin 47    // Choke Ride Cond e Cup Pino
#define ChokeRide_Pin 53 // Choke Crash 1 Pino
#define Aux1_Pin 45 // Pad Aux 1 Pino
#define Aux2_Pin 43 // Pad Aux 2 Pino
#define Aux3_Pin 41 // Pad Aux 3 Pino
#define Aux4_Pin 39 // Pad Aux 4 Pino
#define Aux5_Pin 37 // Pad Aux 5 Pino
#define Aux6_Pin 35 // Pad Aux 6 Pino
#define Aux7_Pin 33 // Pad Aux 7 Pino

int Choke1_State = LOW;
int Choke2_State = LOW; 
int Choke3_State = LOW;
int ChokeRide_State = LOW;
int Aux1_State = LOW;
int Aux2_State = LOW;
int Aux3_State = LOW;
int Aux4_State = LOW;
int Aux5_State = LOW;
int Aux6_State = LOW;
int Aux7_State = LOW; 
int currentSwitchState = LOW;

void printConfigStatus() {
  Serial.println("=== CONFIGURAÇÕES ATIVAS ===");
  Serial.print("Bloqueio 37+38->40: "); Serial.println(ENABLE_NOTE_37_38_TO_40 ? "ATIVO" : "INATIVO");
  Serial.print("Filtro Velocity: "); Serial.println(ENABLE_VELOCITY_FILTER ? "ATIVO" : "INATIVO");
  Serial.print("Rimshot 38->40: "); Serial.println(ENABLE_RIMSHOT_38_TO_40 ? "ATIVO" : "INATIVO");
  Serial.print("Threshold 37+38: "); Serial.println(VELOCITY_THRESHOLD_37_38);
  Serial.print("Janela Detecção (ms): "); Serial.println(DETECTION_WINDOW_MS);
  Serial.print("Nota 37 Custom: "); Serial.println(CUSTOM_NOTE_37);
  Serial.print("Nota 40 Custom: "); Serial.println(CUSTOM_NOTE_40);
  Serial.print("Nota 40 Forçado: "); Serial.println(CUSTOM_RIMSHOT_FORCE_NOTE);
  Serial.println("============================");
}