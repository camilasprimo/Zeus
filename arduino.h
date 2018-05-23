
//Carrega as bibliotecas do Sensor
#include "EmonLib.h" 
#include <LiquidCrystal.h>
 
EnergyMonitor emon1;
//Tensao da rede eletrica
int rede = 110.0;
//Pino do sensor SCT
int pino_sct = 1;

//Carrega as bibliotecas do FTP
#include <SD.h>
#include <SPI.h>
#include <Ethernet.h>
// comment out next line to write to SD from FTP server
#define FTPWRITE

// this must be unique
byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x59, 0x67 };  

// change to your network settings
IPAddress ip( 192, 168, 0, 111 );    
IPAddress gateway( 192, 168, 0, 1 );
IPAddress subnet( 255, 255, 255, 0 );

// change to your server
IPAddress server( 185,176,43,86 );

EthernetClient client;
EthernetClient dclient;

char outBuf[128];
char outCount;

// change fileName to your file (8.3 format!)
char fileName[13] = "zeus.txt";

void setup()
{
  Serial.begin(9600);
  
  emon1.current(pino_sct, 29);
  
  digitalWrite(10,HIGH);

  if(SD.begin(4) == 0)
  {
    Serial.println(F("SD init fail"));          
  }

  Ethernet.begin(mac, ip, gateway, gateway, subnet); 
  digitalWrite(10,HIGH);
  delay(2000);
  Serial.println(F("Ready. Press f or r"));
}

void loop()
{
  byte inChar;

  inChar = Serial.read();

  if(inChar == 'f')
  {
    if(doFTP()) Serial.println(F("FTP OK"));
    else Serial.println(F("FTP FAIL"));
  }

  if(inChar == 'r')
  {
    readSD();    
  }

}

File myFile;


byte doFTP()
{

  if (SD.exists(fileName)) {
    SD.remove(fileName);
    Serial.print("deleting zeus.txt...");
  } 
  
  myFile = SD.open(fileName, FILE_WRITE);
  // if the file opened okay, write to it:
  Serial.print("Writing to zeus.txt...");
  if (myFile) {
    myFile.print(",0001");
    myFile.print(",Thiago Santiago");
    myFile.print(",BRA");
    myFile.print(",-23.3645");
    myFile.print(",-46.7403");
    myFile.print(",TV");
    
    //Calcula a corrente  
    double Irms = emon1.calcIrms(1480);
    //Mostra o valor da corrente
    myFile.print(",");
    myFile.print(Irms); // Irms
    
    //Calcula e mostra o valor da potencia
    myFile.print(",");
    myFile.println(Irms*rede);
   
    // close the file:
    myFile.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("error creating zeus.txt");
  }
  
  myFile = SD.open(fileName,FILE_READ);

  if(!myFile)
  {
    Serial.println(F("SD open fail"));
    return 0;    
  }

  Serial.println(F("SD opened"));

  if (client.connect(server,21)) {
    Serial.println(F("Command connected"));
  } 
  else {
    myFile.close();
    Serial.println(F("Command connection failed"));
    return 0;
  }

  Serial.println(F("teste"));

  if(!eRcv()) return 0;

  client.println(F("USER 2729001_thiago"));

  if(!eRcv()) return 0;

  client.println(F("PASS zeus_2018"));

  if(!eRcv()) return 0;

  client.println(F("SYST"));

  if(!eRcv()) return 0;

  client.println(F("Type I"));

  if(!eRcv()) return 0;

  client.println(F("PASV"));

  if(!eRcv()) return 0;

  char *tStr = strtok(outBuf,"(,");
  int array_pasv[6];
  for ( int i = 0; i < 6; i++) {
    tStr = strtok(NULL,"(,");
    array_pasv[i] = atoi(tStr);
    if(tStr == NULL)
    {
      Serial.println(F("Bad PASV Answer"));    

    }
  }

  unsigned int hiPort,loPort;

  hiPort = array_pasv[4] << 8;
  loPort = array_pasv[5] & 255;

  Serial.print(F("Data port: "));
  hiPort = hiPort | loPort;
  Serial.println(hiPort);

  if (dclient.connect(server,hiPort)) {
    Serial.println(F("Data connected"));
  } 
  else {
    Serial.println(F("Data connection failed"));
    client.stop();
    myFile.close();
    return 0;
  }

#ifdef FTPWRITE 
  client.print(F("STOR "));
  client.println(fileName);
#else
  client.print(F("RETR "));
  client.println(fileName);
#endif

  if(!eRcv())
  {
    dclient.stop();
    return 0;
  }

#ifdef FTPWRITE
  Serial.println(F("Writing"));

  byte clientBuf[64];
  int clientCount = 0;

  while(myFile.available())
  {
    clientBuf[clientCount] = myFile.read();
    clientCount++;

    if(clientCount > 63)
    {
      dclient.write(clientBuf,64);
      clientCount = 0;
    }
  }

  if(clientCount > 0) dclient.write(clientBuf,clientCount);

#else
  while(dclient.connected())
  {
    while(dclient.available())
    {
      char c = dclient.read();
      myFile.write(c);      
      Serial.write(c); 
    }
  }
#endif

  dclient.stop();
  Serial.println(F("Data disconnected"));

  if(!eRcv()) return 0;

  client.println(F("QUIT"));

  if(!eRcv()) return 0;

  client.stop();
  Serial.println(F("Command disconnected"));

  myFile.close();
  Serial.println(F("SD closed"));
  return 1;
}

byte eRcv()
{
  byte respCode;
  byte thisByte;

  while(!client.available()) delay(1);

  respCode = client.peek();

  outCount = 0;

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);

    if(outCount < 127)
    {
      outBuf[outCount] = thisByte;
      outCount++;      
      outBuf[outCount] = 0;
    }
  }

  if(respCode >= '4')
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

  while(!client.available()) delay(1);

  while(client.available())
  {  
    thisByte = client.read();    
    Serial.write(thisByte);
  }

  client.stop();
  Serial.println(F("Command disconnected"));
  myFile.close();
  Serial.println(F("SD closed"));
}

void readSD()
{
  myFile = SD.open(fileName,FILE_READ);

  if(!myFile)
  {
    Serial.println(F("SD open fail"));
    return;    
  }

  while(myFile.available())
  {
    Serial.write(myFile.read());
  }

  myFile.close();
}
