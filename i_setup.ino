///=========================================================================================//
//=>                         J-DRUMS v8.0.19 - CONTROLADOR MIDI BATERIA                    <= 
//=>                        Sistema de Bateria Eletrônica Arduino                          <=
//=>                     Copyright (c) 2026 Joanaldo Jhon Leonez de Melo                   <=
//=>                             Licensed under the MIT License.                           <=
//=>         See LICENSE.txt file in the project root for full license information.        <=
//=>                      DESENVOLVIDO POR JOANALDO JHON LEONEZ DE MELO                    <=
//=>                                   Janeiro/2026                                        <=
//=>---------------------------------------------------------------------------------------<=

void slideText(const char* text, int delayTime1) {
  int textLength = strlen(text);
  int startColumn = (16 - textLength) / 2;

  for (int i = textLength - 1; i >= 0; i--) {
    lcd.clear();
    lcd.setCursor(startColumn, 0);
    for (int j = i; j < textLength; j++) {
      lcd.print(text[j]);
    }
    delay(delayTime1);
  }
}

void progressBar(int width, int delayTime1) {
  lcd.setCursor(0, 1);
  lcd.print(".");  

  for (int i = 0; i < width; i++) {
    lcd.print(".");  
    delay(50);
  }

  lcd.print(".");  
}

void formText(const char* text, int delayTime1) {
  int textLength = strlen(text);
  int startColumn = (16 - textLength) / 2;

  for (int i = 0; i < textLength; i++) {
    lcd.setCursor(startColumn + i, 0);
    lcd.print(text[i]);
    delay(delayTime1);
  }
}

//==============================
//     SETUP - VERSÃO 4.0 SEM MULTIPLEXAÇÃO (16 PINOS FIXOS)
//==============================
void setup()
{
  Wire.begin();
  pinMode(13, OUTPUT);

  pinMode(Choke1_Pin, INPUT_PULLUP);
  pinMode(Choke2_Pin, INPUT_PULLUP);
  pinMode(Choke3_Pin, INPUT_PULLUP);
  pinMode(ChokeRide_Pin, INPUT_PULLUP);
  pinMode(Aux1_Pin, INPUT_PULLUP);
  pinMode(Aux2_Pin, INPUT_PULLUP);
  pinMode(Aux3_Pin, INPUT_PULLUP);
  pinMode(Aux4_Pin, INPUT_PULLUP);
  pinMode(Aux5_Pin, INPUT_PULLUP);
  pinMode(Aux6_Pin, INPUT_PULLUP);
  pinMode(Aux7_Pin, INPUT_PULLUP);

  #if ENCODER
  initEncoder();
  #endif
  
  #if BUZZER
  initBuzzer();
  #endif

  GlobalTime=TIMEFUNCTION;
  
  // *** INICIALIZA APENAS 16 PINOS (A0-A15) ***
  for (int count=0; count < 16; count++)
  {
    Pin[count].set(count);
  }
  
  // *** PAD A0 (HHC): InvertSensor padrão = Invertido (TCRT5000) ***
  // LoadAllEEPROM() sobrescreve se o usuário já salvou outro valor.
  // Na EEPROM virgem garante que sobe no modo correto sem precisar configurar.
  Pin[0].InvertSensor = 0;
  
  // *** INICIALIZA NOMES PARA 16 PINOS ***
  for(int i = 0; i < 16; i++) {
    if(i < NUM_AVAILABLE_NAMES) {
      selected_names[i] = i;
    } else {
      selected_names[i] = 0;
    }
  }
  
  #if USE_LCD
  pinMode(7, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  #endif
  
  #if SERIALSPEED
    Serial.begin(115200);
    Serial1.begin(115200);
    initInvisibleNoteOff();
    initInvisibleMidiRX1();
  #else
    initMidiSerial();
  #endif
  Serial.flush();
  Serial1.flush();
        
  #if defined(__AVR__) 
    analogReference(DEFAULT);
  #endif
  
  LoadAllEEPROM();
  
  // *** PRESET JÁ APLICADO DENTRO DO LoadAllEEPROM — NÃO repetir aqui ***
  // applyVSTPreset() chamado de novo sobrescreveria Pin[i].Note com VST_NOTES_PERSONA[]
  // antes de loadPersonaNotes() ter carregado os valores corretos da EEPROM,
  // zerando as notas de A8/A9 no padrão Persona.
  // *** FORÇA CARREGAMENTO DO ANTICROSSTALK (BYPASS ExecCommand) ***
  #if defined(__AVR__)
  for(int i = 0; i < 16; i++) {
    int addr = 100 + (i * 16) + 15;
    byte valor = EEPROM.read(addr);
    if(valor <= 127) {
      Pin[i].XCanCost = valor;  // ✅ CARREGA DIRETO NA RAM!
    }
  }
  #endif
  
  // ✅ CORREÇÃO: INICIALIZA E CARREGA PADS PRO MICRO DA EEPROM
  initProMicroPads();
  
  InitBackupSystem();
  initCacheSystem();
  
  #if defined(__AVR__)
  #if FASTADC
    setPrescaler(Prescaler_16);
  #endif
  #if VERYFASTADC
    setPrescaler(Prescaler_8);
  #endif
  #if VERYVERYFASTADC
    setPrescaler(Prescaler_4);
  #endif
  #elif defined(__arm__) 
    REG_ADC_MR = (REG_ADC_MR & 0xFFF0FFFF) | 0x00030000;
  #endif
  
  #if USE_LCD
  lcd.begin(16, 2);  
  {
    lcd.backlight();
    slideText("INICIANDO", 30);
    progressBar(20, 200);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CONTROLADO MIDI");
    progressBar(20, 200);
    lcd.clear();
    formText(" AR J-DRUMS 8.0", 100);
    lcd.setCursor(0, 1);
    lcd.noBacklight();
    delay(100);
    lcd.backlight();
    lcd.print(" SISTEMA PRONTO");
    delay(100);
    lcd.noBacklight();
    lcd.backlight();
    delay(1000);
    
    #if BUZZER
    if(buzzerEnabled) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("  Som Do Menu  ");
      lcd.setCursor(0, 1);
      lcd.print("   SOM ATIVO");
      
      for(int i = 0; i < 3; i++) {
        playBeep();
        delay(200);
        updateBuzzer();
        delay(200);
      }
      delay(500);
    }
    #endif
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  FINALIZANDO");
    
    lcd.setCursor(0, 1);
    for (int i = 0; i < 16; i++) {
      lcd.print((char)255);
      delay(80);
    }
    
    initBacklight();
    
    // *** Sempre inicia no Persona ao ligar ***
    // (sem mensagem de boot, sem lógica vstMsg)
    
    // *** MODIFICADO: Menu inicial é PAD 0 (página 2) ***
    eMenuPage = 2;
    eMenuSelect = 0;
    
    Draw();
  }  
  #endif
  
  CheckNamesIntegrity();
  
  delay(100);
  
  // *** LED: Pisca 1 vez (16 pinos fixos) ***
  digitalWrite(13, HIGH);
  delay(150);
  digitalWrite(13, LOW);
  delay(150);
  
  #if BUZZER
  if(buzzerEnabled) {
    playBeep();
    delay(100);
    updateBuzzer();
  }
  #endif
  
  #if SERIALSPEED
  Serial.println("Sistema inicializado - 16 pinos A0-A15");
  Serial.print("Janela de deteccao: ");
  Serial.print(DETECTION_WINDOW_MS);
  Serial.println("ms");
  #endif
}
