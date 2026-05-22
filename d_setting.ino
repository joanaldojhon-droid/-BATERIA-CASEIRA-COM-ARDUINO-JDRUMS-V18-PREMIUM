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
//=>  • Leitura e processamento de piezo sensors                                           <=
//=>  • Aplicação de curvas de velocidade                                                  <=
//=>  • Sistema anti-crosstalk                                                             <=
//=>  • Detecção de dual zones                                                             <=
//=>  • Processamento de HiHat controller                                                  <=
//=========================================================================================//

//===========SETTING - 16 PINOS FIXOS============
const byte NOTE      = 0x00;
const byte THRESOLD  = 0x01;
const byte SCANTIME  = 0x02;
const byte MASKTIME  = 0x03;
const byte RETRIGGER = 0x04;
const byte CURVE     = 0x05;
const byte XTALK     = 0x06;
const byte XTALKGROUP= 0x07;
const byte CURVEFORM = 0x08;
const byte CHOKENOTE = 0x09;
const byte DUAL      = 0x0A;
const byte TYPE      = 0x0D;
const byte CHANNEL   = 0x0E;
const byte ANTICROSSTALK = 0x0F;  // XCanCost (mantém código 0x0F para compatibilidade EEPROM)
const byte VEL_MINIMO    = 0x10;  // *** Piso de velocity individual por porta (0-60) ***

const byte ADV_ENABLE_37_38_TO_40 = 0x10;
const byte ADV_ENABLE_VEL_FILTER = 0x11;
const byte ADV_ENABLE_RIMSHOT = 0x12;
const byte ADV_ENABLE_101_TO_102 = 0x13;
const byte ADV_ENABLE_103_TO_104 = 0x14;
const byte ADV_BLOCK_WINDOW = 0x15;
const byte ADV_VEL_THRESHOLD_37_38 = 0x16;
const byte ADV_DETECTION_WINDOW = 0x17;

// *** NOTAS CONFIGURÁVEIS ***
const byte RIMSHOT_NOTE_37       = 0x18;
const byte RIMSHOT_NOTE_40       = 0x19;
const byte RIMSHOT_FORCE_NOTE_40 = 0x1A;

// *** VST PRESET ***
const byte VST_PRESET            = 0x1B;

// *** NOVO: CONTROLE SAÍDAS MIDI ***
const byte MIDI_OUTPUT_USB       = 0x1C;
const byte MIDI_OUTPUT_TX1       = 0x1D;

//==============================
//    SETTING - 16 PINOS FIXOS
//==============================
void SendPinSetting(byte pin, byte Set)
{
  if(pin >= 16) return;
  
  if(Set==0x7F)
  { 
    simpleSysex(0x02,pin,0x00,Pin[pin].Note);
    simpleSysex(0x02,pin,0x01,Pin[pin].Thresold);
    simpleSysex(0x02,pin,0x02,Pin[pin].ScanTime);
    simpleSysex(0x02,pin,0x03,Pin[pin].MaskTime);
    simpleSysex(0x02,pin,0x04,Pin[pin].Retrigger);
    simpleSysex(0x02,pin,0x05,Pin[pin].Curve);
    simpleSysex(0x02,pin,0x06,Pin[pin].Xtalk);
    simpleSysex(0x02,pin,0x07,Pin[pin].XtalkGroup);
    simpleSysex(0x02,pin,0x08,Pin[pin].CurveForm);
    simpleSysex(0x02,pin,0x09,Pin[pin].ChokeNote);
    simpleSysex(0x02,pin,0x0A,DualSensor(pin));
    simpleSysex(0x02,pin,0x0D,Pin[pin].Type);
    #if ENABLE_CHANNEL
    simpleSysex(0x02,pin,0x0E,Pin[pin].Channel);
    #endif
    return;
  } 
 
  simpleSysex(0x02,pin,Set,GetPinSetting(pin,Set)); 
}

byte GetPinSetting(byte pin, byte Set)
{
  if(pin >= 16) return 0;
  
  byte Value=0;
  switch(Set)
  {
    case NOTE:
      Value=Pin[pin].Note;
    break;
    case THRESOLD:
      Value=Pin[pin].Thresold;
    break;
    case SCANTIME:
      Value=Pin[pin].ScanTime;
    break;
    case MASKTIME:
      Value=Pin[pin].MaskTime;
    break;
    case RETRIGGER:
      Value=Pin[pin].Retrigger;
    break;
    case CURVE:
      Value=Pin[pin].Curve;
    break;
    case XTALK:
      Value=Pin[pin].Xtalk;
    break;
    case XTALKGROUP:
      Value=Pin[pin].XtalkGroup;
    break;
    case CURVEFORM:
      Value=Pin[pin].CurveForm;
    break;
    case CHOKENOTE:
      Value=Pin[pin].ChokeNote;
      break;
    case DUAL:
      Value=DualSensor(pin);
      break;    
    case TYPE:
      Value=Pin[pin].Type;
      break;
    case CHANNEL:
    #if ENABLE_CHANNEL
      Value=Pin[pin].Channel;
    #endif
      break;
    case ANTICROSSTALK: // XCanCost
      Value=Pin[pin].XCanCost;
      break;
    case VEL_MINIMO:
      Value=Pin[pin].VelMinimo;
      break;
  } 
 
  return Value;
}

void SendHHSetting(byte Set)
{
  byte Value=0;
  if(Set==0x7F)
  {
    simpleSysex(0x02,0x4C,0x00,HHNoteSensor[0]);
    simpleSysex(0x02,0x4C,0x01,HHNoteSensor[1]);
    simpleSysex(0x02,0x4C,0x02,HHNoteSensor[2]);
    simpleSysex(0x02,0x4C,0x03,HHNoteSensor[3]);
    simpleSysex(0x02,0x4C,0x04,HHThresoldSensor[0]);
    simpleSysex(0x02,0x4C,0x05,HHThresoldSensor[1]);
    simpleSysex(0x02,0x4C,0x06,HHThresoldSensor[2]);
    simpleSysex(0x02,0x4C,0x07,HHThresoldSensor[3]);
    
    simpleSysex(0x02,0x4C,0x08,HHFootNoteSensor[0]);
    simpleSysex(0x02,0x4C,0x09,HHFootNoteSensor[1]);
    simpleSysex(0x02,0x4C,0x0A,HHFootThresoldSensor[0]);
    simpleSysex(0x02,0x4C,0x0B,HHFootThresoldSensor[1]);
    return;
  }
  else if(Set<4)Value=HHNoteSensor[Set];
  else if(Set<8)Value=HHThresoldSensor[Set-4];
  else if(Set<10) Value=HHFootNoteSensor[Set-8];
  else Value=HHFootThresoldSensor[Set-10];
  
  simpleSysex(0x02,0x4C,Set,Value);
}

void SendGeneralSetting(byte Set)
{
  byte Value=0;
  switch(Set)
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
     
     case 0x7F:
       simpleSysex(0x02,0x7E,0x00,(byte)(delayTime/2));
       simpleSysex(0x02,0x7E,0x02,(byte)NSensor);
       simpleSysex(0x02,0x7E,0x03,(byte)GeneralXtalk);
       return;
     break;
  }
  simpleSysex(0x02,0x7E,Set,Value);
}

void SendAdvancedSetting(byte Set)
{
  byte Value=0;
  switch(Set)
  {
    case ADV_ENABLE_37_38_TO_40:
      Value=ENABLE_NOTE_37_38_TO_40;
    break;
    case ADV_ENABLE_VEL_FILTER:
      Value=ENABLE_VELOCITY_FILTER;
    break;
    case ADV_ENABLE_RIMSHOT:
      Value=ENABLE_RIMSHOT_38_TO_40;
    break;
    case ADV_ENABLE_101_TO_102:
      Value=ENABLE_NOTE_101_TO_102;
    break;
    case ADV_ENABLE_103_TO_104:
      Value=ENABLE_NOTE_103_TO_104;
    break;
    case ADV_BLOCK_WINDOW:
      Value=(byte)(BLOCK_WINDOW_MS/10);
    break;
    case ADV_VEL_THRESHOLD_37_38:
      Value=VELOCITY_THRESHOLD_37_38;
    break;
    case ADV_DETECTION_WINDOW:
      Value=(byte)DETECTION_WINDOW_MS;
    break;
    case RIMSHOT_NOTE_37:
      Value=CUSTOM_NOTE_37;
    break;
    case RIMSHOT_NOTE_40:
      Value=CUSTOM_NOTE_40;
    break;
    case RIMSHOT_FORCE_NOTE_40:
      Value=CUSTOM_RIMSHOT_FORCE_NOTE;
    break;
    // *** NOVO: Controle saídas MIDI ***
    case MIDI_OUTPUT_USB:
      Value=MIDI_USB_ENABLED;
    break;
    case MIDI_OUTPUT_TX1:
      Value=MIDI_TX1_ENABLED;
    break;
    
    case 0x7F:
      simpleSysex(0x02,0x7D,ADV_ENABLE_37_38_TO_40,ENABLE_NOTE_37_38_TO_40);
      simpleSysex(0x02,0x7D,ADV_ENABLE_VEL_FILTER,ENABLE_VELOCITY_FILTER);
      simpleSysex(0x02,0x7D,ADV_ENABLE_RIMSHOT,ENABLE_RIMSHOT_38_TO_40);
      simpleSysex(0x02,0x7D,ADV_ENABLE_101_TO_102,ENABLE_NOTE_101_TO_102);
      simpleSysex(0x02,0x7D,ADV_ENABLE_103_TO_104,ENABLE_NOTE_103_TO_104);
      simpleSysex(0x02,0x7D,ADV_BLOCK_WINDOW,(byte)(BLOCK_WINDOW_MS/10));
      simpleSysex(0x02,0x7D,ADV_VEL_THRESHOLD_37_38,VELOCITY_THRESHOLD_37_38);
      simpleSysex(0x02,0x7D,ADV_DETECTION_WINDOW,(byte)DETECTION_WINDOW_MS);
      simpleSysex(0x02,0x7D,RIMSHOT_NOTE_37,CUSTOM_NOTE_37);
      simpleSysex(0x02,0x7D,RIMSHOT_NOTE_40,CUSTOM_NOTE_40);
      simpleSysex(0x02,0x7D,RIMSHOT_FORCE_NOTE_40,CUSTOM_RIMSHOT_FORCE_NOTE);
      simpleSysex(0x02,0x7D,MIDI_OUTPUT_USB,MIDI_USB_ENABLED);
      simpleSysex(0x02,0x7D,MIDI_OUTPUT_TX1,MIDI_TX1_ENABLED);
      return;
    break;
  }
  simpleSysex(0x02,0x7D,Set,Value);
}

void ExecCommand(int Cmd, int Data1, int Data2, int Data3)
{
  switch(Cmd)
  {
    case 0x00:
      simpleSysex(0x00,Mode,0x00,0x00);
    break;
    
    case 0x01:
      Serial.flush();
      switch(Data1)
      {
         case Off: Mode=Off; break;
         case Standby: Mode=Standby; break;
         case MIDI: Mode=MIDI; break;
         case Tool: Mode=Tool; break;
      } 
      simpleSysex(0x01,Mode,0x00,0x00);
    break;
    
    case 0x02:
      if(Data1==0x7E)
        SendGeneralSetting(Data2);
      else if(Data1==0x4C)
        SendHHSetting(Data2);
      else if(Data1==0x7D)
        SendAdvancedSetting(Data2);
      else
        SendPinSetting(Data1,Data2);
        
       simpleSysex(0x02,0x7F,0x7F,0x7F);
    break;
    
    case 0x03:
      if(Data1==0x7F)
      {
        // Comando geral
      }
      else if(Data1==0x7E)
      {
        switch(Data2)
        {
          case 0x02: NSensor=Data3; markGeneralChanged(); break;
          case 0x03: GeneralXtalk=Data3; markGeneralChanged(); break;
        }
      }
      else if(Data1==0x4C)
      {
        switch(Data2)
        {
          case 0x00: HHNoteSensor[0]=Data3; markHHChanged(); break;
          case 0x01: HHNoteSensor[1]=Data3; markHHChanged(); break;
          case 0x02: HHNoteSensor[2]=Data3; markHHChanged(); break;
          case 0x03: HHNoteSensor[3]=Data3; markHHChanged(); break;
          case 0x04: HHThresoldSensor[0]=Data3; markHHChanged(); break;
          case 0x05: HHThresoldSensor[1]=Data3; markHHChanged(); break;
          case 0x06: HHThresoldSensor[2]=Data3; markHHChanged(); break;
          case 0x07: HHThresoldSensor[3]=Data3; markHHChanged(); break;
          
          case 0x08: HHFootNoteSensor[0]=Data3; markHHChanged(); break;
          case 0x09: HHFootNoteSensor[1]=Data3; markHHChanged(); break;
          case 0x0A: HHFootThresoldSensor[0]=Data3; markHHChanged(); break;
          case 0x0B: HHFootThresoldSensor[1]=Data3; markHHChanged(); break;
        }
      }
      else if(Data1==0x7D)
      {
        switch(Data2)
        {
          case ADV_ENABLE_37_38_TO_40: ENABLE_NOTE_37_38_TO_40=Data3; markAdvancedChanged(); break;
          case ADV_ENABLE_VEL_FILTER: ENABLE_VELOCITY_FILTER=Data3; markAdvancedChanged(); break;
          case ADV_ENABLE_RIMSHOT: ENABLE_RIMSHOT_38_TO_40=Data3; markAdvancedChanged(); break;
          case ADV_ENABLE_101_TO_102: ENABLE_NOTE_101_TO_102=Data3; markAdvancedChanged(); break;
          case ADV_ENABLE_103_TO_104: ENABLE_NOTE_103_TO_104=Data3; markAdvancedChanged(); break;
          case ADV_BLOCK_WINDOW: BLOCK_WINDOW_MS=(unsigned long)Data3*10; markAdvancedChanged(); break;
          case ADV_VEL_THRESHOLD_37_38: VELOCITY_THRESHOLD_37_38=Data3; markAdvancedChanged(); break;
          case ADV_DETECTION_WINDOW: DETECTION_WINDOW_MS=(unsigned long)Data3; markAdvancedChanged(); break;
          case RIMSHOT_NOTE_37: CUSTOM_NOTE_37=Data3; markAdvancedChanged(); break;
          case RIMSHOT_NOTE_40: CUSTOM_NOTE_40=Data3; markAdvancedChanged(); break;
          case RIMSHOT_FORCE_NOTE_40: CUSTOM_RIMSHOT_FORCE_NOTE=Data3; markAdvancedChanged(); break;
          // *** NOVO: Controle saídas MIDI ***
          case MIDI_OUTPUT_USB: MIDI_USB_ENABLED=Data3; markAdvancedChanged(); break;
          case MIDI_OUTPUT_TX1: MIDI_TX1_ENABLED=Data3; markAdvancedChanged(); break;
        }
      }
      else
      {
        if(Data1 < 16) {
          switch(Data2)
          {
            case NOTE: Pin[Data1].Note=Data3; markPinChanged(Data1); break;
            case THRESOLD: Pin[Data1].Thresold=Data3; markPinChanged(Data1); break;
            case SCANTIME: Pin[Data1].ScanTime=Data3; markPinChanged(Data1); break;
            case MASKTIME: Pin[Data1].MaskTime=Data3; markPinChanged(Data1); break;
            case RETRIGGER: Pin[Data1].Retrigger=Data3; markPinChanged(Data1); break;
            case CURVE: Pin[Data1].Curve=(curve)Data3; markPinChanged(Data1); break;
            case XTALK: Pin[Data1].Xtalk=Data3; markPinChanged(Data1); break;
            case XTALKGROUP: Pin[Data1].XtalkGroup=Data3; markPinChanged(Data1); break;
            case CURVEFORM: Pin[Data1].CurveForm=Data3; markPinChanged(Data1); break;
            case CHOKENOTE: Pin[Data1].ChokeNote=Data3; markPinChanged(Data1); break;
            case TYPE: Pin[Data1].Type=(type)Data3; markPinChanged(Data1); break;
            #if ENABLE_CHANNEL
            case CHANNEL: Pin[Data1].Channel=Data3; markPinChanged(Data1); break;
            #endif
            case ANTICROSSTALK: Pin[Data1].XCanCost=Data3; markPinChanged(Data1); break;
            case VEL_MINIMO:    { byte v=Data3; if(v>60) v=60; Pin[Data1].VelMinimo=v; markPinChanged(Data1); } break;
          }
        }
      }
    break;
    
    case 0x04:
      if(Data1==0x7F)
      {
        // Salvar tudo
      }
      else if(Data1==0x7E)
      {
         SaveGeneralEEPROM(Data2,Data3);
      }
      else if(Data1==0x4C)
      {
         SaveHHEEPROM(Data2,Data3); 
      }
      else if(Data1==0x7D)
      {
         SaveAdvancedEEPROM(Data2,Data3);
      }
      else
      {
         if(Data1 < 16) {
           SaveEEPROM(Data1,Data2,Data3);
         }
      }
    break;
    
    case 0x6D:
    #if USE_PROFILER
      if(Data1==0)
      {
        TimeProf=0;
        NProf=0;
      }
      else if (Data1==1)
      {
       SendProfiling(); 
      }
    #endif
    break;
    
    case 0x6E:
      LogPin=Data1;
      LogThresold=Data2;
      N=0;
    break;
    
    case 0x6F:
      Diagnostic=(Data1==1);
    break;
    
    case 0x61:
#if defined(__AVR__)
      simpleSysex(0x61,Data1,Data2,EEPROM.read((Data1*256)+Data2));
#endif
    break;
    
    case 0x7F:
      Serial.flush();
      Mode=Off;
      softReset();
    break;
  }
}

//==============================
//    INPUT
//==============================
void Input()
{
  while(Serial.peek()>=0 && Serial.peek()!=0xF0) Serial.read();
  
  if (Serial.available() > 6)
  {
    byte Start=Serial.read();
    byte ID=Serial.read(); 
    int Cmd=Serial.read();
    int Data1=Serial.read();
    int Data2=Serial.read();
    int Data3=Serial.read();
    byte End=Serial.read();
    
    ExecCommand(Cmd,Data1,Data2,Data3);
  }
}