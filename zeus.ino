//criando arquivos sem charset
#include <SdFat.h>
SdFat sdCard;

// Pino ligado ao CS do modulo
const int chipSelect = 4;

//Carrega as bibliotecas do Sensor
#include "EmonLib.h"
EnergyMonitor emon1;
//Tensao da rede eletrica
int rede = 110.0;
//Pino do sensor SCT
int pino_sct = 1;

//Carrega as bibliotecas do FTP
#include <SPI.h>
#include <Ethernet.h>
#define FTPWRITE

// include the library code:
#include <LiquidCrystal.h>
// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 9, en = 8, d4 = 5, d5 = 6, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// MAC unico
byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x59, 0x67 };

// Config de rede
IPAddress ip( 192, 168, 0, 111 );
IPAddress gateway( 192, 168, 0, 1 );
IPAddress subnet( 255, 255, 255, 0 );
IPAddress server( 185, 176, 43, 86 );

EthernetClient client;
EthernetClient dclient;

char outBuf[128];
char outCount;

// Nome do Arquivo
String fileName = String(".txt");
char fileNameChar[13];

File sdFile;
int filecount = 0;

void setup() {

  Serial.begin(9600);
  emon1.current(pino_sct, 29);
  digitalWrite(10, HIGH);

  pinMode(A5, INPUT);
  // Inicializa o modulo SD
  if (!sdCard.begin(chipSelect, SPI_HALF_SPEED))sdCard.initErrorHalt();

  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  digitalWrite(10, HIGH);

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  //lcd.clear();
  lcd.setCursor(0, 0);
  // Print a message to the LCD.
  lcd.print("ZEUS Uploading");

  delay(2000);

}

void loop() {

  if (doFTP()) Serial.println(F("FTP OK"));
  else Serial.println(F("FTP FAIL"));

  delay(1000);
}



byte doFTP()
{

  filecount = filecount + 1;
  fileName = String(".txt");
  fileName = millis() + fileName;
  fileName.toCharArray(fileNameChar, 13);

  //lcd.clear();
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  lcd.print(filecount);


  if (!sdFile.open(fileNameChar, O_RDWR | O_CREAT | O_AT_END))
  {
    sdCard.errorHalt("Erro na criação do arquivo");
  }
  else
  {
    sdFile.print("dt");
    sdFile.print(",0001");
    sdFile.print(",TRS");
    sdFile.print(",BRA");
    sdFile.print(",-23.3645");
    sdFile.print(",-46.7403");
    sdFile.print(",TV");

    //Calcula a corrente
    double Irms = emon1.calcIrms(1480);
    //Mostra o valor da corrente
    sdFile.print(",");
    sdFile.print(Irms); // Irms

    //Calcula e mostra o valor da potencia
    sdFile.print(",");
    sdFile.println(Irms * rede);

    // flush the file:
    sdFile.flush();
    sdFile.close();

    Serial.println("Arquivo criado com Sucesso!");
  }

  sdFile.open(fileNameChar, O_RDONLY);
  //myFile = SD.open(fileName, FILE_READ);

  if (client.connect(server, 21)) {
    Serial.println(F("Porta conectada"));
  }
  else {
    //sdFile.close();
    Serial.println(F("Porta com falha"));
    return 0;
  }

  if (!eRcv()) return 0;

  client.println(F("USER 2729001_thiago"));

  if (!eRcv()) return 0;

  client.println(F("PASS zeus_2018"));

  if (!eRcv()) return 0;

  client.println(F("SYST"));

  if (!eRcv()) return 0;

  client.println(F("Type I"));

  if (!eRcv()) return 0;

  client.println(F("PASV"));

  if (!eRcv()) return 0;


  char *tStr = strtok(outBuf, "(,");
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) {
    tStr = strtok(NULL, "(,");
    array_pasv[i] = atoi(tStr);
    if (tStr == NULL)
    {
      Serial.println(F("Bad PASV Answer"));
    }
  }

  unsigned int hiPort, loPort;

  hiPort = array_pasv[4] << 8;
  loPort = array_pasv[5] & 255;

  //Serial.print(F("Data port: "));
  hiPort = hiPort | loPort;
  Serial.println(hiPort);

  if (dclient.connect(server, hiPort)) {
    Serial.println(F("Dados conectados"));
  }
  else {
    Serial.println(F("Data connection failed"));
    client.stop();
    //sdFile.close();
    return 0;
  }

#ifdef FTPWRITE
  client.print(F("STOR "));
  client.println(fileName);
#else
  client.print(F("RETR "));
  client.println(fileName);
#endif

  if (!eRcv())
  {
    dclient.stop();
    return 0;
  }

#ifdef FTPWRITE
  Serial.println(F("Escrevendo no FTP"));

  byte clientBuf[64];
  int clientCount = 0;

  while (sdFile.available())
  {
    clientBuf[clientCount] = sdFile.read();
    clientCount++;

    if (clientCount > 63)
    {
      dclient.write(clientBuf, 64);
      clientCount = 0;
    }
  }

  if (clientCount > 0) dclient.write(clientBuf, clientCount);

#else
  while (dclient.connected())
  {
    while (dclient.available())
    {
      char c = dclient.read();
      sdFile.write(c);
      Serial.write(c);
    }
  }
#endif


  dclient.stop();
  Serial.println(F("Dados desconectados"));

  if (!eRcv()) return 0;

  client.println(F("QUIT"));

  if (!eRcv()) return 0;

  client.stop();
  Serial.println(F("Command disconnected"));

  sdFile.close();
  Serial.println(F("SD closed"));
  return 1;
}

byte eRcv()
{
  byte respCode;
  byte thisByte;

  while (!client.available()) delay(1);

  respCode = client.peek();

  outCount = 0;

  while (client.available())
  {
    thisByte = client.read();
    Serial.write(thisByte);

    if (outCount < 127)
    {
      outBuf[outCount] = thisByte;
      outCount++;
      outBuf[outCount] = 0;
    }
  }

  if (respCode >= '4')
  {
    efail();
    return 0;
  }

  return 1;
}

void efail()
{
  byte thisByte = 0;

  client.println(F("QUIT"));

  while (!client.available()) delay(1);

  while (client.available())
  {
    thisByte = client.read();
    Serial.write(thisByte);
  }

  client.stop();
  Serial.println(F("Cliente FTP Desconectado"));

  //sdFile.flush();
  sdFile.close();
  Serial.println(F("Arquivo fechado!"));
}

