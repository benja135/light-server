/*
 *   Light Server - Lampe connectée
 * 
 *   Serveur pour Arduino et ESP8266, permet de contrôler une
 *   lampe (via un relais) ainsi que des LEDs RGB.
 *
 *   Auteur: LACHERAY Benjamin
 *   Date de création: 05/12/2015
 *   Dernière modification: 20/12/2015
 *
 */

#include <SoftwareSerial.h>

SoftwareSerial EspSerial(10, 11);

#define DEBUG 0
#define OFF 0
#define ON 1

#define BUFFER_SIZE 128
char buffer[BUFFER_SIZE];

// Variable de notre réseau
const String NomduReseauWifi = "x";
const String MotDePasse      = "x";

const int redPin = 9;
const int greenPin = 5;
const int bluePin = 6;
const int lightPin = 12;  // relais

// Variables que nous allons contrôler
unsigned char redLvl = 0;
unsigned char greenLvl = 120;
unsigned char blueLvl = 0;
unsigned char intensLvl = 0;      // intensité en %
unsigned char lightState = OFF;


/* INITIALISATION */
void setup()
{
  pinMode(lightPin, OUTPUT);
  digitalWrite(lightPin, LOW);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  updateLed();
  if (DEBUG) Serial.begin(9600);
  EspSerial.begin(115200);  // par défaut l'ESP communique à 115200 Bauds (dépend du modéle)
  initESP();
  intensLvl = 100;
  updateLed();
}


/* Initialise l'ESP */
void initESP()
{
  if (DEBUG) Serial.println("Initialisation de l'ESP");

  /* Redémarre l'ESP */
  EspSerial.println("AT+RST");
  receive(2000);

  EspSerial.println("AT+CIOBAUD=9600");
  receive(2000);

  EspSerial.end();
  EspSerial.begin(9600);  // 9600 Bauds pour éviter les erreurs de transmissions

  EspSerial.println("AT+CWMODE=1");
  receive(5000);

  /* On se connecte au réseau */
  EspSerial.println("AT+CWJAP=\""+ NomduReseauWifi + "\",\"" + MotDePasse +"\"");
  receive(10000);

  /* On récupére notre adresse IP */
  EspSerial.println("AT+CIFSR");
  receive(1000);

  /* Mode connexion multiple */
  EspSerial.println("AT+CIPMUX=1");
  receive(1000);

  /* Mode serveur sur le port 80 */
  EspSerial.println("AT+CIPSERVER=1,80");
  receive(1000);

  if (DEBUG) Serial.println("Initialisation terminée");
}


/* MAIN */
void loop()
{
  int ch_id, packet_len;
  char *pb;
  EspSerial.readBytesUntil('\n', buffer, BUFFER_SIZE);  // on lit tout
  clearEspSerialBuffer();

  if(strncmp(buffer, "+IPD,", 5) == 0)
  {
    // forme du message: +IPD,ch,len:data
    sscanf(buffer+5, "%d,%d", &ch_id, &packet_len);
    if (packet_len > 0) 
    {
      // on avance tant qu'on est pas au ":" qui précéde les données
      pb = buffer+5;
      while(*pb!=':') {
        pb++;
      }
      pb++;

      if (strncmp(pb, "GET /off", 8) == 0)  // Le client veut éteindre la lampe
      {
        if (DEBUG) printDebug(buffer, "off", ch_id);

        lightState = ON;
        digitalWrite(lightPin, HIGH);
        sendEtat(ch_id);

      } else if (strncmp(pb, "GET /on", 7) == 0) {  // Le client veut allumer la lampe

        if (DEBUG) printDebug(buffer, "on", ch_id);

        lightState = OFF;
        digitalWrite(lightPin, LOW);
        sendEtat(ch_id);

      } else if (strncmp(pb, "GET /c", 6) == 0) {   // Changement de couleur des LED

        if (DEBUG) printDebug(buffer, "color", ch_id);

        int tmpRedLvl, tmpGreenLvl, tmpBlueLvl, tmpIntensity;
        sscanf(pb+5, "c%d,%d,%d,%d", &tmpRedLvl, &tmpGreenLvl, &tmpBlueLvl, &tmpIntensity);
        if (tmpRedLvl >= 0 && tmpRedLvl <= 255 && tmpGreenLvl >= 0 && tmpGreenLvl <= 255 
                  && tmpBlueLvl >= 0 && tmpBlueLvl <= 255 && tmpIntensity >= 0 && tmpIntensity <= 100) {
          redLvl = tmpRedLvl;
          greenLvl = tmpGreenLvl;
          blueLvl = tmpBlueLvl;
          intensLvl = tmpIntensity;
          updateLed();
        }

        sendEtat(ch_id);

      } else if (strncmp(pb, "GET / ", 6) == 0) {   // Consultation sans changements du système

        if (DEBUG) printDebug(buffer, "/", ch_id);

        sendEtat(ch_id);
      }
    }
  }
  clearBuffer();
}

/* Envoie une réponse HTTP au client ch_id :
   page web contenant l'état du système */
void sendEtat(int ch_id) {
  String Header;
 
  Header =  "HTTP/1.1 200 OK\r\n";
  Header += "Content-Type: text/html\r\n";
  Header += "Connection: close\r\n";  
   
  String Content;
  Content = String(lightState);

  Content += intTo3char(redLvl);
  Content += intTo3char(greenLvl);
  Content += intTo3char(blueLvl);
  Content += intTo3char(intensLvl);

  Header += "Content-Length: ";
  Header += (int)(Content.length());
  Header += "\r\n\r\n";
   

  EspSerial.print("AT+CIPSEND=");
  EspSerial.print(ch_id);
  EspSerial.print(",");
  EspSerial.println(Header.length()+Content.length());
  delay(20);

  if (EspSerial.find(">")) {
    EspSerial.print(Header);
    EspSerial.print(Content);
    delay(20);
  }
}

/* Lit et affiche (si DEBUG) ou purge les messages envoyés par l'ESP */
void receive(const int timeout)
{
  long int time = millis();
  if (DEBUG) {
    String reponse = "";  
    while((time+timeout) > millis())
    {
      while(EspSerial.available())
      {
        char c = EspSerial.read();
        reponse+=c;
      }
    }
    Serial.println(reponse);
  } else {
    while((time+timeout) > millis())
    {
      clearEspSerialBuffer();
    }
  }
}

void clearEspSerialBuffer() {
  while (EspSerial.available() > 0) {
    EspSerial.read();
  }
}

void clearBuffer() {
  for (int i =0; i<BUFFER_SIZE; i++ ) {
    buffer[i]=0;
  }
}

void updateLed() {
    analogWrite(redPin, 255 - (float)intensLvl/100.0 * redLvl);
    analogWrite(greenPin, 255 - (float)intensLvl/100.0 * greenLvl);
    analogWrite(bluePin, 255 - (float)intensLvl/100.0 * blueLvl);
}

String intTo3char(int valeur) {
  if (valeur < 10) {
    return "00" + String(valeur);
  } else if (valeur < 100) {
    return "0" + String(valeur);
  } else {
    return String(valeur);
  }
}

void printDebug(char* buffer, char* type, int ch_id) {
  Serial.print(millis());
  Serial.print(" : ");
  Serial.println(buffer);
  Serial.print("get " + String(type) + " from ch :");
  Serial.println(ch_id);
  delay(100);
}