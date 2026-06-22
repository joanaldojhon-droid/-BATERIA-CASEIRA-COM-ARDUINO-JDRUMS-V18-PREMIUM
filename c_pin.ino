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
//=>  • Estrutura de dados dos 16 pads principais                                          <=
//=>  • Configuração de sensibilidade e timing                                             <=
//=>  • Curvas de velocidade                                                               <=
//=>  • Anti-crosstalk entre pads                                                          <=
//=>  • Suporte a HiHat controller                                                         <=
//=========================================================================================//
#include <math.h>  // expf() para decay exponencial

// *** TIMESTAMP FOOTCLOSE — protege HH de vibracoes mecanicas do pedal fechando ***
// Gravado quando HHC detecta Footclose. HH bloqueia entrada em Scan_Time por HH_FOOTCLOSE_GUARD_MS.
#define HH_FOOTCLOSE_GUARD_MS 300
unsigned long hhFootcloseTime = 0;
byte hhLastCC = 127;  // ultimo CC do HHC (0=fechado, 127=aberto)
unsigned long hhMovingTime = 0;  // timestamp do ultimo movimento do pedal
#define HH_MOVING_GUARD_MS 0   // chimbal surdo Xms apos qualquer movimento do pedal
#define HH_CLOSED_GUARD_CC 0   // chimbal surdo quando pedal quase fechado (0-127)

// *** XCANCEL GLOBAL — filtro automatico de crosstalk entre pads ***
// Qualquer pad que dispara registra aqui. Outros pads comparam ao tentar disparar.
// Se MaxReading < gLastStrongHit * fator(XCanCost) dentro da janela → bloqueia.
#define XCANCEL_GLOBAL_WINDOW_MS 120  // janela fixa — mesma do par A1→A2 confirmado
int   gLastStrongHit  = 0;
unsigned long gLastStrongTime = 0;
byte  gLastStrongPad  = 255;  // índice do pad que gerou o último hit forte

// *** SISTEMA ANTI-CROSSTALK DIRECIONAL — 16 PARES SOURCE→TARGET ***
// Cada par monitora uma relação específica entre dois pads físicos.
// Source: pad que você bate. Target: pad que vaza como fantasma.
// Janela: tempo em ms após batida no source dentro do qual o target é monitorado.
// GhostVel: velocity máxima considerada fantasma no target (abaixo disso → bloqueia).
// 255 = par desativado (ignorado).
//
// Como testar: bata em cada pad e observe no monitor de velocity se outro pad dispara.
// Se aparecer nota fantasma, cadastre o par aqui.
//
// Índices: A0=0, A1=1, A2=2, A3=3, A4=4, A5=5, A6=6, A7=7,
//          A8=8, A9=9, A10=10, A11=11, A12=12, A13=13, A14=14, A15=15

#define XPAIR_COUNT 16  // número de pares (não alterar)

struct XtalkPair {
  byte source;    // índice do pad que dispara (0-15), 255 = desativado
  byte target;    // índice do pad que vaza (0-15)
  byte windowMs;  // janela em ms (1-255)
  byte ghostVel;  // velocity máxima considerada fantasma (1-127)
};

// *** TABELA DE PARES — RAM editável (ajustável pelo menu) ***
// Par  | Source | Target | Janela | GhostVel | Descrição
XtalkPair xpairRam[XPAIR_COUNT] = {
  {255, 255,    0,   0  },  //  0: A1(Chimbal) → A2(Caixa)    ← CONFIRMADO
  {255, 255,    0,   0 },  //  1: vazio — testar A1→A3 (aro)
  {255, 255,    0,   0 },  //  2: vazio — testar A8→A2 (ride→caixa)
  {255, 255,    0,   0 },  //  3: vazio — testar A10→A2 (crash1→caixa)
  {255, 255,    0,   0 },  //  4: vazio
  {255, 255,    0,   0 },  //  5: vazio
  {255, 255,    0,   0 },  //  6: vazio
  {255, 255,    0,   0 },  //  7: vazio
  {255, 255,    0,   0 },  //  8: vazio
  {255, 255,    0,   0 },  //  9: vazio
  {255, 255,    0,   0 },  // 10: vazio
  {255, 255,    0,   0 },  // 11: vazio
  {255, 255,    0,   0 },  // 12: vazio
  {255, 255,    0,   0 },  // 13: vazio
  {255, 255,    0,   0 },  // 14: vazio
  {255, 255,    0,   0 },  // 15: vazio
};

// Timestamps da última batida forte por pad (RAM — 16 × 4 bytes = 64 bytes)
unsigned long xpairLastHit[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

#define DualSensor(i) (_DualSensor[(i)&0x07]+((i)&0xF8))
const byte _DualSensor[] = {4,3};

// *** AUTO-CALIBRAÇÃO HHC — RANGE DE BARRAS ***
// Aprende o min/max do sensor do pedal em tempo real.
// Usado SOMENTE para calibrar HH_CLOSED_POINT/HH_OPEN_POINT (barras LCD).
// EEPROM 585 = hhcCalMin,  EEPROM 586 = hhcCalMax
#define EEPROM_HHC_CAL_MIN 585
#define EEPROM_HHC_CAL_MAX 586
byte hhcCalMin = 10;   // mínimo aprendido (pedal totalmente fechado)
byte hhcCalMax = 117;  // máximo aprendido (pedal totalmente aberto)

void SaveHHCCalEEPROM() {
  #if defined(__AVR__)
  EEPROM.write(EEPROM_HHC_CAL_MIN, hhcCalMin);
  EEPROM.write(EEPROM_HHC_CAL_MAX, hhcCalMax);
  #endif
}

void LoadHHCCalEEPROM() {
  #if defined(__AVR__)
  byte vMin = EEPROM.read(EEPROM_HHC_CAL_MIN);
  byte vMax = EEPROM.read(EEPROM_HHC_CAL_MAX);
  if(vMin <= 127) hhcCalMin = vMin;
  if(vMax <= 127 && vMax > hhcCalMin) hhcCalMax = vMax;
  #endif
}

void ResetHHCCal() {
  hhcCalMin = 127;
  hhcCalMax = 0;
  SaveHHCCalEEPROM();
}

// *** MONITOR VELOCITY: seta o sensor antes de enviar MIDI ***
// Declaradas em a_midi.ino — externs para acesso aqui
extern bool monitorVelocityAtivo;
extern volatile byte monitorLastSensor;
inline void setMonitorSensor(byte s) {
  monitorLastSensor = s;  // sempre atualiza — necessário para _isRimshotPad no fastNoteOn
  // (monitorVelocityAtivo controla apenas a exibição no LCD, não o rastreamento do sensor)
}

// *** ZONA DUPLA VIA PAD AUX — externs declarados em l_loop.ino ***
#define ZONE_DUAL_PAIRS 6
extern byte zoneDual_NoteAux[ZONE_DUAL_PAIRS];
extern byte zoneDual_NoteBlock[ZONE_DUAL_PAIRS];
extern byte zoneDual_PadIdx[ZONE_DUAL_PAIRS];
extern bool       zoneDual_Active[ZONE_DUAL_PAIRS];
extern byte digitalPadNotes[7];
extern byte         zoneDual_HoldNote[ZONE_DUAL_PAIRS];
extern bool         zoneDual_HoldFired[ZONE_DUAL_PAIRS];
extern bool         zoneDual_EdgeFired[ZONE_DUAL_PAIRS];
extern byte         chokeNotes[4];
extern byte         triZoneMode;   // 0=Desativado 1=Ativado (h_menu.ino)
extern byte         chokeMode;     // 0=Simp 1=TrZn 2=S+Tz 3=Off (h_menu.ino)
extern byte         a8a9BellBias;  // ajuste A8A9_BELL_BIAS, editável via menu A8 (h_menu.ino)

// *** A8/A9 STRONGEST — acoplado ao triZoneMode e ao Fi-TipBell ***
// triZoneMode==1 (Ati) → TriZone ativo  → A8A9 strongest DESATIVADO
// triZoneMode==0 (Des) → TriZone inativo → A8A9 strongest ATIVO (só a mais forte)
// a8a9BellBias==31 (Des/Fi-TipBell) → filtro força-DESATIVADO, mesmo com TriZone Des
//   (permite testar A8/A9 disparando independentemente sem precisar ativar o TriZone)
#define A8A9_BELLBIAS_DES  31
#define A8A9_STRONGEST  (triZoneMode == 0 && a8a9BellBias != A8A9_BELLBIAS_DES)
#define A8A9_WINDOW_MS   80   // janela de comparação em ms (≤80ms → compara; >80ms → sequencial)

// *** ARBITRAGEM ROLAND-STYLE: energia relativa + bias para A9 (bell/cúpula) ***
// Problema: A8 (bow/borda) e A9 (bell/cúpula) ficam no mesmo PVC rígido.
// Quando bate na cúpula, A8 também vibra. Se ganho de A8 alto → A8 vence pela
// velocity bruta → nota errada dispara.
//
// Solução: comparar energia RELATIVA = velocity / Gain de cada pad.
// Isso normaliza ganhos diferentes entre os dois piezos. A9 recebe um bias fixo
// (A8A9_BELL_BIAS) para compensar sua posição desfavorável no platô.
//
// Ajuste A8A9_BELL_BIAS:
//   0  = sem vantagem para a cúpula (só energia relativa pura)
//   10 = padrão — cúpula vence quando energias forem próximas
//   20 = favorece muito a cúpula
//   → borda disparando ao bater na cúpula: AUMENTAR o bias
//   → cúpula disparando ao bater na borda: DIMINUIR o bias
// *** AGORA AJUSTÁVEL VIA MENU (porta A8, EEPROM 690) — variável a8a9BellBias acima ***
// Padrão de fábrica: 10 (igual ao valor fixo anterior)

// *** REGRAS A8/A9 (modo Des / A8A9_STRONGEST):
//   Regra 1 — Diferença > 80ms  → a PRIMEIRA que chegou dispara, a segunda é cancelada.
//             (ressonância/vazamento mecânico do mesmo golpe: a segunda sempre é crosstalk)
//   Regra 2 — Chegaram juntas (≤ 80ms) → maior ENERGIA RELATIVA vence (vel/Gain + bias A9)
//   Regra 3 — Empate exato de energia → A8 vence (critério de borda fixo)
//   Regra 4 — triZoneMode==1 (Ati) → TriZone normal; A8/A9 independentes (sem lógica de platô)
//   Regra 5 — triZoneMode==0 (Des) → TriZone off; este bloco ativo

struct A8A9State {
  bool          pending;
  byte          vel;
  byte          padIdx;
  unsigned long time;
  bool          fired;    // true = este pad já disparou e está no período de bloqueio
};
static A8A9State a8a9[2] = {{false,0,8,0,false},{false,0,9,0,false}};

//===========CURVE============
const float _Exp[9]={2.33, 3.85, 6.35, 10.48, 17.28, 28.5, 46.99 , 77.47, 127.74};
const float _Log[9]={0, 83.67, 98.23, 106.74, 112.78, 117.47, 121.30 , 124.53, 127.34};
const float _Sigma[9]={2.28, 6.02, 15.13, 34.15, 63.5, 92.84, 111.86 , 120.97, 127.71};
const float _Flat[9]={0, 32.86, 46.42, 55.82, 64.0, 72.17, 81.57 , 95.13, 127};

//===========TYPE============
enum type:byte
{
  Piezo    = 0,
  Switch   = 1,
  HHC      = 2,
  HH       = 3,
  HHs      = 4,
  YSwitch  = 5,
  Disabled = 15
};

//===========CURVE============
enum curve:byte
{
  Linear  = 0,
  Exp     = 1,
  Log     = 2,
  Sigma   = 3,
  Flat    = 4
};

//===========TIME============
enum state:byte
{
  Normal_Time     = 0,
  Scan_Time       = 1,
  Mask_Time       = 2,
  Retrigger_Time  = 3,
  Piezo_Time      = 4,
  Switch_Time     = 5,
  Choke_Time      = 6,
  Footsplash_Time = 7,
  Footclose_Time  = 8,
  Scanretrigger_Time = 9
};

//===========================
//===========================
//   NOTAS — ADDICTIVE DRUMMER 2
//===========================

// TABELA PARA ADDICTIVE DRUMMER 2
const byte VST_NOTES_ADDICTIVE[16] PROGMEM = {
  4,   // Pad 0  - C CHIMBAL (HHC)
  8,   // Pad 1  - CHIMBAL   
  38,  // Pad 2  - CAIXA TP 
  42,  // Pad 3  - ARO CX   
  36,  // Pad 4  - BUMBU      
  71,  // Pad 5  - TOM 2
  69,  // Pad 6  - TOM 3
  67,  // Pad 7  - SURDO 1
  60,  // Pad 8  - RIDE COND
  61,  // Pad 9  - RIDE CUPULA
  77,  // Pad 10 - CRASH 1
  79,  // Pad 11 - CRASH 2
  81,  // Pad 12 - CRASH 3
  65,  // Pad 13 - SURDO 2
  47,  // Pad 14 - TOM 1
  96   // Pad 15 - PAD EF 1
};

//===========================
//   ESTRUTURA DE PRESET POR PAD
//===========================
struct PadPreset {
  byte type;
  byte gain;
  byte threshold;
  byte scanTime;
  byte maskTime;
  byte retrigger;
  byte curve;
  byte curveForm;
  byte xcanCost;       // XCancel Global — velocity máxima considerada fantasma (0-127)
};

//===========================
//   PRESETS OTIMIZADOS PARA TODAS AS 16 PORTAS (A0-A15)
//===========================
const PadPreset PAD_PRESETS[16] PROGMEM = {
 // A0 - CONTROLE CHIMBAL (HHC)
  {
   HHC,      // type: Hi-Hat Controller
    20,       // gain: 
    12,        // threshold: 
    10,       // scanTime
    30,       // maskTime
    255,      // retrigger
    Linear,   // curve
    110,      // curveForm
    0         // xcanCost: HHC nao precisa filtro
  },
    // A1 - CHIMBAL 
  {
    Piezo,    // type: Piezo
    32,       // gain: 
    12,       // threshold: 
    10,       // scanTime: 
    18,       // maskTime: 
    255,      // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A2 - CAIXA 
  {
    Piezo,    // type: 
    36,       // gain: 
    12,       // threshold: 
    10,       // scanTime: 
    13,       // maskTime: 
    255,      // retrigger: 
    Linear,   // curve: 
    62,       // curveForm:
    34        // xcanCost
  },
    // A3 - ARO DA CAIXA (Piezo)
  {
    Piezo,   // type:
    68,      // gain: 
    20,      // threshold: 
    10,      // scanTime: 
    30,      // maskTime: 
    255,     // retrigger: 
    Linear,  // curve: 
    72,      // curveForm:
    34        // xcanCost

  },
    // A4 - BUMBO (Kick)
  {
    Piezo,    // type: 
    48,       // gain: 
    20,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,       // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A5 - TOM 2 (Tom médio)
  {
    Piezo,    // type: 
    20,       // gain: 
    20,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,       // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A6 - TOM 3 (Tom grave)
  {
    Piezo,    // type: 
    20,       // gain: 
    20,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,       // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A7 - SURDO 1 (Floor Tom 1)
  {
    Piezo,    // type: 
    20,       // gain: 
    20,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,       // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A8 - RIDE CONDUÇÃO (Ride Bow)
  {
    Piezo,    // type: 
    20,       // gain: 
    18,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,      // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A9 - RIDE CÚPULA (Ride Bell)
  {
    Piezo,    // type: 
    20,       // gain: 
    20,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,      // retrigger: 
    Log,      // curve: 
    99,       // curveForm:
    34        // xcanCost
  },
    // A10 - CRASH 1
  {
    Piezo,    // type: 
    20,       // gain: 
    18,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,      // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A11 - CRASH 2
  {
    Piezo,    // type: 
    20,       // gain: 
    18,       // threshold:
    10,       // scanTime: 
    30,       // maskTime: 
    255,      // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A12 - CRASH 3 / CHINA
  {
    Piezo,    // type: 
    20,       // gain: 
    18,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,      // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A13 - SURDO 2 (Floor Tom 2)
  {
    Piezo,    // type: 
    20,       // gain: 
    20,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,       // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A14 - TOM 1 (Tom agudo)
  {
    Piezo,    // type:
    20,       // gain: 
    20,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,       // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  },
    // A15 - PAD EFEITO 
  {
    Piezo,    // type: 
    20,       // gain: 
    18,       // threshold: 
    10,       // scanTime: 
    30,       // maskTime: 
    255,       // retrigger: 
    Linear,   // curve: 
    110,      // curveForm:
    34        // xcanCost
  }
};

//===========================
//   FUNÇÃO PARA OBTER NOTA
//===========================
byte getVSTNote(byte pad) {
  if(pad >= 16) return 0;
  return pgm_read_byte(&VST_NOTES_ADDICTIVE[pad]);
}

//===========================
//   DECLARAÇÃO FORWARD DA FUNÇÃO applyVSTPreset
//===========================
void applyVSTPreset();

//===========================
//   DEFAULT PIN - 16 PINOS ANALÓGICOS DIRETOS
//===========================

#if MEGA

const byte DP_HHC       = 0x00;
const byte DP_SNAREHEAD = 0x03;
const byte DP_SNARERIM  = 0x04;
const byte DP_KICK      = 0x02;
const byte DP_TOM1HEAD  = 0x05;
const byte DP_TOM2HEAD  = 0x06;
const byte DP_TOM3HEAD  = 0x07;
const byte DP_TOM4HEAD  = 0x08;
const byte DP_EXTRA1    = 0x09;
const byte DP_EXTRA2    = 0x0F;  
const byte DP_HHBOW     = 0x01;
const byte DP_CHINA     = 0x0E;
const byte DP_SPLASH    = 0x0A;
const byte DP_CRASHEDGE = 0x0B;
const byte DP_RIDEBOW   = 0x0C;
const byte DP_RIDEEDGE  = 0x0D;

#else

const byte DP_SNAREHEAD = 0x04;
const byte DP_SNARERIM  = 0x06;
const byte DP_KICK      = 0x07;
const byte DP_EFFECT    = 0x05;
const byte DP_HHBOW     = 0x03;
const byte DP_HHEDGE    = 0x00;
const byte DP_HHC       = 0x01;
const byte DP_HHC_RING  = 0x02;

const byte DP_RIDEBOW   = 0x0C;
const byte DP_RIDEEDGE  = 0x0E;
const byte DP_TOM1HEAD  = 0x0F;
const byte DP_TOM1RIM   = 0x0D;
const byte DP_TOM2HEAD  = 0x0B;
const byte DP_TOM2RIM   = 0x08;
const byte DP_CRASHBOW  = 0x09;
const byte DP_CRASHEDGE = 0x0A;

#endif

//===========================
//   PIN - CLASSE PARA 16 PINOS ANALÓGICOS DIRETOS
//===========================
class pin
{
  public:
  
  pin()
  {    
    Type=Disabled;
    State=Normal_Time;  // *** FIX: inicializa State explicitamente (bitfield não é zerado pelo compilador) ***
    Note=0;
    
    Gain=26;
    #if ENABLE_CHANNEL
    Channel=9;
    #endif
    
    Thresold=12;
    ScanTime=10;
    MaskTime=30;
    Retrigger=255;  // 255 = Aut (decay automatico)

    Curve=Linear;
    CurveForm=88;
    Xtalk=0;
    XtalkGroup=0;
    XCanCost=34;      // XtalkCan — padrão 34
    VelMinimo=0;      // *** Piso de velocity individual (0-26): 0 = desligado ***
    InvertSensor=0;   // *** Padrão: sensor normal ***

    MaxReading=-1;
    yn_1=0;           // *** FIX: zera leitura anterior — evita disparo espúrio no primeiro scan ***
  }

  void set(byte pin)
  {
    Time=TIMEFUNCTION;
    #if MEGA
    this->_pin=pin;
    #endif
    
    if(pin < 16) {
      PadPreset preset;
      memcpy_P(&preset, &PAD_PRESETS[pin], sizeof(PadPreset));
      
      this->Type = preset.type;
      this->Gain = preset.gain;
      this->Thresold = preset.threshold;
      this->ScanTime = preset.scanTime;
      this->MaskTime = preset.maskTime;
      this->Retrigger = preset.retrigger;
      this->Curve = preset.curve;
      this->CurveForm = preset.curveForm;
      this->XCanCost = preset.xcanCost; // *** CARREGA DO PRESET ***
      // VelMinimo não tem valor no preset — começa em 0 (desligado), carregado pela EEPROM
      
      this->Note = getVSTNote(pin);
      
      // *** FIX: inicia em Mask_Time para silenciar o pad durante MaskTime ms ***
      // Evita que ruído elétrico / ADC instável dispare notas espúrias no boot.
      this->State = Mask_Time;
      this->yn_1  = 0;
      this->Time  = Time;  // Mask_Time conta a partir de agora
    }
  }
  
  void scan(byte sensor, byte count)
  {
    byte pin = sensor;
    int yn_0 = -1;
    
    if(Type==HHC) {
      // *** HHC sempre faz scan normalmente (MIDI funciona sempre) ***
      // O Monitor Hi-Hat lê o sensor de forma independente na página 31
      scanHHC(pin, analogRead(sensor)/8);
      return;
    }
    
    if(Type==Switch)
    {
      yn_0 = analogRead(sensor);
      
      if(State==Normal_Time) 
      {
        if(yn_0<Thresold*10 && yn_1<Thresold*10 )  
        {
          DrawDiagnostic(pin,0);
          State=Scan_Time;
          MaxReading=0;
        }
      }
      else if(State==Scan_Time) 
      {      
        if(yn_0<Thresold*10 && yn_1<Thresold*10 ) MaxReading=MaxReading+1;
        else
        {
          if(MaxReading>ScanTime) State=Switch_Time;
          else State=Normal_Time;
        }
      
        if(MaxReading>MaskTime) State=Choke_Time;
      }
      else if(State==Mask_Time)  
      { 
        if(MaxReading > 0)
        {
          MaxReading=MaxReading-1;
        }
        else
        {
          if(yn_0>=Thresold*10 && yn_1>=Thresold*10 ) 
            State=Normal_Time;
        }
      }
    }
    else if(Type==YSwitch)
    {
      yn_0 = analogRead(sensor);
      
      if(yn_0<Thresold*4 )
      {
        State=Scan_Time;
      
        if(MaxReading<=MaskTime) MaxReading=MaxReading+1;
      }
      else
      {
        if(MaxReading!=255 && MaxReading>ScanTime)
        {
          if(yn_0>CurveForm*4) MaxReading = MaxReading;
          else MaxReading = 512+MaxReading;
        }
        State=Switch_Time; 
      }
    }
    else
    {
      GlobalTime=TIMEFUNCTION;
    
      if(State==Mask_Time)  
      { 
        if ((GlobalTime-Time)>MaskTime)
        {
          State = Retrigger_Time;
          Time = GlobalTime;
        }
      }
      
      yn_0 = 0.5 + ((float)analogRead(sensor)*(float)Gain)/64.0;
        
      if(State==Retrigger_Time)
      {
        // *** DECAY EXPONENCIAL — todos os pads incluindo HH/HHs ***
        // Retrigger=255 (Aut): k fixo 0.018 — teto cai rapido, comportamento automatico ideal
        // Retrigger=0:  k=0.018 — teto cai rapido, quase nao bloqueia (rufes livres)
        // Retrigger=16: k=0.002 — teto cai devagar, bloqueia duplo por mais tempo
        // Logica invertida em relacao ao Roland: 0=mais livre, 16=mais restritivo
        unsigned long dt = GlobalTime - Time;
        float k;
        if(Retrigger == 255) {
          k = 0.018f;  // Aut: decay rapido automatico
        } else {
          // Manual: 0→k=0.018 (rapido/livre), 16→k=0.002 (lento/restritivo)
          k = 0.018f - (Retrigger * 0.001f);
          if(k < 0.002f) k = 0.002f;
        }
        int decayCeil = (int)((float)MaxReading * expf(-k * (float)dt));
        if(decayCeil > 2)
        {
          if((yn_0 - yn_1) > Thresold && yn_0 > decayCeil)
          {
            State = Scan_Time;
            Time = GlobalTime;
            MaxReading = 0;
          }
        }
        else
          State = Normal_Time;
      }
      else if(State==Normal_Time) 
      {
        if((yn_0 - yn_1)> Thresold) 
        {
          State = Scan_Time;
          Time = GlobalTime;
          MaxReading = 0;
        }
      }
      else if(State==Scan_Time) 
      {
        if ((GlobalTime-Time) < ScanTime)
        {
          if(yn_0 > MaxReading)
          {
            MaxReading = yn_0;
          }
        }
        else
          State=Piezo_Time;
      }
    }
  
    yn_1=yn_0;
  }
  
  void play(byte i, pin* dual)
  {
    if(Type==Disabled) return;

    if(Type==HHC)
    {
      if(State==Footsplash_Time)
      {
        if (Mode==MIDI) fastNoteOn(Channel,HHFootNoteSensor[1],127);
        State=Normal_Time;
      }
      else if(State==Footclose_Time)
      {
        if (Mode==MIDI) fastNoteOn(Channel,HHFootNoteSensor[0],127);
        hhFootcloseTime = millis(); // grava timestamp para proteger HH
        State=Normal_Time;
      }
      return;
    }
   
    if(State==Normal_Time || State==Scan_Time || State==Mask_Time || State==Retrigger_Time) return;

    if(Mode==Tool && Diagnostic==true) playTOOL(i,dual);
    else
    {
      playMIDI(i,dual);
    }
  }
  
  void playTOOL(byte i, pin* dual)
  {
    if(Type==Switch)
    { 
      simpleSysex(0x6F,i,MaxReading,0);
    
      if(State==Switch_Time)
      {   
        State=Mask_Time;
        MaxReading = -1;
      }
      return;
    }
    
    if(Type==YSwitch)
    {
      simpleSysex(0x6F,i,MaxReading,0);
      MaxReading = -1;
      return;
    }
    
    if (State==Piezo_Time)
    {          
      if(Type==Piezo)
      {
        simpleSysex(0x6F,i,useCurve(),0);
        
        State=Mask_Time;
              
        if(dual->Type==Switch && dual->State==Switch_Time )
        {
          simpleSysex(0x6F,DualSensor(i),127,0);
          dual->State=Mask_Time;
          dual->MaxReading = -1;
        }
      }
      else
        simpleSysex(0x6F,i,useCurve(),0);
               
      MaxReading = -1;
    }
  }

  void playMIDI(byte i, pin* dual)
  {
    if(Type==Switch)
    { 
      if(State==Switch_Time)
      {
        setMonitorSensor(i);
        fastNoteOn(Channel,Note,127);
          
        State=Mask_Time;
        MaxReading = Retrigger;
      }
      else if(State==Choke_Time)
      {
        setMonitorSensor(i);
        fastNoteOn(Channel,ChokeNote,127);
      
        State = Mask_Time;
        MaxReading = Retrigger;
      }
      return;
    }
  
    GlobalTime=TIMEFUNCTION;
  
    if(Type==YSwitch)
    {
      setMonitorSensor(i);
      if(MaxReading <= 512)
      {
        fastNoteOn(Channel,Note,min(127,MaxReading*8));
      }
      else
        fastNoteOn(Channel,DualSensor(i),min(127,(MaxReading-512)*8));
          
      if(DualSensor(i)!=127)
      {
        dual->MaxReading=-1;
        dual->Time=GlobalTime - dual->ScanTime;
      }
    
      MaxReading = -1;
      return;
    }
  
    if (State==Piezo_Time)
    {          
      if(Type==Piezo)
      {
        byte v=useCurve();
        
        // *** CAMADA 1: VelMinimo — piso de velocity individual por pad ***
        // Corta ruído/crosstalk de velocity muito baixa específico desta porta
        // Se v < VelMinimo: descarta silenciosamente, não envia, não registra
        unsigned long currentTime=TIMEFUNCTION;
        // *** GUARD CHIMBAL: bloqueia A1 se:
        //   a) pedal se moveu ha menos de HH_MOVING_GUARD_MS (vibracao da haste)
        //   b) pedal quase fechado (hhLastCC < HH_CLOSED_GUARD_CC) — disco perto do piezo
        if(dual->Type==HHC &&
           ((millis()-hhMovingTime) < HH_MOVING_GUARD_MS || hhLastCC < HH_CLOSED_GUARD_CC))
        {
          State=Mask_Time;
          MaxReading=-1;
          return;
        }
        if(v >= VelMinimo)
        {
          // *** ZONA DUPLA VIA PAD AUX ***
          // Se este pad tem um switch Aux fechado associado (ZoneDual),
          // bloqueia a nota normal e envia a nota do Aux com a velocity do piezo.
          byte noteToSend = Note;
          bool zoneDualHandled = false;
          if(triZoneMode == 1) {  // *** ZoneDual só ativo se triZoneMode=1 ***
          for(byte p = 0; p < ZONE_DUAL_PAIRS; p++) {
            if(i == zoneDual_PadIdx[p] && zoneDual_Active[p]) {
              if(Note != zoneDual_NoteBlock[p]) continue;
              setMonitorSensor(i);
              if(zoneDual_HoldNote[p] && zoneDual_HoldFired[p]) {
                zoneDual_EdgeFired[p] = false;
                zoneDualHandled = true;
                break;
              }
              fastNoteOn(Channel, zoneDual_NoteAux[p], v);
              zoneDual_EdgeFired[p] = true;
              zoneDualHandled = true;
              break;
            }
          }
          } // fim triZoneMode==1

          // *** ANTI-CROSSTALK DIRECIONAL: registra timestamp deste pad como source ***
          xpairLastHit[i] = currentTime;

          // *** ANTI-CROSSTALK DIRECIONAL: verifica se este pad é target de algum par ativo ***
          {
            bool xBlocked = false;
            for(byte p = 0; p < XPAIR_COUNT; p++) {
              if(xpairRam[p].source == 255) continue;          // par desativado
              if(xpairRam[p].target != i) continue;            // não é o target
              if(v >= xpairRam[p].ghostVel) continue;          // velocity legítima — passa
              if((currentTime - xpairLastHit[xpairRam[p].source]) < xpairRam[p].windowMs) {
                xBlocked = true;
                break;
              }
            }
            if(xBlocked) {
              State=Mask_Time;
              MaxReading=-1;
              return;
            }
          }

          if(!zoneDualHandled)
          {
          // *** XCANCEL GLOBAL — filtro automático de crosstalk entre pads ***
          // Compara MaxReading deste pad com o último hit forte de QUALQUER outro pad.
          // XCanCost (0-127): velocity máxima considerada fantasma.
          //   0   = filtro desligado (nunca bloqueia)
          //   36  = padrão — elimina vazamentos comuns sem bloquear rufes
          //   127 = máximo — bloqueia qualquer coisa abaixo do hit forte anterior
          // Rolos/rufes: cada batida registra novo gLastStrongHit → não bloqueia sequência
          bool shouldBlock = false;

          if(XCanCost > 0 && gLastStrongPad != i && gLastStrongHit > 0) {
            if((currentTime - gLastStrongTime) < XCANCEL_GLOBAL_WINDOW_MS) {
              // Bloqueia se este pad disparou com velocity abaixo do limiar do XCanCost
              if(v <= XCanCost) {
                shouldBlock = true;
              }
            }
          }

          if(!shouldBlock) {
            // *** A8/A9 STRONGEST: se TriZone desativado e pad é A8 ou A9, adia envio ***
            if(A8A9_STRONGEST && (i == 8 || i == 9)) {
              byte pidx  = (i == 8) ? 0 : 1;
              byte other = 1 - pidx;
              unsigned long nowA89 = millis();
              // *** REGRA 1: se o pad irmão já disparou (fired=true) dentro da janela
              //              → este golpe chegou tarde demais → é crosstalk → descarta
              if(a8a9[other].fired &&
                 (nowA89 - a8a9[other].time) < A8A9_WINDOW_MS) {
                // segundo golpe sequencial — cancela silenciosamente
              }
              // Caso normal: registra como pendente (pode substituir se for mais forte
              // e a janela ainda não tiver expirado)
              else if(!a8a9[pidx].pending || v > a8a9[pidx].vel) {
                a8a9[pidx].pending = true;
                a8a9[pidx].fired   = false;
                a8a9[pidx].vel     = v;
                a8a9[pidx].padIdx  = i;
                a8a9[pidx].time    = nowA89;
              }
            } else {
              setMonitorSensor(i);
              fastNoteOn(Channel,Note,v);
            }
            // Registra este pad como último hit forte para proteger os outros
            gLastStrongHit  = MaxReading;
            gLastStrongTime = currentTime;
            gLastStrongPad  = i;
          }
          } // fim if(!zoneDualHandled)
        } // fim VelMinimo
        
        State=Mask_Time;
              
        if(dual->Type==Switch && dual->State==Switch_Time )
        {
          fastNoteOn(dual->Channel,dual->Note,127);
          dual->State=Mask_Time;
        }
      }
      else
      {
        byte note=Note;
        if(dual->MaxReading>dual->Thresold)
          note=ChokeNote;
        else if(dual->MaxReading>HHThresoldSensor[3])
          note=HHNoteSensor[3];
        else if(dual->MaxReading>HHThresoldSensor[2])
          note=HHNoteSensor[2];
        else if(dual->MaxReading>HHThresoldSensor[1])
          note=HHNoteSensor[1];
        else if(dual->MaxReading>HHThresoldSensor[0])
          note=HHNoteSensor[0];

        byte vHH = useCurve();
        // Aplica velocity floor no HiHat bow/edge também
        if(vHH >= VelMinimo) {
          setMonitorSensor(i);
          fastNoteOn(Channel,note,vHH);
        }
      }
    }
  }
  
  byte Type:4;
  byte State:4;
  byte _pin;
    
  byte Note;
  union
  {
    byte ChokeNote;
    byte Gain;
  };
  
  #if ENABLE_CHANNEL
  byte Channel;
  #endif
  
  byte Thresold;
  byte ScanTime;
  byte MaskTime;
  byte Retrigger;
  
  byte CurveForm;
  byte Curve:4;
  byte XtalkGroup:4;
  byte Xtalk;
  byte XCanCost;      // *** XCancel Global: velocity máxima fantasma (0-127, padrão 36) ***
  byte VelMinimo;     // *** Piso de velocity individual por porta (0-26) ***
  byte InvertSensor;  // *** INVERTER SENSOR: 0=Normal, 1=Invertido (TCRT5000) ***

  unsigned long Time;
  int MaxReading;
  int yn_1;

  byte useCurve()
  {
    int ret=0;
    float f=((float)CurveForm)/32.0;
    
    if(Curve==Linear)
    {
      ret=0.5 + ((float)MaxReading*f/8.0);
    }
    else
    {
      int i=MaxReading/128;
      int m=MaxReading % 128;
    
      switch(Curve)
      {
        case Exp: ret = 0.5 + (((float)m*(_Exp[i+1]-_Exp[i])/128.0) + _Exp[i])*f; break;
        case Log: ret = 0.5 + (((float)m*(_Log[i+1]-_Log[i])/128.0) + _Log[i])*f; break;
        case Sigma: ret = 0.5 + (((float)m*(_Sigma[i+1]-_Sigma[i])/128.0) + _Sigma[i])*f; break;
        case Flat: ret = 0.5 + (((float)m*(_Flat[i+1]-_Flat[i])/128.0) + _Flat[i])*f; break;
      
        default: ret = i*16; break;
      }
    }
  
    if(ret<=0) return 0;
    if(ret>=127) return 127;
    return ret;
  }

  void scanHHC(byte pin, byte sensorReading)
  {
    if ((GlobalTime-Time) > MaskTime)
    {
      if(sensorReading>(MaxReading+Thresold) || sensorReading<(MaxReading-Thresold))
      {
        if (Mode==MIDI)
        {
          // *** REMAPEIA usando pontos de calibração HH_CLOSED_POINT e HH_OPEN_POINT ***
          // Garante que o VST receba 0-127 independente da posição física do disco
          // HH_CLOSED_POINT = sensor com pedal FECHADO
          // HH_OPEN_POINT   = sensor com pedal ABERTO
          extern byte HH_CLOSED_POINT;
          extern byte HH_OPEN_POINT;
          int ccMapped;
          if(InvertSensor) {
            // TCRT5000 invertido: mais reflexão = pedal fechado = valor maior no sensor
            ccMapped = map((int)sensorReading, (int)HH_CLOSED_POINT, (int)HH_OPEN_POINT, 127, 0);
          } else {
            // Sensor normal (potenciômetro, FSR): pedal fechado = valor menor
            ccMapped = map((int)sensorReading, (int)HH_CLOSED_POINT, (int)HH_OPEN_POINT, 0, 127);
          }
          if(ccMapped < 0)   ccMapped = 0;
          if(ccMapped > 127) ccMapped = 127;
          hhLastCC = (byte)ccMapped;  // salva posicao atual para guard do chimbal
          fastMidiCC(Channel, Note, (byte)ccMapped);
        }
        else if(Mode==Tool && Diagnostic==true)
          simpleSysex(0x6F,pin,sensorReading,0);
        
        // *** FIX: protege divisão por zero quando Time==GlobalTime (loop muito rápido) ***
        float _divisor = (float)Time - (float)GlobalTime;
        float m = 0;
        if(_divisor != 0 && MaxReading >= 0) {
          m = (((float)MaxReading - (float)sensorReading) / _divisor) * 100;
        }

        MaxReading=sensorReading;
        
        if(InvertSensor) {
          // TCRT5000 invertido: sinal SOBE ao fechar, DESCE ao abrir
          if(m<0 && -m>HHFootThresoldSensor[1])
            State=Footsplash_Time;
          else if(m>0 && m>HHFootThresoldSensor[0])
            State=Footclose_Time;
        } else {
          if(m>0 && m>HHFootThresoldSensor[1])
            State=Footsplash_Time;
          else if(m<0 && -m>HHFootThresoldSensor[0])
            State=Footclose_Time;
        }

        Time=GlobalTime;
        hhMovingTime = millis(); // qualquer movimento do pedal — guard chimbal
      }
    }
  }
  
} Pin[MAX_PINS];

//===========================
//===========================
//   FUNÇÃO PARA APLICAR NOTAS ADDICTIVE DRUMMER 2
//===========================
void applyVSTPreset() {
  for(byte i = 0; i < 16; i++) {
    Pin[i].Note = getVSTNote(i);
  }

  CUSTOM_NOTE_37 = 42;
  CUSTOM_NOTE_40 = 37;
  CUSTOM_RIMSHOT_FORCE_NOTE = 37;

  // ZoneDual — Addictive Drummer 2
  zoneDual_NoteAux[0]   = 61; zoneDual_NoteBlock[0] = 60; zoneDual_PadIdx[0] =  8;
  zoneDual_NoteAux[1]   = 62; zoneDual_NoteBlock[1] = 60; zoneDual_PadIdx[1] =  8;
  zoneDual_NoteAux[2]   =  7; zoneDual_NoteBlock[2] =  8; zoneDual_PadIdx[2] =  1;
  zoneDual_NoteAux[3]   =  9; zoneDual_NoteBlock[3] =  8; zoneDual_PadIdx[3] =  1;
  zoneDual_NoteAux[4]   = 27; zoneDual_NoteBlock[4] = 77; zoneDual_PadIdx[4] = 10;
  zoneDual_NoteAux[5]   = 31; zoneDual_NoteBlock[5] = 79; zoneDual_PadIdx[5] = 11;

  // Pad Digital Aux 1-7 — Addictive Drummer 2
  digitalPadNotes[0] = 61;
  digitalPadNotes[1] = 62;
  digitalPadNotes[2] =  7;
  digitalPadNotes[3] =  9;
  digitalPadNotes[4] = 27;
  digitalPadNotes[5] = 31;
  digitalPadNotes[6] = 90;
}

// *** A8/A9 STRONGEST — envia só a nota mais forte entre A8 e A9 ***
// Chamado no final de cada loop() em l_loop.ino.
// Ativo automaticamente quando triZoneMode==0 (TriZone Des).
//
// REGRAS:
//   Regra 1 — Diferença > 80ms  → a PRIMEIRA dispara, a segunda é cancelada.
//             O campo fired=true bloqueia o pad irmão pelo restante da janela.
//   Regra 2 — Chegaram juntas (≤ 80ms) → maior ENERGIA RELATIVA vence (vel/Gain + bias A9).
//   Regra 3 — Empate exato de energia → A8 vence (pidx==0).
//   Regra 4/5 — controlado por A8A9_STRONGEST (triZoneMode).
void processA8A9Strongest() {
  if(!A8A9_STRONGEST) return;
  unsigned long now = millis();

  for(byte p = 0; p < 2; p++) {
    byte other = 1 - p;

    // Limpa estado fired após a janela expirar (libera o pad para novos golpes)
    if(a8a9[p].fired && (now - a8a9[p].time) >= A8A9_WINDOW_MS) {
      a8a9[p].fired = false;
    }

    if(!a8a9[p].pending) continue;
    if((now - a8a9[p].time) < A8A9_WINDOW_MS) continue;  // janela ainda aberta

    // Janela de p expirou — decide o que fazer
    if(a8a9[other].pending) {
      // *** REGRA 2/3: ambos chegaram na janela → compara ENERGIA RELATIVA ***
      // Energia relativa = vel / Gain — normaliza ganhos diferentes entre os piezos.
      // A9 (bell/cúpula) recebe bonus A8A9_BELL_BIAS para compensar posição desfavorável.
      // Sem normalização: se A8 tiver ganho alto, ele vence mesmo em batidas na cúpula.
      // Ajuste A8A9_BELL_BIAS em c_pin.ino:
      //   → borda disparando ao bater na cúpula: AUMENTAR o bias
      //   → cúpula disparando ao bater na borda: DIMINUIR o bias
      byte gainA8 = (Pin[8].Gain > 0) ? Pin[8].Gain : 1;
      byte gainA9 = (Pin[9].Gain > 0) ? Pin[9].Gain : 1;
      // Usa vel*100/Gain para manter precisão inteira (evita float)
      uint16_t energyA8 = ((uint16_t)a8a9[0].vel * 100) / gainA8;
      uint16_t energyA9 = ((uint16_t)a8a9[1].vel * 100) / gainA9 + a8a9BellBias;
      uint16_t energyP     = (a8a9[p].padIdx    == 8) ? energyA8 : energyA9;
      uint16_t energyOther = (a8a9[other].padIdx == 8) ? energyA8 : energyA9;
      // Regra 3: empate → A8 vence
      bool pWins = (energyP > energyOther) ||
                   (energyP == energyOther && a8a9[p].padIdx == 8);
      if(pWins) {
        setMonitorSensor(a8a9[p].padIdx);
        fastNoteOn(Pin[a8a9[p].padIdx].Channel, Pin[a8a9[p].padIdx].Note, a8a9[p].vel);
        // Marca p como fired para bloquear o irmão se tentar de novo logo
        a8a9[p].fired   = true;
        a8a9[p].time    = now;
      } else {
        setMonitorSensor(a8a9[other].padIdx);
        fastNoteOn(Pin[a8a9[other].padIdx].Channel, Pin[a8a9[other].padIdx].Note, a8a9[other].vel);
        a8a9[other].fired = true;
        a8a9[other].time  = now;
      }
      a8a9[p].pending     = false;
      a8a9[other].pending = false;
    } else {
      // *** REGRA 1: só p pendente → irmão não chegou dentro da janela
      //              → dispara normalmente (foi o único / foi o primeiro)
      setMonitorSensor(a8a9[p].padIdx);
      fastNoteOn(Pin[a8a9[p].padIdx].Channel, Pin[a8a9[p].padIdx].Note, a8a9[p].vel);
      a8a9[p].fired   = true;   // bloqueia o irmão pelo resto da janela
      a8a9[p].pending = false;
    }
  }
}