#include <NTPCentral.h>

///////////////
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h> 
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
//#include <RS485_protocol.h>
#include <SoftwareSerial.h>
#include <WiFiUdp.h>



///////////////
//<wifi constant:Start>
/* Set these to your desired credentials. */
const char *ssid = "dink1618";  //ENTER YOUR WIFI SETTINGS
const char *password = "00000000";

// Define NTP Client to get time
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 3600;
NTPCentral timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

BearSSL::WiFiClientSecure httpsClient;
//WiFiClientSecure httpsClient;    //Declare object of class WiFiClient

//Link to read data from https://jsonplaceholder.typicode.com/comments?postId=7
//Web/Server address to read/write from 
const char *host = "pvoutput.org";
const int httpsPort = 443;  //HTTPS= 443 and HTTP = 80

//SHA1 finger print of certificate use web browser to view and copy
const char fingerprint[] PROGMEM = "‎62 27 d3 30 02 b9 03 90 00 1b 01 7e 02 6d 86 49 8a 50 2c 96";

 //<wifi constant:End>
 
//////////////
#define BaudRate  19200 
//////////
//RS485 control
#define SSerialTxControl  4 //D2
#define SSerialTx  14  //D5
#define SSerialRx  5  //D1
/////////

#define RS485Transmit HIGH   
#define RS485Receive  LOW

//SoftwareSerial Serial(10, 11); // RX, TX
//SoftwareSerial Serial(SSerialRx, SSerialTx); // RX, TX
SoftwareSerial Serial485(SSerialRx, SSerialTx); // RX, TX

  bool SendStatus = false;
  bool ReceiveStatus = false;
  int MaxAttempt = 1;
  byte ReceiveData[8];
  byte Address = 2;


void SerialPrintData(byte *data) {
  for (int i = 0; i < 8; i++) {
    Serial.print((int)data[i]);
    Serial.print(F(" "));
  }
  Serial.println(F(" "));
}

void clearTerm() {
  Serial.write(27); Serial.print("[2J");
  Serial.write(27); Serial.print("[;H");
  Serial.write(27); Serial.print("[0;40;37m");
}

void clearRow() {
  Serial.write(27); Serial.print("[K");
}
  void clearData(byte *data, byte len) {
    for (int i = 0; i < len; i++) {
      data[i] = 0;
    }
  }
  
int Crc16(byte *data, int offset, int count)
  {
    byte BccLo = 0xFF;
    byte BccHi = 0xFF;

    for (int i = offset; i < (offset + count); i++)
    {
      byte New = data[offset + i] ^ BccLo;
      byte Tmp = New << 4;
      New = Tmp ^ New;
      Tmp = New >> 5;
      BccLo = BccHi;
      BccHi = New ^ Tmp;
      Tmp = New << 3;
      BccLo = BccLo ^ Tmp;
      Tmp = New >> 4;
      BccLo = BccLo ^ Tmp;

    }

    return (int)word(~BccHi, ~BccLo);
  }

  bool Send(byte address, byte param0, byte param1, byte param2, byte param3, byte param4, byte param5, byte param6)
  {
    
    SendStatus = false;
    ReceiveStatus = false;

    byte SendData[10];
    SendData[0] = address;
    SendData[1] = param0;
    SendData[2] = param1;
    SendData[3] = param2;
    SendData[4] = param3;
    SendData[5] = param4;
    SendData[6] = param5;
    SendData[7] = param6;

    int crc = Crc16(SendData, 0, 8);
    SendData[8] = lowByte(crc);
    SendData[9] = highByte(crc);
    clearReceiveData();

    for (int i = 0; i < MaxAttempt; i++)
    {
      digitalWrite(SSerialTxControl, RS485Transmit);
      
      delay(50);
      
      if (Serial485.write(SendData, sizeof(SendData)) != 0) {
        Serial485.flush();
        SendStatus = true;

        digitalWrite(SSerialTxControl, RS485Receive);
        delay(200);
        if (Serial485.available() ){
            if (Serial485.readBytes(ReceiveData, sizeof(ReceiveData)) >= 8) {
             /*
             Serial.println("---> Someting received !!! check CRC 16");
             Serial.println(String(ReceiveData[0])+","+
                            String(ReceiveData[1])+","+
                            String(ReceiveData[2])+","+
                            String(ReceiveData[3])+","+
                            String(ReceiveData[4])+","+
                            String(ReceiveData[5])+","+
                            String(ReceiveData[6])+","+
                            String(ReceiveData[7])+","+
                            String(ReceiveData[8])+","+
                            String(ReceiveData[9]) );
             Serial.println(String(ReceiveData[7])+","+String(ReceiveData[6])+" : "+String( Crc16(ReceiveData, 0, 6)) );
             Serial.println("Transmission state ="+TransmissionState(ReceiveData[0]));
             */
             
              if ((int)word(ReceiveData[7], ReceiveData[6]) == Crc16(ReceiveData, 0, 6)) {
                
                ReceiveStatus = true;
                //Serial.println("---> Reception OKK");
                break;
              }
            }
        }
      }
    }
    Serial485.flush();
    
    return ReceiveStatus;
  }

  void clearReceiveData() {
    clearData(ReceiveData, 8);
  }

  String TransmissionState(byte id) {
    switch (id)
    {
    case 0:
      return F("Everything is OK.");
      break;
    case 51:
      return F("Command is not implemented");
      break;
    case 52:
      return F("Variable does not exist");
      break;
    case 53:
      return F("Variable value is out of range");
      break;
    case 54:
      return F("EEprom not accessible");
      break;
    case 55:
      return F("Not Toggled Service Mode");
      break;
    case 56:
      return F("Can not send the command to internal micro");
      break;
    case 57:
      return F("Command not Executed");
      break;
    case 58:
      return F("The variable is not available, retry");
      break;
    default:
      return F("Unknown");
      break;
    }
  }

  String GlobalState(byte id) {
    switch (id)
    {
    case 0:
      return F("Sending Parameters");
      break;
    case 1:
      return F("Wait Sun / Grid");
      break;
    case 2:
      return F("Checking Grid");
      break;
    case 3:
      return F("Measuring Riso");
      break;
    case 4:
      return F("DcDc Start");
      break;
    case 5:
      return F("Inverter Start");
      break;
    case 6:
      return F("Run");
      break;
    case 7:
      return F("Recovery");
      break;
    case 8:
      return F("Pausev");
      break;
    case 9:
      return F("Ground Fault");
      break;
    case 10:
      return F("OTH Fault");
      break;
    case 11:
      return F("Address Setting");
      break;
    case 12:
      return F("Self Test");
      break;
    case 13:
      return F("Self Test Fail");
      break;
    case 14:
      return F("Sensor Test + Meas.Riso");
      break;
    case 15:
      return F("Leak Fault");
      break;
    case 16:
      return F("Waiting for manual reset");
      break;
    case 17:
      return F("Internal Error E026");
      break;
    case 18:
      return F("Internal Error E027");
      break;
    case 19:
      return F("Internal Error E028");
      break;
    case 20:
      return F("Internal Error E029");
      break;
    case 21:
      return F("Internal Error E030");
      break;
    case 22:
      return F("Sending Wind Table");
      break;
    case 23:
      return F("Failed Sending table");
      break;
    case 24:
      return F("UTH Fault");
      break;
    case 25:
      return F("Remote OFF");
      break;
    case 26:
      return F("Interlock Fail");
      break;
    case 27:
      return F("Executing Autotest");
      break;
    case 30:
      return F("Waiting Sun");
      break;
    case 31:
      return F("Temperature Fault");
      break;
    case 32:
      return F("Fan Staucked");
      break;
    case 33:
      return F("Int.Com.Fault");
      break;
    case 34:
      return F("Slave Insertion");
      break;
    case 35:
      return F("DC Switch Open");
      break;
    case 36:
      return F("TRAS Switch Open");
      break;
    case 37:
      return F("MASTER Exclusion");
      break;
    case 38:
      return F("Auto Exclusion");
      break;
    case 98:
      return F("Erasing Internal EEprom");
      break;
    case 99:
      return F("Erasing External EEprom");
      break;
    case 100:
      return F("Counting EEprom");
      break;
    case 101:
      return F("Freeze");
    default:
      return F("Unknown");
      break;
    }
  }

  String DcDcState(byte id) {
    switch (id)
    {
    case 0:
      return F("DcDc OFF");
      break;
    case 1:
      return F("Ramp Start");
      break;
    case 2:
      return F("MPPT");
      break;
    case 3:
      return F("Not Used");
      break;
    case 4:
      return F("Input OC");
      break;
    case 5:
      return F("Input UV");
      break;
    case 6:
      return F("Input OV");
      break;
    case 7:
      return F("Input Low");
      break;
    case 8:
      return F("No Parameters");
      break;
    case 9:
      return F("Bulk OV");
      break;
    case 10:
      return F("Communication Error");
      break;
    case 11:
      return F("Ramp Fail");
      break;
    case 12:
      return F("Internal Error");
      break;
    case 13:
      return F("Input mode Error");
      break;
    case 14:
      return F("Ground Fault");
      break;
    case 15:
      return F("Inverter Fail");
      break;
    case 16:
      return F("DcDc IGBT Sat");
      break;
    case 17:
      return F("DcDc ILEAK Fail");
      break;
    case 18:
      return F("DcDc Grid Fail");
      break;
    case 19:
      return F("DcDc Comm.Error");
      break;
    default:
      return F("Unknown");
      break;
    }
  }

  String InverterState(byte id) {
    switch (id)
    {
    case 0:
      return F("Stand By");
      break;
    case 1:
      return F("Checking Grid");
      break;
    case 2:
      return F("Run");
      break;
    case 3:
      return F("Bulk OV");
      break;
    case 4:
      return F("Out OC");
      break;
    case 5:
      return F("IGBT Sat");
      break;
    case 6:
      return F("Bulk UV");
      break;
    case 7:
      return F("Degauss Error");
      break;
    case 8:
      return F("No Parameters");
      break;
    case 9:
      return F("Bulk Low");
      break;
    case 10:
      return F("Grid OV");
      break;
    case 11:
      return F("Communication Error");
      break;
    case 12:
      return F("Degaussing");
      break;
    case 13:
      return F("Starting");
      break;
    case 14:
      return F("Bulk Cap Fail");
      break;
    case 15:
      return F("Leak Fail");
      break;
    case 16:
      return F("DcDc Fail");
      break;
    case 17:
      return F("Ileak Sensor Fail");
      break;
    case 18:
      return F("SelfTest: relay inverter");
      break;
    case 19:
      return F("SelfTest : wait for sensor test");
      break;
    case 20:
      return F("SelfTest : test relay DcDc + sensor");
      break;
    case 21:
      return F("SelfTest : relay inverter fail");
      break;
    case 22:
      return F("SelfTest timeout fail");
      break;
    case 23:
      return F("SelfTest : relay DcDc fail");
      break;
    case 24:
      return F("Self Test 1");
      break;
    case 25:
      return F("Waiting self test start");
      break;
    case 26:
      return F("Dc Injection");
      break;
    case 27:
      return F("Self Test 2");
      break;
    case 28:
      return F("Self Test 3");
      break;
    case 29:
      return F("Self Test 4");
      break;
    case 30:
      return F("Internal Error");
      break;
    case 31:
      return F("Internal Error");
      break;
    case 40:
      return F("Forbidden State");
      break;
    case 41:
      return F("Input UC");
      break;
    case 42:
      return F("Zero Power");
      break;
    case 43:
      return F("Grid Not Present");
      break;
    case 44:
      return F("Waiting Start");
      break;
    case 45:
      return F("MPPT");
      break;
    case 46:
      return F("Grid Fail");
      break;
    case 47:
      return F("Input OC");
      break;
    default:
      return F("Unknown");
      break;
    }
  }

  String AlarmState(byte id) {
    switch (id)
    {
    case 0:
      return F("No Alarm");
      break;
    case 1:
      return F("Sun Low");
      break;
    case 2:
      return F("Input OC");
      break;
    case 3:
      return F("Input UV");
      break;
    case 4:
      return F("Input OV");
      break;
    case 5:
      return F("Sun Low");
      break;
    case 6:
      return F("No Parameters");
      break;
    case 7:
      return F("Bulk OV");
      break;
    case 8:
      return F("Comm.Error");
      break;
    case 9:
      return F("Output OC");
      break;
    case 10:
      return F("IGBT Sat");
      break;
    case 11:
      return F("Bulk UV");
      break;
    case 12:
      return F("Internal error");
      break;
    case 13:
      return F("Grid Fail");
      break;
    case 14:
      return F("Bulk Low");
      break;
    case 15:
      return F("Ramp Fail");
      break;
    case 16:
      return F("Dc / Dc Fail");
      break;
    case 17:
      return F("Wrong Mode");
      break;
    case 18:
      return F("Ground Fault");
      break;
    case 19:
      return F("Over Temp.");
      break;
    case 20:
      return F("Bulk Cap Fail");
      break;
    case 21:
      return F("Inverter Fail");
      break;
    case 22:
      return F("Start Timeout");
      break;
    case 23:
      return F("Ground Fault");
      break;
    case 24:
      return F("Degauss error");
      break;
    case 25:
      return F("Ileak sens.fail");
      break;
    case 26:
      return F("DcDc Fail");
      break;
    case 27:
      return F("Self Test Error 1");
      break;
    case 28:
      return F("Self Test Error 2");
      break;
    case 29:
      return F("Self Test Error 3");
      break;
    case 30:
      return F("Self Test Error 4");
      break;
    case 31:
      return F("DC inj error");
      break;
    case 32:
      return F("Grid OV");
      break;
    case 33:
      return F("Grid UV");
      break;
    case 34:
      return F("Grid OF");
      break;
    case 35:
      return F("Grid UF");
      break;
    case 36:
      return F("Z grid Hi");
      break;
    case 37:
      return F("Internal error");
      break;
    case 38:
      return F("Riso Low");
      break;
    case 39:
      return F("Vref Error");
      break;
    case 40:
      return F("Error Meas V");
      break;
    case 41:
      return F("Error Meas F");
      break;
    case 42:
      return F("Error Meas Z");
      break;
    case 43:
      return F("Error Meas Ileak");
      break;
    case 44:
      return F("Error Read V");
      break;
    case 45:
      return F("Error Read I");
      break;
    case 46:
      return F("Table fail");
      break;
    case 47:
      return F("Fan Fail");
      break;
    case 48:
      return F("UTH");
      break;
    case 49:
      return F("Interlock fail");
      break;
    case 50:
      return F("Remote Off");
      break;
    case 51:
      return F("Vout Avg errror");
      break;
    case 52:
      return F("Battery low");
      break;
    case 53:
      return F("Clk fail");
      break;
    case 54:
      return F("Input UC");
      break;
    case 55:
      return F("Zero Power");
      break;
    case 56:
      return F("Fan Stucked");
      break;
    case 57:
      return F("DC Switch Open");
      break;
    case 58:
      return F("Tras Switch Open");
      break;
    case 59:
      return F("AC Switch Open");
      break;
    case 60:
      return F("Bulk UV");
      break;
    case 61:
      return F("Autoexclusion");
      break;
    case 62:
      return F("Grid df / dt");
      break;
    case 63:
      return F("Den switch Open");
      break;
    case 64:
      return F("Jbox fail");
      break;
    default:
      return F("Unknown");
      break;
    }
  }

  typedef struct {
    byte TransmissionState;
    byte GlobalState;
    byte InverterState;
    byte Channel1State;
    byte Channel2State;
    byte AlarmState;
    bool ReadState;
  } DataState;

  DataState State;
  
  bool ReadState() {
    State.ReadState = Send(Address, (byte)50, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    if (State.ReadState == false) {
      ReceiveData[0] = 255;
      ReceiveData[1] = 255;
      ReceiveData[2] = 255;
      ReceiveData[3] = 255;
      ReceiveData[4] = 255;
      ReceiveData[5] = 255;
    }

    State.TransmissionState = ReceiveData[0];
    State.GlobalState = ReceiveData[1];
    State.InverterState = ReceiveData[2];
    State.Channel1State = ReceiveData[3];
    State.Channel2State = ReceiveData[4];
    State.AlarmState = ReceiveData[5];

    return State.ReadState;
  }

  float BytesToFloat(byte b3,byte b2,byte b1,byte b0){
    union {
     float f;
     byte b[4];
     } u;
     u.b[3] = b0;
     u.b[2] = b1;
     u.b[1] = b2;
     u.b[0] = b3;
     return u.f;
    }

  
  //1  grid voltage ret = Send(Address, (byte)59, (byte)1, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  //2  grid current ret = Send(Address, (byte)59, (byte)2, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  //3  grid power   ret = Send(Address, (byte)59, (byte)3, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  //21 Inverter Temperature ret = Send(Address, (byte)59, (byte)21, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  //23 Input 1 Voltage      ret = Send(Address, (byte)59, (byte)23, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  //25 Input 1 Current* ret = Send(Address, (byte)59, (byte)25, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
  //35 Power Peak Today       ret = Send(Address, (byte)59, (byte)35, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

 float ReadDspInfos(byte DspCode) {
    bool ret;
    float value;
    ret = Send(Address, (byte)59, (byte)DspCode, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
    value = BytesToFloat(ReceiveData[5],ReceiveData[4],ReceiveData[3],ReceiveData[2]);
    delay(50);
    if(ret) return value; 
    else return -1;   
    }

  
  
  float ReadPvEnergy() {
    bool ret;
    float value;
    ret = Send(Address, (byte)78, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);
    value = (ReceiveData[4]*256)+ReceiveData[5];
    
    if(ret) return value; 
    else return -1; 
    }

  
  float ReadPvPower() {
     float DayPvPower;//v2
     DayPvPower = ReadDspInfos(3);
     return DayPvPower; 
    }  
 
  //modification command 21 to 22
  //from 21 : inverter temperature to  22 :booster Temperature  
  float ReadPvTemperature() {
     float DayPvTemperature;//v5 inverter temperature
     DayPvTemperature = ReadDspInfos(22);//21
     return DayPvTemperature; 
    }  
      
  
  float ReadPvVoltage1() {
      float DayPvVoltage;//v6
     DayPvVoltage = ReadDspInfos(23);
     return DayPvVoltage; 
      
    }  

  float ReadPvCurrent1() {
      float DayPvCurrent1;//
     DayPvCurrent1 = ReadDspInfos(25);
     return DayPvCurrent1; 
      
    }  

  float ReadPvPowerPeak() {
      float DayPvPowerPeak;//
     DayPvPowerPeak = ReadDspInfos(35);
     return DayPvPowerPeak; 
      
    } 
        
typedef struct {
    String SerialNumber;
    bool ReadState;
  } DataSystemSerialNumber;

  DataSystemSerialNumber SystemSerialNumber;

  bool ReadSystemSerialNumber() {
    SystemSerialNumber.ReadState = Send(Address, (byte)63, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0);

    SystemSerialNumber.SerialNumber = String(String((char)ReceiveData[0]) + String((char)ReceiveData[1]) + String((char)ReceiveData[2]) + String((char)ReceiveData[3]) + String((char)ReceiveData[4]) + String((char)ReceiveData[5]));

    return SystemSerialNumber.ReadState;
  }
      
//====================================================================
void setup() {
  // put your setup code here, to run once:
  
  Serial485.begin(BaudRate);
  Serial485.setTimeout(1000);
  Serial.begin(BaudRate);
  clearTerm();
  pinMode(SSerialTxControl, OUTPUT);
  pinMode(SSerialTx, OUTPUT); 
  pinMode(SSerialRx, INPUT);
  digitalWrite(SSerialTxControl, RS485Receive);  // Init Transceiver   
  ////////////////////////////////////////////////////////////////////
  //<Setup_wifi:Start>
  Serial.begin(BaudRate);  
  WiFi.mode(WIFI_OFF);        //Prevents reconnection issue (taking too long to connect)
  delay(1000);
  WiFi.mode(WIFI_STA);        //Only Station No AP, This line hides the viewing of ESP as wifi hotspot 
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("");
  Serial.print("Connecting");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
  timeClient.begin();
  Serial.begin (BaudRate);
 //<Setup_wifi:End>
 /////////////////////////////////////////////////////////////////


}

//====================================================================
void loop() {
  // put your main code here, to run repeatedly:
  bool PvDataOk = false;
  clearTerm();
  ReadSystemSerialNumber() ;
  
  //ReadState();
  ReadPvEnergy();
  /*
  String PvEnergy;//v1
  String PvPower;//v2
  String PvEnergyUsed;//v3
  String PvPowerUsed;//v4
  String PvTemperature;//v5
  String PvVoltage;//v6

  float ReadPvEnergy()
  float ReadPvPower()
  float ReadPvTemperature()
  float ReadPvVoltage1()
  float ReadPvCurrent1()
  float ReadPvPowerPeak()
  */

 
  Serial.println("INVERTER 1");
  Serial.println("SystemSerialNumber.SerialNumber ="+String(SystemSerialNumber.SerialNumber));
  //Serial.println("State.TransmissionState ="+String(State.TransmissionState));
  String PvEnergy = String(ReadPvEnergy());//v1
  String PvPower  = String(ReadPvPower());//v2
  String PvEnergyUsed;//v3
  String PvPowerUsed;//v4
  String PvTemperature = String(ReadPvTemperature());//v5
  String PvVoltage     = String(ReadPvVoltage1());//v6
  
  
  
  Serial.println("DayPvEnergy      ="+PvEnergy);
  Serial.println("DayPvPower       ="+PvPower);
  Serial.println("DayPvTemperature ="+PvTemperature);
  Serial.println("DayPvVoltage1    ="+PvVoltage);
  Serial.println("DayPvCurrent1    ="+String(ReadPvCurrent1()));
  Serial.println("DayPvPowerPeak   ="+String(ReadPvPowerPeak()));
  
  
  
  ////////////////////////////////////
  //<loop_wifi:Start>

  

  Serial.println(host);

  Serial.printf("Using fingerprint '%s'\n", fingerprint);
  httpsClient.setFingerprint(fingerprint);
  httpsClient.setTimeout(15000); // 15 Seconds
  httpsClient.setInsecure();
  delay(1000);
  
  Serial.print("HTTPS Connecting");
  int r=0; //retry counter
  
  int maxr = 30;
  while((!httpsClient.connect(host, httpsPort)) && (r < maxr)){
      delay(100);
      Serial.print("*");
      r++;
  }
  if(r==maxr) {
    Serial.println("Connection failed");
  }
  else {
    Serial.println("Connected to web");
  }
  

   //****************************
  timeClient.update();

  Serial.print(timeClient.getHours());
  Serial.print(":");
  Serial.print(timeClient.getMinutes());
  Serial.print(":");
  Serial.println(timeClient.getSeconds());
  String pvtime = timeClient.getHours()+String(":")+timeClient.getMinutes();
  Serial.println(pvtime);
  //*********************
  String formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  String dayStamp = formattedDate.substring(0, splitT);
  Serial.print("DATE: ");
  dayStamp.replace("-","");
  Serial.println(dayStamp);
  //********************
    
  String ADCData, getData, Link;
  int adcvalue=analogRead(A0);  //Read Analog value of LDR
  ADCData = String(adcvalue);   //String to interger conversion


  //*********************************
    /*
    PvEnergy = "1000";//v1
    PvPower = "1000";//v2
    PvEnergyUsed = "150";//v3
    PvPowerUsed = "150";//v4
    PvTemperature = "25";//v5
    PvVoltage = "227";//v6
    */

    if ( ( PvEnergy == "-1.00" ) || ( PvPower == "-1.00"  ) ||
      ( PvTemperature == "-1.00" ) || ( PvVoltage == "-1.00" ) ){
             PvDataOk = false;
             Serial.println("PV Data KO...");
      } 
    else {
      Serial.println("PV Data OK...");
      PvDataOk = true;
      }
    
  //*********************************
  if (PvDataOk) {
  //GET Data
  //Link = "/comments?postId=" + ADCData;
  
    Link = "/service/r2/addstatus.jsp?key=0c0000000000000000c42b998330f2dc06ae80cc&sid=70580&d="+dayStamp+"&t="+pvtime+"&v1="+PvEnergy+"&v2="+PvPower+"&v3="+PvEnergyUsed+"&v4="+PvPowerUsed+"&v5="+PvTemperature+"&v6="+PvVoltage ;

  Serial.print("requesting URL: ");
  Serial.println(host+Link);

  httpsClient.print(String("GET ") + Link + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +  
               "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/56.0.2924.87 Safari/537.36\r\n" +             
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
                  
  while (httpsClient.connected()) {
    String line = httpsClient.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }

  Serial.println("reply was:");
  Serial.println("==========");
  String line;
  while(httpsClient.available()){        
    line = httpsClient.readStringUntil('\n');  //Read Line by Line
    Serial.println(line); //Print response
  }
  Serial.println("==========");
  
  Serial.println("closing connection");
  httpsClient.stop();
  
  //delay( 6000); //Post Data every 1 min 
  //delay(30000); //Post Data every 1/2 min 
  Serial.println("nexy send in 4 min");
  delay(240000); //Post Data every 4min
  //<loop_wifi:End>
  ////////////////////////////////////
  }
  else {
      Serial.println("Data Error : Retry after 20 s");
      delay( 20000); //retry in 20 s if data error
    }

}
