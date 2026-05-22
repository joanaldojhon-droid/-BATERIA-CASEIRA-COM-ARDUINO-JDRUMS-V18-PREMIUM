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
//=>  • Transmissão MIDI via USB e Serial TX1                                              <=
//=>  • Saídas habilitáveis independentemente                                              <=
//=>  • Sistema Note Off automático (12 notas simultâneas)                                 <=
//=>  • Duração consistente de 120ms para sustain                                          <=
//=>  • LED de indicação de transmissão                                                    <=
//=>  • Suporte a MIDI Control Change (CC)                                                 <=
//=>  • Choke automático via switches digitais                                             <=
//=========================================================================================//

// *** CONFIGURAÇÃO DO LED EXTERNO ***
#define LED_TX_PIN 22        // Pino para LED TX
#define LED_DURATION 35      // Duração do LED aceso em ms

// *** VARIÁVEIS GLOBAIS PARA CONTROLE DE SAÍDAS ***
byte MIDI_USB_ENABLED = 1;   // 0=OFF, 1=ON
byte MIDI_TX1_ENABLED = 1;   // 0=OFF, 1=ON

// Controle do LED
unsigned long ledTxOffTime = 0;
bool ledTxState = false;

// *** MONITOR VELOCITY ***
bool monitorVelocityAtivo = false;   // true = página 30 ativa
volatile byte monitorLastVelocity = 0;   // último velocity recebido (0-127)
volatile byte monitorLastSensor   = 255; // último sensor (0-15=A0-A15, 255=nenhum)
volatile byte monitorLastNote     = 0;   // última nota MIDI recebida (0-127)

// *** SISTEMA NOTE OFF AUTOMÁTICO ***
const byte INVISIBLE_MAX_NOTES = 12;
const unsigned int INVISIBLE_NOTE_DURATION = 120; // 120ms

// Estrutura para controle de Note OFF
struct InvisibleNoteControl {
  byte channel;
  byte note;
  unsigned long noteOffTime;
  bool isActive;
};

InvisibleNoteControl invisibleNotes[12];

// *** FUNÇÃO PARA CONTROLAR LED TX ***
void activateLedTx() {
  // Se USB ativo OU TX1 ativo, acende o LED
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    if(!ledTxState) {
      digitalWrite(LED_TX_PIN, HIGH);
      ledTxState = true;
      ledTxOffTime = millis() + LED_DURATION;
    } else {
      unsigned long currentTime = millis();
      if((ledTxOffTime - currentTime) < 50) {
        ledTxOffTime = currentTime + LED_DURATION;
      }
    }
  }
}

// *** FUNÇÃO PRINCIPAL PARA PROCESSAR LED ***
void processLeds() {
  unsigned long currentTime = millis();
  
  if(ledTxState && currentTime >= ledTxOffTime) {
    digitalWrite(LED_TX_PIN, LOW);
    ledTxState = false;
  }
}

// *** FORÇA DESLIGAR LED ***
void forceLedsOff() {
  digitalWrite(LED_TX_PIN, LOW);
  ledTxState = false;
  ledTxOffTime = 0;
}

// *** INICIALIZAÇÃO DO SISTEMA NOTE OFF ***
void initInvisibleNoteOff() {
  for(int i = 0; i < 12; i++) {
    invisibleNotes[i].isActive = false;
    invisibleNotes[i].channel = 0;
    invisibleNotes[i].note = 0;
    invisibleNotes[i].noteOffTime = 0;
  }
}

// *** INICIALIZAÇÃO DO LED ***
void initMidiLeds() {
  pinMode(LED_TX_PIN, OUTPUT);
  digitalWrite(LED_TX_PIN, LOW);
  
  ledTxState = false;
  ledTxOffTime = 0;
  
  delay(100);
  
  // TESTE INICIAL - Pisca LED 3 vezes (se QUALQUER saída ativa)
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    for(int i = 0; i < 3; i++) {
      digitalWrite(LED_TX_PIN, HIGH);
      delay(200);
      digitalWrite(LED_TX_PIN, LOW);
      delay(200);
    }
  }
}

// *** ADICIONA NOTA ATIVA NO SISTEMA ***
void addInvisibleNote(byte channel, byte note) {
  unsigned long currentTime = millis();
  
  // Remove qualquer nota igual que já existe
  for(int i = 0; i < 12; i++) {
    if(invisibleNotes[i].isActive && 
       invisibleNotes[i].channel == channel && 
       invisibleNotes[i].note == note) {
      sendMidiNoteOffDirect(channel, note);
      invisibleNotes[i].isActive = false;
      break;
    }
  }
  
  // Procura slot vazio
  for(int i = 0; i < 12; i++) {
    if(!invisibleNotes[i].isActive) {
      invisibleNotes[i].channel = channel;
      invisibleNotes[i].note = note;
      invisibleNotes[i].noteOffTime = currentTime + INVISIBLE_NOTE_DURATION;
      invisibleNotes[i].isActive = true;
      return;
    }
  }
  
  // Se não há slots vazios, substitui o mais antigo
  int oldestIndex = 0;
  unsigned long oldestTime = invisibleNotes[0].noteOffTime;
  
  for(int i = 1; i < 12; i++) {
    if(invisibleNotes[i].noteOffTime < oldestTime) {
      oldestTime = invisibleNotes[i].noteOffTime;
      oldestIndex = i;
    }
  }
  
  if(invisibleNotes[oldestIndex].isActive) {
    sendMidiNoteOffDirect(invisibleNotes[oldestIndex].channel, invisibleNotes[oldestIndex].note);
  }
  
  invisibleNotes[oldestIndex].channel = channel;
  invisibleNotes[oldestIndex].note = note;
  invisibleNotes[oldestIndex].noteOffTime = currentTime + INVISIBLE_NOTE_DURATION;
  invisibleNotes[oldestIndex].isActive = true;
}

// *** ENVIA NOTE OFF DIRETO (COM CONTROLE USB/TX1) ***
void sendMidiNoteOffDirect(byte channel, byte note) {
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(0x80 | (channel & 0x0F));
    Serial.write(note & 0x7F);
    Serial.write((byte)0);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(0x80 | (channel & 0x0F));
    Serial1.write(note & 0x7F);
    Serial1.write((byte)0);
  }
}

// *** PROCESSA NOTE OFF AUTOMÁTICO ***
void processInvisibleNoteOff() {
  unsigned long currentTime = millis();
  
  for(int i = 0; i < 12; i++) {
    if(invisibleNotes[i].isActive) {
      if(currentTime >= invisibleNotes[i].noteOffTime) {
        sendMidiNoteOffDirect(invisibleNotes[i].channel, invisibleNotes[i].note);
        invisibleNotes[i].isActive = false;
      }
    }
  }
}

// *** FORÇA NOTE OFF DE TODAS AS NOTAS ***
void invisibleAllNotesOff() {
  for(int i = 0; i < 12; i++) {
    if(invisibleNotes[i].isActive) {
      sendMidiNoteOffDirect(invisibleNotes[i].channel, invisibleNotes[i].note);
      invisibleNotes[i].isActive = false;
    }
  }
}

// *** FORÇA NOTE OFF DE UMA NOTA ESPECÍFICA ***
void invisibleForceNoteOff(byte channel, byte note) {
  for(int i = 0; i < 12; i++) {
    if(invisibleNotes[i].isActive && 
       invisibleNotes[i].channel == channel && 
       invisibleNotes[i].note == note) {
      
      sendMidiNoteOffDirect(channel, note);
      invisibleNotes[i].isActive = false;
      break;
    }
  }
}

// *** FUNÇÕES MIDI MODIFICADAS (COM CONTROLE USB/TX1) ***
void MIDI_TX(byte cmd, byte data1, byte data2)
{
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(cmd);
    Serial.write(data1); 
    Serial.write(data2);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(cmd);
    Serial1.write(data1);
    Serial1.write(data2);
  }
}

void MIDI_TX(byte cmd, byte data1)
{
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(cmd);
    Serial.write(data1);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(cmd);
    Serial1.write(data1);
  }
}

void simpleSysex(byte cmd, byte data1, byte data2, byte data3)
{
  if(cmd==0x6F) DrawDiagnostic(data1,data2);
  
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(0xF0);
    Serial.write(0x43);
    Serial.write(0x12);
    Serial.write(cmd);
    Serial.write(data1);
    Serial.write(data2);
    Serial.write(data3);
    Serial.write(0xF7);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(0xF0);
    Serial1.write(0x43);
    Serial1.write(0x12);
    Serial1.write(cmd);
    Serial1.write(data1);
    Serial1.write(data2);
    Serial1.write(data3);
    Serial1.write(0xF7);
  }
}

void Sysex(byte cmd, byte* data, byte length)
{
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(0xF0);
    Serial.write(0x43);
    Serial.write(0x12);
    Serial.write(cmd);
    for(byte i = 0; i < length; i++) {
      Serial.write(data[i]);
    }
    Serial.write(0xF7);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(0xF0);
    Serial1.write(0x43);
    Serial1.write(0x12);
    Serial1.write(cmd);
    for(byte i = 0; i < length; i++) {
      Serial1.write(data[i]);
    }
    Serial1.write(0xF7);
  }
}

// *** FUNÇÃO PRINCIPAL: NOTE ON COM SISTEMA AUTOMÁTICO ***
void sendMidiNote(byte channel, byte note, byte velocity)
{
  if(velocity == 0) {
    sendMidiNoteOffDirect(channel, note);
    return;
  }
  
  // *** MONITOR VELOCITY: captura sem custo quando ativo ***
  if(monitorVelocityAtivo) {
    monitorLastVelocity = velocity;
    monitorLastNote     = note;
    // monitorLastSensor é setado em setMonitorSensor() antes desta chamada
  }
  
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(0x90 | (channel & 0x0F));
    Serial.write(note & 0x7F);
    Serial.write(velocity & 0x7F);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(0x90 | (channel & 0x0F));
    Serial1.write(note & 0x7F);
    Serial1.write(velocity & 0x7F);
  }
  
  addInvisibleNote(channel & 0x0F, note & 0x7F);
}

// *** NOTE OFF MANUAL ***
void sendMidiNoteOff(byte channel, byte note)
{
  invisibleForceNoteOff(channel & 0x0F, note & 0x7F);
}

void sendMidiCC(byte channel, byte controller, byte value)
{
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(0xB0 | (channel & 0x0F));
    Serial.write(controller & 0x7F);
    Serial.write(value & 0x7F);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(0xB0 | (channel & 0x0F));
    Serial1.write(controller & 0x7F);
    Serial1.write(value & 0x7F);
  }
}

// processMidiInput() removida - leitura de entrada MIDI não utilizada nesta versão

// *** INICIALIZAÇÃO: USB + TX1 + NOTE OFF + LED ***
void initMidiSerial()
{
  Serial.begin(31250);
  Serial1.begin(31250);
  
  initInvisibleNoteOff();
  initMidiLeds();
  
  Serial.flush();
  Serial1.flush();
}

// Funções MIDI de controle
void sendMidiClock()
{
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(0xF8);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(0xF8);
  }
}

void sendMidiStart()
{
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(0xFA);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(0xFA);
  }
}

void sendMidiStop()
{
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(0xFC);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(0xFC);
  }
  
  invisibleAllNotesOff();
}

void sendMidiContinue()
{
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    activateLedTx();
  }
  
  if(MIDI_USB_ENABLED) {
    Serial.write(0xFB);
  }
  
  if(MIDI_TX1_ENABLED) {
    Serial1.write(0xFB);
  }
}

// *** FUNÇÃO PRINCIPAL PARA SER CHAMADA NO LOOP ***
void processInvisibleMidiSystem() {
  processInvisibleNoteOff();
  processLeds();
}

// *** FUNÇÕES EXTRAS PARA CHOKES ***
void sendMidiNoteForChoke(byte channel, byte note, byte velocity) {
  invisibleForceNoteOff(channel, note);
  sendMidiNote(channel, note, velocity);
}

void sendChokeNoteOff(byte channel, byte note) {
  invisibleForceNoteOff(channel, note);
}

// *** FUNÇÃO DE TESTE DO LED ***
void testLeds() {
  if(MIDI_USB_ENABLED || MIDI_TX1_ENABLED) {
    digitalWrite(LED_TX_PIN, HIGH);
    delay(500);
    digitalWrite(LED_TX_PIN, LOW);
    delay(500);
  }
}