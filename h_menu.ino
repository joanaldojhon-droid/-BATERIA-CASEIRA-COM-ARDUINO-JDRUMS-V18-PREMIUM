
//========================================================================================//
//=>                         J-DRUMS v8.0.16 - CONTROLADOR MIDI BATERIA                    <= 
//=>                        Sistema de Bateria Eletrônica Arduino                          <=
//=>                     Copyright (c) 2026 Joanaldo Jhon Leonez de Melo                   <=
//=>                             Licensed under the MIT License.                           <=
//=>         See LICENSE.txt file in the project root for full license information.        <=
//=>                      DESENVOLVIDO POR JOANALDO JHON LEONEZ DE MELO                    <=
//=>                                   Janeiro/2026                                        <=
//=>                                                                                       <=
//=>  FUNCIONALIDADES:                                                                     <=
//=>  • Menu completo em LCD 16x2 I2C                                                      <=
//=>  • Navegação via encoder rotativo + 2 botões                                          <=
//=>  • Edição de nomes personalizados                                                     <=
//=>  • Sistema de confirmação e cancelamento                                              <=
//=>  • Backlight inteligente                                                              <=
//=>  • Feedback via buzzer                                                                <=
//=========================================================================================//

///==============================
//    PARTE 1/3 - CABEÇALHO E FUNÇÕES AUXILIARES - COM MENU SAÍDA MIDI E PERSONA
//==============================
#if USE_LCD
#define HOLDDELAY 500
#define DEBOUNCEDELAY 100
#define SUPERHOLD_ADDCTIVE_MS 5000UL  // 5s segurado em A ou B → restaura preset Addctive da PROGMEM
#define MENU_NAV_DEBOUNCE 50  // ✅ NOVO: Debounce reduzido para navegação em menus
#define ENCODER_DEBOUNCE 80



// =============================================================
// VeMinimo: piso de velocity individual por pad (A0-A15)
// Configurado no menu de cada pad após o parâmetro Gain
// Limite: 0-100.  0 = desligado (não filtra nada)
// =============================================================

// =============================================================
// MENU XCANCEL — atalho encoder 20s após atalho PAD (página 34)
// Navega pelos 16 pares direcionais da tabela XPAIR_TABLE
// Edita: Source, Target, Janela (windowMs), GhostVel por par
// Sai: encoder curto ou botão B médio
// =============================================================
#define XCANCEL_PAGE       34
#define XCANCEL_HOLD_MS    20000UL  // 20 segundos segurando para ativar

bool xcancelMenuAtivo = false;   // true = página 34 ativa
byte xcancelPar    = 0;          // par selecionado (0-15)
byte xcancelParam  = 0;          // parâmetro: 0=Source 1=Target 2=GhostVel (Janela fixo=120ms)
static bool xcancelHoldFired = false;

// *** PAR 0 (1/16) É SOMENTE LEITURA: restaura padrão ao sair do menu XCancel ***
// Valores fixos: source=1(A1), target=2(A2), windowMs=120, ghostVel=18
inline void xcancelResetPar0() {
  xpairRam[0].source   = 1;
  xpairRam[0].target   = 2;
  xpairRam[0].windowMs = 120;
  xpairRam[0].ghostVel = 18;
}

// Acesso de escrita à tabela de pares (em RAM — cópia da const para edição)
// A tabela XPAIR_TABLE em c_pin.ino é const; precisamos de uma cópia RAM editável
extern XtalkPair xpairRam[XPAIR_COUNT];

// =============================================================
// MENU TIMING — hold encoder 20s com atalho PAD ativo (página 35)
// Mostra só os 3 ajustes: R>37+38-Ms / R>NotAgeMs / R>BlkWinMs
// Sai: clique curto no encoder ou botão B médio
// =============================================================
#define MENU_TIMING_PAGE    35
#define MENU_TIMING_HOLD_MS 20000UL   // 20s segurando ativa o menu

bool menuTimingAtivo = false;   // true = página 35 ativa
byte menuTimingParam = 0;       // 0=DetWin  1=NotAge  2=BlkWin
static bool menuTimingHoldFired = false;  // dispara só uma vez por pressão

// =============================================================
// MENU RÁPIDO — 10 CLIQUES NO ENCODER (página 33)
// Ativado por 10 cliques rápidos (dentro de 2s) no botão do encoder
// Sai SOMENTE após segurar qualquer botão ou encoder por 3 segundos
// =============================================================
#define MENU_RAPIDO_PAGE        33        // página do menu rápido
#define MENU_RAPIDO_CLICKS      10        // número de cliques para ativar
#define MENU_RAPIDO_JANELA_MS   2000UL    // janela de tempo para os cliques
#define MENU_RAPIDO_SAIR_MS     3000UL    // tempo segurado para sair (3s)

bool menuRapidoAtivo = false;             // true = página 33 ativa
byte menuRapidoParam = 0;                 // parâmetro selecionado dentro do menu rápido

// Contador de cliques para ativar o menu rápido
static byte  _clickCount        = 0;
static unsigned long _firstClickTime = 0;

// Debounce de saída do menu rápido (3s segurado)
static unsigned long _sairMenuRapidoStart = 0;  // millis() do início do hold de saída
static bool          _sairMenuRapidoAtivo = false; // true = está segurando para sair

unsigned long lastLCDUpdate = 0;
unsigned long lastBacklightUpdate = 0;
const unsigned long LCD_UPDATE_INTERVAL = 200;
const unsigned long BACKLIGHT_UPDATE_INTERVAL = 100;
bool needsRedraw = true;
bool menuSystemActive = true;

extern unsigned long lastInteractionTime;

// *** MONITOR VELOCITY: acesso às variáveis de a_midi.ino ***
extern bool monitorVelocityAtivo;
extern volatile byte monitorLastVelocity;
extern volatile byte monitorLastSensor;
extern volatile byte monitorLastNote;

// *** MONITOR HI-HAT ***
bool monitorHiHatAtivo = false;  // true = página 31 ativa (bloqueia scan HHC no MIDI)

// *** CALIBRAÇÃO MONITOR HI-HAT ***
// HH_CLOSED_POINT: valor do sensor (0-127) onde a barra some completamente (chimbal fechado)
//   → Aumente se a barra ainda mostra 1-2 blocos com chimbal totalmente fechado
//   → Diminua se a barra some antes de fechar de verdade
// HH_OPEN_POINT: valor do sensor (0-127) onde a barra fica completamente cheia (chimbal aberto)
//   → Diminua se a barra não enche toda com chimbal totalmente aberto
//   → Aumente se a barra já enche antes de abrir de verdade
// *** CALIBRAÇÃO HH: agora são variáveis ajustáveis pelo menu e salvas na EEPROM ***
// Endereços EEPROM: 583 (fechado) e 584 (aberto)
#define EEPROM_HH_CLOSED 583
#define EEPROM_HH_OPEN   584
byte HH_CLOSED_POINT = 1;   // padrão: 15 — ajuste para calibrar o ponto de fechamento
byte HH_OPEN_POINT   = 118;  // padrão: 110 — ajuste para calibrar o ponto de abertura

void SaveHHCalibEEPROM() {
  #if defined(__AVR__)
  EEPROM.write(EEPROM_HH_CLOSED, HH_CLOSED_POINT);
  EEPROM.write(EEPROM_HH_OPEN,   HH_OPEN_POINT);
  #endif
}

void LoadHHCalibEEPROM() {
  #if defined(__AVR__)
  byte vc = EEPROM.read(EEPROM_HH_CLOSED);
  byte vo = EEPROM.read(EEPROM_HH_OPEN);
  if(vc <= 127) HH_CLOSED_POINT = vc;
  if(vo <= 127) HH_OPEN_POINT   = vo;
  #endif
}



unsigned long lastEncoderMove = 0;
unsigned long lastButtonPress = 0;
int lastEncoderValue = 0;
const unsigned long MENU_NAV_DELAY = 200;

// *** CORREÇÃO: flags para resetar os static botoesForamSoltos dos menus de edição/confirmação ***
bool resetProMicroEditButtons  = false;
bool resetPresetEditButtons    = false;
bool resetProMicroConfirmButtons = false;
bool resetPresetConfirmButtons = false;

char editingName[13];
byte editPosition = 0;
unsigned long cursorBlinkTimer = 0;
bool cursorVisible = true;

unsigned long confirmationStartTime = 0;
const unsigned long CONFIRMATION_TIMEOUT = 5000;

const char EDIT_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";
const byte EDIT_CHARS_COUNT = 63;
byte currentCharIndex = 0;

byte eMenuSelect=0;
byte eMenuPage=1;        // *** MODIFICADO: Inicia em página 1 (Menu Principal) ***
byte eMenuGeneral=0;
byte eMenuPin=0;
byte eMenuLog=0;
byte eMenuBacklight=0;
byte eMenuAdvanced=0;
byte eMenuBuzzer=0;
byte eMenuMidi=0;
byte eMenuConfig=0;      // *** NOVO: Menu Configurações ***
byte rimSub=0;           // *** Sub-ajuste Rimshot pad A2: 0=Comp 1=Forc — SALVO NA EEPROM ***
#define EEPROM_RIMSUB_MODE  686   // endereço para salvar rimSub (logo após InvertSensor flag em 685)

// *** VARIÁVEIS DO MENU PRO MICRO - 4 NÍVEIS ***
byte eMenuProMicro = 0;        // NÍVEL 0/1: Seleção de pad (0-9)
byte eMenuProMicroParam = 0;   // NÍVEL 2: Seleção de parâmetro (0-11)
bool proMicroInPadSelection = false; // true = NÍVEL 1, false = NÍVEL 0

// *** VARIÁVEIS PARA EDIÇÃO DE NOMES PRO MICRO ***
bool proMicroNameEditMode = false;
bool proMicroConfirmationMode = false;
byte editingProMicroPad = 0;

// *** NOMES PERSONALIZADOS DOS PRESETS DE BACKUP ***
bool presetNameEditMode  = false;  // true = editando nome de preset
byte editingPresetNum    = 0;      // 1..3 qual preset está editando
bool presetConfirmMode   = false;  // true = confirmando limpeza de nome
byte confirmingPresetNum = 0;      // 1..3

// *** NOVO: Controle Pads Auxiliar (SALVO NA EEPROM) ***
bool menuPadsAuxiliarAtivo = false;  // Por padrão OCULTO
bool confirmacaoPadsAuxiliar = false;
unsigned long confirmacaoPadsAuxiliarTime = 0;
unsigned long bothButtonsPadsAuxTime = 0;
bool bothButtonsPadsAuxPressed = false;

// *** NOVO: Escolha de modo 4 ou 6 pads ***
bool selecaoModoPads = false;         // true = está na tela de seleção A(6 pads) ou B(4 pads)
byte modoPadsAuxiliar = 6;            // 6 = 6 pads (padrão), 4 = 4 pads
byte maxPadsAuxiliar = 5;             // máximo de pads (5 = pads 0-5, 3 = pads 0-3)

// *** NOVO: Modo dos Botões (SALVO NA EEPROM) ***
byte modoBotoes = 1;                  // 0 = Normal (A=6, B=7), 1 = Invertido (A=7, B=6)
byte eMenuBotoes = 0;                 // Índice do menu Botoes

// *** NOVO: Notas Pads Digitais (Aux1-7) - SALVO NA EEPROM (endereços 526-532) ***
byte digitalPadNotes[7] = {61, 62,  7,  9, 27, 31, 90};  // Valores padrão Aux1-7 (Addictive)
byte eMenuDigital = 0;                // Índice do pad digital (0-6)
byte eMenuDigitalParam = 0;          // 0 = seleção do pad, 1 = edição da nota

// *** NOVO: Notas Choke (4 chokes) - SALVO NA EEPROM (endereços 533-536) ***
// Crash1=chokeRide=78, Crash2=choke1=80, Crash3=choke2=82, Ride=choke3=63
byte chokeNotes[4] = {78, 80, 82, 63};  // Crash1(P53), Crash2(P51), Crash3(P49), Ride(P47)
byte eMenuChokeNote = 0;             // Índice do choke (0-3)



//==============================
//    FUNÇÕES DE NOME PERSONALIZADO PRO MICRO
//    (BACKUP - caso aa_pro_micro.ino não tenha sido substituído)
//==============================

#ifndef PRO_MICRO_CUSTOM_NAMES_DEFINED
#define PRO_MICRO_CUSTOM_NAMES_DEFINED

// Salva nome personalizado para pad Pro Micro
void saveProMicroCustomName_local(byte padIndex, char* name) {
  if(padIndex >= 11) return;
  
  int address = 732 + (padIndex * 13);
  #if defined(__AVR__)
  
  // Salva o nome (13 bytes)
  for(int i = 0; i < 13; i++) {
    EEPROM.write(address + i, name[i]);
  }
  
  // Salva a FLAG
  int flagAddress = 875 + padIndex;
  EEPROM.write(flagAddress, 1);
  
  #endif
  markNameChanged();
}

// Carrega nome personalizado para pad Pro Micro
bool loadProMicroCustomName_local(byte padIndex, char* buffer) {
  if(padIndex >= 11) return false;
  #if defined(__AVR__)
  if(EEPROM.read(875 + padIndex) != 1) {
    return false;
  }
  int address = 732 + (padIndex * 13);
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

// Verifica se pad Pro Micro tem nome personalizado
bool hasProMicroCustomName_local(byte padIndex) {
  if(padIndex >= 11) return false;
  #if defined(__AVR__)
  return EEPROM.read(875 + padIndex) == 1;
  #else
  return false;
  #endif
}

// Limpa nome personalizado do pad Pro Micro
void clearProMicroCustomName_local(byte padIndex) {
  if(padIndex >= 11) return;
  #if defined(__AVR__)
  EEPROM.write(875 + padIndex, 0);
  #endif
  markNameChanged();
}

// Define aliases para usar as funções locais se as externas não existirem
#ifndef saveProMicroCustomName
  #define saveProMicroCustomName saveProMicroCustomName_local
#endif
#ifndef loadProMicroCustomName
  #define loadProMicroCustomName loadProMicroCustomName_local
#endif
#ifndef hasProMicroCustomName
  #define hasProMicroCustomName hasProMicroCustomName_local
#endif
#ifndef clearProMicroCustomName
  #define clearProMicroCustomName clearProMicroCustomName_local
#endif

#endif // PRO_MICRO_CUSTOM_NAMES_DEFINED

unsigned long btnA_Time=0;
unsigned long btnB_Time=0;
byte btnB_State=0;
byte btnA_State=0;



LiquidCrystal_I2C lcd(0x27, 16, 2);

//==============================
//    ATALHO DE AJUSTE DE PAD — HOLD LONGO DO ENCODER (>= 3s)
//==============================
#define PAD_SHORTCUT_VEL_MIN   100
#define PAD_SHORTCUT_TIMEOUT   20000UL

bool padShortcutMode       = false;
bool padShortcutActivated  = false;
unsigned long padShortcutStartTime = 0;

// *** SUPER-HOLD 5s: restaura preset Addctive da PROGMEM ***
bool superHoldA_Fired = false;  // evita disparar múltiplas vezes enquanto segura A
bool superHoldB_Fired = false;  // evita disparar múltiplas vezes enquanto segura B

void drawPadShortcutScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Aguardando    ");
  lcd.setCursor(0, 1);
  lcd.print("  Bata no PAD   ");
}

void activatePadShortcut() {
  padShortcutMode      = true;
  padShortcutActivated = true;
  padShortcutStartTime = millis();
  drawPadShortcutScreen();
  resetBacklightTimer();
  #if BUZZER
 if(buzzerEnabled) { playBeep(); delay(80); playBeep(); }
  #endif
}

void deactivatePadShortcut() {
  padShortcutMode      = false;
  padShortcutActivated = false;
  needsRedraw = true;
  lcd.clear();
}

void checkPadShortcutTimeout() {
  if(padShortcutMode && (millis() - padShortcutStartTime >= PAD_SHORTCUT_TIMEOUT)) {
    deactivatePadShortcut();
  }
}

bool checkPadShortcutHit(byte sensor) {
  if(!padShortcutMode) return false;
  // (VeMinimo agora é parâmetro inline no menu de cada pad — sem página separada)
  if(xcancelMenuAtivo)  return false;  // no menu XCancel: pad nao muda de tela
  if(sensor >= 16) return false;
  if(Pin[sensor].Type == Disabled) return false;
  if(Pin[sensor].State != Piezo_Time) return false;
  byte vel = Pin[sensor].useCurve();
  if(vel < PAD_SHORTCUT_VEL_MIN) return false;
  padShortcutMode      = false;
  padShortcutActivated = false;
  eMenuPage   = sensor + 2;
  eMenuPin    = 0;
  eMenuSelect = 1;
  lcd.clear();      // *** FIX: limpa display ao entrar no menu do pad ***
  #if BUZZER
  if(buzzerEnabled) { playBeep(); delay(60); playBeep(); }
  #endif
  needsRedraw = true;
  resetBacklightTimer();
  return true;
}

//==============================
//    STRINGS DO MENU
//==============================

#define S_HITSOFT PSTR("HIT SOFT")
#define S_HITHARD PSTR("HIT HARD")
#define S_WAIT PSTR("WAIT...")
#define S_END PSTR("END")
#define S_NOISE PSTR("NOISE..")
#define ADD(x) Pin[eMenuPage-2].x=(Pin[eMenuPage-2].x+1)%128; markPinChanged(eMenuPage-2);
#define TADD(x,t) Pin[eMenuPage-2].x=(t)((Pin[eMenuPage-2].x+1)%128); markPinChanged(eMenuPage-2);
#define SUB(x) Pin[eMenuPage-2].x=Pin[eMenuPage-2].x-1>-1?Pin[eMenuPage-2].x-1:127; markPinChanged(eMenuPage-2);
#define TSUB(x,t) Pin[eMenuPage-2].x=(t)(Pin[eMenuPage-2].x-1>-1?Pin[eMenuPage-2].x-1:127); markPinChanged(eMenuPage-2);

#define S_MODE PSTR("Modo")
#define S_GENERAL PSTR("Geral Delay")
#define S_BACKLIGHT PSTR("Luz Do Display")
#define S_ADVANCED PSTR("   Dual Pad   ")
#define S_BACKUP_CONF PSTR("Salvar Presets")
#define S_BUZZER_MENU PSTR(" Som Do Menu ")
#define S_MIDI_OUTPUT_MENU PSTR("Saida Midi")
#define S_PROMICRO_MENU PSTR("Pads Auxiliar")
#define S_CONFIG_MENU PSTR("Configuracoes")
#define S_MENU_PRINCIPAL PSTR("Menu Principal")
#define S_PADS_AUX_CONFIRM PSTR("Pads Auxiliar?")

// ========================================================================
// 🎯 DOIS NOMES PARA "MODO DE PADS" - VOCÊ ESCOLHE QUAL USAR!
// ========================================================================
#define S_MODO_PADS_NORMAL PSTR("Leonardo ProMicr?")    // ← Nome para NORMAL PLACA COM P10
#define S_MODO_PADS_INVERTIDO PSTR("ProMicr Leonardo?") // ← Nome para INVERTIDO PLACA PRETA
// Ou você pode usar outros nomes como quiser!

#define S_6_PADS PSTR("A=4Pads B=6Pads") // INVERTIDO
#define S_6_PADS_INV PSTR("B=6Pads A=4Pads") // NORMAL
#define S_A_SIM_B_NAO PSTR("A=NAO  B=Sim") // INVERTIDO
#define S_B_SIM_A_NAO PSTR("A=SIM  B=Nao") // NORMAL
#define S_A_ENTRA_B_VOLTA PSTR("A=Volta B=Entra")
#define S_B_ENTRA_A_VOLTA PSTR("B=Entra A=Volta")
#define S_BOTOES_MENU PSTR("Inverte Botoes")
#define S_BOTOES_NORMAL PSTR("P6 BT A")
#define S_BOTOES_INVERTIDO PSTR("P7 BT A")
#define S_ENTER PSTR("A=Volta B=Entra")
// *** MENU RÁPIDO (10 cliques no encoder → página 33) ***
#define S_MENU_RAPIDO      PSTR("Menu Rapido")
#define S_MENU_RAP_SEGURE  PSTR("Segure 3s p/Sair")
#define S_MENU_RAP_SAINDO  PSTR("Saindo...")
#define S_PIN PSTR("PIN")
#define S_OFF PSTR("OFF")
#define S_STANDBY PSTR("Standby")
#define S_MIDI PSTR("MIDI")
#define S_TOOL PSTR("Tool")
#define S_DELAY PSTR("Delay")
// *** NOVO: Strings menus Pad Digital N e Notas Choke ***
#define S_DIGITAL_MENU PSTR("Notas Trizone")
#define S_CHOKE_MENU PSTR("Notas Choke")
#define S_RESET_MENU PSTR("Resetar Modulo")
#define S_MONITOR_VELOCITY PSTR("Monitor Midi")
#define S_MONITOR_HIHAT PSTR("Monitor Hi-Hat")
#define S_VEL_MINIMO_MENU PSTR("VeMin>Cada Pad")
#define S_XCANCEL_MENU PSTR("XCancel Croslk")
#define S_RESET_CONFIRM_NORMAL   PSTR("A=NAO  B=SIM")   // modoBotoes==0: A=confirma
#define S_RESET_CONFIRM_INVERTIDO PSTR("A=SIM  B=NAO")  // modoBotoes==1: B=confirma
#define S_RESET_DONE PSTR("Reset OK!")
#define S_DIG1 PSTR("Dig 1 P45")
#define S_DIG2 PSTR("Dig 2 P43")
#define S_DIG3 PSTR("Dig 3 P41")
#define S_DIG4 PSTR("Dig 4 P39")
#define S_DIG5 PSTR("Dig 5 P37")
#define S_DIG6 PSTR("Dig 6 P35")
#define S_DIG7 PSTR("Dig 7 P33")
#define S_CHOKE_CRASH1 PSTR("Crash1 P53")
#define S_CHOKE_CRASH2 PSTR("Crash2 P51")
#define S_CHOKE_CRASH3 PSTR("Choke3 P49")
#define S_CHOKE_RIDE   PSTR("Ride  P47")
#define S_XTALK PSTR("XTalk")
#define S_HHCT100 PSTR("HHC T100")
#define S_HHCT75 PSTR("HHC T75")
#define S_HHCT50 PSTR("HHC T50")
#define S_HHCT25 PSTR("HHC T25")
#define S_NSENSOR PSTR("Sensor N")
#define S_BACKLIGHT_MODE PSTR("Modo")
#define S_30_SEGUNDOS PSTR("30 Segundos")
#define S_SEMPRE_ACESO PSTR("Semp Aceso")
#define S_BUZZER_STATUS PSTR("Status")
#define S_BUZZER_ATIVADO PSTR("Ativo")
#define S_BUZZER_DESATIVADO PSTR("Inativo")
#define S_ENABLE_37_38 PSTR("R>37+38-40")
#define S_RIMSHOT PSTR("R>For38-40")
#define S_RIMSHOT_MENU PSTR("Rimshot")
#define S_RIMSHOT_COMP PSTR("Comp")
#define S_RIMSHOT_FORC PSTR("Forc")
#define S_NOTE_101_102 PSTR("DPd101-102")
#define S_NOTE_103_104 PSTR("DPd103-104")
#define S_CUSTOM_NOTE_37 PSTR("R>Aro-Not")
#define S_CUSTOM_NOTE_40 PSTR("R>37+38Not")
#define S_CUSTOM_FORCE_40 PSTR("R>For-Not ")
// *** A3: ajustes de nota do rimshot (movidos de A0 para A3) ***
#define S_RIMSHOT_A3_NOTE   PSTR("R>37+38Not")
#define S_RIMSHOT_A3_FORCE  PSTR("R>For-Not ")
#define S_VEL_THRESH_37_38 PSTR("R>37+38Vel")     
#define S_DETECTION_WINDOW PSTR("R>37+38-Ms")
#define S_MAX_NOTE_AGE     PSTR("R>NotAgeMs")
#define S_BLOCK_WINDOW     PSTR("R>BlkWinMs")
#define S_VEL_FILTER       PSTR("R>37+38Fil")
#define S_MIDI_USB PSTR("USB")
#define S_MIDI_TX1 PSTR("TX1")
#define S_ATIVADO PSTR("Ativo")
#define S_DESATIVADO PSTR("Inativo")
#define S_BACKUP_1 PSTR("Preset 1")
#define S_BACKUP_2 PSTR("Preset 2") 
#define S_BACKUP_3 PSTR("Preset 3")
#define S_RESTAURAR_1 PSTR("Rest Pres 1")
#define S_RESTAURAR_2 PSTR("Rest Pres 2")
#define S_RESTAURAR_3 PSTR("Rest Pres 3")
#define S_NOME_PRESET_1 PSTR("Nome Preset 1")
#define S_NOME_PRESET_2 PSTR("Nome Preset 2")
#define S_NOME_PRESET_3 PSTR("Nome Preset 3")
#define S_SALVAR PSTR("Salvar")
#define S_RESTAURAR PSTR("Restaurar")
#define S_SALVO PSTR("Salvo!")
#define S_RESTAURADO PSTR("Restaurado!")
#define S_NAO_EXISTE PSTR("Nao Existe")
#define S_NOVO_NOME PSTR("NOVO NOME")
#define S_SALVANDO PSTR("SALVANDO")
#define S_SALVO_NOME PSTR("NOME SALVO!")
#define S_CANCELANDO PSTR("CANCELANDO")
#define S_CANCELADO PSTR("CANCELADO!")
#define S_LIMPA_NOME PSTR("Limpa nome?")
#define S_LIN PSTR("LIN")
#define S_EXP PSTR("EXP")
#define S_LOG PSTR("LOG")
#define S_SGM PSTR("SGM")
#define S_FLT PSTR("FLT")
#define S_NOTE PSTR("Note")
#define S_THRESOLD PSTR("Thresold")
#define S_SCANTIME PSTR("ScanTime")
#define S_MASKTIME PSTR("MaskTime")
#define S_RETRIG PSTR("Retrig")
#define S_AUT    PSTR("Aut")
#define S_CURVE PSTR("Curve")
#define S_CURVEF PSTR("CurveF")
#define S_XTALKG PSTR("XTalkG")
#define S_XCANCOST PSTR("XtakCancel")
#define S_XCANCOST_AUX PSTR("Anticros")
#define S_VE_MINIMO     PSTR("VelMinimo")
#define S_INVERT_SENSOR PSTR("InvSensor")
#define S_RIMSHOT_NOTE  PSTR("RishotNote")
#define S_VEL_THRESH_PAD PSTR("RishotVel")
#define S_TYPE PSTR("Type")
#define S_CKNOTE PSTR("CkNote")
#define S_GAIN PSTR("Gain")
#define S_DUAL PSTR("Dual")
#define S_CHANNEL PSTR("Channel")
#define S_NAME PSTR("Name")
#define S_PIEZO PSTR("Piezo")
#define S_SWITCH PSTR("Switch")
#define S_HHC PSTR("HHC")
#define S_HH PSTR("HH")
#define S_HHS PSTR("HHs")
#define S_YSWITCH PSTR("YSwitch")
#define S_DISABLED PSTR("Disabled")

void Up();
void Down();
void Draw();

//==============================
//    encoderEnterLevel() — entra no nível (igual botão A longo)
//    encoderGoBack()     — volta de nível (igual botão B longo)
//==============================

void encoderEnterLevel() {
  // === PÁGINA 25: Menu Configurações — nível 1 (item selecionado) → entra no submenu ===
  if(eMenuPage == 25 && eMenuSelect == 1) {
    lcd.clear();  // ✅ Limpa display ao entrar em qualquer submenu
    switch(eMenuConfig) {
      case 0:  eMenuPage = 18; break;
      case 1:  eMenuPage = 19; eMenuAdvanced = 2; break;
      case 2:  eMenuPage = 20; break;
      case 3:  eMenuPage = 21; break;
      case 5:  eMenuPage = 23; break;
      case 6:  if(menuPadsAuxiliarAtivo) { eMenuPage = 24; proMicroInPadSelection = true; eMenuProMicro = 0; } break;
      case 7:  eMenuPage = 26; break;
      case 8:  eMenuPage = 27; break;
      case 9:  eMenuPage = 28; break;
      case 10: eMenuPage = 29; break;
      case 11:
        monitorVelocityAtivo = true;
        monitorLastSensor = 255;
        monitorLastVelocity = 0;
        eMenuPage = 30;
        break;
      case 12:
        monitorHiHatAtivo = true;
        eMenuPage = 31;
        break;
      case 13:
        xcancelMenuAtivo = true;
        xcancelPar   = 0;
        xcancelParam = 0;
        eMenuPage    = XCANCEL_PAGE;
        needsRedraw  = true;
        lcd.clear();
        // windowMs fixo=120 e ghostVel padrão=18 para todos os pares ao entrar
        for(byte _xi = 0; _xi < XPAIR_COUNT; _xi++) {
          xpairRam[_xi].windowMs = 120;
          if(xpairRam[_xi].ghostVel == 0) xpairRam[_xi].ghostVel = 18;
        }
        #if BUZZER
        if(buzzerEnabled) { playBeep(); delay(80); playBeep(); delay(80); playBeep(); delay(80); playBeep(); }
        #endif
        eMenuSelect = 1;
        needsRedraw = true;
        return;  // evita eMenuSelect=0 abaixo
    }
    eMenuSelect = 0;
  }
  // === PÁGINA 25: nível 0 (título) → avança para nível 1 (item selecionado) ===
  else if(eMenuPage == 25 && eMenuSelect == 0) {
    eMenuSelect = 1;
  }
  // === PÁGINA 24 (Pads Auxiliar): navegação por níveis ===
  else if(eMenuPage == 24 && !proMicroInPadSelection && eMenuSelect == 0) {
    // Entra direto na lista de pads (sem tela de título intermediária)
    proMicroInPadSelection = true;
    eMenuProMicro = 0;
  }
  else if(eMenuPage == 24 && proMicroInPadSelection && eMenuSelect == 0) {
    eMenuSelect = 1;
    eMenuProMicroParam = 0;
  }
  else if(eMenuPage == 24 && eMenuSelect == 1) {
    eMenuSelect = 2;
  }
  // === PADS A0-A15 (páginas 2-17): nível 0 → nível 1, nível 1 → nível 2 ===
  // nível 2 = máximo: NÃO avança (retorna sem mudar nada → sem bipe)
  else if(eMenuPage >= 2 && eMenuPage < 18) {
    if(eMenuSelect < 2) {
      eMenuSelect++;
    }
    // eMenuSelect já é 2: trava silenciosamente (não bipa nem pisca)
    else {
      return;  // sai sem tocar needsRedraw — processEncoder detecta que nada mudou
    }
  }
  // === SUBMENUS DE CONFIGURAÇÕES (páginas 18-23, 26-28): nível 0 → 1 → 2 ===
  else if((eMenuPage >= 18 && eMenuPage <= 23) || eMenuPage == 26 || eMenuPage == 27 || eMenuPage == 28) {
    if(eMenuSelect < 2) {
      eMenuSelect++;
    } else {
      return;  // já no máximo — trava silenciosamente
    }
  }
  // === OUTROS: avança se não estiver no máximo ===
  else {
    if(eMenuSelect < 2) {
      eMenuSelect++;
    } else {
      return;
    }
  }
  needsRedraw = true;
}

void encoderGoBack() {
  // *** MENU TIMING (35): clique curto sai e volta para tela do pad ***
  if(eMenuPage == MENU_TIMING_PAGE) {
    menuTimingAtivo = false;
    eMenuPage = 2;      // volta para o pad A0 (menu padrão)
    eMenuSelect = 0;
    lcd.clear();
    needsRedraw = true;
    return;
  }
  // *** MENU RÁPIDO (33): qualquer saída volta pro menu principal ***
  if(eMenuPage == MENU_RAPIDO_PAGE) {
    menuRapidoAtivo = false;
    eMenuPage = 2;
    eMenuSelect = 0;
    _sairMenuRapidoAtivo = false;
    lcd.clear();
    needsRedraw = true;
    return;
  }
  // *** MENU XCANCEL (34): clique curto sai e volta para Configurações ***
  if(eMenuPage == XCANCEL_PAGE) {
    if(eMenuSelect == 2) {
      eMenuSelect = 1;
    } else {
      xcancelResetPar0();  // *** Par 1/16 é somente leitura: restaura A1/A2/18 ao sair ***
      SaveXCancelEEPROM(); // *** Salva pares 2/16-16/16 na EEPROM ***
      xcancelMenuAtivo = false;
      eMenuPage = 25;
      eMenuConfig = 12;  // XCancel removido — Monitor HiHat é o último
      eMenuSelect = 1;
      lcd.clear();
    }
    needsRedraw = true;
    return;
  }
  // === PÁGINA 24 (Pads Auxiliar): volta nível por nível dentro do próprio menu ===
  if(eMenuPage == 24 && eMenuSelect == 2) {
    eMenuSelect = 1;
  }
  else if(eMenuPage == 24 && eMenuSelect == 1) {
    eMenuSelect = 0;
  }
  else if(eMenuPage == 24 && proMicroInPadSelection && eMenuSelect == 0) {
    // Nível mínimo → volta para Menu Configurações (página 25)
    proMicroInPadSelection = false;
    eMenuPage = 25;
    eMenuSelect = 1;
  }
  else if(eMenuPage == 24 && !proMicroInPadSelection && eMenuSelect == 0) {
    // Título fantasma → volta para Menu Configurações (página 25)
    eMenuPage = 25;
    eMenuSelect = 1;
  }
  // === OUTROS SUBMENUS (18-23, 26-31): eMenuSelect==0 volta para Configurações ===
  else if(((eMenuPage >= 18 && eMenuPage <= 23) || eMenuPage == 26 || eMenuPage == 27 || eMenuPage == 28 || eMenuPage == 29 || eMenuPage == 30 || eMenuPage == 31) && eMenuSelect == 0) {
    if(eMenuPage == 30) monitorVelocityAtivo = false;
    if(eMenuPage == 31) { monitorHiHatAtivo = false; eMenuConfig = 12; }
    eMenuPage = 25;
    eMenuSelect = 1;
    lcd.clear();
  }
  else if(eMenuPage == 25 && eMenuSelect == 0) {
    // Trava silenciosamente — não há nível anterior em Configurações
    return;
  }
  else {
    if(eMenuSelect > 0) eMenuSelect--;
    // eMenuSelect já é 0: não muda nada → caller não vai bipar
  }
  needsRedraw = true;
}

//==============================
//    FUNÇÕES AUXILIARES
//==============================

// *** FUNÇÕES PARA LER BOTÕES RESPEITANDO O MODO ***
byte readBtnA() {
  if(modoBotoes == 0) {
    // Normal: A = Pino 7 (INVERTIDO!)
    return digitalRead(7);
  } else {
    // Invertido: A = Pino 6 (INVERTIDO!)
    return digitalRead(6);
  }
}

byte readBtnB() {
  if(modoBotoes == 0) {
    // Normal: B = Pino 6 (INVERTIDO!)
    return digitalRead(6);
  } else {
    // Invertido: B = Pino 7 (INVERTIDO!)
    return digitalRead(7);
  }
}

void DrawDiagnostic(byte i, byte val)
{
  if(i>15) return;
  lcd.setCursor(i%16, i/16);
  lcd.print(val/16);
}

void CheckNamesIntegrity()
{
  bool needsCorrection = false;
  for(byte i = 0; i < 16; i++) {
    if(selected_names[i] >= NUM_AVAILABLE_NAMES || selected_names[i] == 255) {
      selected_names[i] = (i < NUM_AVAILABLE_NAMES) ? i : 0;
      needsCorrection = true;
    }
  }
  if(needsCorrection) {
    markNameChanged();
  }
}

void saveCustomName(byte pin, char* name) {
  if(pin >= 16) return;
  int address = 600 + (pin * 13);
  #if defined(__AVR__)
  for(int i = 0; i < 13; i++) {
    EEPROM.write(address + i, name[i]);
  }
  EEPROM.write(600 + 208 + pin, 1);
  #endif
  markNameChanged();
}

bool loadCustomName(byte pin, char* buffer) {
  if(pin >= 16) return false;
  #if defined(__AVR__)
  if(EEPROM.read(600 + 208 + pin) != 1) {
    return false;
  }
  int address = 600 + (pin * 13);
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

bool hasCustomName(byte pin) {
  if(pin >= 16) return false;
  #if defined(__AVR__)
  return EEPROM.read(600 + 208 + pin) == 1;
  #else
  return false;
  #endif
}

void clearCustomName(byte pin) {
  if(pin >= 16) return;
  #if defined(__AVR__)
  EEPROM.write(600 + 208 + pin, 0);
  #endif
  markNameChanged();
}

void MenuInt(int inter, bool sel)
{
  if(sel) {
    char cInter[]="<000>";
    if(inter>999) cInter[3]='e';
    else {
      cInter[3]+=inter%10;
      cInter[2]+=(inter%100)/10;
      cInter[1]+=(inter%1000)/100;
    }
    for(byte i=0; i<5; i++) lcd.print(cInter[i]);
  } else {
    char cInter[]=" 000 ";
    if(inter>999) cInter[4]='e';
    else {
      cInter[3]+=inter%10;
      cInter[2]+=(inter%100)/10;
      cInter[1]+=(inter%1000)/100;
    }
    for(byte i=0; i<5; i++) lcd.print(cInter[i]);
  }
}

void MenuInt(int inter, char A, char B)
{
  if(inter>999) B='e';
  char cInter[]=" 000 ";
  cInter[0]=A; cInter[4]=B;
  cInter[3]+=inter%10;
  cInter[2]+=(inter%100)/10;
  cInter[1]+=(inter%1000)/100;
  for(byte i=0; i<5; i++) {
    lcd.print(cInter[i]);
  }
}

void MenuString(const PROGMEM char *s, bool sel)
{
  char c;
  if(sel) lcd.print('<');
  while ((c = pgm_read_byte_near(s++)) != 0)
     lcd.print(c);
  if(sel) lcd.print('>');
}

void PrintCustomName(byte pin, bool sel)
{
  if(pin < 16) {
    if(hasCustomName(pin)) {
      char customName[13];
      if(loadCustomName(pin, customName)) {
        if(sel) lcd.print('<');
        lcd.print(customName);
        if(sel) lcd.print('>');
        return;
      }
    }
    if(selected_names[pin] >= NUM_AVAILABLE_NAMES || selected_names[pin] == 255) {
      selected_names[pin] = (pin < NUM_AVAILABLE_NAMES) ? pin : 0;
      markNameChanged();
    }
    byte safeIndex = selected_names[pin];
    if(safeIndex >= NUM_AVAILABLE_NAMES) {
      safeIndex = 0;
      selected_names[pin] = 0;
      markNameChanged();
    }
    char buffer[13];
    memset(buffer, 0, sizeof(buffer));
    strcpy_P(buffer, (char*)pgm_read_word(&(available_names[safeIndex])));
    
    if(sel) lcd.print('<');
    lcd.print(buffer);
    if(sel) lcd.print('>');
  }
}

// *** NOVA: Imprime nome direto do índice (para Pro Micro) ***
void printNameFromIndex(byte nameIdx, bool sel) {
  if(nameIdx >= NUM_AVAILABLE_NAMES) nameIdx = 0;
  
  char buffer[13];
  memset(buffer, 0, sizeof(buffer));
  strcpy_P(buffer, (char*)pgm_read_word(&(available_names[nameIdx])));
  
  if(sel) lcd.print('<');
  lcd.print(buffer);
  if(sel) lcd.print('>');
}

// *** NOVA: Imprime nome do pad Pro Micro (com suporte a nomes personalizados) ***
void PrintProMicroCustomName(byte padIndex, bool sel) {
  if(padIndex < NUM_PRO_MICRO_PADS) {
    // Usa GetProMicroName que já faz tudo
    char buffer[13];
    GetProMicroName(padIndex, buffer);
    
    if(sel) lcd.print('<');
    lcd.print(buffer);
    if(sel) lcd.print('>');
  }
}

// *** NOVA: Pega o nome do pad Pro Micro (personalizado ou da lista) para buffer ***
void GetProMicroName(byte padIndex, char* buffer) {
  if(padIndex >= NUM_PRO_MICRO_PADS) {
    buffer[0] = '\0';
    return;
  }
  
  // *** LEITURA DIRETA DA EEPROM - IGUAL AO MEGA ***
  #if defined(__AVR__)
  byte customFlag = EEPROM.read(875 + padIndex);
  
  // Se tem nome personalizado (flag == 1), lê DIRETO da EEPROM
  if(customFlag == 1) {
    int address = 732 + (padIndex * 13);
    for(int i = 0; i < 12; i++) {
      buffer[i] = EEPROM.read(address + i);
    }
    buffer[12] = '\0';
    
    // Verifica se tem conteúdo válido (aceita espaços!)
    // Só rejeita se TUDO for \0 ou 0xFF
    bool hasContent = false;
    for(int i = 0; i < 12; i++) {
      if(buffer[i] != '\0' && buffer[i] != 0xFF) {
        hasContent = true;
        break;
      }
    }
    
    if(hasContent) {
      return; // Retorna com nome personalizado
    }
  }
  #endif
  
  // Se não tem nome personalizado, usa available_names
  byte nameIdx = proMicroPads[padIndex].nameIndex;
  if(nameIdx >= NUM_AVAILABLE_NAMES) {
    nameIdx = padIndex < NUM_AVAILABLE_NAMES ? padIndex : 0;
  }
  
  memset(buffer, 0, 13);
  strcpy_P(buffer, (char*)pgm_read_word(&(available_names[nameIdx])));
}

//==============================
//    BACKLIGHT E EDIÇÃO DE NOME
//==============================

void updateBacklight() {
  unsigned long currentTime = millis();
  if(currentTime - lastBacklightUpdate < BACKLIGHT_UPDATE_INTERVAL) {
    return;
  }
  if (backlightMode == 1) {
    lcd.backlight();
    backlightState = true;
  } else {
    if (millis() - backlightTimer > BACKLIGHT_TIMEOUT) {
      lcd.noBacklight();
      backlightState = false;
    } else {
      lcd.backlight();
      backlightState = true;
    }
  }
  lastBacklightUpdate = currentTime;
}

void resetBacklightTimer() {
  backlightTimer = millis();
  if (backlightMode == 0) {
    lcd.backlight();
    backlightState = true;
  }
}

void initBacklight() {
  backlightTimer = millis();
  if (backlightMode == 1) {
    lcd.backlight();
    backlightState = true;
  } else {
    lcd.backlight();
    backlightState = true;
  }
}



//==============================
//    FUNÇÕES PADS AUXILIAR - SAVE/LOAD EEPROM
//    *** IMPORTANTE: Chamar LoadPadsAuxiliarEEPROM() na inicialização (i_setup.ino) ***
//    *** IMPORTANTE: Adicionar no e_eeprom.ino junto com LoadBacklightFromEEPROM() ***
//==============================
void SavePadsAuxiliarEEPROM() {
  #if defined(__AVR__)
  EEPROM.write(522, menuPadsAuxiliarAtivo ? 1 : 0);  // Endereço 522
  EEPROM.write(523, modoPadsAuxiliar);  // Endereço 523 - Modo (4 ou 6)
  #endif
}

void LoadPadsAuxiliarEEPROM() {
  #if defined(__AVR__)
  byte value = EEPROM.read(522);
  menuPadsAuxiliarAtivo = (value == 1);
  
  // Carrega o modo de pads (4 ou 6)
  byte modo = EEPROM.read(523);
  if(modo == 4 || modo == 6) {
    modoPadsAuxiliar = modo;
    maxPadsAuxiliar = (modo == 4) ? 3 : 5;
  } else {
    modoPadsAuxiliar = 6;  // Padrão
    maxPadsAuxiliar = 5;
  }
  #else
  menuPadsAuxiliarAtivo = false;
  modoPadsAuxiliar = 6;
  maxPadsAuxiliar = 5;
  #endif
}

//==============================
//    FUNÇÕES MODO BOTÕES - SAVE/LOAD EEPROM
//==============================
void SaveModoBotoesEEPROM() {
  #if defined(__AVR__)
  EEPROM.write(524, modoBotoes);  // Endereço 524 - Modo Botões (0=Normal, 1=Invertido)
  #endif
}

void LoadModoBotoesEEPROM() {
  #if defined(__AVR__)
  byte value = EEPROM.read(524);
  if(value == 0 || value == 1) {
    modoBotoes = value;
  } else {
    modoBotoes = 1;  // Padrão: Invertido (para sua placa)
  }
  #else
  modoBotoes = 1;  // Padrão: Invertido
  #endif
}



// *** RESET COMPLETO PARA VALORES PADRÃO DE FÁBRICA ***
void resetToFactoryDefaults() {
  // --- Pads principais: recarrega presets de fábrica ---
  for(int i = 0; i < 16; i++) {
    Pin[i].set(i);  // Carrega PAD_PRESETS[i] do PROGMEM
  }

  // --- Aplica notas Addictive Drummer 2 ---
  applyVSTPreset();

  // --- Nomes dos pads: sequência padrão 0..15 ---
  for(int i = 0; i < 16; i++) {
    selected_names[i] = (i < NUM_AVAILABLE_NAMES) ? i : 0;
    clearCustomName(i);
  }

  // --- Notas pads digitais Aux 1-7 — Addictive ---
  byte defDigital[7] = {61, 62,  7,  9, 27, 31, 90};
  for(int i = 0; i < 7; i++) digitalPadNotes[i] = defDigital[i];
  // *** CORRECAO BUG CRITICO: grava 0xA3 (LoadDigitalNotesEEPROM espera 0xA3, nao 0xA2) ***
  // Com 0xA2, o boot detectava "flag errada" e forcava defaults, sobrescrevendo o reset.
  EEPROM.write(537, 0xA3);

  // --- Notas Chokes ---
  byte defChoke[4] = {78, 80, 82, 63};
  for(int i = 0; i < 4; i++) chokeNotes[i] = defChoke[i];

  // --- Configurações gerais ---
  NSensor       = 2;
  GeneralXtalk  = 0;

  // --- Backlight ---
  backlightMode = 0;

  // --- Buzzer ---
  #if BUZZER
  buzzerEnabled = true;
  #endif

  // --- Rimshot / DualPad / Avançado ---
  ENABLE_NOTE_37_38_TO_40   = 1;
  ENABLE_VELOCITY_FILTER    = 1;
  ENABLE_RIMSHOT_38_TO_40   = 0;
  ENABLE_NOTE_101_TO_102    = 0;
  ENABLE_NOTE_103_TO_104    = 0;
  BLOCK_WINDOW_MS           = 60;
  VELOCITY_THRESHOLD_37_38  = 82;
  DETECTION_WINDOW_MS       = 12;
  MAX_NOTE_AGE_MS           = 30;
  CUSTOM_NOTE_37            = 42;
  CUSTOM_NOTE_40            = 37;
  CUSTOM_RIMSHOT_FORCE_NOTE = 37;

  // --- Rimshot modo: volta para Comp (0) ---
  rimSub = 0;
  #if defined(__AVR__)
  EEPROM.write(EEPROM_RIMSUB_MODE, 0);
  #endif

  // --- Ressincroniza ZoneDual com notas digitais e pads restaurados ---
  for(byte p = 0; p < ZONE_DUAL_PAIRS; p++) {
    zoneDual_NoteAux[p]   = digitalPadNotes[p];
    zoneDual_NoteBlock[p] = Pin[zoneDual_PadIdx[p]].Note;
  }

  // --- Saídas MIDI ---
  MIDI_USB_ENABLED = 1;
  MIDI_TX1_ENABLED = 1;

  // --- HiHat ---
  HHNoteSensor[0]=20; HHNoteSensor[1]=50; HHNoteSensor[2]=80; HHNoteSensor[3]=100;
  HHThresoldSensor[0]=48; HHThresoldSensor[1]=36; HHThresoldSensor[2]=24; HHThresoldSensor[3]=12;
  HHFootNoteSensor[0]=0; HHFootNoteSensor[1]=19;
  HHFootThresoldSensor[0]=127; HHFootThresoldSensor[1]=127;

  // *** CORRECAO: resetar VelMinimo, InvertSensor e XCancel ***
  // Antes esses valores eram ignorados no reset — voltavam ao que estava na EEPROM no proximo boot.

  // --- Velocity Minimo por pad: tudo zero (sem piso) ---
  for(int i = 0; i < 16; i++) Pin[i].VelMinimo = 0;

  // --- Invert Sensor por pad: A0=Invertido (HHC/TCRT5000), demais Normal ---
  for(int i = 0; i < 16; i++) Pin[i].InvertSensor = 0;
  Pin[0].InvertSensor = 0;

  // --- XCancel: restaura valores padrao de fabrica dos pares editaveis (i>0) ---
  // Os valores padrao sao os mesmos do c_pin.ino (xpairRam inicial)
  for(byte i = 1; i < XPAIR_COUNT; i++) {
    xpairRam[i].source   = 255;  // desativado
    xpairRam[i].target   = 255;
    xpairRam[i].windowMs = 30;
    xpairRam[i].ghostVel = 20;
  }

  // --- Salva tudo na EEPROM ---
  forceImmediateSaveToEEPROM();

  #if BUZZER
  if(buzzerEnabled) { playBeep(); delay(80); playBeep(); }
  #endif
}

// *** NOVO: Save/Load notas Pads Digitais (Aux1-7) - EEPROM 526-532 ***
void SaveDigitalNotesEEPROM() {
  #if defined(__AVR__)
  for(byte i = 0; i < 7; i++) {
    EEPROM.write(526 + i, digitalPadNotes[i]);
  }
  #endif
}

void LoadDigitalNotesEEPROM() {
  #if defined(__AVR__)
  // Valores padrão ZoneDual — Aux1-7
  byte defaults[7] = {61, 62,  7,  9, 27, 31, 90};

  // *** FLAG DE VERSÃO (endereço 537) ***
  // Se o valor na EEPROM não for 0xA2, significa que os defaults
  // ainda não foram gravados (EEPROM tem valores antigos).
  // Força a gravação dos novos defaults uma única vez e marca como feito.
  const byte VERSION_FLAG = 0xA3;  // Incrementado: força gravação dos novos defaults {53,59,7,9,27,31,90}
  if(EEPROM.read(537) != VERSION_FLAG) {
    for(byte i = 0; i < 7; i++) {
      digitalPadNotes[i] = defaults[i];
      EEPROM.write(526 + i, defaults[i]);
    }
    EEPROM.write(537, VERSION_FLAG);
    return;
  }

  // EEPROM já tem os novos defaults — carrega normalmente
  for(byte i = 0; i < 7; i++) {
    byte val = EEPROM.read(526 + i);
    if(val <= 127) {
      digitalPadNotes[i] = val;
    } else {
      digitalPadNotes[i] = defaults[i];
    }
  }
  #endif
}

// *** NOVO: Save/Load notas Choke - EEPROM 533-536 ***
void SaveChokeNotesEEPROM() {
  #if defined(__AVR__)
  for(byte i = 0; i < 4; i++) {
    EEPROM.write(533 + i, chokeNotes[i]);
  }
  #endif
}

void LoadChokeNotesEEPROM() {
  #if defined(__AVR__)
  byte defaults[4] = {78, 80, 82, 63};  // Crash1, Crash2, Crash3, Ride
  for(byte i = 0; i < 4; i++) {
    byte val = EEPROM.read(533 + i);
    if(val <= 127) {
      chokeNotes[i] = val;
    } else {
      chokeNotes[i] = defaults[i];
    }
  }
  #endif
}

// *** NOVO: Save/Load Filtro Hold Choke - EEPROM 585-589 ***
void drawNameEdit() {
  unsigned long currentTime = millis();
  if(currentTime - cursorBlinkTimer >= 500) {
    cursorVisible = !cursorVisible;
    cursorBlinkTimer = currentTime;
    needsRedraw = true;
  }
  if(!needsRedraw) return;
  lcd.clear();
  lcd.setCursor(0, 0);
  for(int i = 0; i < 12; i++) {
    if(i == editPosition && cursorVisible) {
      lcd.print(EDIT_CHARS[currentCharIndex]);
    } else if(i == editPosition && !cursorVisible) {
      lcd.print(' ');
    } else {
      lcd.print(editingName[i]);
    }
  }
  lcd.setCursor(0, 1);
  MenuString(S_NOVO_NOME, false);
  needsRedraw = false;
}



//==============================
//    CONFIRMAÇÃO E EDIÇÃO DE NOME - PRO MICRO
//==============================

void startProMicroConfirmation(byte padIndex) {
  if(padIndex >= NUM_PRO_MICRO_PADS) return;
  proMicroConfirmationMode = true;
  editingProMicroPad = padIndex;
  confirmationStartTime = millis();
  resetProMicroConfirmButtons = true;
  needsRedraw = true;
}

void exitProMicroConfirmation(bool confirmed) {
  if(!proMicroConfirmationMode) return;
  if(confirmed) {
    clearProMicroCustomName(editingProMicroPad);
    proMicroPads[editingProMicroPad].nameIndex = 15; // *** volta para PAD EFE 1 ao limpar ***
    saveProMicroPadToEEPROM(editingProMicroPad);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("LIMPANDO");
    delay(1000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("LIMPO!");
    delay(1000);
    markNameChanged();
  } else {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("CANCELADO!");
    delay(1000);
  }
  proMicroConfirmationMode = false;
  needsRedraw = true;
}

// *** MENU RESET PADRÃO ***
// FLUXO RESET - 2 FASES:
//  FASE 0: "Resetar Modulo" / "A=Sim B=Nao"
//          SIM → FASE 1  |  NAO → Cancelado → Config
//  FASE 1: "Cancelar Reset" / "A  B     10s"
//          Qualquer botão → "Cancelado Reset!" → Config
//          Sem apertar 10s → executa reset
void ResetMenu() {
  byte btnA = readBtnA();
  byte btnB = readBtnB();

  static byte  resetFase             = 0;
  static bool  resetBotoesForamSoltos = false;
  static bool  resetTelaJaDesenhada  = false;
  static unsigned long resetContagemInicio = 0;
  static byte  resetUltimoSegundo    = 255;

  // --------------------------------------------------
  // FASE 0 — primeira confirmação
  // --------------------------------------------------
  if(resetFase == 0) {
    if(!resetTelaJaDesenhada) {
      lcd.clear();
      lcd.setCursor(0,0);
      MenuString(S_RESET_MENU, false);
      lcd.setCursor(0,1);
      if(modoBotoes == 1) MenuString(S_RESET_CONFIRM_INVERTIDO, false); 
      else                MenuString(S_RESET_CONFIRM_NORMAL,    false); 
      resetTelaJaDesenhada = true;
    }
    if(!resetBotoesForamSoltos) {
      if(btnA == HIGH && btnB == HIGH) resetBotoesForamSoltos = true;
      return;
    }
    // modoBotoes==1: B=SIM, A=NAO  |  modoBotoes==0: A=SIM, B=NAO
    bool btnSim = (modoBotoes == 1) ? (btnB == LOW) : (btnA == LOW);
    bool btnNao = (modoBotoes == 1) ? (btnA == LOW) : (btnB == LOW);
    if(btnSim) {
      delay(200);
      resetFase             = 1;
      resetBotoesForamSoltos = false;
      resetTelaJaDesenhada  = false;
      resetContagemInicio   = millis();
      resetUltimoSegundo    = 255;
      return;
    }
    if(btnNao) {
      delay(200);
      resetFase             = 0;
      resetBotoesForamSoltos = false;
      resetTelaJaDesenhada  = false;
      lcd.clear(); lcd.setCursor(0,0); lcd.print(F("Cancelado"));
      delay(1500);
      eMenuPage = 25; eMenuSelect = 1; needsRedraw = true;
    }
    return;
  }

  // --------------------------------------------------
  // FASE 1 — contagem regressiva 10s
  // --------------------------------------------------
  if(resetFase == 1) {
    // Aguarda soltar botões antes de aceitar cancelamento
    if(!resetBotoesForamSoltos) {
      if(btnA == HIGH && btnB == HIGH) {
        resetBotoesForamSoltos = true;
        resetTelaJaDesenhada   = false;
      }
      return;
    }
    unsigned long decorrido = millis() - resetContagemInicio;
    byte seg = (decorrido >= 10000UL) ? 0 : (byte)((10000UL - decorrido) / 1000UL + 1);
    if(seg > 10) seg = 10;
    // Redesenha só quando segundo muda
    if(!resetTelaJaDesenhada || seg != resetUltimoSegundo) {
      lcd.clear();
      lcd.setCursor(0,0); lcd.print(F("Cancelar Reset"));
      lcd.setCursor(0,1); lcd.print(F("A ou B =SIM   "));
      if(seg < 10) lcd.print(' ');
      lcd.print(seg); lcd.print(F("s"));
      resetTelaJaDesenhada = true;
      resetUltimoSegundo   = seg;
    }
    // Qualquer botão = cancela
    if(btnA == LOW || btnB == LOW) {
      delay(200);
      resetFase             = 0;
      resetBotoesForamSoltos = false;
      resetTelaJaDesenhada  = false;
      resetUltimoSegundo    = 255;
      lcd.clear(); lcd.setCursor(0,0); lcd.print(F("Cancelado"));
      lcd.setCursor(0,1); lcd.print(F("Reset!"));
      delay(1500);
      eMenuPage = 25; eMenuSelect = 1; needsRedraw = true;
      return;
    }
    // Passou 10s → executa
    if(decorrido >= 10000UL) {
      resetFase             = 0;
      resetBotoesForamSoltos = false;
      resetTelaJaDesenhada  = false;
      resetUltimoSegundo    = 255;
      lcd.clear(); lcd.setCursor(0,0); lcd.print(F("Resetando"));
      lcd.setCursor(0,1); lcd.print(F("Valores..."));
      // *** CORRECAO: descarrega fila pendente ANTES de resetar ***
      // Garante que nenhum job antigo sobrescreva os defaults apos o reset
      _flushQueueSync();
      // Zera fila para evitar qualquer residuo
      queueHead  = 0;
      queueTail  = 0;
      queueCount = 0;
      pendingChanges = false;
      // Executa reset — grava tudo sincronamente via forceImmediateSaveToEEPROM()
      resetToFactoryDefaults();
      // Garante que nao ha nada pendente apos o reset
      _flushQueueSync();
      queueHead  = 0;
      queueTail  = 0;
      queueCount = 0;
      pendingChanges = false;
      delay(100);
      lcd.clear(); lcd.setCursor(0,0); lcd.print(F("Resetado!"));
      lcd.setCursor(0,1); lcd.print(F("Valores Padrao"));
      delay(2000);
      eMenuPage = 25; eMenuSelect = 1; needsRedraw = true;
    }
  }
}

void ProMicroConfirmationMenu() {
  byte btnA = readBtnA();
  byte btnB = readBtnB();
  unsigned long now = millis();

  // *** IGUAL AO PADS AUXILIAR: espera soltar A+B antes de aceitar qualquer comando ***
  static bool botoesForamSoltos = false;
  static bool telaJaDesenhada = false;
  if(resetProMicroConfirmButtons) { botoesForamSoltos = false; telaJaDesenhada = false; resetProMicroConfirmButtons = false; }

  if(!botoesForamSoltos) {
    if(btnA == HIGH && btnB == HIGH) {
      botoesForamSoltos = true;
      telaJaDesenhada = false;
    }
    if(!telaJaDesenhada) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Limpa nome?");
      lcd.setCursor(0, 1);
      if(modoBotoes == 1) lcd.print("A=SIM  B=NAO");
      else lcd.print("A=NAO  B=SIM");
      telaJaDesenhada = true;
    }
    return;
  }

  if(!telaJaDesenhada) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Limpa nome?");
    lcd.setCursor(0, 1);
    if(modoBotoes == 1) lcd.print("A=NAO  B=Sim");
    else lcd.print("A=SIM  B=Nao");
    telaJaDesenhada = true;
  }

  if(btnA == LOW || btnB == LOW) resetBacklightTimer();

  if(now - confirmationStartTime >= CONFIRMATION_TIMEOUT) {
    telaJaDesenhada = false; botoesForamSoltos = false;
    exitProMicroConfirmation(false);
    return;
  }

  #if ENCODER
  static bool lastEncoderButtonState = false;
  static unsigned long encoderButtonPressTime = 0;
  bool currentEncoderButtonState = readEncoderButton();
  if(currentEncoderButtonState && !lastEncoderButtonState) {
    encoderButtonPressTime = millis(); resetBacklightTimer();
  } else if(!currentEncoderButtonState && lastEncoderButtonState) {
    unsigned long pressDuration = millis() - encoderButtonPressTime;
    telaJaDesenhada = false; botoesForamSoltos = false;
    if(pressDuration >= 1000) exitProMicroConfirmation(true);
    else exitProMicroConfirmation(false);
    lastEncoderButtonState = currentEncoderButtonState;
    return;
  }
  lastEncoderButtonState = currentEncoderButtonState;
  #endif

  bool btnLimpaPressionado   = (modoBotoes == 1) ? (btnB == LOW) : (btnA == LOW);
  bool btnCancelaPressionado = (modoBotoes == 1) ? (btnA == LOW) : (btnB == LOW);

  static bool btn_limpa_pressed = false;
  static bool btn_cancela_pressed = false;

  if(btnLimpaPressionado && !btn_limpa_pressed) {
    btn_limpa_pressed = true;
    delay(50); telaJaDesenhada = false; botoesForamSoltos = false;
    exitProMicroConfirmation(true); return;
  } else if(!btnLimpaPressionado && btn_limpa_pressed) { btn_limpa_pressed = false; }

  if(btnCancelaPressionado && !btn_cancela_pressed) {
    btn_cancela_pressed = true;
    delay(50); telaJaDesenhada = false; botoesForamSoltos = false;
    exitProMicroConfirmation(false); return;
  } else if(!btnCancelaPressionado && btn_cancela_pressed) { btn_cancela_pressed = false; }

  updateBacklight();
}

void startProMicroNameEdit(byte padIndex) {
  if(padIndex >= NUM_PRO_MICRO_PADS) return;
  proMicroNameEditMode = true;
  editingProMicroPad = padIndex;
  editPosition = 0;
  currentCharIndex = 0;
  cursorBlinkTimer = millis();
  cursorVisible = true;
  memset(editingName, ' ', 12);
  editingName[12] = '\0';
  resetProMicroEditButtons = true;
  needsRedraw = true;
}

void exitProMicroNameEdit(bool save) {
  if(!proMicroNameEditMode) return;
  if(save) {
    lcd.clear();
    lcd.setCursor(0, 0);
    MenuString(S_SALVANDO, false);
    delay(1000);
    
    editingName[editPosition] = EDIT_CHARS[currentCharIndex];
    
    // Salva o nome
    saveProMicroCustomName_local(editingProMicroPad, editingName);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    MenuString(S_SALVO_NOME, false);
    delay(1500);
    
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    MenuString(S_CANCELANDO, false);
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    MenuString(S_CANCELADO, false);
    delay(1500);
  }
  proMicroNameEditMode = false;
  btnA_State = 0;
  btnB_State = 0;
  btnA_Time = 0;
  btnB_Time = 0;
  eMenuSelect = 2;
  eMenuProMicroParam = 11;
  needsRedraw = true;
  
  // Força redesenho completo
  lcd.clear();
  delay(100);
  lastLCDUpdate = 0;
  
  Draw();
}

void ProMicroNameEditMenu() {
  byte btnA = readBtnA();
  byte btnB = readBtnB();
  unsigned long now = millis();

  // *** IGUAL AO PADS AUXILIAR: espera soltar A+B antes de aceitar qualquer comando ***
  static bool botoesForamSoltos = false;
  if(resetProMicroEditButtons) { botoesForamSoltos = false; resetProMicroEditButtons = false; }
  if(!botoesForamSoltos) {
    if(btnA == HIGH && btnB == HIGH) {
      botoesForamSoltos = true;
      btnA_State = 0; btnB_State = 0;
      btnA_Time  = 0; btnB_Time  = 0;
    }
    drawNameEdit();
    return;
  }

  #if ENCODER
  int encoderDirection = readEncoder();
  if(encoderDirection != 0) {
    if(encoderDirection > 0) {
      currentCharIndex = (currentCharIndex + 1) % EDIT_CHARS_COUNT;
    } else {
      currentCharIndex = (currentCharIndex - 1 + EDIT_CHARS_COUNT) % EDIT_CHARS_COUNT;
    }
    #if BUZZER
    if(buzzerEnabled) {
      playBeep();
    }
    #endif
    needsRedraw = true;
    resetBacklightTimer();
  }
  
  static bool lastEncoderButtonState = false;
  static unsigned long encoderButtonPressTime = 0;
  bool currentEncoderButtonState = readEncoderButton();
  
  if(currentEncoderButtonState && !lastEncoderButtonState) {
    encoderButtonPressTime = millis();
    resetBacklightTimer();
  }
  else if(!currentEncoderButtonState && lastEncoderButtonState) {
    unsigned long pressDuration = millis() - encoderButtonPressTime;
    if(pressDuration >= 2000) {
      exitProMicroNameEdit(false);
      lastEncoderButtonState = currentEncoderButtonState;
      return;
    }
    else if(pressDuration >= 500) {
      editingName[editPosition] = EDIT_CHARS[currentCharIndex];
      exitProMicroNameEdit(true);
      lastEncoderButtonState = currentEncoderButtonState;
      return;
    }
    else {
      editingName[editPosition] = EDIT_CHARS[currentCharIndex];
      editPosition++;
      if(editPosition > 11) editPosition = 0;
      if(editingName[editPosition] != '\0' && editingName[editPosition] != ' ') {
        for(int i = 0; i < EDIT_CHARS_COUNT; i++) {
          if(EDIT_CHARS[i] == editingName[editPosition]) {
            currentCharIndex = i;
            break;
          }
        }
      } else {
        currentCharIndex = 0;
      }
      needsRedraw = true;
    }
  }
  lastEncoderButtonState = currentEncoderButtonState;
  #endif
  
  if (btnA == LOW || btnB == LOW) {
    resetBacklightTimer();
  }
  
  static unsigned long bothPressedStart = 0;
  if (btnB == LOW && btnA == LOW) {
    if(bothPressedStart == 0) {
      bothPressedStart = now;
    } else if(now - bothPressedStart >= 500) {
      exitProMicroNameEdit(false);
      bothPressedStart = 0;
      return;
    }
  } else {
    bothPressedStart = 0;
  }
  
  if (btnA_State == 0) {
    if (btnA == LOW && (now - lastButtonPress > DEBOUNCEDELAY)) {
      btnA_State = 1;
      btnA_Time = now;
      lastButtonPress = now;
    }
  } else if (btnA_State == 1) {
    if (btnA == HIGH) {
      btnA_State = 2;
    } else if ((btnA == LOW) && (now > btnA_Time + 700)) {
      editingName[editPosition] = EDIT_CHARS[currentCharIndex];
      editPosition++;
      if(editPosition > 11) editPosition = 0;
      if(editingName[editPosition] != '\0' && editingName[editPosition] != ' ') {
        for(int i = 0; i < EDIT_CHARS_COUNT; i++) {
          if(EDIT_CHARS[i] == editingName[editPosition]) {
            currentCharIndex = i;
            break;
          }
        }
      } else {
        currentCharIndex = 0;
      }
      needsRedraw = true;
      btnA_State = 3;
    }
  } else if (btnA_State == 2) {
    if (now > btnA_Time + DEBOUNCEDELAY) {
      currentCharIndex = (currentCharIndex + 1) % EDIT_CHARS_COUNT;
      #if BUZZER
      if(buzzerEnabled) {
        playBeep();
      }
      #endif
      needsRedraw = true;
    }
    btnA_State = 0;
  } else if (btnA_State == 3) {
    if (btnA == HIGH) {
      btnA_State = 0;
    }
  }
  
  if (btnB_State == 0) {
    if (btnB == LOW && (now - lastButtonPress > DEBOUNCEDELAY)) {
      btnB_State = 1;
      btnB_Time = now;
      lastButtonPress = now;
    }
  } else if (btnB_State == 1) {
    if (btnB == HIGH) {
      btnB_State = 2;
    } else if ((btnB == LOW) && (now > btnB_Time + 2000)) {
      editingName[editPosition] = EDIT_CHARS[currentCharIndex];
      exitProMicroNameEdit(true);
      btnB_State = 3;
      return;
    }
  } else if (btnB_State == 2) {
    if (now > btnB_Time + DEBOUNCEDELAY) {
      currentCharIndex = (currentCharIndex - 1 + EDIT_CHARS_COUNT) % EDIT_CHARS_COUNT;
      #if BUZZER
      if(buzzerEnabled) {
        playBeep();
      }
      #endif
      needsRedraw = true;
    }
    btnB_State = 0;
  } else if (btnB_State == 3) {
    if (btnB == HIGH) {
      btnB_State = 0;
    }
  }
  
  drawNameEdit();
  updateBacklight();
}

//==============================
//    EDIÇÃO E CONFIRMAÇÃO DE NOME - PRESETS DE BACKUP
//    Mesmo sistema dos pads principais:
//    - A curto  = próximo char
//    - A longo  = avança posição
//    - B curto  = char anterior
//    - B longo  = SALVA e sai
//    - A+B juntos = CANCELA
//==============================

void startPresetNameEdit(byte presetNum) {
  if(presetNum < 1 || presetNum > 3) return;
  presetNameEditMode  = true;
  editingPresetNum    = presetNum;
  editPosition        = 0;
  currentCharIndex    = 0;
  cursorBlinkTimer    = millis();
  cursorVisible       = true;
  memset(editingName, ' ', 12);
  editingName[12]     = '\0';
  resetPresetEditButtons = true;
  needsRedraw         = true;
}

void exitPresetNameEdit(bool save) {
  if(!presetNameEditMode) return;
  if(save) {
    lcd.clear();
    lcd.setCursor(0, 0);
    MenuString(S_SALVANDO, false);
    delay(1000);
    editingName[editPosition] = EDIT_CHARS[currentCharIndex];
    savePresetCustomName(editingPresetNum, editingName);
    lcd.clear();
    lcd.setCursor(0, 0);
    MenuString(S_SALVO_NOME, false);
    delay(1500);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    MenuString(S_CANCELANDO, false);
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    MenuString(S_CANCELADO, false);
    delay(1500);
  }
  presetNameEditMode = false;
  btnA_State = 0;
  btnB_State = 0;
  btnA_Time  = 0;
  btnB_Time  = 0;
  eMenuPage   = 20;
  eMenuSelect = 1;
  needsRedraw = true;
  lcd.clear();
  delay(50);
  Draw();
}

void PresetNameEditMenu() {
  byte btnA = readBtnA();
  byte btnB = readBtnB();
  unsigned long now = millis();

  // *** IGUAL AO PADS AUXILIAR: espera soltar A+B antes de aceitar qualquer comando ***
  static bool botoesForamSoltos = false;
  if(resetPresetEditButtons) { botoesForamSoltos = false; resetPresetEditButtons = false; }
  if(!botoesForamSoltos) {
    if(btnA == HIGH && btnB == HIGH) {
      botoesForamSoltos = true;
      btnA_State = 0; btnB_State = 0;
      btnA_Time  = 0; btnB_Time  = 0;
    }
    drawNameEdit();
    return;
  }

  #if ENCODER
  int encoderDirection = readEncoder();
  if(encoderDirection != 0) {
    if(encoderDirection > 0)
      currentCharIndex = (currentCharIndex + 1) % EDIT_CHARS_COUNT;
    else
      currentCharIndex = (currentCharIndex - 1 + EDIT_CHARS_COUNT) % EDIT_CHARS_COUNT;
    #if BUZZER
    if(buzzerEnabled) playBeep();
    #endif
    needsRedraw = true;
    resetBacklightTimer();
  }

  static bool lastEncBtn = false;
  static unsigned long encBtnTime = 0;
  bool curEncBtn = readEncoderButton();
  if(curEncBtn && !lastEncBtn) {
    encBtnTime = millis();
    resetBacklightTimer();
  } else if(!curEncBtn && lastEncBtn) {
    unsigned long dur = millis() - encBtnTime;
    if(dur >= 2000) {
      exitPresetNameEdit(false);
      lastEncBtn = curEncBtn;
      return;
    } else if(dur >= 500) {
      editingName[editPosition] = EDIT_CHARS[currentCharIndex];
      exitPresetNameEdit(true);
      lastEncBtn = curEncBtn;
      return;
    } else {
      editingName[editPosition] = EDIT_CHARS[currentCharIndex];
      editPosition++;
      if(editPosition > 11) editPosition = 0;
      currentCharIndex = 0;
      needsRedraw = true;
    }
  }
  lastEncBtn = curEncBtn;
  #endif

  if(btnA == LOW || btnB == LOW) resetBacklightTimer();

  // A+B juntos = cancela
  static unsigned long bothStart = 0;
  if(btnB == LOW && btnA == LOW) {
    if(bothStart == 0) bothStart = now;
    else if(now - bothStart >= 500) {
      exitPresetNameEdit(false);
      bothStart = 0;
      return;
    }
  } else { bothStart = 0; }

  // Botão A
  if(btnA_State == 0) {
    if(btnA == LOW && (now - lastButtonPress > DEBOUNCEDELAY)) {
      btnA_State = 1; btnA_Time = now; lastButtonPress = now;
    }
  } else if(btnA_State == 1) {
    if(btnA == HIGH) {
      btnA_State = 2;
    } else if(now > btnA_Time + 700) {
      // longo: avança posição
      editingName[editPosition] = EDIT_CHARS[currentCharIndex];
      editPosition++;
      if(editPosition > 11) editPosition = 0;
      currentCharIndex = 0;
      needsRedraw = true;
      btnA_State = 3;
    }
  } else if(btnA_State == 2) {
    if(now > btnA_Time + DEBOUNCEDELAY) {
      currentCharIndex = (currentCharIndex + 1) % EDIT_CHARS_COUNT;
      #if BUZZER
      if(buzzerEnabled) playBeep();
      #endif
      needsRedraw = true;
    }
    btnA_State = 0;
  } else if(btnA_State == 3) {
    if(btnA == HIGH) btnA_State = 0;
  }

  // Botão B
  if(btnB_State == 0) {
    if(btnB == LOW && (now - lastButtonPress > DEBOUNCEDELAY)) {
      btnB_State = 1; btnB_Time = now; lastButtonPress = now;
    }
  } else if(btnB_State == 1) {
    if(btnB == HIGH) {
      btnB_State = 2;
    } else if(now > btnB_Time + 2000) {
      // longo: salva
      editingName[editPosition] = EDIT_CHARS[currentCharIndex];
      exitPresetNameEdit(true);
      btnB_State = 3;
      return;
    }
  } else if(btnB_State == 2) {
    if(now > btnB_Time + DEBOUNCEDELAY) {
      currentCharIndex = (currentCharIndex - 1 + EDIT_CHARS_COUNT) % EDIT_CHARS_COUNT;
      #if BUZZER
      if(buzzerEnabled) playBeep();
      #endif
      needsRedraw = true;
    }
    btnB_State = 0;
  } else if(btnB_State == 3) {
    if(btnB == HIGH) btnB_State = 0;
  }

  drawNameEdit();
  updateBacklight();
}

// Confirmação de limpeza de nome do preset
void exitPresetConfirmation(bool confirmed) {
  if(!presetConfirmMode) return;
  if(confirmed) {
    clearPresetCustomName(confirmingPresetNum);
    lcd.clear(); lcd.setCursor(0,0); lcd.print("LIMPANDO"); delay(1000);
    lcd.clear(); lcd.setCursor(0,0); lcd.print("LIMPO!");   delay(1000);
  } else {
    lcd.clear(); lcd.setCursor(0,0); lcd.print("CANCELADO!"); delay(1000);
  }
  presetConfirmMode = false;
  needsRedraw = true;
}

// Confirmação de limpeza: "Limpa nome?" / "A=Sim B=Nao" (ou invertido)
// Apenas UMA confirmação, botões respeitam modoBotoes.
// modoBotoes==1 (Invertido): A=SIM (limpa), B=NAO (cancela)
// modoBotoes==0 (Normal)   : B=SIM (limpa), A=NAO (cancela)
void PresetConfirmationMenu() {
  byte btnA = readBtnA();
  byte btnB = readBtnB();
  unsigned long now = millis();

  static bool telaDesenhada = false;
  static bool botoesForamSoltos = false;
  if(resetPresetConfirmButtons) { botoesForamSoltos = false; telaDesenhada = false; resetPresetConfirmButtons = false; }

  if(!telaDesenhada) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Limpa nome?");
    lcd.setCursor(0, 1);
    if(modoBotoes == 1) lcd.print("A=NAO  B=SIM");  // Invertido
    else                lcd.print("A=SIM  B=NAO");  // Normal
    telaDesenhada = true;
  }

  // Aguarda soltar antes de aceitar input
  if(!botoesForamSoltos) {
    if(btnA == HIGH && btnB == HIGH) botoesForamSoltos = true;
    return;
  }

  // Timeout 5s → cancela
  if(now - confirmationStartTime >= CONFIRMATION_TIMEOUT) {
    telaDesenhada    = false;
    botoesForamSoltos = false;
    exitPresetConfirmation(false);
    return;
  }

  // modoBotoes==1: A=SIM, B=NAO
  // modoBotoes==0: B=SIM, A=NAO
  bool btnSim = (modoBotoes == 1) ? (btnA == LOW) : (btnB == LOW);
  bool btnNao = (modoBotoes == 1) ? (btnB == LOW) : (btnA == LOW);

  if(btnSim) {
    delay(50);
    telaDesenhada    = false;
    botoesForamSoltos = false;
    exitPresetConfirmation(true);   // LIMPA
    return;
  }
  if(btnNao) {
    delay(50);
    telaDesenhada    = false;
    botoesForamSoltos = false;
    exitPresetConfirmation(false);  // CANCELA
    return;
  }

  updateBacklight();
}

#if ENCODER
void processEncoder() {
  unsigned long currentTime = millis();

  if(currentTime - lastEncoderMove < ENCODER_DEBOUNCE) {
    return;
  }
  
  int encoderDirection = readEncoder();
  
  if(encoderDirection != 0) {
    lastEncoderMove = currentTime;
    
    #if BUZZER
    if(buzzerEnabled && eMenuSelect != 2) {
      playBeep();
    }
    #endif
    resetBacklightTimer();
    
    if(encoderDirection > 0) {
      Up();
    } else {
      Down();
    }
    needsRedraw = true;
    delay(50);
  }
  
  static bool lastEncoderButtonState = false;
  static unsigned long encoderButtonPressTime = 0;
  static bool holdMediumFired = false;  // garante que o hold médio age só uma vez
  bool currentEncoderButtonState = readEncoderButton();

  // *** BORDA DE DESCIDA: botão pressionado ***
  if(currentEncoderButtonState && !lastEncoderButtonState) {
    encoderButtonPressTime = millis();
    holdMediumFired = false;
    menuTimingHoldFired = false;  // *** RESET: nova pressão reinicia contagem 20s ***
    resetBacklightTimer();
  }

  // *** ENQUANTO SEGURA: age assim que atinge o tempo, sem esperar soltar ***
  if(currentEncoderButtonState) {
    unsigned long held = millis() - encoderButtonPressTime;

    // *** Hold muito longo (20s): padShortcutMode ativo → abre menu Timing (página 35) ***
    // Fluxo: segura 3s → aparece "Aguardando / Bata no PAD" → continua segurando até 20s
    //        → abre menu com R>37+38-Ms / R>NotAgeMs / R>BlkWinMs
    if(held >= MENU_TIMING_HOLD_MS && padShortcutMode && !menuTimingAtivo && !menuTimingHoldFired) {
      menuTimingHoldFired = true;
      // Sai do modo atalho PAD pois vamos entrar no menu timing
      padShortcutMode      = false;
      padShortcutActivated = false;
      // Ativa menu Timing
      menuTimingAtivo  = true;
      menuTimingParam  = 0;
      eMenuPage        = MENU_TIMING_PAGE;
      eMenuSelect      = 1;
      lcd.clear();
      needsRedraw = true;
      #if BUZZER
      if(buzzerEnabled) {
        // 4 bips rápidos = menu timing ativado
        playBeep(); delay(60); playBeep(); delay(60);
        playBeep(); delay(60); playBeep();
      }
      #endif
    }
    // Hold longo (3s): ativa atalho PAD (3 bips) — só se timing ainda não disparou
    else if(held >= RESETDELAY && !padShortcutActivated && !menuTimingHoldFired) {
      if(!proMicroNameEditMode
         && !proMicroConfirmationMode
         && !presetNameEditMode
         && !presetConfirmMode
         && !confirmacaoPadsAuxiliar
         && !selecaoModoPads
         && eMenuPage != 29) {
        activatePadShortcut();  // bipa 3x dentro da função
      }
    }
    // Hold médio (500ms): entra no nível — age AO ATINGIR, sem esperar soltar
    // Bipa SOMENTE se realmente avançou de nível (sem bipe/piscar quando travado no máximo)
    else if(held >= HOLDDELAY && !holdMediumFired && !padShortcutActivated && !menuTimingHoldFired) {
      holdMediumFired = true;
      byte selectAntes = eMenuSelect;
      byte pageAntes   = eMenuPage;
      encoderEnterLevel();
      // Bipa só se algo mudou (avançou de nível ou mudou de página)
      bool mudou = (eMenuSelect != selectAntes || eMenuPage != pageAntes);
      #if BUZZER
      if(buzzerEnabled && mudou) { playBeep(); }
      #endif
    }
  }

  // *** BORDA DE SUBIDA: botão solto ***
  if(!currentEncoderButtonState && lastEncoderButtonState) {
    unsigned long pressDuration = millis() - encoderButtonPressTime;

    if(menuTimingHoldFired) {
      // Hold 20s já executou — ao soltar não faz nada
      menuTimingHoldFired = false;
    }
    else if(padShortcutActivated) {
      // Atalho ativado enquanto segurava — ao soltar só reseta a flag
      padShortcutActivated = false;
    }
    else if(holdMediumFired) {
      // Hold médio já executou enquanto segurava — ao soltar não faz nada
    }
    else if(pressDuration >= 100) {
      // *** CLIQUE CURTO: volta de nível — bipa só se algo mudou ***
      byte selectAntes = eMenuSelect;
      byte pageAntes   = eMenuPage;
      encoderGoBack();
      bool mudou = (eMenuSelect != selectAntes || eMenuPage != pageAntes);
      #if BUZZER
      if(buzzerEnabled && mudou) playBeep();
      #endif
    }
  }
  lastEncoderButtonState = currentEncoderButtonState;
}
#endif

//==============================
//    MENUS DE EDIÇÃO E CONFIRMAÇÃO
//==============================



///==============================
//    PARTE 2/3 - FUNÇÕES UP, DOWN E MENU PRINCIPAL - CORRIGIDO
//    *** REMOVIDO USB/TX1 DO MENU RIMSHOT (SÓ FICA EM SAÍDA MIDI) ***
//==============================

void Menu()
{
  #if BUZZER
  updateBuzzer();
  #endif

  // *** Carrega calibração HH da EEPROM na primeira chamada ***
  static bool hhCalibLoaded = false;
  if(!hhCalibLoaded) {
    hhCalibLoaded = true;
    LoadHHCalibEEPROM();
    LoadHHCCalEEPROM();   // carrega hhcCalMin/Max (auto-cal barras)
  }
  
  // *** TELA DE CONFIRMAÇÃO PADS AUXILIAR ***
  if(confirmacaoPadsAuxiliar) {
    byte btnA = readBtnA();  // Lê respeitando modo
    byte btnB = readBtnB();  // Lê respeitando modo
    unsigned long now = millis();
    
    // *** ESPERA SOLTAR OS BOTÕES PRIMEIRO ***
    static bool botoesForamSoltos = false;
    static bool telaJaDesenhada = false;  // *** NOVO: Controle para desenhar só uma vez ***
    
    if(!botoesForamSoltos) {
      // Verifica se os botões foram soltos
      if(btnA == HIGH && btnB == HIGH) {
        botoesForamSoltos = true;
        telaJaDesenhada = false;  // *** Reseta flag quando solta botões ***
      }
      
      // Mostra a tela mas não aceita input ainda
      if(!telaJaDesenhada) {
        lcd.clear();
        lcd.setCursor(0, 0);
        MenuString(S_PADS_AUX_CONFIRM, false);
        lcd.setCursor(0, 1);
        // ✅ TEXTO DINÂMICO: Muda automaticamente conforme o modo
        if(modoBotoes == 1) MenuString(S_A_SIM_B_NAO, false);  // INVERTIDO
        else MenuString(S_B_SIM_A_NAO, false);  // NORMAL
        telaJaDesenhada = true;
      }
      return;
    }
    
    // *** DESENHA A TELA APENAS UMA VEZ APÓS SOLTAR OS BOTÕES ***
    if(!telaJaDesenhada) {
      lcd.clear();
      lcd.setCursor(0, 0);
      MenuString(S_PADS_AUX_CONFIRM, false);
      lcd.setCursor(0, 1);
      // ✅ TEXTO DINÂMICO: Muda automaticamente conforme o modo
      if(modoBotoes == 1) MenuString(S_A_SIM_B_NAO, false);  // INVERTIDO
      else MenuString(S_B_SIM_A_NAO, false);  // NORMAL
      telaJaDesenhada = true;
    }
    
    // Timeout 5 segundos
    if(now - confirmacaoPadsAuxiliarTime > 5000) {
      confirmacaoPadsAuxiliar = false;
      botoesForamSoltos = false;
      telaJaDesenhada = false;  // *** Reseta flag ***
      needsRedraw = true;
      return;
    }
    
    // Quando INVERTIDO: btnA=SIM (físico A), btnB=NAO (físico B)
    // Quando NORMAL: btnB=SIM (físico A lido como B), btnA=NAO (físico B lido como A)
    
    bool btnSimPressionado = (modoBotoes == 1) ? (btnA == LOW) : (btnB == LOW);
    bool btnNaoPressionado = (modoBotoes == 1) ? (btnB == LOW) : (btnA == LOW);
    
    // SIM (Vai para seleção de modo)
    if(btnSimPressionado) {
      delay(200);
      confirmacaoPadsAuxiliar = false;
      selecaoModoPads = true;  // *** VAI PARA SELEÇÃO DE MODO ***
      botoesForamSoltos = false;
      telaJaDesenhada = false;
      needsRedraw = true;
      return;
    }
    
    // NAO (Desativa menu)
    if(btnNaoPressionado) {
      delay(200);
      menuPadsAuxiliarAtivo = false;
      SavePadsAuxiliarEEPROM();
      confirmacaoPadsAuxiliar = false;
      botoesForamSoltos = false;
      telaJaDesenhada = false;  // *** Reseta flag ***
      needsRedraw = true;
      // *** SEM BEEP - REMOVIDO EFEITO SONORO ***
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Pads Auxiliar");
      lcd.setCursor(0, 1);
      lcd.print("DESATIVADO!");
      delay(1500);
      return;
    }
    
    return;
  }
  
  // *** TELA DE SELEÇÃO DE MODO (6 PADS ou 4 PADS) ***
  if(selecaoModoPads) {
    byte btnA = readBtnA();  // Lê respeitando modo
    byte btnB = readBtnB();  // Lê respeitando modo
    
    static bool botoesForamSoltos = false;
    static bool telaJaDesenhada = false;
    
    if(!botoesForamSoltos) {
      if(btnA == HIGH && btnB == HIGH) {
        botoesForamSoltos = true;
        telaJaDesenhada = false;
      }
      
      if(!telaJaDesenhada) {
        lcd.clear();
        lcd.setCursor(0, 0);
        // ✅ TEXTO DINÂMICO: Título muda conforme o modo
        if(modoBotoes == 1) MenuString(S_MODO_PADS_INVERTIDO, false);  // INVERTIDO
        else MenuString(S_MODO_PADS_NORMAL, false);  // NORMAL
        lcd.setCursor(0, 1);
        // ✅ TEXTO DINÂMICO: Opções mudam conforme o modo
        if(modoBotoes == 1) MenuString(S_6_PADS, false);  // INVERTIDO
        else MenuString(S_6_PADS_INV, false);  // NORMAL
        telaJaDesenhada = true;
      }
      return;
    }
    
    if(!telaJaDesenhada) {
      lcd.clear();
      lcd.setCursor(0, 0);
      // ✅ TEXTO DINÂMICO: Título muda conforme o modo
      if(modoBotoes == 1) MenuString(S_MODO_PADS_INVERTIDO, false);  // INVERTIDO
      else MenuString(S_MODO_PADS_NORMAL, false);  // NORMAL
      lcd.setCursor(0, 1);
      // ✅ TEXTO DINÂMICO: Opções mudam conforme o modo
      if(modoBotoes == 1) MenuString(S_6_PADS, false);  // INVERTIDO
      else MenuString(S_6_PADS_INV, false);  // NORMAL
      telaJaDesenhada = true;
    }
    
    // Quando INVERTIDO: btnA=6Pads, btnB=4Pads
    // Quando NORMAL: btnB=6Pads, btnA=4Pads
    
    bool btn6PadsPressionado = (modoBotoes == 1) ? (btnA == LOW) : (btnB == LOW);
    bool btn4PadsPressionado = (modoBotoes == 1) ? (btnB == LOW) : (btnA == LOW);
    
    // 6 PADS
    if(btn6PadsPressionado) {
      delay(200);
      modoPadsAuxiliar = 6;
      maxPadsAuxiliar = 5;  // Pads 0-5
      menuPadsAuxiliarAtivo = true;
      SavePadsAuxiliarEEPROM();
      selecaoModoPads = false;
      botoesForamSoltos = false;
      telaJaDesenhada = false;
      needsRedraw = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Leonardo 6 Pads");
      lcd.setCursor(0, 1);
      lcd.print("ATIVADO!");
      delay(1500);
      return;
    }
    
    // 4 PADS
    if(btn4PadsPressionado) {
      delay(200);
      modoPadsAuxiliar = 4;
      maxPadsAuxiliar = 3;  // Pads 0-3
      menuPadsAuxiliarAtivo = true;
      SavePadsAuxiliarEEPROM();
      selecaoModoPads = false;
      botoesForamSoltos = false;
      telaJaDesenhada = false;
      needsRedraw = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ProMicro 4 Pads");
      lcd.setCursor(0, 1);
      lcd.print("ATIVADO!");
      delay(1500);
      return;
    }
    
    return;
  }
  

  // *** EDIÇÃO / CONFIRMAÇÃO DE NOMES DOS PRESETS ***
  if(presetNameEditMode) {
    PresetNameEditMenu();
    return;
  }
  if(presetConfirmMode) {
    PresetConfirmationMenu();
    return;
  }
  
  // *** MENU RESET PADRÃO ***
  if(eMenuPage == 29) {
    ResetMenu();
    return;
  }
  
  // *** MONITOR MIDI (página 30) - atualiza display sem travar o loop ***
  if(eMenuPage == 30) {
    static unsigned long monitorLastDraw = 0;
    static byte monitorLastVelDrawn = 255;
    static byte monitorLastSensDrawn = 255;
    static byte monitorLastNoteDrawn = 255;
    static bool monitorVelFirstDraw = true;
    unsigned long nowMon = millis();

    if(monitorVelFirstDraw) {
      monitorVelFirstDraw = false;
      monitorLastVelDrawn = 255;
      monitorLastSensDrawn = 255;
      monitorLastNoteDrawn = 255;
      lcd.clear();
      lcd.backlight();
      lcd.setCursor(0,0);
      lcd.print("  Monitor Midi  ");
      lcd.setCursor(0,1);
      lcd.print("--- Aguardando -");
    }

    bool changed = (monitorLastVelocity != monitorLastVelDrawn || monitorLastSensor != monitorLastSensDrawn || monitorLastNote != monitorLastNoteDrawn);
    if(changed && (nowMon - monitorLastDraw >= 80)) {
      monitorLastDraw = nowMon;
      lcd.setCursor(0,1);
      if(monitorLastSensor == 255) {
        lcd.print("--- Aguardando -");
      } else {
        // Formato: "A2 N38 Vel 127" — 1 espaco fixo entre Vel e numero
        lcd.print('A');
        lcd.print(monitorLastSensor);
        lcd.print(' ');
        lcd.print('N');
        lcd.print(monitorLastNote);
        lcd.print(' ');
        lcd.print("Vel ");  // 1 espaco fixo sempre
        lcd.print(monitorLastVelocity);
        // Preenche resto com espacos para limpar lixo de leitura anterior
        byte portaLen = (monitorLastSensor < 10) ? 2 : 3;
        byte notaLen  = (monitorLastNote < 10) ? 2 : (monitorLastNote < 100) ? 3 : 4;
        byte velLen   = (monitorLastVelocity < 10) ? 5 : (monitorLastVelocity < 100) ? 6 : 7;
        byte total = portaLen + 1 + notaLen + 1 + velLen;
        for(byte p = total; p < 16; p++) lcd.print(' ');
      }
      monitorLastVelDrawn  = monitorLastVelocity;
      monitorLastSensDrawn = monitorLastSensor;
      monitorLastNoteDrawn = monitorLastNote;
    }

    // --- Botões A e B no Monitor MIDI ---
    // CURTO A ou B: NADA (igual outros menus — curto nao sai)
    // MEDIO A: entra no ajuste (nao tem ajuste aqui, trava silencioso)
    // MEDIO B: sai -> Menu Configurações (volta ao nível anterior)
    // Encoder CURTO: sai -> Menu Configurações
    {
      static byte  monBtnA = 0, monBtnB = 0;
      static unsigned long monTA = 0, monTB = 0;
      byte bA = readBtnA(), bB = readBtnB();
      unsigned long nowM = millis();

      // Botão A state-machine (curto=nada, médio=trava)
      if(monBtnA == 0) {
        if(bA == LOW && (nowM - lastButtonPress > DEBOUNCEDELAY)) { monBtnA=1; monTA=nowM; lastButtonPress=nowM; resetBacklightTimer(); }
      } else if(monBtnA == 1) {
        if(bA == HIGH) { monBtnA=0; } // curto: nao faz nada
        else if(nowM > monTA + HOLDDELAY) { monBtnA=3; } // médio: trava
      } else if(monBtnA == 3) { if(bA == HIGH) monBtnA=0; }

      // Botão B state-machine (curto=nada, médio=sai)
      if(monBtnB == 0) {
        if(bB == LOW && (nowM - lastButtonPress > DEBOUNCEDELAY)) { monBtnB=1; monTB=nowM; lastButtonPress=nowM; resetBacklightTimer(); }
      } else if(monBtnB == 1) {
        if(bB == HIGH) { monBtnB=0; } // curto: nao faz nada
        else if(nowM > monTB + HOLDDELAY) {
          // MEDIO B: sai -> Menu Configurações
          monBtnB = 3;
          monitorVelocityAtivo = false;
          monitorVelFirstDraw = true;
          eMenuPage = 25; eMenuSelect = 1;
          needsRedraw = true;
          btnB_State = 3; // *** CORREÇÃO: bloqueia btnB global para não disparar duplo ao voltar
          #if BUZZER
          if(buzzerEnabled) playBeep();
          #endif
          return;
        }
      } else if(monBtnB == 3) { if(bB == HIGH) monBtnB=0; }

      // Encoder CURTO: sai -> Menu Configurações
      #if ENCODER
      {
        static bool  monEncLast = false;
        static unsigned long monEncT = 0;
        bool cur = readEncoderButton();
        if(cur && !monEncLast) { monEncT = millis(); resetBacklightTimer(); }
        else if(!cur && monEncLast) {
          unsigned long dur = millis() - monEncT;
          if(dur < HOLDDELAY) {
            // Curto: sai
            monitorVelocityAtivo = false;
            monitorVelFirstDraw = true;
            eMenuPage = 25; eMenuSelect = 1;
            needsRedraw = true;
            btnB_State = 3; // *** CORREÇÃO: bloqueia btnB global para não disparar duplo
            #if BUZZER
            if(buzzerEnabled) playBeep();
            #endif
            monEncLast = cur;
            return;
          }
          // Médio: trava (sem ação)
        }
        monEncLast = cur;
      }
      #endif
    }
    return;
  }
  
  // *** MONITOR / CALIBRAÇÃO HI-HAT (página 31) ***
  //
  // eMenuSelect == 0 → ajustando ABERTO  (A<xxx>)  linha 0 destaca A
  // eMenuSelect == 1 → ajustando FECHADO (F<xxx>)  linha 0 destaca F
  //
  // Botão A MÉDIO  (pino 6, HOLDDELAY): avança parâmetro
  //     select 0 → 1  (aberto → fechado)
  //     select 1 → sai para menu configurações
  // Botão B MÉDIO  (pino 7, HOLDDELAY): volta parâmetro
  //     select 1 → 0  (fechado → aberto)
  //     select 0 → sai para menu configurações
  // Botão A CURTO  (pino 6): aumenta valor (+1)
  // Botão B CURTO  (pino 7): diminui valor (-1)
  // Encoder GIRA   : aumenta/diminui valor
  // Encoder MÉDIO  : avança parâmetro (igual A médio)
  // Encoder CURTO  : volta parâmetro  (igual B médio)
  if(eMenuPage == 31) {
    static unsigned long hhLastDraw  = 0;
    static byte          hhLastBar   = 255;  // 255 = força redesenho
    static bool          hhFirstDraw = true;
    unsigned long nowHH = millis();

    #define HH_MAX_BLOCOS 16

    // --- Redesenha linha 0 conforme parâmetro ativo ---
    // eMenuSelect == 2 → só monitora, sem editar (nenhum destaque)
    // eMenuSelect == 0 → ajustando ABERTO  (A<xxx>)
    // eMenuSelect == 1 → ajustando FECHADO (F<xxx>)
    #define HH_DRAW_LINE0() { \
      lcd.setCursor(0,0); \
      char _buf[17]; \
      if(eMenuSelect == 2) \
        snprintf(_buf, sizeof(_buf), "A %03d  Hi F %03d ", HH_OPEN_POINT, HH_CLOSED_POINT); \
      else if(eMenuSelect == 0) \
        snprintf(_buf, sizeof(_buf), "A<%03d> Hi F %03d ", HH_OPEN_POINT, HH_CLOSED_POINT); \
      else \
        snprintf(_buf, sizeof(_buf), "A %03d  Hi F<%03d>", HH_OPEN_POINT, HH_CLOSED_POINT); \
      lcd.print(_buf); \
    }

    // --- Salva EEPROM ---
    #define HH_SAVE() { \
      _Pragma("GCC diagnostic ignored \"-Wunused-value\"") \
      EEPROM.write(EEPROM_HH_OPEN,   HH_OPEN_POINT); \
      EEPROM.write(EEPROM_HH_CLOSED, HH_CLOSED_POINT); \
    }

    // --- Ajusta valor do parametro ativo por delta (com wrap-around 0<->127) ---
    #define HH_AJUSTA(delta) { \
      if(eMenuSelect == 0) { \
        int _v = (int)HH_OPEN_POINT + (delta); \
        if(_v < 0) _v = 127; if(_v > 127) _v = 0; \
        HH_OPEN_POINT = (byte)_v; \
      } else { \
        int _v = (int)HH_CLOSED_POINT + (delta); \
        if(_v < 0) _v = 127; if(_v > 127) _v = 0; \
        HH_CLOSED_POINT = (byte)_v; \
      } \
      EEPROM.write(EEPROM_HH_OPEN,   HH_OPEN_POINT); \
      EEPROM.write(EEPROM_HH_CLOSED, HH_CLOSED_POINT); \
      HH_DRAW_LINE0(); \
      hhLastBar = 255; \
      resetBacklightTimer(); \
    }

    #if BUZZER
    #define _IF_BUZZER(x) if(buzzerEnabled) { x; }
    #else
    #define _IF_BUZZER(x)
    #endif

    // === PRIMEIRA ENTRADA ===
    static bool hhEncReset = false;  // flag para resetar encoder na entrada
    if(hhFirstDraw) {
      hhFirstDraw = false;
      hhLastBar   = 255;
      hhEncReset  = true;       // força reset do encoder na próxima iteração
      eMenuSelect = 2;          // começa em modo MONITOR (sem editar nenhum parâmetro)
      // Zera estados de botão e bloqueia leitura imediata (debounce de entrada)
      btnA_State  = 0;
      btnB_State  = 0;
      btnA_Time   = 0;
      btnB_Time   = 0;
      lastButtonPress = millis() + 300;  // bloqueia por DEBOUNCEDELAY+300ms após entrar (evita sair imediatamente)
      lcd.clear();
      lcd.backlight();
      HH_DRAW_LINE0();
      lcd.setCursor(0, 1);
      for(byte b = 0; b < HH_MAX_BLOCOS; b++) lcd.print('-');
      for(byte b = HH_MAX_BLOCOS; b < 16; b++) lcd.print(' ');
    }

    // === LÊ SENSOR E ATUALIZA BARRA (linha 1) ===
    byte hhRaw    = analogRead(0) / 8;

    // *** AUTO-CAL BARRAS: pressionar A+B entra em modo calibração ***
    // Funciona em qualquer sub-estado da página 31 (monitor, ajuste A, ajuste F).
    // Lógica: A+B juntos → contagem regressiva 8s → lê min/max → salva → volta ao monitor.
    {
      static bool          hhCalInProgress      = false;
      static bool          hhCalBothWasReleased = true;
      static byte          hhCalHoldState       = 0;
      static unsigned long hhCalStartTime       = 0;
      static unsigned long hhCalLastUpdate      = 0;
      #define HH_CAL_DURACAO_MS  8000UL

      byte btnA_cal = digitalRead(6);
      byte btnB_cal = digitalRead(7);
      unsigned long now_cal = millis();
      bool ambosPressed = (btnA_cal == LOW && btnB_cal == LOW);

      if(!ambosPressed) hhCalBothWasReleased = true;

      // === ENTRADA: A+B juntos em qualquer sub-estado ===
      if(!hhCalInProgress) {
        if(hhCalHoldState == 0) {
          if(ambosPressed && hhCalBothWasReleased && (now_cal - lastButtonPress > DEBOUNCEDELAY)) {
            hhCalBothWasReleased = false;
            hhCalHoldState  = 2;
            ResetHHCCal();
            hhCalInProgress = true;
            hhCalStartTime  = now_cal;
            hhCalLastUpdate = now_cal - 200UL;
            lastButtonPress = now_cal;
            btnA_State = 3;
            btnB_State = 3;
            lcd.setCursor(0, 0); lcd.print("F000 A000 Cal08s");
            hhLastBar = 255;
            _IF_BUZZER(playBeep(); delay(80); playBeep());
            resetBacklightTimer();
          }
        } else if(hhCalHoldState == 2) {
          if(!ambosPressed) hhCalHoldState = 0;
        }
      }

      // === CALIBRAÇÃO EM PROGRESSO ===
      if(hhCalInProgress) {
        btnA_State = 3;
        btnB_State = 3;

        if(hhRaw < hhcCalMin) hhcCalMin = hhRaw;
        if(hhRaw > hhcCalMax) hhcCalMax = hhRaw;

        if(now_cal - hhCalLastUpdate >= 200UL) {
          hhCalLastUpdate = now_cal;
          unsigned long elapsed = now_cal - hhCalStartTime;
          int segundos = (int)((HH_CAL_DURACAO_MS - elapsed) / 1000UL) + 1;
          if(segundos < 1) segundos = 1;
          char _cbuf[17];
          snprintf(_cbuf, sizeof(_cbuf), "F%03d A%03d Cal%02ds", hhcCalMin, hhcCalMax, segundos);
          lcd.setCursor(0, 0); lcd.print(_cbuf);
          resetBacklightTimer();
        }

        // === FINALIZA: 8 segundos esgotados ===
        if(now_cal - hhCalStartTime >= HH_CAL_DURACAO_MS) {
          hhCalInProgress = false;

          if(hhcCalMax > hhcCalMin) {
            HH_CLOSED_POINT = hhcCalMin;
            HH_OPEN_POINT   = hhcCalMax;
          }
          SaveHHCCalEEPROM();
          SaveHHCalibEEPROM();

          // Auto-distribui os 4 limiares proporcionalmente no range aprendido
          {
            int hhRange = (int)HH_OPEN_POINT - (int)HH_CLOSED_POINT;
            if(hhRange > 4) {
              for(byte z = 0; z < 4; z++) {
                int thr = (int)HH_OPEN_POINT - ((int)(z + 1) * hhRange / 5);
                if(thr < 0)   thr = 0;
                if(thr > 127) thr = 127;
                HHThresoldSensor[z] = (byte)thr;
                SaveHHEEPROM(4 + z);
              }
            }
          }

          // *** FIX BUG A8/A9: reseta HHC e pads afetados após calibração ***
          // Evita que o State do HHC fique preso em Footclose/Footsplash,
          // e que A8/A9 fiquem travados em Mask_Time ou Retrigger_Time.
          Pin[0].MaxReading = -1;
          Pin[0].State      = Normal_Time;
          Pin[0].Time       = millis();
          // Reseta todos os pads analógicos para garantir que nenhum ficou preso
          for(byte _r = 1; _r < 16; _r++) {
            if(Pin[_r].State == Mask_Time || Pin[_r].State == Retrigger_Time) {
              Pin[_r].State = Normal_Time;
              Pin[_r].MaxReading = -1;
            }
          }

          hhLastBar = 255;
          eMenuSelect = 2;  // volta ao modo monitor após calibrar
          lcd.clear();
          lcd.setCursor(0, 0); lcd.print("Cal HH Salvo!   ");
          char _doneMsg[17];
          snprintf(_doneMsg, sizeof(_doneMsg), "F:%03d  A:%03d    ", HH_CLOSED_POINT, HH_OPEN_POINT);
          lcd.setCursor(0, 1); lcd.print(_doneMsg);
          delay(2000);
          char _thrMsg[17];
          snprintf(_thrMsg, sizeof(_thrMsg), "Z:%03d %03d %03d %03d",
            HHThresoldSensor[0], HHThresoldSensor[1],
            HHThresoldSensor[2], HHThresoldSensor[3]);
          lcd.setCursor(0, 0); lcd.print(_thrMsg);
          lcd.setCursor(0, 1); lcd.print("Zonas Ajustadas ");
          delay(1500);
          HH_DRAW_LINE0();
          lastButtonPress = millis();
          btnA_State = 3;
          btnB_State = 3;
          _IF_BUZZER(playBeep(); delay(80); playBeep());
        }
      }
      #undef HH_CAL_DURACAO_MS
    }

    int  blocosTmp = map((int)hhRaw, (int)HH_CLOSED_POINT, (int)HH_OPEN_POINT, 0, HH_MAX_BLOCOS);
    if(blocosTmp < 0) blocosTmp = 0;
    byte blocos = (blocosTmp > HH_MAX_BLOCOS) ? HH_MAX_BLOCOS : (byte)blocosTmp;

    if((blocos != hhLastBar) && (nowHH - hhLastDraw >= 50)) {
      hhLastDraw = nowHH;
      if(blocos > hhLastBar || hhLastBar == 255) {
        byte from = (hhLastBar == 255) ? 0 : hhLastBar;
        for(byte b = from; b < blocos && b < HH_MAX_BLOCOS; b++) {
          lcd.setCursor(b, 1); lcd.write(byte(255));
        }
        if(hhLastBar == 255) {
          for(byte b = blocos; b < HH_MAX_BLOCOS; b++) {
            lcd.setCursor(b, 1); lcd.print('-');
          }
        }
      } else {
        for(byte b = blocos; b < hhLastBar && b < HH_MAX_BLOCOS; b++) {
          lcd.setCursor(b, 1); lcd.print('-');
        }
      }
      hhLastBar = blocos;
    }

    // === ENCODER GIRA: ajusta valor (so em modo ajuste, nao em monitor) ===
    #if ENCODER
    {
      int hhEncDir = readEncoder();
      if(hhEncDir != 0 && eMenuSelect != 2) { HH_AJUSTA(hhEncDir); }
    }
    #endif

    // === BOTÕES A e B — state-machine ===
    //
    // NÍVEIS: monitor(2) → ajuste Aberto(0) → ajuste Fechado(1) → trava
    //
    // Botão A MÉDIO:
    //   monitor(2) → Aberto(0): bipa, avança
    //   Aberto(0)  → Fechado(1): bipa, avança
    //   Fechado(1) → trava: NÃO bipa (último nível)
    //
    // Botão B MÉDIO:
    //   Fechado(1) → Aberto(0): bipa, volta
    //   Aberto(0)  → monitor(2): bipa, volta
    //   monitor(2) → sai para Menu Config: bipa, sai
    //
    // Botão A CURTO: +1 (só em ajuste, não em monitor)
    // Botão B CURTO: -1 (só em ajuste, não em monitor)
    byte btnA_hh = readBtnA();
    byte btnB_hh = readBtnB();
    unsigned long now_hh = millis();

    // --- BOTÃO A ---
    // CURTO: +1 (só em ajuste, não em monitor)
    // MÉDIO: avança parâmetro AO ATINGIR O TEMPO (sem esperar soltar)
    //   monitor(2) → Aberto(0): bipa
    //   Aberto(0)  → Fechado(1): bipa
    //   Fechado(1) → trava silencioso (último nível)
    if(btnA_State == 0) {
      if(btnA_hh == LOW && (now_hh - lastButtonPress > DEBOUNCEDELAY)) {
        btnA_State = 1; btnA_Time = now_hh; lastButtonPress = now_hh;
      }
    } else if(btnA_State == 1) {
      if(btnA_hh == HIGH) {
        btnA_State = 2;                      // solto antes do hold -> curto
      } else if(now_hh > btnA_Time + HOLDDELAY) {
        // MÉDIO: age AO ATINGIR o tempo (igual outros menus)
        btnA_State = 3;
        if(eMenuSelect == 2) {
          // MÉDIO em MONITOR: entra em ajuste ABERTO
          eMenuSelect = 0;
          HH_DRAW_LINE0();
          _IF_BUZZER(playBeep());
          resetBacklightTimer();
        } else if(eMenuSelect == 0) {
          // MÉDIO em ABERTO: vai para FECHADO
          eMenuSelect = 1;
          HH_DRAW_LINE0();
          _IF_BUZZER(playBeep());
          resetBacklightTimer();
        }
        // MÉDIO em FECHADO: trava silencioso (último nível, NÃO bipa)
      }
    } else if(btnA_State == 2) {
      // CURTO: aumenta valor +1 (so se estiver em ajuste, nao em monitor)
      if(now_hh > btnA_Time + DEBOUNCEDELAY) {
        if(eMenuSelect != 2) { HH_AJUSTA(+1); _IF_BUZZER(playBeep()); }
      }
      btnA_State = 0;
    } else if(btnA_State == 3) {
      if(btnA_hh == HIGH) btnA_State = 0;   // soltou após médio: só reseta
    }

    // --- BOTÃO B ---
    // CURTO: -1 (só em ajuste, não em monitor)
    // MÉDIO: volta parâmetro AO ATINGIR O TEMPO (sem esperar soltar)
    //   Fechado(1) → Aberto(0): bipa
    //   Aberto(0)  → monitor(2): bipa
    //   monitor(2) → sai para Menu Config: bipa
    if(btnB_State == 0) {
      if(btnB_hh == LOW && (now_hh - lastButtonPress > DEBOUNCEDELAY)) {
        btnB_State = 1; btnB_Time = now_hh; lastButtonPress = now_hh;
      }
    } else if(btnB_State == 1) {
      if(btnB_hh == HIGH) {
        btnB_State = 2;                      // solto antes do hold -> curto
      } else if(now_hh > btnB_Time + HOLDDELAY) {
        // MÉDIO: age AO ATINGIR o tempo (igual outros menus)
        btnB_State = 3;
        if(eMenuSelect == 1) {
          // MÉDIO em FECHADO: volta para ABERTO
          eMenuSelect = 0;
          HH_DRAW_LINE0();
          _IF_BUZZER(playBeep());
          resetBacklightTimer();
        } else if(eMenuSelect == 0) {
          // MÉDIO em ABERTO: volta para MONITOR
          eMenuSelect = 2;
          HH_DRAW_LINE0();
          _IF_BUZZER(playBeep());
          resetBacklightTimer();
        } else {
          // MÉDIO em MONITOR: sai -> Menu Config
          monitorHiHatAtivo = false;
          hhFirstDraw = true; hhLastBar = 255;
          eMenuPage = 25; eMenuSelect = 1; eMenuConfig = 12; needsRedraw = true;
          btnB_State = 3; // *** CORREÇÃO: bloqueia btnB global para não disparar duplo
          _IF_BUZZER(playBeep());
          resetBacklightTimer();
          return;
        }
      }
    } else if(btnB_State == 2) {
      // CURTO: diminui valor -1 (so se estiver em ajuste, nao em monitor)
      if(now_hh > btnB_Time + DEBOUNCEDELAY) {
        if(eMenuSelect != 2) { HH_AJUSTA(-1); _IF_BUZZER(playBeep()); }
      }
      btnB_State = 0;
    } else if(btnB_State == 3) {
      if(btnB_hh == HIGH) btnB_State = 0;   // soltou após médio: só reseta
    }

    // === ENCODER BOTÃO ===
    // MEDIO (segura HOLDDELAY): avança parâmetro (igual botão A médio)
    //   monitor(2) → Aberto(0): bipa
    //   Aberto(0)  → Fechado(1): bipa
    //   Fechado(1) → trava silencioso (ultimo nivel)
    // CURTO (pressiona rapido): volta parâmetro (igual botão B médio)
    //   Fechado(1) → Aberto(0): bipa
    //   Aberto(0)  → monitor(2): bipa
    //   monitor(2) → sai para Menu Config: bipa
    #if ENCODER
    {
      static byte          hhEncState = 0;  // 0=solto 1=pressionado 2=medio disparado
      static unsigned long hhEncTime  = 0;

      // Reseta estado do encoder ao entrar no menu (evita sair imediatamente)
      static bool hhEncWaitRelease = false;  // aguarda soltar o botao apos entrada
      if(hhEncReset) {
        hhEncReset       = false;
        hhEncState       = 0;
        hhEncTime        = 0;
        hhEncWaitRelease = true;  // bloqueia ate soltar
      }

      bool cur = readEncoderButton();

      // Enquanto aguarda soltar apos entrada: ignora encoder completamente
      if(hhEncWaitRelease) {
        if(!cur) hhEncWaitRelease = false;  // soltou: libera
      }

      if(!hhEncWaitRelease && hhEncState == 0) {
        if(cur) {
          hhEncState = 1;
          hhEncTime  = millis();
          resetBacklightTimer();
        }
      } else if(hhEncState == 1) {
        if(!cur) {
          // Soltou antes do HOLDDELAY = CURTO: volta nível (igual botão B médio)
          hhEncState = 0;
          if(eMenuSelect == 1) {
            // Fechado → Aberto
            eMenuSelect = 0;
            HH_DRAW_LINE0();
            _IF_BUZZER(playBeep());
            resetBacklightTimer();
          } else if(eMenuSelect == 0) {
            // Aberto → Monitor
            eMenuSelect = 2;
            HH_DRAW_LINE0();
            _IF_BUZZER(playBeep());
            resetBacklightTimer();
          } else {
            // Monitor → sai para Menu Config
            monitorHiHatAtivo = false;
            hhFirstDraw = true; hhLastBar = 255;
            eMenuPage = 25; eMenuSelect = 1; eMenuConfig = 12; needsRedraw = true;
            btnB_State = 3; // *** CORREÇÃO: bloqueia btnB global para não disparar duplo
            _IF_BUZZER(playBeep());
            return;
          }
        } else if(millis() - hhEncTime >= HOLDDELAY) {
          // MEDIO atingido enquanto segura: avança parâmetro (igual botão A médio)
          hhEncState = 2;
          if(eMenuSelect == 2) {
            // Monitor → Aberto
            eMenuSelect = 0;
            HH_DRAW_LINE0();
            _IF_BUZZER(playBeep());
            resetBacklightTimer();
          } else if(eMenuSelect == 0) {
            // Aberto → Fechado
            eMenuSelect = 1;
            HH_DRAW_LINE0();
            _IF_BUZZER(playBeep());
            resetBacklightTimer();
          }
          // Fechado: trava silencioso (ultimo nivel)
        }
      } else if(hhEncState == 2) {
        if(!cur) hhEncState = 0;  // soltou apos medio, reseta
      }
    }
    #endif

    // Limpa macros locais
    #undef HH_DRAW_LINE0
    #undef HH_SAVE
    #undef HH_AJUSTA
    #undef HH_MAX_BLOCOS
    #undef _IF_BUZZER

    return;
  }
  
  byte btnA = readBtnA();  // Lê respeitando modo
  byte btnB = readBtnB();  // Lê respeitando modo
  unsigned long now = millis();
  
  // *** SUPER-HOLD INDEPENDENTE: A ou B segurado por 5s → restaura Addctive da PROGMEM ***
  // Roda em paralelo à máquina de estados — não depende de nenhum state específico.
  // Monitora o sinal físico direto (btnA/btnB == LOW = pressionado).
  {
    static unsigned long superHoldA_Start = 0;
    static unsigned long superHoldB_Start = 0;

    // Botão A
    if(btnA == LOW) {
      if(superHoldA_Start == 0) superHoldA_Start = now;
      else if(!superHoldA_Fired && (now - superHoldA_Start >= SUPERHOLD_ADDCTIVE_MS)) {
        superHoldA_Fired = true;
        // Navega para página 20 (Backup), item Addctive (eMenuBackup=6)
        eMenuPage   = 20;
        eMenuBackup = 6;
        eMenuSelect = 1;  // cursor no nome do preset
        lcd.clear();
        needsRedraw = true;
        #if BUZZER
        if(buzzerEnabled) { playBeep(); delay(80); playBeep(); }
        #endif
      }
    } else {
      superHoldA_Start = 0;
      superHoldA_Fired = false;
    }

    // Botão B
    if(btnB == LOW) {
      if(superHoldB_Start == 0) superHoldB_Start = now;
      else if(!superHoldB_Fired && (now - superHoldB_Start >= SUPERHOLD_ADDCTIVE_MS)) {
        superHoldB_Fired = true;
        // Navega para página 20 (Backup), item Addctive (eMenuBackup=6)
        eMenuPage   = 20;
        eMenuBackup = 6;
        eMenuSelect = 1;  // cursor no nome do preset
        lcd.clear();
        needsRedraw = true;
        #if BUZZER
        if(buzzerEnabled) { playBeep(); delay(80); playBeep(); }
        #endif
      }
    } else {
      superHoldB_Start = 0;
      superHoldB_Fired = false;
    }
  }
  
  #if ENCODER
  processEncoder();
  #endif
  
  if (btnA == LOW || btnB == LOW) {
    resetBacklightTimer();
    lastInteractionTime = millis();
  }

  // *** DETECÇÃO DE 2 BOTÕES PRESSIONADOS ***
  if (btnB == LOW && btnA == LOW) {
    // *** MENU CONFIGURAÇÕES (página 25, nível 0): Ativa Pads Auxiliar com 2s ***
    if(eMenuPage == 25 && eMenuSelect == 0) {
      if(!bothButtonsPadsAuxPressed) {
        bothButtonsPadsAuxPressed = true;
        bothButtonsPadsAuxTime = now;
      }
      else if(now - bothButtonsPadsAuxTime >= 2000) {
        confirmacaoPadsAuxiliar = true;
        confirmacaoPadsAuxiliarTime = now;
        bothButtonsPadsAuxPressed = false;
        needsRedraw = true;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      }
      return;
    }
    // *** PADS PRINCIPAL (páginas 2-17, parâmetro NAME=14): A+B sem ação (edição removida) ***
    else if(eMenuPage >= 2 && eMenuPage < 18 && eMenuPin == 14) {
      // Edição livre de nome removida — usa apenas lista de nomes via encoder/botões
      return;
    }
    // *** PADS AUXILIAR (página 24, parâmetro NAME=11, lível 2): A+B sem ação (usa apenas lista de nomes) ***
    else if(eMenuPage == 24 && eMenuSelect >= 1 && eMenuProMicroParam == 11) {
      return;
    }
    // *** PÁGINA 20 (backup/presets): não reseta com A+B ***
    else if(eMenuPage == 20) {
      return;
    }
    // *** EM TODOS OS OUTROS MENUS: RESET ***
    else {
      softReset();
    }
  }
  else {
    bothButtonsPadsAuxPressed = false;
  }
  
  if (btnA_State == 0) {
    // ✅ USA DEBOUNCE MENOR quando está navegando em menus (eMenuSelect == 1)
    unsigned long debounce = (eMenuSelect == 1) ? MENU_NAV_DEBOUNCE : DEBOUNCEDELAY;
    if (btnA == LOW && (now - lastButtonPress > debounce)) {
      btnA_State = 1;
      btnA_Time = now;
      lastButtonPress = now;
    }
  } else if (btnA_State == 1) {
    if (btnA == HIGH) {
      btnA_State = 2;
      superHoldA_Fired = false;  // reset ao soltar
    } else if ((btnA == LOW) && (now > btnA_Time + HOLDDELAY)) {
      // *** BOTÃO A LONGO - ENTRA NO NÍVEL SEGUINTE ***
      // *** Menu Configurações (página 25) - SEM GERAL DELAY ***
      if(eMenuPage == 25 && eMenuSelect == 1) {
        // Entra no submenu selecionado
        switch(eMenuConfig) {
          case 0: eMenuPage = 18; break; // Luz Do Display
          case 1: eMenuPage = 19; eMenuAdvanced = 2; break; // Dual Pad
          case 2: eMenuPage = 20; break; // Backup
          case 3: eMenuPage = 21; break; // Buzzer
          case 5: eMenuPage = 23; break; // MIDI Output
          case 6: 
            if(menuPadsAuxiliarAtivo) {  // *** Só entra se ativo ***
              eMenuPage = 24; // Pads Auxiliar
              proMicroInPadSelection = true;
              eMenuProMicro = 0;
            }
            break;
          case 7: eMenuPage = 26; break; // *** NOVO: Botoes ***
          case 8: eMenuPage = 27; break; // *** NOVO: Pad Digital N ***
          case 9: eMenuPage = 28; break; // *** NOVO: Notas Choke ***
          case 10: eMenuPage = 29; break; // Reset Padrão
          case 11: // Monitor Velocity - entra na página 30 e ativa o monitor
            monitorVelocityAtivo = true;
            monitorLastSensor = 255;
            monitorLastVelocity = 0;
            eMenuPage = 30;
            break;
          case 12: // Monitor Hi-Hat - entra na página 31
            monitorHiHatAtivo = true;
            eMenuPage = 31;
            break;
          case 13: // XCancel - entra na página 34
            xcancelMenuAtivo = true;
            xcancelPar   = 0;
            xcancelParam = 0;
            eMenuPage    = XCANCEL_PAGE;
            needsRedraw  = true;
            lcd.clear();
            // windowMs fixo=120 e ghostVel padrão=18 para todos os pares ao entrar
            for(byte _xi = 0; _xi < XPAIR_COUNT; _xi++) {
              xpairRam[_xi].windowMs = 120;
              if(xpairRam[_xi].ghostVel == 0) xpairRam[_xi].ghostVel = 18;
            }
            #if BUZZER
            if(buzzerEnabled) { playBeep(); delay(80); playBeep(); delay(80); playBeep(); delay(80); playBeep(); }
            #endif
            eMenuSelect = 1;
            btnA_State = 3;
            return;
        }
        eMenuSelect = 0;
        needsRedraw = true;
        btnA_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      }
      else if(eMenuPage == 24 && !proMicroInPadSelection && eMenuSelect == 0) {
        // NÍVEL 0 → NÍVEL 1 (entra em seleção de pads)
        proMicroInPadSelection = true;
        eMenuProMicro = 0;
        needsRedraw = true;
        btnA_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      } else if(eMenuPage == 24 && proMicroInPadSelection && eMenuSelect == 0) {
        // NÍVEL 1 → NÍVEL 2 (entra no pad selecionado)
        eMenuSelect = 1;
        eMenuProMicroParam = 0;
        needsRedraw = true;
        btnA_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      } else if(eMenuPage == 24 && eMenuSelect == 1) {
        // NÍVEL 2 → NÍVEL 3 (entra na edição do parâmetro)
        // Para parâmetro NAME (11): edição é por A+B juntos
        eMenuSelect = 2;
        needsRedraw = true;
        btnA_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      } else if(eMenuPage >= 2 && eMenuPage < 18 && eMenuPin == 14 && eMenuSelect == 1) {
        // No parâmetro NAME, botão A longo avança nível normalmente
        // (edição de nome é feita por A+B juntos)
        if(eMenuSelect < 2) eMenuSelect++;
        needsRedraw = true;
        btnA_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      }
      // *** MENU XCANCEL (34) ***
      // eMenuSelect==1 (selecionando par) + A médio → entra no parâmetro (nível 2)
      // eMenuSelect==2 (editando param)  + A médio → avança para próximo parâmetro
      else if(eMenuPage == XCANCEL_PAGE) {
        if(eMenuSelect == 1) {
          eMenuSelect = 2;
          xcancelParam = 0;
        } else {
          xcancelParam = (xcancelParam + 1) % 3;
        }
        needsRedraw = true;
        btnA_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      } else {
        if(eMenuSelect < 2) {
          eMenuSelect++;
          needsRedraw = true;
          btnA_State = 3;
          #if BUZZER
          if(buzzerEnabled) playBeep();
          #endif
        } else {
          btnA_State = 3; // trava sem bipar
        }
      }
    }
  } else if (btnA_State == 2) {
    if (now > btnA_Time + DEBOUNCEDELAY) {
      Up();
      needsRedraw = true;
      #if BUZZER
      if(buzzerEnabled && eMenuSelect != 2) {
        playBeep();
      }
      #endif
      delay(50);
    }
    btnA_State = 0;
  } else if (btnA_State == 3) {
    if (btnA == HIGH) {
      btnA_State = 0;
      superHoldA_Fired = false;
    }
  }
  
  if (btnB_State == 0) {
    // ✅ USA DEBOUNCE MENOR quando está navegando em menus (eMenuSelect == 1)
    unsigned long debounce = (eMenuSelect == 1) ? MENU_NAV_DEBOUNCE : DEBOUNCEDELAY;
    if (btnB == LOW && (now - lastButtonPress > debounce)) {
      btnB_State = 1;
      btnB_Time = now;
      lastButtonPress = now;
    }
  } else if (btnB_State == 1) {
    if (btnB == HIGH) {
      btnB_State = 2;
      superHoldB_Fired = false;  // reset ao soltar
    } else if ((btnB == LOW) && (now > btnB_Time + HOLDDELAY)) {
      // *** BOTÃO B LONGO - VOLTA PARA NÍVEL ANTERIOR ***
      // *** Nos submenus de Configurações, volta para Menu Config ***

      // *** MENU XCANCEL (34) ***
      // eMenuSelect==2 (editando) + B médio → volta para seleção (nível 1)
      // eMenuSelect==1 (selecionando) + B médio → fecha e volta para Configurações
      if(eMenuPage == XCANCEL_PAGE) {
        if(eMenuSelect == 2) {
          eMenuSelect = 1;
        } else {
          xcancelResetPar0();  // *** Par 1/16 é somente leitura: restaura A1/A2/18 ao sair ***
          SaveXCancelEEPROM(); // *** Salva pares 2/16-16/16 na EEPROM ***
          xcancelMenuAtivo = false;
          eMenuPage = 25;
          eMenuConfig = 12;  // XCancel removido — Monitor HiHat é o último
          eMenuSelect = 1;
          lcd.clear();
        }
        needsRedraw = true;
        btnB_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      }
      // *** MENU TIMING (35) ***
      // eMenuSelect==2 (editando) + B médio → volta para seleção (nível 1)
      // eMenuSelect==1 (selecionando) + B médio → fecha e volta para pad A0
      else if(eMenuPage == MENU_TIMING_PAGE) {
        if(eMenuSelect == 2) {
          eMenuSelect = 1;
        } else {
          menuTimingAtivo = false;
          eMenuPage  = 2;   // volta para pad A0
          eMenuSelect = 0;
          lcd.clear();
        }
        needsRedraw = true;
        btnB_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      }
      else if(((eMenuPage >= 18 && eMenuPage <= 23) || eMenuPage == 26 || eMenuPage == 27 || eMenuPage == 28 || eMenuPage == 29 || eMenuPage == 30 || eMenuPage == 31) && eMenuSelect == 0) {
        // Volta para Menu Configurações
        if(eMenuPage == 30) monitorVelocityAtivo = false;
        if(eMenuPage == 31) { monitorHiHatAtivo = false; eMenuConfig = 12; }
        bool voltaItemSelecionado = (eMenuPage == 29 || eMenuPage == 30 || eMenuPage == 31);
        eMenuPage = 25;
        eMenuSelect = voltaItemSelecionado ? 1 : 0;
        needsRedraw = true;
        btnB_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      }
      // *** Menu Configurações: eMenuSelect==0 → trava sem bipar ***
      else if(eMenuPage == 25 && eMenuSelect == 0) {
        btnB_State = 3;
      }
      else if(eMenuPage == 24 && eMenuSelect == 2) {
        // NÍVEL 3 → NÍVEL 2 (volta do valor para parâmetro)
        eMenuSelect = 1;
        needsRedraw = true;
        btnB_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      } else if(eMenuPage == 24 && eMenuSelect == 1) {
        // NÍVEL 2 → NÍVEL 1 (volta do parâmetro para seleção de pad)
        eMenuSelect = 0;
        needsRedraw = true;
        btnB_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      } else if(eMenuPage == 24 && proMicroInPadSelection && eMenuSelect == 0) {
        // Nível mínimo → volta para Menu Configurações (página 25)
        proMicroInPadSelection = false;
        eMenuPage = 25;
        eMenuSelect = 1;
        needsRedraw = true;
        btnB_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      } else if(eMenuPage == 24 && !proMicroInPadSelection && eMenuSelect == 0) {
        // Título fantasma → volta para Menu Configurações (página 25)
        eMenuPage = 25;
        eMenuSelect = 1;
        needsRedraw = true;
        btnB_State = 3;
        #if BUZZER
        if(buzzerEnabled) playBeep();
        #endif
      } else {
        // Pad A0-A15: só volta se estiver dentro de submenu
        if(eMenuSelect > 0) {
          eMenuSelect--;
          needsRedraw = true;
          #if BUZZER
          if(buzzerEnabled) playBeep();
          #endif
        }
        btnB_State = 3;
      }
    }
  } else if (btnB_State == 2) {
    if (now > btnB_Time + DEBOUNCEDELAY) {
      Down();
      needsRedraw = true;
      #if BUZZER
      if(buzzerEnabled && eMenuSelect != 2) {
        playBeep();
      }
      #endif
      delay(50);
    }
    btnB_State = 0;
  } else if (btnB_State == 3) {
    if (btnB == HIGH) {
      btnB_State = 0;
      superHoldB_Fired = false;
    }
  }
  
  if(needsRedraw && (millis() - lastLCDUpdate >= LCD_UPDATE_INTERVAL)) {
    Draw();
    needsRedraw = false;
  }
  updateBacklight();
}

//==============================
//    FUNÇÃO UP - COM PERSONA E SEM DUPLICAÇÃO
//==============================

void Up()
{
  if(eMenuSelect==0) 
  { 
    // NÍVEL 0: Navega entre menus OU NÍVEL 1: Navega entre pads
    if(eMenuPage == 24 && proMicroInPadSelection) {
      // NÍVEL 1: Navega entre pads (LIMITADO conforme maxPadsAuxiliar)
      eMenuProMicro = (eMenuProMicro + 1) % (maxPadsAuxiliar + 1);
    }
    // *** DENTRO DE SUBMENU: loop entre submenus ***
    else if((eMenuPage == 25 && eMenuSelect == 1) || (eMenuPage >= 18 && eMenuPage <= 23) || eMenuPage == 26 || eMenuPage == 27 || eMenuPage == 28) {
      if(eMenuPage == 25) {
        // Em Config (página 25): navega eMenuConfig para frente
        eMenuConfig = (eMenuConfig + 1);
        if(eMenuConfig == 4) eMenuConfig = 5;  // Pula Padrao Midi (removido)
        if(!menuPadsAuxiliarAtivo && eMenuConfig == 6) eMenuConfig = 7;
        if(eMenuConfig == 13) eMenuConfig = 0;  // XCancel removido — volta ao início
        if(eMenuConfig > 14) eMenuConfig = 0;
        lcd.clear();  // ✅ Limpa display ao trocar de submenu via navegação
        switch(eMenuConfig) {
          case 0: eMenuPage = 18; eMenuSelect = 0; break;
          case 1: eMenuPage = 19; eMenuSelect = 0; eMenuAdvanced = 2; break;
          case 2: eMenuPage = 20; eMenuSelect = 0; break;
          case 3: eMenuPage = 21; eMenuSelect = 0; break;
          case 5: eMenuPage = 23; eMenuSelect = 0; break;
          // *** PROTEÇÃO PAD AUX: case 6 NÃO entra na página 24 por clique curto ***
          case 6: eMenuPage = 25; eMenuSelect = 1; break;
          case 7: eMenuPage = 26; eMenuSelect = 0; break;
          case 8: eMenuPage = 27; eMenuSelect = 0; break;
          case 9: eMenuPage = 28; eMenuSelect = 0; break;
          // 10=Reset, 11=MonitorV, 12=MonitorHH, 13=XCancel ficam em page=25
          default: eMenuPage = 25; eMenuSelect = 1; break;
        }
      } else {
        // Em outros submenus: avança para o próximo
        byte prox = eMenuPage + 1;
        // *** PROTEÇÃO PAD AUX: página 24 só é acessível via hold longo (botão A médio/longo) ***
        // Navegação por clique curto NUNCA entra na página 24 — pula para 26
        if(prox == 22) prox = 23;  // página 22 removida (era Padrão MIDI) — pula para Saída MIDI
        if(prox == 24) prox = 26;
        if(prox > 28) {
          // Passou de Notas Choke(28) → vai para Config(25) mostrando Reset Módulo
          eMenuPage = 25;
          eMenuConfig = 10;
          eMenuSelect = 1;  // ✅ Mostra o item, não o título "Configurações"
          lcd.clear();
        } else if(prox == 25) {
          prox = 26;
          eMenuPage = prox;
          lcd.clear();
        } else {
          eMenuPage = prox;
          lcd.clear();
        }
      }
    } else {
      // *** NAVEGAÇÃO CIRCULAR: PAD 0 → PAD 15 → Config → PAD 0 ***
      eMenuPage = (eMenuPage + 1);
      
      // *** Reseta eMenuPin ao trocar de pad SOMENTE se o parâmetro for inválido na nova porta ***
      // eMenuPin 15 (InvSensor): só existe em A0 (page==2)
      // eMenuPin 16/17/18 (Rimshot/RishotNote/RishotVel): só existem em A2 (page==4)
      // eMenuPin 0-14 (Note, Threshold ... Name): válidos em qualquer porta → mantém
      if(eMenuPin == 15 && eMenuPage != 2) eMenuPin = 0;
      else if((eMenuPin == 16 || eMenuPin == 17 || eMenuPin == 18) && eMenuPage != 4) eMenuPin = 0;
      
      // Pula as páginas antigas (18-24) e vai direto para Config (25)
      if(eMenuPage == 18) {
        eMenuPage = 25;
        // ✅ Protege contra Pads Aux aparecer quando desativado
        if(!menuPadsAuxiliarAtivo && eMenuConfig == 6) {
          eMenuConfig = 7;
        }
      }
      
      // *** PROTEÇÃO: Pula pagina 24 se Pads Aux desativado - continua para 25 ***
      if(eMenuPage == 24 && !menuPadsAuxiliarAtivo) {
        eMenuPage = 25;
      }
      
      // Pula a página Botões (26) e digitais/choke (27,28,29,30) - elas só aparecem DENTRO de Config
      if(eMenuPage == 26 || eMenuPage == 27 || eMenuPage == 28 || eMenuPage == 29 || eMenuPage == 30 || eMenuPage == 31) eMenuPage = 2;
      
      // Depois da Config (25), volta para PAD 0 (2)
      if(eMenuPage > 31) eMenuPage = 2;
      
      lcd.clear();  // *** FIX: limpa display ao mudar de pad ***
      
      byte realPin = (eMenuPage >= 2 && eMenuPage < 18) ? (eMenuPage - 2) : 0;
      LogPin = realPin;
      #if MENU_LOG
      log_state=0;
      #endif
    }
  }
  else if(eMenuSelect==1)
  {
    if(eMenuPage==0) Mode=(mode)(((int)Mode+1)%4);
    else if(eMenuPage==1) eMenuGeneral=(eMenuGeneral+1)%2;
    else if(eMenuPage==18) eMenuBacklight=(eMenuBacklight+1)%1;
    else if(eMenuPage==19) {
      // Itens visíveis: 2,3
      const byte adv[] = {2,3};
      const byte adv_n = 2;
      byte cur = 0;
      for(byte i=0;i<adv_n;i++) if(adv[i]==eMenuAdvanced){cur=i;break;}
      eMenuAdvanced = adv[(cur+1)%adv_n];
    }
    else if(eMenuPage==20) eMenuBackup=(eMenuBackup+1)%9;
    else if(eMenuPage==21) eMenuBuzzer=(eMenuBuzzer+1)%1;
    else if(eMenuPage==23) eMenuMidi=(eMenuMidi+1)%2;
    else if(eMenuPage==24) {
      eMenuProMicroParam = (eMenuProMicroParam + 1) % 12;
      // param 10 = XtakCancel — agora visível, não pula mais
    }
    else if(eMenuPage==26) eMenuBotoes=(eMenuBotoes+1)%1;  // *** NOVO: Página 26 ***
    else if(eMenuPage==27) eMenuDigital=(eMenuDigital+1)%7;  // *** NOVO: Página 27 - Pad Digital N ***
    else if(eMenuPage==28) {
      // 0-3=notas (Crash1/Crash2/Crash3/Ride)
      eMenuChokeNote = (eMenuChokeNote + 1) % 4;
    }
    // *** MENU XCANCEL (página 34) — navega entre os 16 pares ***
    else if(eMenuPage==XCANCEL_PAGE && eMenuSelect==1) {
      xcancelPar = (xcancelPar + 1) % XPAIR_COUNT;
      needsRedraw = true;
    }
    // *** MENU TIMING (página 35) — navega entre os 3 parâmetros ***
    else if(eMenuPage==MENU_TIMING_PAGE && eMenuSelect==1) {
      menuTimingParam = (menuTimingParam + 1) % 3;
      needsRedraw = true;
    }
    else if(eMenuPage==25) {
      // *** CORRIGIDO: Navegação para frente SEMPRE pula opção 6 se desativado ***
      eMenuConfig = (eMenuConfig + 1);
      
      // ✅ DEBUG: Pisca LED para confirmar que entrou aqui
      digitalWrite(13, HIGH);
      delay(100);
      digitalWrite(13, LOW);
      
      if(eMenuConfig == 4) eMenuConfig = 5;  // Pula Padrao Midi (removido)
      // ✅ PULA o índice 6 se Pads Aux está DESATIVADO
      if(!menuPadsAuxiliarAtivo && eMenuConfig == 6) {
        eMenuConfig = 7;  // Pula direto para Botões
      }
      
      // ✅ Volta ao início após o 12 (XCancel removido)
      if(eMenuConfig > 12) {
        eMenuConfig = 0;
      }
    }
    else if(eMenuPage>=2 && eMenuPage<18) {
      eMenuPin=(eMenuPin+1)%19;
      if(eMenuPin==8) eMenuPin=9;   // pin 8 removido (Xtalk)
      if(eMenuPin==9) eMenuPin=10;  // pin 9 removido (alias XCanCost)
      // InvSensor (15): só A0 (page==2)
      if(eMenuPin==15 && eMenuPage!=2) eMenuPin=16;
      // Itens 16/17/18: SÓ no pad A2 (page==4) — em qualquer outro pad pula direto para 0
      if(eMenuPin==16 && eMenuPage!=4) eMenuPin=0;
      if(eMenuPin==17 && eMenuPage!=4) eMenuPin=0;
      if(eMenuPin==18 && eMenuPage!=4) eMenuPin=0;
    }
  }
  else if(eMenuSelect==2)
  {
    if(eMenuPage==1)
    {
      if(eMenuGeneral==1) { GeneralXtalk=(GeneralXtalk+1)%8; markGeneralChanged(); }
      else if(eMenuGeneral==3) { HHThresoldSensor[0]=(HHThresoldSensor[0]+1)%128; markHHChanged(); }
      else if(eMenuGeneral==4) { HHThresoldSensor[1]=(HHThresoldSensor[1]+1)%128; markHHChanged(); }
      else if(eMenuGeneral==5) { HHThresoldSensor[2]=(HHThresoldSensor[2]+1)%128; markHHChanged(); }
      else if(eMenuGeneral==6) { HHThresoldSensor[3]=(HHThresoldSensor[3]+1)%128; markHHChanged(); }
      else if(eMenuGeneral==7) { NSensor=(NSensor+1)%6; markGeneralChanged(); }
    }
    else if(eMenuPage==18)
    {
      if(eMenuBacklight==0) { backlightMode=(backlightMode+1)%2; markBacklightChanged(); }
    }
    // *** Rimshot DualPd - opções visíveis: 0,1,2,3,5,7,9,10,11,12 ***
    else if(eMenuPage==19)
    {
      if(eMenuAdvanced==0) { ENABLE_NOTE_37_38_TO_40=(ENABLE_NOTE_37_38_TO_40+1)%2; markAdvancedChanged(); }
      else if(eMenuAdvanced==1) { ENABLE_RIMSHOT_38_TO_40=(ENABLE_RIMSHOT_38_TO_40+1)%2; markAdvancedChanged(); }
      else if(eMenuAdvanced==2) { ENABLE_NOTE_101_TO_102=(ENABLE_NOTE_101_TO_102+1)%2; markAdvancedChanged(); }
      else if(eMenuAdvanced==3) { ENABLE_NOTE_103_TO_104=(ENABLE_NOTE_103_TO_104+1)%2; markAdvancedChanged(); }
      // else if(eMenuAdvanced==4) { CUSTOM_NOTE_37=(CUSTOM_NOTE_37+1)%128; markAdvancedChanged(); }  // OCULTO
      else if(eMenuAdvanced==5) { CUSTOM_NOTE_40=(CUSTOM_NOTE_40+1)%128; if(rimSub==1) CUSTOM_RIMSHOT_FORCE_NOTE=CUSTOM_NOTE_40; markAdvancedChanged(); }
      // else if(eMenuAdvanced==6) { CUSTOM_RIMSHOT_FORCE_NOTE=(CUSTOM_RIMSHOT_FORCE_NOTE+1)%128; markAdvancedChanged(); }  // OCULTO
      else if(eMenuAdvanced==7) { VELOCITY_THRESHOLD_37_38=(VELOCITY_THRESHOLD_37_38+1)%128; markAdvancedChanged(); }
      // 8 oculto (Detection Window - acesso via item 9 abaixo com nova lógica)
      else if(eMenuAdvanced==9) {
        DETECTION_WINDOW_MS = (DETECTION_WINDOW_MS + 1) % 19;
        markAdvancedChanged();
      }
      else if(eMenuAdvanced==10) {
        if(MAX_NOTE_AGE_MS < 30) MAX_NOTE_AGE_MS++;
        else MAX_NOTE_AGE_MS = 1;
        markAdvancedChanged();
      }
      else if(eMenuAdvanced==11) {
        if(BLOCK_WINDOW_MS < 500) BLOCK_WINDOW_MS += 10;
        else BLOCK_WINDOW_MS = 10;
        markAdvancedChanged();
      }
      else if(eMenuAdvanced==12) {
        ENABLE_NOTE_37_38_TO_40=(ENABLE_NOTE_37_38_TO_40+1)%2;
        markAdvancedChanged();
      }
      // *** REMOVIDO: opções USB/TX1 - agora só no menu Saída MIDI ***
    }
    // *** MENU XCANCEL (página 34) — incrementa parâmetro do par selecionado ***
    else if(eMenuPage==XCANCEL_PAGE && eMenuSelect==2)
    {
      switch(xcancelParam) {
        case 0: // Source: 0-15 + 255(desativado)
          if(xpairRam[xcancelPar].source == 255) xpairRam[xcancelPar].source = 0;
          else if(xpairRam[xcancelPar].source >= 15) xpairRam[xcancelPar].source = 255;
          else xpairRam[xcancelPar].source++;
          break;
        case 1: // Target: 0-15 + 255(desativado)
          if(xpairRam[xcancelPar].target == 255) xpairRam[xcancelPar].target = 0;
          else if(xpairRam[xcancelPar].target >= 15) xpairRam[xcancelPar].target = 255;
          else xpairRam[xcancelPar].target++;
          break;
        case 2: // ghostVel: 0-100 (windowMs=120 fixo, não aparece)
          if(xpairRam[xcancelPar].ghostVel >= 100) xpairRam[xcancelPar].ghostVel = 0;
          else xpairRam[xcancelPar].ghostVel++;
          break;
      }
      SaveXCancelEEPROM();  // *** SALVA NA EEPROM para persistir após desligar ***
      needsRedraw = true;
    }
    // *** MENU TIMING (página 35) — incrementa o valor do parâmetro selecionado ***
    else if(eMenuPage==MENU_TIMING_PAGE && eMenuSelect==2)
    {
      switch(menuTimingParam) {
        case 0: // DETECTION_WINDOW_MS: 1-30 ms
          DETECTION_WINDOW_MS = (DETECTION_WINDOW_MS < 30) ? DETECTION_WINDOW_MS + 1 : 1;
          markAdvancedChanged();
          break;
        case 1: // MAX_NOTE_AGE_MS: 1-60 ms
          MAX_NOTE_AGE_MS = (MAX_NOTE_AGE_MS < 60) ? MAX_NOTE_AGE_MS + 1 : 1;
          markAdvancedChanged();
          break;
        case 2: // BLOCK_WINDOW_MS: 10-500 ms (passo 10)
          BLOCK_WINDOW_MS = (BLOCK_WINDOW_MS < 500) ? BLOCK_WINDOW_MS + 10 : 10;
          markAdvancedChanged();
          break;
      }
      needsRedraw = true;
    }
    else if(eMenuPage==20)
    {
      if(eMenuBackup>=0 && eMenuBackup<=2) {
        saveBackupWithCache(eMenuBackup+1);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Preset ");
        lcd.print(eMenuBackup+1);
        lcd.setCursor(0,1);
        MenuString(S_SALVO,false);
        delay(1500);
        needsRedraw = true;
      }
      else if(eMenuBackup>=3 && eMenuBackup<=5) {
        byte presetNum = eMenuBackup-2;
        if(BackupExists(presetNum)) {
          restoreBackupWithCache(presetNum);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Preset ");
          lcd.print(presetNum);
          lcd.setCursor(0,1);
          MenuString(S_RESTAURADO,false);
          delay(1500);
          needsRedraw = true;
        }
      }
      else if(eMenuBackup>=6 && eMenuBackup<=8) {
        // Presets de fábrica da PROGMEM — apenas restaurar
        byte factoryNum = eMenuBackup - 2;  // 6->4, 7->5, 8->6
        RestoreFactoryPreset(factoryNum);
        lcd.clear();
        lcd.setCursor(0,0);
        char buf[13];
        getFactoryPresetName(factoryNum, buf);
        lcd.print(buf);
        lcd.setCursor(0,1);
        MenuString(S_RESTAURADO,false);
        delay(1500);
        needsRedraw = true;
      }
      // *** itens 6/7/8: entrada por A+B juntos no Menu(), não por encoder/botão simples ***
    }
    else if(eMenuPage==21)
    {
      if(eMenuBuzzer==0) { 
        #if BUZZER
        buzzerEnabled=(buzzerEnabled+1)%2; 
        markBuzzerChanged();
        forceBuzzerSave();
        if(buzzerEnabled) {
          playBeep();
        }
        #endif
      }
    }
    // *** Menu Saída MIDI (23) - MANTIDO ***
    else if(eMenuPage==23)
    {
      if(eMenuMidi==0) { 
        MIDI_USB_ENABLED=(MIDI_USB_ENABLED+1)%2; 
        markMidiOutputChanged();
      }
      else if(eMenuMidi==1) {
        MIDI_TX1_ENABLED=(MIDI_TX1_ENABLED+1)%2;
        markMidiOutputChanged();
      }
    }
    // *** NOVO: Menu Botoes (26) ***
    else if(eMenuPage==26)
    {
      if(eMenuBotoes==0) {
        modoBotoes = (modoBotoes + 1) % 2;  // Alterna entre 0 (Normal) e 1 (Invertido)
        SaveModoBotoesEEPROM();
        needsRedraw = true;  // ✅ Força redesenho para aplicar textos dinâmicos
      }
    }
    // *** Pad Digital N (27) ***
    else if(eMenuPage==27)
    {
      digitalPadNotes[eMenuDigital] = (digitalPadNotes[eMenuDigital] + 1) % 128;
      // Sincroniza ZoneDual/TriZone — nota Aux e NoteBlock acompanham
      if(eMenuDigital < ZONE_DUAL_PAIRS) {
        zoneDual_NoteAux[eMenuDigital]   = digitalPadNotes[eMenuDigital];
        zoneDual_NoteBlock[eMenuDigital] = Pin[zoneDual_PadIdx[eMenuDigital]].Note;
      }
      SaveDigitalNotesEEPROM();
    }
    // *** NOVO: Notas Choke (28) ***
    else if(eMenuPage==28)
    {
      if(eMenuChokeNote <= 3) {
        // Notas Crash1/Crash2/Crash3/Ride
        chokeNotes[eMenuChokeNote] = (chokeNotes[eMenuChokeNote] + 1) % 128;
        SaveChokeNotesEEPROM();
      }
    }
    // *** PRO MICRO: Aumenta valor do parâmetro ***
    else if(eMenuPage==24)
    {
      int currentValue = 0;
      byte paramCode = 0;
      
      switch(eMenuProMicroParam) {
        case 0: // Note
          currentValue = proMicroPads[eMenuProMicro].note;
          paramCode = PARAM_NOTE;
          break;
        case 1: // Threshold
          currentValue = proMicroPads[eMenuProMicro].threshold;
          paramCode = PARAM_THRESHOLD;
          break;
        case 2: // ScanTime
          currentValue = proMicroPads[eMenuProMicro].scanTime;
          paramCode = PARAM_SCANTIME;
          break;
        case 3: // MaskTime
          currentValue = proMicroPads[eMenuProMicro].maskTime;
          paramCode = PARAM_MASKTIME;
          break;
        case 4: { // Retrigger — Aut(255)→0→1→...→16→Aut(255)
          byte r = proMicroPads[eMenuProMicro].retrigger;
          if(r == 255) r = 0;
          else if(r >= 16) r = 255;
          else r++;
          updateProMicroParameter(eMenuProMicro, PARAM_RETRIGGER, r);
          return;
        }
        case 5: // Curve (0-4)
          currentValue = proMicroPads[eMenuProMicro].curve;
          currentValue = (currentValue + 1) % 5;
          updateProMicroParameter(eMenuProMicro, PARAM_CURVE, currentValue);
          return;
        case 6: // CurveForm
          currentValue = proMicroPads[eMenuProMicro].curveForm;
          paramCode = PARAM_CURVEFORM;
          break;
        case 7: // Gain
          currentValue = proMicroPads[eMenuProMicro].gain;
          paramCode = PARAM_GAIN;
          break;
        case 8: // Type (0-1)
          currentValue = proMicroPads[eMenuProMicro].type;
          currentValue = (currentValue + 1) % 2;
          updateProMicroParameter(eMenuProMicro, PARAM_TYPE, currentValue);
          return;
        case 9: // Channel
          currentValue = proMicroPads[eMenuProMicro].channel;
          paramCode = PARAM_CHANNEL;
          break;
        case 10: // XtakCancel (XCancel Global) — 0-127
          currentValue = proMicroPads[eMenuProMicro].xcanCost;
          paramCode = PARAM_XCANCOST;
          break;
        case 11: // Name
          #if defined(__AVR__)
          bool hasCustom = (EEPROM.read(875 + eMenuProMicro) == 1);
          #else
          bool hasCustom = false;
          #endif
          if(hasCustom) {
            startProMicroConfirmation(eMenuProMicro);
          } else {
            proMicroPads[eMenuProMicro].nameIndex = (proMicroPads[eMenuProMicro].nameIndex + 1) % 25;
            saveProMicroPadToEEPROM(eMenuProMicro);
          }
          return;
      }
      
      currentValue = (currentValue + 1) % 128;
      updateProMicroParameter(eMenuProMicro, paramCode, currentValue);
    }
    else if(eMenuPage>=2 && eMenuPage<18)
    {
      byte pinIndex = eMenuPage - 2;
      switch(eMenuPin)
      {
        case 0: // Note
          ADD(Note);
          if(pinIndex == 2) {
            CUSTOM_NOTE_38 = Pin[2].Note;
            markAdvancedChanged();
          }
          if(pinIndex == 3) {
            CUSTOM_NOTE_37 = Pin[3].Note;
            markAdvancedChanged();
          }
          break;
        case 1: ADD(Thresold); break;
        case 2: ADD(ScanTime); break;
        case 3: ADD(MaskTime); break;
        case 4: {
          // Aut(255)→0→1→...→16→Aut(255)
          byte &r = Pin[eMenuPage-2].Retrigger;
          if(r == 255) r = 0;
          else if(r >= 16) r = 255;
          else r++;
          markPinChanged(eMenuPage-2);
          break;
        }
        case 5: Pin[eMenuPage-2].Curve=(curve)((Pin[eMenuPage-2].Curve+1)%5); markPinChanged(eMenuPage-2); break;
        case 6: ADD(CurveForm); break;
        case 7: Pin[eMenuPage-2].XCanCost = (Pin[eMenuPage-2].XCanCost < 127) ? Pin[eMenuPage-2].XCanCost + 1 : 0; markPinChanged(eMenuPage-2); break;
        case 8: break;  // Xtalk removido — não faz nada
        case 9: break;  // alias removido — não faz nada
        case 10: { byte t=Pin[pinIndex].Type; if(t==5) t=15; else if(t==15) t=0; else t++; if(t>5 && t!=15) t=0; Pin[pinIndex].Type=(type)t; markPinChanged(pinIndex); } break;
        case 11: // VeMinimo — incrementa 0-60 com wrap
          {
            byte vm = Pin[pinIndex].VelMinimo;
            if(vm < 100) vm++; else vm = 0;
            Pin[pinIndex].VelMinimo = vm;
            #if defined(__AVR__)
            EEPROM.write(652 + pinIndex, vm);
            #endif
            markPinChanged(pinIndex);
          }
          break;
        case 12: ADD(ChokeNote); break;
        case 13: ADD(Channel); break;
        case 15: // InvertSensor — alterna 0/1
          {
            Pin[pinIndex].InvertSensor = Pin[pinIndex].InvertSensor ? 0 : 1;
            #if defined(__AVR__)
            EEPROM.write(669 + pinIndex, Pin[pinIndex].InvertSensor);
            EEPROM.write(685, 0xCC); // *** CORREÇÃO: atualiza flag de validade ***
            #endif
            markPinChanged(pinIndex);
          }
          break;
        case 16: // Rimshot Comp/Forc — só A2
          rimSub = rimSub ? 0 : 1;
          #if defined(__AVR__)
          EEPROM.write(EEPROM_RIMSUB_MODE, rimSub);
          #endif
          // Comp(0): ativa rimshot completo (38+nota), desativa forçado
          // Forc(1): ativa rimshot forçado (vel>125), desativa completo
          if(rimSub == 0) {
            ENABLE_NOTE_37_38_TO_40 = 1;
            ENABLE_RIMSHOT_38_TO_40 = 0;
          } else {
            ENABLE_NOTE_37_38_TO_40 = 0;
            ENABLE_RIMSHOT_38_TO_40 = 1;
            CUSTOM_RIMSHOT_FORCE_NOTE = CUSTOM_NOTE_40; // Forc: sincroniza ao ativar
          }
          markAdvancedChanged();
          break;
        case 17: // RishotNote — só A2
          CUSTOM_NOTE_40 = (CUSTOM_NOTE_40 + 1) % 128;
          if(rimSub == 1) CUSTOM_RIMSHOT_FORCE_NOTE = CUSTOM_NOTE_40; // Forc: sincroniza nota forçada
          markAdvancedChanged();
          break;
        case 18: // RishotVel — velocidade mínima rimshot (só A2)
          {
            VELOCITY_THRESHOLD_37_38 = (VELOCITY_THRESHOLD_37_38 + 1) % 128;
            markAdvancedChanged();
          }
          break;
        case 14:
          if(pinIndex < 16) {
            selected_names[pinIndex] = (selected_names[pinIndex] + 1) % NUM_AVAILABLE_NAMES;
            if(selected_names[pinIndex] >= NUM_AVAILABLE_NAMES) {
              selected_names[pinIndex] = 0;
            }
            markNameChanged();
          }
          break;
      }
    }
  }
}

//==============================
//    FUNÇÃO DOWN - COM PERSONA E SEM DUPLICAÇÃO
//==============================

void Down()
{
  if(eMenuSelect==0) 
  { 
    // NÍVEL 0 ou NÍVEL 1
    if(eMenuPage == 24 && proMicroInPadSelection) {
      // NÍVEL 1: Navega entre pads (volta) (LIMITADO conforme maxPadsAuxiliar)
      if(eMenuProMicro == 0) {
        eMenuProMicro = maxPadsAuxiliar;  // Volta para o último pad
      } else {
        eMenuProMicro--;
      }
    }
    // *** DENTRO DE SUBMENU: loop reverso entre submenus ***
    else if((eMenuPage == 25 && eMenuSelect == 1) || (eMenuPage >= 18 && eMenuPage <= 23) || eMenuPage == 26 || eMenuPage == 27 || eMenuPage == 28) {
      if(eMenuPage == 25) {
        // Em Config (página 25): navega eMenuConfig para trás
        if(eMenuConfig == 0) {
          eMenuConfig = 12;  // XCancel removido — Monitor HiHat é o último
        } else {
          eMenuConfig = eMenuConfig - 1;
        }
        if(eMenuConfig == 4) eMenuConfig = 3;  // Pula Padrao Midi (removido)
        if(!menuPadsAuxiliarAtivo && eMenuConfig == 6) eMenuConfig = 5;
        // Quando eMenuConfig está em 0-9: vai para a página do submenu correspondente
        lcd.clear();  // ✅ Limpa display ao trocar de submenu via navegação reversa
        switch(eMenuConfig) {
          case 0: eMenuPage = 18; eMenuSelect = 0; break;
          case 1: eMenuPage = 19; eMenuSelect = 0; eMenuAdvanced = 2; break;
          case 2: eMenuPage = 20; eMenuSelect = 0; break;
          case 3: eMenuPage = 21; eMenuSelect = 0; break;
          case 5: eMenuPage = 23; eMenuSelect = 0; break;
          // *** PROTEÇÃO PAD AUX: case 6 NÃO entra na página 24 por clique curto ***
          case 6: eMenuPage = 25; eMenuSelect = 1; break;
          case 7: eMenuPage = 26; eMenuSelect = 0; break;
          case 8: eMenuPage = 27; eMenuSelect = 0; break;
          case 9: eMenuPage = 28; eMenuSelect = 0; break;
          // 10=Reset, 11=MonitorV, 12=MonitorHH, 13=XCancel ficam em page=25
          default: eMenuPage = 25; eMenuSelect = 1; break;
        }
      } else {
        byte prox = eMenuPage - 1;
        // *** PROTEÇÃO PAD AUX: página 24 só é acessível via hold longo (botão A médio/longo) ***
        // Navegação por clique curto NUNCA entra na página 24 — pula para 23
        if(prox == 22) prox = 21;  // página 22 removida (era Padrão MIDI) — volta para Som
        if(prox == 24) prox = 23;
        if(prox < 18) {
          // Antes de Luz Display(18) → vai para Config(25) mostrando XCancel (último item válido)
          eMenuPage = 25;
          eMenuConfig = 12;  // XCancel removido — Monitor HiHat é o último
          eMenuSelect = 1;  // ✅ Mostra o item, não o título "Configurações"
          lcd.clear();
        } else if(prox == 25) {
          prox = 23;
          eMenuPage = prox;
          lcd.clear();
        } else {
          eMenuPage = prox;
          lcd.clear();
        }
      }
    } else {
      // *** NAVEGAÇÃO CIRCULAR REVERSA: Config → PAD 15 → PAD 0 → Config ***
      eMenuPage = (eMenuPage - 1);
      
      // *** Reseta eMenuPin ao trocar de pad SOMENTE se o parâmetro for inválido na nova porta ***
      // eMenuPin 15 (InvSensor): só existe em A0 (page==2)
      // eMenuPin 16/17/18 (Rimshot/RishotNote/RishotVel): só existem em A2 (page==4)
      // eMenuPin 0-14 (Note, Threshold ... Name): válidos em qualquer porta → mantém
      if(eMenuPin == 15 && eMenuPage != 2) eMenuPin = 0;
      else if((eMenuPin == 16 || eMenuPin == 17 || eMenuPin == 18) && eMenuPage != 4) eMenuPin = 0;
      
      // Volta do PAD 0 (2) para Config (25)
      if(eMenuPage < 2) {
        eMenuPage = 25;
        // ✅ Protege contra Pads Aux aparecer quando desativado
        if(!menuPadsAuxiliarAtivo && eMenuConfig == 6) {
          eMenuConfig = 5;  // Volta para MIDI Output
        }
      }
      
      // Pula a página Botões (26) e digitais/choke (27,28,29,30) - elas só aparecem DENTRO de Config
      if(eMenuPage == 26 || eMenuPage == 27 || eMenuPage == 28 || eMenuPage == 29 || eMenuPage == 30 || eMenuPage == 31) eMenuPage = 25;
      
      // Pula as páginas antigas - volta de Config para PAD 15 (17)
      if(eMenuPage == 24) eMenuPage = 17;
      
      lcd.clear();  // *** FIX: limpa display ao mudar de pad ***
      
      byte realPin = (eMenuPage >= 2 && eMenuPage < 18) ? (eMenuPage - 2) : 0;
      LogPin = realPin;
      #if MENU_LOG
      log_state=0;
      #endif
    }
  }
  else if(eMenuSelect==1) 
  {
    if(eMenuPage==0) Mode=(mode)((int)Mode-1>-1?(int)Mode-1:3);
    else if(eMenuPage==1) eMenuGeneral=eMenuGeneral-1>-1?eMenuGeneral-1:1;
    else if(eMenuPage==18) eMenuBacklight=eMenuBacklight-1>-1?eMenuBacklight-1:0;
    else if(eMenuPage==19) {
      // Itens visíveis: 2,3
      const byte adv[] = {2,3};
      const byte adv_n = 2;
      byte cur = 0;
      for(byte i=0;i<adv_n;i++) if(adv[i]==eMenuAdvanced){cur=i;break;}
      eMenuAdvanced = adv[(cur + adv_n - 1) % adv_n];
    }
    else if(eMenuPage==20) eMenuBackup=eMenuBackup-1>-1?eMenuBackup-1:8;
    else if(eMenuPage==21) eMenuBuzzer=eMenuBuzzer-1>-1?eMenuBuzzer-1:0;
    else if(eMenuPage==23) eMenuMidi=eMenuMidi-1>-1?eMenuMidi-1:1;
    else if(eMenuPage==24) {
      eMenuProMicroParam = (eMenuProMicroParam - 1 > -1) ? eMenuProMicroParam - 1 : 11;
      // param 10 = XtakCancel — agora visível, não pula mais
    }
    else if(eMenuPage==26) eMenuBotoes=eMenuBotoes-1>-1?eMenuBotoes-1:0;  // *** NOVO: Página 26 ***
    else if(eMenuPage==27) eMenuDigital=eMenuDigital-1>-1?eMenuDigital-1:6;  // *** NOVO: Página 27 ***
    else if(eMenuPage==28) {
      eMenuChokeNote = (eMenuChokeNote - 1 > -1) ? eMenuChokeNote - 1 : 3;
    }  // *** Página 28 (0-3=notas) ***
    // *** MENU XCANCEL (página 34) — navega entre os 16 pares para trás ***
    else if(eMenuPage==XCANCEL_PAGE && eMenuSelect==1) {
      if(xcancelPar == 0) xcancelPar = XPAIR_COUNT - 1;
      else xcancelPar--;
      needsRedraw = true;
    }
    // *** MENU TIMING (página 35) — navega entre os 3 parâmetros para trás ***
    else if(eMenuPage==MENU_TIMING_PAGE && eMenuSelect==1) {
      menuTimingParam = (menuTimingParam == 0) ? 2 : menuTimingParam - 1;
      needsRedraw = true;
    }
    else if(eMenuPage==25) {
      // *** CORRIGIDO: Navegação para trás com verificação de wraparound ***
      
      // ✅ DEBUG: Pisca LED para confirmar que entrou aqui
      digitalWrite(13, HIGH);
      delay(100);
      digitalWrite(13, LOW);
      
      // ✅ CRÍTICO: byte não pode ser negativo! 0-1 = 255 (wraparound)
      if(eMenuConfig == 0) {
        eMenuConfig = 12;  // Se está em 0, volta para 12 (último item — XCancel removido)
      } else {
        eMenuConfig = eMenuConfig - 1;  // Decrementa normalmente
      }
      
      if(eMenuConfig == 4) eMenuConfig = 3;  // Pula Padrao Midi (removido)
      // ✅ PULA o índice 6 se Pads Aux está DESATIVADO (voltando de 7 para 5)
      if(!menuPadsAuxiliarAtivo && eMenuConfig == 6) {
        eMenuConfig = 5;  // Volta para MIDI Output
      }
    }
    else if(eMenuPage>=2 && eMenuPage<18) {
      eMenuPin=eMenuPin-1>-1?eMenuPin-1:18;
      if(eMenuPin==9) eMenuPin=7;  // pin 9 removido (alias)
      if(eMenuPin==8) eMenuPin=7;  // pin 8 removido (Xtalk)
      // Itens 16/17/18: SÓ no pad A2 (page==4) — em qualquer outro pad pula para 15 (ou 14)
      if(eMenuPin==18 && eMenuPage!=4) eMenuPin=(eMenuPage==2)?15:14;
      if(eMenuPin==17 && eMenuPage!=4) eMenuPin=(eMenuPage==2)?15:14;
      if(eMenuPin==16 && eMenuPage!=4) eMenuPin=(eMenuPage==2)?15:14;
      // InvSensor (15): só A0 (page==2)
      if(eMenuPin==15 && eMenuPage!=2) eMenuPin=14;
    }
  }
  else if(eMenuSelect==2) 
  {
    if(eMenuPage==1) 
    {
      if(eMenuGeneral==1) { GeneralXtalk=(GeneralXtalk-1>-1)?GeneralXtalk-1:7; markGeneralChanged(); }
      else if(eMenuGeneral==3) { HHThresoldSensor[0]=((HHThresoldSensor[0]-1)>-1)?HHThresoldSensor[0]-1:127; markHHChanged(); }
      else if(eMenuGeneral==4) { HHThresoldSensor[1]=((HHThresoldSensor[1]-1)>-1)?HHThresoldSensor[1]-1:127; markHHChanged(); }
      else if(eMenuGeneral==5) { HHThresoldSensor[2]=((HHThresoldSensor[2]-1)>-1)?HHThresoldSensor[2]-1:127; markHHChanged(); }
      else if(eMenuGeneral==6) { HHThresoldSensor[3]=((HHThresoldSensor[3]-1)>-1)?HHThresoldSensor[3]-1:127; markHHChanged(); }
      else if(eMenuGeneral==7) { NSensor=((NSensor-1)>-1)?(NSensor-1):5; markGeneralChanged(); }
    }
    else if(eMenuPage==18) 
    {
      if(eMenuBacklight==0) { backlightMode=(backlightMode-1>-1)?backlightMode-1:1; markBacklightChanged(); }
    }
    // *** Rimshot DualPd - opções visíveis: 0,1,2,3,5,7,9,10,11,12 ***
    else if(eMenuPage==19)
    {
      if(eMenuAdvanced==0) { ENABLE_NOTE_37_38_TO_40=(ENABLE_NOTE_37_38_TO_40-1>-1)?ENABLE_NOTE_37_38_TO_40-1:1; markAdvancedChanged(); }
      else if(eMenuAdvanced==1) { ENABLE_RIMSHOT_38_TO_40=(ENABLE_RIMSHOT_38_TO_40-1>-1)?ENABLE_RIMSHOT_38_TO_40-1:1; markAdvancedChanged(); }
      else if(eMenuAdvanced==2) { ENABLE_NOTE_101_TO_102=(ENABLE_NOTE_101_TO_102-1>-1)?ENABLE_NOTE_101_TO_102-1:1; markAdvancedChanged(); }
      else if(eMenuAdvanced==3) { ENABLE_NOTE_103_TO_104=(ENABLE_NOTE_103_TO_104-1>-1)?ENABLE_NOTE_103_TO_104-1:1; markAdvancedChanged(); }
      // else if(eMenuAdvanced==4) { CUSTOM_NOTE_37=(CUSTOM_NOTE_37-1>-1)?CUSTOM_NOTE_37-1:127; markAdvancedChanged(); }  // OCULTO
      else if(eMenuAdvanced==5) { CUSTOM_NOTE_40=(CUSTOM_NOTE_40-1>-1)?CUSTOM_NOTE_40-1:127; if(rimSub==1) CUSTOM_RIMSHOT_FORCE_NOTE=CUSTOM_NOTE_40; markAdvancedChanged(); }
      // else if(eMenuAdvanced==6) { CUSTOM_RIMSHOT_FORCE_NOTE=(CUSTOM_RIMSHOT_FORCE_NOTE-1>-1)?CUSTOM_RIMSHOT_FORCE_NOTE-1:127; markAdvancedChanged(); }  // OCULTO
      else if(eMenuAdvanced==7) { VELOCITY_THRESHOLD_37_38=(VELOCITY_THRESHOLD_37_38-1>-1)?VELOCITY_THRESHOLD_37_38-1:127; markAdvancedChanged(); }
      // 8 oculto
      else if(eMenuAdvanced==9) {
        if(DETECTION_WINDOW_MS >= 1) DETECTION_WINDOW_MS -= 1;
        else DETECTION_WINDOW_MS = 18;
        markAdvancedChanged();
      }
      else if(eMenuAdvanced==10) {
        if(MAX_NOTE_AGE_MS > 1) MAX_NOTE_AGE_MS--;
        else MAX_NOTE_AGE_MS = 30;
        markAdvancedChanged();
      }
      else if(eMenuAdvanced==11) {
        if(BLOCK_WINDOW_MS >= 10) BLOCK_WINDOW_MS -= 10;
        else BLOCK_WINDOW_MS = 500;
        markAdvancedChanged();
      }
      else if(eMenuAdvanced==12) {
        ENABLE_NOTE_37_38_TO_40=(ENABLE_NOTE_37_38_TO_40-1>-1)?ENABLE_NOTE_37_38_TO_40-1:1;
        markAdvancedChanged();
      }
      // *** REMOVIDO: opções USB/TX1 ***
    }
    // *** MENU XCANCEL (página 34) — decrementa parâmetro do par selecionado ***
    else if(eMenuPage==XCANCEL_PAGE && eMenuSelect==2)
    {
      switch(xcancelParam) {
        case 0: // Source
          if(xpairRam[xcancelPar].source == 255) xpairRam[xcancelPar].source = 15;
          else if(xpairRam[xcancelPar].source == 0) xpairRam[xcancelPar].source = 255;
          else xpairRam[xcancelPar].source--;
          break;
        case 1: // Target
          if(xpairRam[xcancelPar].target == 255) xpairRam[xcancelPar].target = 15;
          else if(xpairRam[xcancelPar].target == 0) xpairRam[xcancelPar].target = 255;
          else xpairRam[xcancelPar].target--;
          break;
        case 2: // ghostVel: 0-100 (windowMs=120 fixo, não aparece)
          if(xpairRam[xcancelPar].ghostVel == 0) xpairRam[xcancelPar].ghostVel = 100;
          else xpairRam[xcancelPar].ghostVel--;
          break;
      }
      SaveXCancelEEPROM();  // *** SALVA NA EEPROM para persistir após desligar ***
      needsRedraw = true;
    }
    // *** MENU TIMING (página 35) — decrementa o valor do parâmetro selecionado ***
    else if(eMenuPage==MENU_TIMING_PAGE && eMenuSelect==2)
    {
      switch(menuTimingParam) {
        case 0: // DETECTION_WINDOW_MS: 1-30 ms
          DETECTION_WINDOW_MS = (DETECTION_WINDOW_MS > 1) ? DETECTION_WINDOW_MS - 1 : 30;
          markAdvancedChanged();
          break;
        case 1: // MAX_NOTE_AGE_MS: 1-60 ms
          MAX_NOTE_AGE_MS = (MAX_NOTE_AGE_MS > 1) ? MAX_NOTE_AGE_MS - 1 : 60;
          markAdvancedChanged();
          break;
        case 2: // BLOCK_WINDOW_MS: 10-500 ms (passo 10)
          BLOCK_WINDOW_MS = (BLOCK_WINDOW_MS > 10) ? BLOCK_WINDOW_MS - 10 : 500;
          markAdvancedChanged();
          break;
      }
      needsRedraw = true;
    }
    else if(eMenuPage==20) 
    {
      if(eMenuBackup>=0 && eMenuBackup<=2) {
        saveBackupWithCache(eMenuBackup+1);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Preset ");
        lcd.print(eMenuBackup+1);
        lcd.setCursor(0,1);
        MenuString(S_SALVO,false);
        delay(1500);
        needsRedraw = true;
      }
      else if(eMenuBackup>=3 && eMenuBackup<=5) {
        byte presetNum = eMenuBackup-2;
        if(BackupExists(presetNum)) {
          restoreBackupWithCache(presetNum);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Preset ");
          lcd.print(presetNum);
          lcd.setCursor(0,1);
          MenuString(S_RESTAURADO,false);
          delay(1500);
          needsRedraw = true;
        }
      }
      else if(eMenuBackup>=6 && eMenuBackup<=8) {
        byte factoryNum = eMenuBackup - 2;  // 6->4, 7->5, 8->6
        RestoreFactoryPreset(factoryNum);
        lcd.clear();
        lcd.setCursor(0,0);
        char buf[13];
        getFactoryPresetName(factoryNum, buf);
        lcd.print(buf);
        lcd.setCursor(0,1);
        MenuString(S_RESTAURADO,false);
        delay(1500);
        needsRedraw = true;
      }
      // *** itens 6/7/8: entrada por A+B juntos no Menu() ***
    }
    else if(eMenuPage==21)
    {
      if(eMenuBuzzer==0) { 
        #if BUZZER
        buzzerEnabled=(buzzerEnabled-1>-1)?buzzerEnabled-1:1; 
        markBuzzerChanged();
        forceBuzzerSave();
        if(buzzerEnabled) {
          playBeep();
        }
        #endif
      }
    }
    // *** Menu Saída MIDI (23) - MANTIDO ***
    else if(eMenuPage==23)
    {
      if(eMenuMidi==0) { 
        MIDI_USB_ENABLED=(MIDI_USB_ENABLED-1>-1)?MIDI_USB_ENABLED-1:1;
        markMidiOutputChanged();
      }
      else if(eMenuMidi==1) {
        MIDI_TX1_ENABLED=(MIDI_TX1_ENABLED-1>-1)?MIDI_TX1_ENABLED-1:1;
        markMidiOutputChanged();
      }
    }
    // *** NOVO: Menu Botoes (26) ***
    else if(eMenuPage==26)
    {
      if(eMenuBotoes==0) {
        modoBotoes = (modoBotoes - 1 > -1) ? modoBotoes - 1 : 1;  // Alterna entre 0 e 1
        SaveModoBotoesEEPROM();
        needsRedraw = true;  // ✅ Força redesenho para aplicar textos dinâmicos
      }
    }
    // *** Pad Digital N (27) ***
    else if(eMenuPage==27)
    {
      digitalPadNotes[eMenuDigital] = (digitalPadNotes[eMenuDigital] - 1 + 128) % 128;
      // Sincroniza ZoneDual/TriZone — nota Aux e NoteBlock acompanham
      if(eMenuDigital < ZONE_DUAL_PAIRS) {
        zoneDual_NoteAux[eMenuDigital]   = digitalPadNotes[eMenuDigital];
        zoneDual_NoteBlock[eMenuDigital] = Pin[zoneDual_PadIdx[eMenuDigital]].Note;
      }
      SaveDigitalNotesEEPROM();
    }
    // *** NOVO: Notas Choke (28) ***
    else if(eMenuPage==28)
    {
      if(eMenuChokeNote <= 3) {
        chokeNotes[eMenuChokeNote] = (chokeNotes[eMenuChokeNote] - 1 + 128) % 128;
        SaveChokeNotesEEPROM();
      }
    }
    // *** PRO MICRO: Diminui valor do parâmetro ***
    else if(eMenuPage==24)
    {
      int currentValue = 0;
      byte paramCode = 0;
      
      switch(eMenuProMicroParam) {
        case 0: // Note
          currentValue = proMicroPads[eMenuProMicro].note;
          paramCode = PARAM_NOTE;
          break;
        case 1: // Threshold
          currentValue = proMicroPads[eMenuProMicro].threshold;
          paramCode = PARAM_THRESHOLD;
          break;
        case 2: // ScanTime
          currentValue = proMicroPads[eMenuProMicro].scanTime;
          paramCode = PARAM_SCANTIME;
          break;
        case 3: // MaskTime
          currentValue = proMicroPads[eMenuProMicro].maskTime;
          paramCode = PARAM_MASKTIME;
          break;
        case 4: { // Retrigger — Aut(255)→16→15→...→0→Aut(255)
          byte r = proMicroPads[eMenuProMicro].retrigger;
          if(r == 255) r = 16;
          else if(r == 0) r = 255;
          else r--;
          updateProMicroParameter(eMenuProMicro, PARAM_RETRIGGER, r);
          return;
        }
        case 5: // Curve (0-4)
          currentValue = proMicroPads[eMenuProMicro].curve;
          currentValue = (currentValue - 1 + 5) % 5;
          updateProMicroParameter(eMenuProMicro, PARAM_CURVE, currentValue);
          return;
        case 6: // CurveForm
          currentValue = proMicroPads[eMenuProMicro].curveForm;
          paramCode = PARAM_CURVEFORM;
          break;
        case 7: // Gain
          currentValue = proMicroPads[eMenuProMicro].gain;
          paramCode = PARAM_GAIN;
          break;
        case 8: // Type (0-1)
          currentValue = proMicroPads[eMenuProMicro].type;
          currentValue = (currentValue + 1) % 2;
          updateProMicroParameter(eMenuProMicro, PARAM_TYPE, currentValue);
          return;
        case 9: // Channel
          currentValue = proMicroPads[eMenuProMicro].channel;
          paramCode = PARAM_CHANNEL;
          break;
        case 10: // XtakCancel (XCancel Global) — 0-127
          currentValue = proMicroPads[eMenuProMicro].xcanCost;
          paramCode = PARAM_XCANCOST;
          break;
        case 11: // Name
          #if defined(__AVR__)
          bool hasCustom = (EEPROM.read(875 + eMenuProMicro) == 1);
          #else
          bool hasCustom = false;
          #endif
          if(hasCustom) {
            startProMicroConfirmation(eMenuProMicro);
          } else {
            byte currentIdx = proMicroPads[eMenuProMicro].nameIndex;
            proMicroPads[eMenuProMicro].nameIndex = (currentIdx == 0) ? 24 : currentIdx - 1;
            saveProMicroPadToEEPROM(eMenuProMicro);
          }
          return;
      }
      
      currentValue = (currentValue - 1 + 128) % 128;
      updateProMicroParameter(eMenuProMicro, paramCode, currentValue);
    }
    else if(eMenuPage>=2 && eMenuPage<18) 
    {
      byte pinIndex = eMenuPage - 2;
      switch(eMenuPin) 
      {
        case 0: // Note
          SUB(Note);
          if(pinIndex == 2) {
            CUSTOM_NOTE_38 = Pin[2].Note;
            markAdvancedChanged();
          }
          if(pinIndex == 3) {
            CUSTOM_NOTE_37 = Pin[3].Note;
            markAdvancedChanged();
          }
          break;
        case 1: SUB(Thresold); break;
        case 2: SUB(ScanTime); break;
        case 3: SUB(MaskTime); break;
        case 4: {
          // Aut(255)→16→15→...→0→Aut(255)
          byte &r = Pin[eMenuPage-2].Retrigger;
          if(r == 255) r = 16;
          else if(r == 0) r = 255;
          else r--;
          markPinChanged(eMenuPage-2);
          break;
        }
        case 5: Pin[eMenuPage-2].Curve=(curve)(Pin[eMenuPage-2].Curve-1>-1?Pin[eMenuPage-2].Curve-1:4); markPinChanged(eMenuPage-2); break;
        case 6: SUB(CurveForm); break;
        case 7: Pin[eMenuPage-2].XCanCost = (Pin[eMenuPage-2].XCanCost > 0) ? Pin[eMenuPage-2].XCanCost - 1 : 127; markPinChanged(eMenuPage-2); break;
        case 8: break;  // Xtalk removido — não faz nada
        case 9: break;  // alias removido — não faz nada
        case 10: { byte t=Pin[pinIndex].Type; if(t==0) t=15; else if(t==15) t=5; else if(t>0 && t<=5) t--; else t=0; Pin[pinIndex].Type=(type)t; markPinChanged(pinIndex); } break;
        case 11: // VeMinimo — decrementa 0-60 com wrap
          {
            byte vm = Pin[pinIndex].VelMinimo;
            if(vm > 0) vm--; else vm = 100;
            Pin[pinIndex].VelMinimo = vm;
            #if defined(__AVR__)
            EEPROM.write(652 + pinIndex, vm);
            #endif
            markPinChanged(pinIndex);
          }
          break;
        case 12: SUB(ChokeNote); break;
        case 13: SUB(Channel); break;
        case 15: // InvertSensor — alterna 0/1
          {
            Pin[pinIndex].InvertSensor = Pin[pinIndex].InvertSensor ? 0 : 1;
            #if defined(__AVR__)
            EEPROM.write(669 + pinIndex, Pin[pinIndex].InvertSensor);
            EEPROM.write(685, 0xCC); // *** CORREÇÃO: atualiza flag de validade ***
            #endif
            markPinChanged(pinIndex);
          }
          break;
        case 16: // Rimshot Comp/Forc — só A2
          rimSub = rimSub ? 0 : 1;
          #if defined(__AVR__)
          EEPROM.write(EEPROM_RIMSUB_MODE, rimSub);
          #endif
          // Comp(0): ativa rimshot completo (38+nota), desativa forçado
          // Forc(1): ativa rimshot forçado (vel>125), desativa completo
          if(rimSub == 0) {
            ENABLE_NOTE_37_38_TO_40 = 1;
            ENABLE_RIMSHOT_38_TO_40 = 0;
          } else {
            ENABLE_NOTE_37_38_TO_40 = 0;
            ENABLE_RIMSHOT_38_TO_40 = 1;
            CUSTOM_RIMSHOT_FORCE_NOTE = CUSTOM_NOTE_40; // Forc: sincroniza ao ativar
          }
          markAdvancedChanged();
          break;
        case 17: // RishotNote — só A2
          CUSTOM_NOTE_40 = (CUSTOM_NOTE_40 - 1 + 128) % 128;
          if(rimSub == 1) CUSTOM_RIMSHOT_FORCE_NOTE = CUSTOM_NOTE_40; // Forc: sincroniza nota forçada
          markAdvancedChanged();
          break;
        case 18: // RishotVel — velocidade mínima rimshot (só A2)
          {
            VELOCITY_THRESHOLD_37_38 = (VELOCITY_THRESHOLD_37_38 - 1 + 128) % 128;
            markAdvancedChanged();
          }
          break;
        case 14:
          if(pinIndex < 16) {
            if(selected_names[pinIndex] == 0) {
              selected_names[pinIndex] = NUM_AVAILABLE_NAMES - 1;
            } else {
              selected_names[pinIndex]--;
            }
            if(selected_names[pinIndex] >= NUM_AVAILABLE_NAMES) {
              selected_names[pinIndex] = 0;
            }
            markNameChanged();
          }
          break;
      }
    }
  }
}
///==============================
//    PARTE 3/3 - FUNÇÃO DRAW E MENU LOG - CORRIGIDO
//    *** REMOVIDO USB/TX1 DO MENU RIMSHOT (SÓ FICA EM SAÍDA MIDI) ***
//    *** MENU PRO MICRO INTEGRADO DIRETAMENTE NO DRAW() ***
//==============================

//==============================
//    FUNÇÃO DRAW - COM PERSONA E SEM DUPLICAÇÃO
//==============================

void Draw()
{
  // *** MENU TIMING (página 35) — 3 ajustes: DetWin / NotAge / BlkWin ***
  // Linha 0: nome do parâmetro (ex: "R>37+38-Ms")
  // Linha 1: valor atual  <valor> (selecionado/editando)
  if(eMenuPage == MENU_TIMING_PAGE) {
    lcd.clear();
    lcd.setCursor(0, 0);
    switch(menuTimingParam) {
      case 0: MenuString(S_DETECTION_WINDOW, eMenuSelect==1); break;
      case 1: MenuString(S_MAX_NOTE_AGE,     eMenuSelect==1); break;
      case 2: MenuString(S_BLOCK_WINDOW,     eMenuSelect==1); break;
    }
    lcd.setCursor(0, 1);
    switch(menuTimingParam) {
      case 0: MenuInt((int)DETECTION_WINDOW_MS, eMenuSelect==2); break;
      case 1: MenuInt((int)MAX_NOTE_AGE_MS,     eMenuSelect==2); break;
      case 2: MenuInt((int)BLOCK_WINDOW_MS,     eMenuSelect==2); break;
    }
    lastLCDUpdate = millis();
    return;
  }

  // *** MENU XCANCEL (página 34) — Crosstalk Cancellation ***
  // Linha 0: "XCancel P:XX/16"  (par atual / total)
  // Linha 1: nível 1 = ">SrcXX TgtXX"   (selecionando par)
  //          nível 2 = parâmetro editando (param 0=Src 1=Tgt 2=Jnl 3=Vel)
  if(eMenuPage == XCANCEL_PAGE) {
    lcd.setCursor(0, 0);
    lcd.print("XCancel P:");
    if(xcancelPar < 9) lcd.print('0');
    lcd.print(xcancelPar + 1);
    lcd.print('/');
    lcd.print(XPAIR_COUNT);
    lcd.print(' ');

    lcd.setCursor(0, 1);
    bool disabled = (xpairRam[xcancelPar].source == 255);

    if(eMenuSelect == 1) {
      // Nível seleção: mostra resumo do par
      if(disabled) {
        lcd.print(">-- Desativado-");
      } else {
        lcd.print(">A");
        lcd.print(xpairRam[xcancelPar].source);
        lcd.print("->");
        lcd.print("A");
        lcd.print(xpairRam[xcancelPar].target);
        lcd.print(" V<");
        lcd.print(xpairRam[xcancelPar].ghostVel);
        // Preenche resto
        byte usado = 2 + (xpairRam[xcancelPar].source<10?1:2) + 2 + 1 + (xpairRam[xcancelPar].target<10?1:2) + 3 + (xpairRam[xcancelPar].ghostVel<10?1:2);
        for(byte k = usado; k < 15; k++) lcd.print(' ');
      }
    } else {
      // Nível edição: mostra parâmetro destacado (Jnl fixo=120ms, não aparece)
      static const char* paramLabel[] = {"Src", "Tgt", "Vel"};
      lcd.print(paramLabel[xcancelParam]);
      lcd.print(':');
      bool editing = true;
      switch(xcancelParam) {
        case 0:
          if(xpairRam[xcancelPar].source == 255) MenuString(PSTR("Off "), editing);
          else { lcd.print('A'); MenuInt(xpairRam[xcancelPar].source, editing); }
          break;
        case 1:
          if(xpairRam[xcancelPar].target == 255) MenuString(PSTR("Off "), editing);
          else { lcd.print('A'); MenuInt(xpairRam[xcancelPar].target, editing); }
          break;
        case 2:
          lcd.print('<');
          MenuInt(xpairRam[xcancelPar].ghostVel, editing);
          break;
      }
      // Preenche resto da linha
      lcd.setCursor(14, 1);
      lcd.print("  ");
    }
    lastLCDUpdate = millis();
    return;
  }

  lcd.clear();
  lcd.noAutoscroll();
  Diagnostic=false;
  
  // *** PROTEÇÃO: Corrige eMenuConfig se estiver em opção inválida ***
  if(eMenuPage == 25) {
    // ✅ CORREÇÃO CRÍTICA: Se Pads Aux DESATIVADO e está na opção 6, PULA PARA 7
    if(!menuPadsAuxiliarAtivo && eMenuConfig == 6) {
      eMenuConfig = 7;  // PULA PARA FRENTE, não volta!
    }
    // PROTEÇÃO EXTRA: Se de alguma forma passou de 14, volta para 0
    if(eMenuConfig > 14) {
      eMenuConfig = 0;
    }
  }
  
  lcd.setCursor(0,0);
  
  if(eMenuPage==0) {
    return;
  }
  // *** REMOVIDO: Página 1 não é mais usada ***
  else if(eMenuPage==1) {
    return;
  }
  else if(eMenuPage>=2 && eMenuPage<18) {
    byte pinIndex = eMenuPage - 2;
    if(pinIndex < 16) {
      PrintCustomName(pinIndex, false);
    } else {
      MenuString(S_PIN,false);
    }
    MenuInt(pinIndex,eMenuSelect==0);
  }
  else if(eMenuPage==18) MenuString(S_BACKLIGHT,eMenuSelect==0);
  else if(eMenuPage==19) MenuString(S_ADVANCED,eMenuSelect==0);
  else if(eMenuPage==20) MenuString(S_BACKUP_CONF,eMenuSelect==0);
  else if(eMenuPage==21) MenuString(S_BUZZER_MENU,eMenuSelect==0);
  else if(eMenuPage==23) MenuString(S_MIDI_OUTPUT_MENU,eMenuSelect==0);
  else if(eMenuPage==24) {
    // NÍVEL 0 (selecionando pad) ou navegando parâmetros/valores
    if(!proMicroInPadSelection) {
      // NÍVEL 0: título do menu
      MenuString(S_PROMICRO_MENU, eMenuSelect==0);
    } else {
      // NÍVEL 1/2/3: linha 1 = nome do pad (igual menu principal)
      lcd.print("Pad ");
      lcd.print(eMenuProMicro);
      lcd.print(" ");
      PrintProMicroCustomName(eMenuProMicro, false);
    }
  }
  // *** NOVO: Página 25 = Menu Configurações ***
  else if(eMenuPage==25) {
    MenuString(S_CONFIG_MENU,eMenuSelect==0);
  }
  // *** NOVO: Página 26 = Menu Botoes ***
  else if(eMenuPage==26) {
    MenuString(S_BOTOES_MENU,eMenuSelect==0);
  }
  // *** NOVO: Página 27 = Pad Digital N ***
  else if(eMenuPage==27) {
    MenuString(S_DIGITAL_MENU,eMenuSelect==0);
  }
  // *** NOVO: Página 28 = Notas Choke ***
  else if(eMenuPage==28) {
    MenuString(S_CHOKE_MENU,eMenuSelect==0);
  }
  else if(eMenuPage==29) {
    MenuString(S_RESET_MENU, false);
  }
  // *** Monitor Velocity (30) - Draw não é chamado (tratado inline no Menu()) ***
  else if(eMenuPage==30) {
    return; // não usa Draw() padrão
  }
  // *** Monitor Hi-Hat (31) - Draw não é chamado (tratado inline no Menu()) ***
  else if(eMenuPage==31) {
    return; // não usa Draw() padrão
  }
  
  lcd.setCursor(0,1);
  
  if(eMenuPage==0) {
    return;
  }
  // *** REMOVIDO: Página 1 não é mais usada ***
  else if(eMenuPage==1) {
    return;
  }
  else if(eMenuPage>=2 && eMenuPage<18) {
    if(Mode==Tool) return;
    byte pinIndex = eMenuPage - 2;
    switch(eMenuPin) {
      case 0: MenuString(S_NOTE,eMenuSelect==1); break;
      case 1: MenuString(S_THRESOLD,eMenuSelect==1); break;
      case 2: MenuString(S_SCANTIME,eMenuSelect==1); break;
      case 3: MenuString(S_MASKTIME,eMenuSelect==1); break;
      case 4: MenuString(S_RETRIG,eMenuSelect==1); break;
      case 5: MenuString(S_CURVE,eMenuSelect==1); break;
      case 6: MenuString(S_CURVEF,eMenuSelect==1); break;
      case 7: MenuString(S_XCANCOST,eMenuSelect==1); break;
      case 9: break;  // removido
      case 10: MenuString(S_TYPE,eMenuSelect==1); break;
      case 11: MenuString(S_VE_MINIMO,eMenuSelect==1); break;     // *** VeMinimo por pad ***
      case 12:
        if(Pin[pinIndex].Type==Piezo)
          MenuString(S_GAIN,eMenuSelect==1);
        else
          MenuString(S_CKNOTE,eMenuSelect==1); 
        break;
      case 13: MenuString(S_CHANNEL,eMenuSelect==1); break;
      case 15: MenuString(S_INVERT_SENSOR,eMenuSelect==1); break; // *** InvertSensor ***
      case 16: MenuString(S_RIMSHOT_MENU, eMenuSelect==1); break; // *** Rimshot (só A2) ***
      case 17: MenuString(S_RIMSHOT_NOTE, eMenuSelect==1); break; // *** RishotNote (só A2) ***
      case 18: MenuString(S_VEL_THRESH_PAD,eMenuSelect==1); break; // *** RishotVel (só A2) ***
      case 14: MenuString(S_NAME,eMenuSelect==1); break;
    }
  }
  else if(eMenuPage==18) {
    MenuString(S_BACKLIGHT_MODE,eMenuSelect==1);
  }
  // *** Menu Dual Pad: DPd101-102 e DPd103-104 ***
  else if(eMenuPage==19) {
    switch(eMenuAdvanced) {
      case 2: MenuString(S_NOTE_101_102, eMenuSelect==1); break;
      case 3: MenuString(S_NOTE_103_104, eMenuSelect==1); break;
      default: MenuString(S_NOTE_101_102, eMenuSelect==1); break;
    }
  }
  else if(eMenuPage==20) {
    if(eMenuBackup>=0 && eMenuBackup<=2) {
      if(eMenuSelect==1) lcd.print('<');
      lcd.print("Preset "); lcd.print(eMenuBackup+1);
      if(eMenuSelect==1) lcd.print('>');
    } else if(eMenuBackup>=3 && eMenuBackup<=5) {
      if(eMenuSelect==1) lcd.print('<');
      lcd.print("Preset "); lcd.print(eMenuBackup-2);
      if(eMenuSelect==1) lcd.print('>');
    } else if(eMenuBackup>=6 && eMenuBackup<=8) {
      // Presets de fábrica 4/5/6 — só restaurar
      if(eMenuSelect==1) lcd.print('<');
      char buf[13];
      getFactoryPresetName(eMenuBackup - 2, buf);  // 6->4, 7->5, 8->6
      lcd.print(buf);
      if(eMenuSelect==1) lcd.print('>');
    }
  }
  else if(eMenuPage==21) {
    MenuString(S_BUZZER_STATUS,eMenuSelect==1);
  }
  // *** Menu Saída MIDI (23) - MANTIDO ***
  else if(eMenuPage==23) {
    if(eMenuMidi==0) {
      MenuString(S_MIDI_USB,eMenuSelect==1);
    } else if(eMenuMidi==1) {
      MenuString(S_MIDI_TX1,eMenuSelect==1);
    }
  }
  // *** NOVO: Menu Botoes (26) ***
  else if(eMenuPage==26) {
    if(eMenuBotoes==0) {
      MenuString(S_MODE,eMenuSelect==1);  // Mostra "Modo"
    }
  }
  // *** NOVO: Pad Digital N (27) ***
  else if(eMenuPage==27) {
    switch(eMenuDigital) {
      case 0: MenuString(S_DIG1,eMenuSelect==1); break;
      case 1: MenuString(S_DIG2,eMenuSelect==1); break;
      case 2: MenuString(S_DIG3,eMenuSelect==1); break;
      case 3: MenuString(S_DIG4,eMenuSelect==1); break;
      case 4: MenuString(S_DIG5,eMenuSelect==1); break;
      case 5: MenuString(S_DIG6,eMenuSelect==1); break;
      case 6: MenuString(S_DIG7,eMenuSelect==1); break;
    }
  }
  // *** NOVO: Notas Choke (28) ***
  else if(eMenuPage==28) {
    switch(eMenuChokeNote) {
      case 0: MenuString(S_CHOKE_CRASH1,eMenuSelect==1); break;
      case 1: MenuString(S_CHOKE_CRASH2,eMenuSelect==1); break;
      case 2: MenuString(S_CHOKE_CRASH3,eMenuSelect==1); break;
      case 3: MenuString(S_CHOKE_RIDE,eMenuSelect==1); break;
    }
  }
  // *** RESET PADRÃO (29) ***
  else if(eMenuPage==29) {
    if(modoBotoes == 1) MenuString(S_RESET_CONFIRM_INVERTIDO, false);  
    else                MenuString(S_RESET_CONFIRM_NORMAL,    false);  
  }
  // *** Menu Configurações (25) - OPÇÕES DINÂMICAS ***
  else if(eMenuPage==25) {
    // ✅ NÃO LIMPA A LINHA - apenas não desenha se for opção inválida
    switch(eMenuConfig) {
      case 0: MenuString(S_BACKLIGHT,eMenuSelect==1); break;
      case 1: MenuString(S_ADVANCED,eMenuSelect==1); break;
      case 2: MenuString(S_BACKUP_CONF,eMenuSelect==1); break;
      case 3: MenuString(S_BUZZER_MENU,eMenuSelect==1); break;
      case 4: // item removido — redireciona visualmente para Saída Midi
      case 5: MenuString(S_MIDI_OUTPUT_MENU,eMenuSelect==1); break;
      case 6: 
        if(menuPadsAuxiliarAtivo) {
          MenuString(S_PROMICRO_MENU,eMenuSelect==1);
        }
        // ✅ Se não está ativo, a proteção no Draw() já corrigiu para 7
        break;
      case 7: MenuString(S_BOTOES_MENU,eMenuSelect==1); break;
      case 8: MenuString(S_DIGITAL_MENU,eMenuSelect==1); break;
      case 9: MenuString(S_CHOKE_MENU,eMenuSelect==1); break;
      case 10: MenuString(S_RESET_MENU,eMenuSelect==1); break;
      case 11: MenuString(S_MONITOR_VELOCITY,eMenuSelect==1); break;
      case 12: MenuString(S_MONITOR_HIHAT,eMenuSelect==1); break;
      case 13: break;  // XCancel removido — filtro agora em cada pad (XCanCost)
    }
  }

  lcd.setCursor(12,1);
  
  if(eMenuPage>=2 && eMenuPage<18) {
    byte pinIndex = eMenuPage - 2;
    
    if(eMenuPin==0) MenuInt(Pin[pinIndex].Note,eMenuSelect==2);
    else if(eMenuPin==1) MenuInt(Pin[pinIndex].Thresold,eMenuSelect==2);
    else if(eMenuPin==2) MenuInt(Pin[pinIndex].ScanTime,eMenuSelect==2);
    else if(eMenuPin==3) MenuInt(Pin[pinIndex].MaskTime,eMenuSelect==2);
    else if(eMenuPin==4) {
      if(Pin[pinIndex].Retrigger == 255) MenuString(S_AUT, eMenuSelect==2);
      else MenuInt(Pin[pinIndex].Retrigger, eMenuSelect==2);
    }
    else if(eMenuPin==5) {
      byte curveVal = Pin[pinIndex].Curve;
      if(curveVal > 4) { curveVal = 0; Pin[pinIndex].Curve = (curve)0; markPinChanged(pinIndex); }
      switch(curveVal) {
        case 0: MenuString(S_LIN,eMenuSelect==2); break;
        case 1: MenuString(S_EXP,eMenuSelect==2); break;
        case 2: MenuString(S_LOG,eMenuSelect==2); break;
        case 3: MenuString(S_SGM,eMenuSelect==2); break;
        case 4: MenuString(S_FLT,eMenuSelect==2); break;
        default: MenuString(S_LIN,eMenuSelect==2); break;
      }
    }
    else if(eMenuPin==6) MenuInt(Pin[pinIndex].CurveForm,eMenuSelect==2);
    else if(eMenuPin==7) MenuInt(Pin[pinIndex].XCanCost,eMenuSelect==2);
    else if(eMenuPin==10) {
      lcd.setCursor(6,1);
      switch(Pin[pinIndex].Type) {
        case Piezo: MenuString(S_PIEZO,eMenuSelect==2); break;
        case Switch: MenuString(S_SWITCH,eMenuSelect==2); break;
        case HHC: MenuString(S_HHC,eMenuSelect==2); break;
        case HH: MenuString(S_HH,eMenuSelect==2); break;
        case HHs: MenuString(S_HHS,eMenuSelect==2); break;
        case YSwitch: MenuString(S_YSWITCH,eMenuSelect==2); break;
        case Disabled: MenuString(S_DISABLED,eMenuSelect==2); break;
      }
    }
    else if(eMenuPin==11) MenuInt(Pin[pinIndex].VelMinimo,eMenuSelect==2);  // *** VeMinimo ***
    else if(eMenuPin==12) MenuInt(Pin[pinIndex].ChokeNote,eMenuSelect==2);
    else if(eMenuPin==13) MenuInt(Pin[pinIndex].Channel,eMenuSelect==2);
    else if(eMenuPin==15) {  // *** InvertSensor ***
      if(Pin[pinIndex].InvertSensor)
        MenuString(PSTR("Invertido"),eMenuSelect==2);
      else
        MenuString(PSTR("Normal   "),eMenuSelect==2);
    }
    else if(eMenuPin==16) {  // *** Rimshot Comp/Forc (só A2) ***
      lcd.setCursor(7,1);
      if(rimSub==0) MenuString(S_RIMSHOT_COMP, eMenuSelect==2);
      else          MenuString(S_RIMSHOT_FORC,  eMenuSelect==2);
    }
    else if(eMenuPin==17) {  // *** RishotNote (só A2) ***
      MenuInt(CUSTOM_NOTE_40, eMenuSelect==2);
    }
    else if(eMenuPin==18) {  // *** RishotVel — velocidade mínima rimshot ***
      MenuInt(VELOCITY_THRESHOLD_37_38, eMenuSelect==2);
    }
    else if(eMenuPin==14) {
      if(pinIndex < 16) {
        lcd.setCursor(5,1);
        PrintCustomName(pinIndex, eMenuSelect==2);
      }
    }
  }
  else if(eMenuPage==18) {
    if(eMenuBacklight==0) {
      lcd.setCursor(5,1);
      if(backlightMode==0) MenuString(S_30_SEGUNDOS,eMenuSelect==2);
      else MenuString(S_SEMPRE_ACESO,eMenuSelect==2);
    }
  }
  // *** Menu Dual Pad: DPd101-102 e DPd103-104 ***
  else if(eMenuPage==19) {
    if(eMenuAdvanced==3) MenuInt(ENABLE_NOTE_103_TO_104, eMenuSelect==2);
    else                 MenuInt(ENABLE_NOTE_101_TO_102, eMenuSelect==2);
  }
  else if(eMenuPage==20) {
    if(eMenuBackup>=0 && eMenuBackup<=2) {
      MenuString(S_SALVAR,eMenuSelect==2);
    } else if(eMenuBackup>=3 && eMenuBackup<=5) {
      byte presetNum = eMenuBackup-2;
      if(BackupExists(presetNum)) {
        MenuString(S_RESTAURAR,eMenuSelect==2);
      } else {
        MenuString(S_NAO_EXISTE,eMenuSelect==2);
      }
    } else if(eMenuBackup>=6 && eMenuBackup<=8) {
      // Presets de fábrica: sempre disponíveis (PROGMEM), só restaurar
      MenuString(S_RESTAURAR,eMenuSelect==2);
    }
  }
  else if(eMenuPage==21) {
    if(eMenuBuzzer==0) {
      lcd.setCursor(7,1);
      #if BUZZER
      if(buzzerEnabled) MenuString(S_BUZZER_ATIVADO,eMenuSelect==2);
      else MenuString(S_BUZZER_DESATIVADO,eMenuSelect==2);
      #else
      MenuString(S_BUZZER_DESATIVADO,eMenuSelect==2);
      #endif
    }
  }
  // *** Menu Saída MIDI (23) - MANTIDO ***
  else if(eMenuPage==23) {
    if(eMenuMidi==0) {
      lcd.setCursor(7,1);
      if(MIDI_USB_ENABLED) MenuString(S_ATIVADO,eMenuSelect==2);
      else MenuString(S_DESATIVADO,eMenuSelect==2);
    }
    else if(eMenuMidi==1) {
      lcd.setCursor(7,1);
      if(MIDI_TX1_ENABLED) MenuString(S_ATIVADO,eMenuSelect==2);
      else MenuString(S_DESATIVADO,eMenuSelect==2);
    }
  }
  // *** NOVO: Menu Botoes (26) ***
  else if(eMenuPage==26) {
    if(eMenuBotoes==0) {
      lcd.setCursor(5,1);
      if(modoBotoes == 0) MenuString(S_BOTOES_NORMAL,eMenuSelect==2);
      else MenuString(S_BOTOES_INVERTIDO,eMenuSelect==2);
    }
  }
  // *** NOVO: Pad Digital N (27) - mostra nota ***
  else if(eMenuPage==27) {
    MenuInt(digitalPadNotes[eMenuDigital],eMenuSelect==2);
  }
  // *** NOVO: Notas Choke (28) - mostra nota ou status filtro ***
  else if(eMenuPage==28) {
    if(eMenuChokeNote <= 3) {
      MenuInt(chokeNotes[eMenuChokeNote], eMenuSelect==2);
    }
  }
  // *** Menu Pads Auxiliar (24) ***
  else if(eMenuPage==24) {
    if(Mode==Tool) return;
    
    // Garante que proMicroInPadSelection está ativo
    if(!proMicroInPadSelection) {
      proMicroInPadSelection = true;
      eMenuProMicro = 0;
    }
    
    // Linha 2: sempre mostra parâmetro + valor
    // eMenuSelect==0 → navegando entre pads (parâmetro não destacado)
    // eMenuSelect==1 → parâmetro destacado (navegando parâmetros)
    // eMenuSelect==2 → valor editando
    bool paramSel = (eMenuSelect == 1);
    bool valueSel = (eMenuSelect == 2);
    lcd.setCursor(0, 1);
    switch(eMenuProMicroParam) {
      case 0:
        MenuString(S_NOTE, paramSel);
        lcd.setCursor(12,1); MenuInt(proMicroPads[eMenuProMicro].note, valueSel);
        break;
      case 1:
        MenuString(S_THRESOLD, paramSel);
        lcd.setCursor(12,1); MenuInt(proMicroPads[eMenuProMicro].threshold, valueSel);
        break;
      case 2:
        MenuString(S_SCANTIME, paramSel);
        lcd.setCursor(12,1); MenuInt(proMicroPads[eMenuProMicro].scanTime, valueSel);
        break;
      case 3:
        MenuString(S_MASKTIME, paramSel);
        lcd.setCursor(12,1); MenuInt(proMicroPads[eMenuProMicro].maskTime, valueSel);
        break;
      case 4:
        MenuString(S_RETRIG, paramSel);
        lcd.setCursor(12,1);
        if(proMicroPads[eMenuProMicro].retrigger == 255) MenuString(S_AUT, valueSel);
        else MenuInt(proMicroPads[eMenuProMicro].retrigger, valueSel);
        break;
      case 5:
        MenuString(S_CURVE, paramSel);
        lcd.setCursor(7,1);
        switch(proMicroPads[eMenuProMicro].curve) {
          case 0: MenuString(S_LIN, valueSel); break;
          case 1: MenuString(S_EXP, valueSel); break;
          case 2: MenuString(S_LOG, valueSel); break;
          case 3: MenuString(S_SGM, valueSel); break;
          case 4: MenuString(S_FLT, valueSel); break;
        }
        break;
      case 6:
        MenuString(S_CURVEF, paramSel);
        lcd.setCursor(12,1); MenuInt(proMicroPads[eMenuProMicro].curveForm, valueSel);
        break;
      case 7:
        MenuString(S_GAIN, paramSel);
        lcd.setCursor(12,1); MenuInt(proMicroPads[eMenuProMicro].gain, valueSel);
        break;
      case 8:
        MenuString(S_TYPE, paramSel);
        lcd.setCursor(7,1);
        if(proMicroPads[eMenuProMicro].type == 0) MenuString(S_DISABLED, valueSel);
        else                                       MenuString(S_PIEZO,    valueSel);
        break;
      case 9:
        MenuString(S_CHANNEL, paramSel);
        lcd.setCursor(12,1); MenuInt(proMicroPads[eMenuProMicro].channel, valueSel);
        break;
      case 10: // XtakCancel — XCancel Global dos pads auxiliares
        MenuString(S_XCANCOST, paramSel);
        lcd.setCursor(12,1); MenuInt(proMicroPads[eMenuProMicro].xcanCost, valueSel);
        break;
      case 11:
        MenuString(S_NAME, paramSel);
        lcd.setCursor(5,1);
        PrintProMicroCustomName(eMenuProMicro, valueSel);
        break;
    }
    lastLCDUpdate = millis();
    return;
  }

  lastLCDUpdate = millis();
}

//==============================
//    MENU LOG
//==============================

#if MENU_LOG
void DrawLog(byte x)
{
  lcd.setCursor(0,1);
  if(x==0) {
    MenuString(S_WAIT,false);
    MenuInt(log_Vmax>>3,'(',')');
  }
  else if(x==1) {
    MenuString(S_HITSOFT,false);
    MenuInt(d_tnum,'(',')');
  }
  else if(x==2) {
    MenuString(S_HITHARD,false);
    MenuInt(d_tnum,'(',')');
  }
  else if(x==3) {
    MenuString(S_END,false);
  }
  else if(x==4) {
    MenuString(S_NOISE,false);
    MenuInt(log_Nmax,'(',')');
  }
}


#endif


#endif // USE_LCD