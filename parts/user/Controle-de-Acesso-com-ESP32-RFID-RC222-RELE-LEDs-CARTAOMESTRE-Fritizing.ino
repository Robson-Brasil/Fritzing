/*
IoT - Automa��o Residencial
Controle de Acesso por RFID
Autor : Robson Brasil
Dispositivo : ESP32 WROOM32
Preferences--> Aditional boards Manager URLs: 
                                  http://arduino.esp8266.com/stable/package_esp8266com_index.json,https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
Download Board ESP32 (2.0.5):
M�dulo RFID RC-522
LCD I2C
Cadastramento por Cart�o Mestre
Vers�o : 5 - Alfa
�ltima Modifica��o : 15/22/2023


   --------------------------------------------------------------------------------------------------------------------
   Example sketch/program showing An Arduino Door Access Control featuring RFID, EEPROM, Rele
   --------------------------------------------------------------------------------------------------------------------
   This is a MFRC522 library example; for further details and other examples see: https://github.com/miguelbalboa/rfid


   This example showing a complete Door Access Control System


  Simple Work Flow (not limited to) :
                                     +---------+
  +----------------------------------->READ TAGS+^------------------------------------------+
  |                              +--------------------+                                     |
  |                              |                    |                                     |
  |                              |                    |                                     |
  |                         +----v-----+        +-----v----+                                |
  |                         |MASTER TAG|        |OTHER TAGS|                                |
  |                         +--+-------+        ++-------------+                            |
  |                            |                 |             |                            |
  |                            |                 |             |                            |
  |                      +-----v---+        +----v----+   +----v------+                     |
  |         +------------+READ TAGS+---+    |KNOWN TAG|   |UNKNOWN TAG|                     |
  |         |            +-+-------+   |    +-----------+ +------------------+              |
  |         |              |           |                |                    |              |
  |    +----v-----+   +----v----+   +--v--------+     +-v----------+  +------v----+         |
  |    |MASTER TAG|   |KNOWN TAG|   |UNKNOWN TAG|     |GRANT ACCESS|  |DENY ACCESS|         |
  |    +----------+   +---+-----+   +-----+-----+     +-----+------+  +-----+-----+         |
  |                       |               |                 |               |               |
  |       +----+     +----v------+     +--v---+             |               +--------------->
  +-------+EXIT|     |DELETE FROM|     |ADD TO|             |                               |
        +----+     |  EEPROM   |     |EEPROM|             |                               |
                   +-----------+     +------+             +-------------------------------+



   Use a Master Card which is act as Programmer then you can able to choose card holders who will granted access or not


 * **Easy User Interface**


   Just one RFID tag needed whether Delete or Add Tags. You can choose to use Leds for output or Serial LCD module to inform users.


 * **Stores Information on EEPROM**


   Information stored on non volatile Arduino's EEPROM memory to preserve Users' tag and Master Card. No Information lost
   if power lost. EEPROM has unlimited Read cycle but roughly 100,000 limited Write cycle.


 * **Security**
   To keep it simple we are going to use Tag's Unique IDs. It's simple and not hacker proof.


   @license Released into the public domain.


   Typical pin layout used:
   ---------------------------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino     ESP32
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   ---------------------------------------------------------------------------------------------------------
   RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST         2
   SPI SS      SDA(SS)      10            53        D10        10               10          5
   SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16          23
   SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14          19
   SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15          18
*/


#include <EEPROM.h>   // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>      // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices
#include <LiquidCrystal_I2C.h>
#include <Wire.h>


/*
Em vez de um rel�, voc� pode querer usar um servo. Os servos tamb�m podem bloquear e desbloquear fechaduras de portas
   O rel� ser� usado por padr�o
*/


// #include <Servo.h>


/*
  Para visualizar o que est� acontecendo no hardware, precisamos de alguns leds e para controlar a trava da porta, um rel� e um bot�o de limpeza
   (ou algum outro hardware) Led de �nodo comum usado, digitalWriting HIGH desliga o led Lembre-se de que, se voc� estiver indo
   para usar led de c�todo comum ou apenas leds separados, simplesmente comente #define COMMON_ANODE.
*/


#define PortaAberta 4  //Sensor de fim de curso, o RFID s� ler� outro cart�o, quando a porta for fechada


#define COMMON_ANODE


#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif


constexpr uint8_t LedVermelho = 27;  // Set Led Pins
constexpr uint8_t LedVerde = 26;
constexpr uint8_t LedAzul = 25;


constexpr uint8_t Rele = 13;  // Set Rele Pin
constexpr uint8_t BotaoWipe = 33;  // Button pin for WipeMode


boolean match = false;        // initialize card match to false
boolean programMode = false;  // initialize programming mode to false
boolean replaceMaster = false;


uint8_t successRead;  // Variable integer to keep if we have Successful Read from Reader


byte storedCard[4];  // Stores an ID read from EEPROM
byte readCard[4];    // Stores scanned ID read from RFID Module
byte masterCard[4];  // Stores master card's ID read from EEPROM


// Create MFRC522 instance.
constexpr uint8_t RST_PIN = 2;  // Configurable, see typical pin layout above
constexpr uint8_t SS_PIN = 5;   // Configurable, see typical pin layout above


MFRC522 mfrc522(SS_PIN, RST_PIN);


// --- Vari�veis Globais ---
char st[20];


// set the LCD number of columns and rows
int ColunasLCD = 20;
int LinhasLCD = 4;


// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, ColunasLCD, LinhasLCD);


String messageStatic0 = "POR FAVOR";
String messageStatic1 = "APROXIME SEU CARTAO!";


// Function to scroll text
// The function acepts the following arguments:
// row: row number where the text will be displayed
// message: message to scroll
// delayTime: delay between each character shifting
// ColunasLCD: number of columns of your LCD
void scrollText(int row, String message, int delayTime, int ColunasLCD) {
  for (int i = 0; i < ColunasLCD; i++) {
    message = " " + message;
  }
  message = message + " ";
  for (int pos = 0; pos < message.length(); pos++) {
    lcd.setCursor(0, row);
    lcd.print(message.substring(pos, pos + ColunasLCD));
    delay(delayTime);
  }
}
///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  EEPROM.begin(1024);


  //Arduino Pin Configuration
  pinMode(PortaAberta, INPUT_PULLUP);  //Sensor de fim de curso, o RFID s� ler� outro cart�o, quando a porta for fechada
  pinMode(LedVermelho, OUTPUT);
  pinMode(LedVerde, OUTPUT);
  pinMode(LedAzul, OUTPUT);
  pinMode(BotaoWipe, INPUT_PULLUP);  // Enable pin's pull up resistor
  pinMode(Rele, OUTPUT);
  //Be careful how Rele circuit behave on while resetting or power-cycling your Arduino
  digitalWrite(Rele, HIGH);        // Make sure door is locked
  digitalWrite(LedVermelho, LED_OFF);    // Make sure led is off
  digitalWrite(LedVerde, LED_OFF);  // Make sure led is off
  digitalWrite(LedAzul, LED_OFF);   // Make sure led is off


  //Protocol Configuration
  Serial.begin(115200);  // Initialize serial communications with PC
  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware


  //If you set Antenna Gain to Max it will increase reading distance
  //mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);


  Serial.println(F("Controle de Acesso v0.1"));  // For debugging purposes
  ShowReaderDetails();                           // Show details of PCD - MFRC522 Card Reader details


  //Wipe Code - If the Button (BotaoWipe) Pressed while setup run (powered on) it wipes EEPROM
  if (digitalRead(BotaoWipe) == LOW) {  // when button pressed pin should get low, button connected to ground
    digitalWrite(LedVermelho, LED_ON);   // Red Led stays on to inform user we are going to wipe
    Serial.println(F("Botao de formatacao apertado"));
    Serial.println(F("Voce tem 10 segundos para cancelar"));
    Serial.println(F("Isso vai apagar todos os seus registros e nao tem como desfazer"));
    bool buttonState = monitorBotaoWipeutton(10000);             // Give user enough time to cancel operation
    if (buttonState == true && digitalRead(BotaoWipe) == LOW) {  // If button still be pressed, wipe EEPROM
      Serial.println(F("Inicio da formatacao da EEPROM"));
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {  //Loop end of EEPROM address
        if (EEPROM.read(x) == 0) {                            //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        } else {
          EEPROM.write(x, 0);  // if not write 0 to clear, it takes 3.3mS
        }
      }
      Serial.println(F("EEPROM formatada com sucesso"));
      digitalWrite(LedVermelho, LED_OFF);  // visualize a successful wipe
      delay(200);
      digitalWrite(LedVermelho, LED_ON);
      delay(200);
      digitalWrite(LedVermelho, LED_OFF);
      delay(200);
      digitalWrite(LedVermelho, LED_ON);
      delay(200);
      digitalWrite(LedVermelho, LED_OFF);
    } else {
      Serial.println(F("Formatacao cancelada"));  // Show some feedback that the wipe button did not pressed for 15 seconds
      digitalWrite(LedVermelho, LED_OFF);
    }
  }
  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    Serial.println(F("Cartao Mestre nao definido"));
    Serial.println(F("Leia um chip para definir cartao Mestre"));
    do {
      successRead = getID();          // sets successRead to 1 when we get read from reader otherwise 0
      digitalWrite(LedAzul, LED_ON);  // Visualize Master Card need to be defined
      delay(200);
      digitalWrite(LedAzul, LED_OFF);
      delay(200);
    } while (!successRead);              // Program will not go further while you not get a successful read
    for (uint8_t j = 0; j < 4; j++) {    // Loop 4 times
      EEPROM.write(2 + j, readCard[j]);  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);  // Write to EEPROM we defined Master Card.
    Serial.println(F("Cartao Mestre definido"));
  }
  Serial.println(F("-------------------"));
  Serial.println(F("UID do cartao Mestre"));
  for (uint8_t i = 0; i < 4; i++) {      // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i);  // Write it to masterCard
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Tudo esta pronto"));
  Serial.println(F("Aguardando pelos chips para serem lidos"));
  cycleLeds();  // Everything ready lets give user some feedback by cycling leds


  EEPROM.commit();


  // initialize LCD
  lcd.begin();
  // turn on LCD backlight
  lcd.backlight();
}
///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop() {


  lcd.setCursor(5, 0);
  // print static message
  lcd.print(messageStatic0);
  // set cursor to second column, first row
  lcd.setCursor(0, 1);
  // print static message
  lcd.print(messageStatic1);


  do {
    successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0
    // When device is in use if wipe button pressed for 10 seconds initialize Master Card wiping
    if (digitalRead(BotaoWipe) == LOW) {  // Check if button is pressed
      // Visualize normal operation is iterrupted by pressing wipe button Red is like more Warning to user
      digitalWrite(LedVermelho, LED_ON);     // Make sure led is off
      digitalWrite(LedVerde, LED_OFF);  // Make sure led is off
      digitalWrite(LedAzul, LED_OFF);   // Make sure led is off
      // Give some feedback
      Serial.println(F("Botao de formatacao apertado"));
      Serial.println(F("O cartao Mestre sera apagado! em 10 segundos"));
      bool buttonState = monitorBotaoWipeutton(10000);             // Give user enough time to cancel operation
      if (buttonState == true && digitalRead(BotaoWipe) == LOW) {  // If button still be pressed, wipe EEPROM
        EEPROM.write(1, 0);                                    // Reset Magic Number.
        EEPROM.commit();
        Serial.println(F("Cartao Mestre desvinculado do dispositivo"));
        Serial.println(F("Aperte o reset da placa para reprogramar o cartao Mestre"));
        while (1)
          ;
      }
      Serial.println(F("Desvinculo do cartao Mestre cancelado"));
    }
    if (programMode) {
      cycleLeds();  // Program Mode cycles through Red Green Blue waiting to read a new card
    } else {
      normalModeOn();  // Normal mode, blue Power LED is on, all others are off
    }
  } while (!successRead);  //the program will not go further while you are not getting a successful read
  if (programMode) {
    if (isMaster(readCard)) {  //When in program mode check First If master card scanned again to exit program mode
      Serial.println(F("Leitura do cartao Mestre"));
      Serial.println(F("Saindo do modo de programacao"));
      Serial.println(F("-----------------------------"));
      programMode = false;
      return;
    } else {
      if (findID(readCard)) {  // If scanned card is known delete it
        Serial.println(F("Conheco este chip, removendo..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
        Serial.println(F("Leia um chip para adicionar ou remover da EEPROM"));
      } else {  // If scanned card is not known add it
        Serial.println(F("Nao conheco este chip, incluindo..."));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
        Serial.println(F("Leia um chip para adicionar ou remover da EEPROM"));
      }
    }
  } else {
    if (isMaster(readCard)) {  // If scanned card's ID matches Master Card's ID - enter program mode
      programMode = true;
      Serial.println(F("Ola Mestre - Modo de programacao iniciado"));
      uint8_t count = EEPROM.read(0);  // Read the first Byte of EEPROM that
      Serial.print(F("Existem "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" registro(s) na EEPROM"));
      Serial.println("");
      Serial.println(F("Leia um chip para adicionar ou remover da EEPROM"));
      Serial.println(F("Leia o cartao Mestre novamente para sair do modo de programacao"));
      Serial.println(F("-----------------------------"));
    } else {
      if (findID(readCard)) {  // If not, see if the card is in the EEPROM
        lcd.setCursor(2, 3);
        // print scrolling message
        lcd.print("Voce pode passar");
        Serial.println(F("Bem-vindo, voce pode passar"));
        granted(300);  // Open the door lock for 300 ms
      } else {         // If not, show that the ID was not valid
        Serial.println(F("Voce nao pode passar"));
        lcd.setCursor(0, 3);
        lcd.print("Voce nao pode passar");
        delay(5000);
        lcd.clear();  // limpa a tela
        denied();
      }
    }
  }
}


/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted(uint16_t setDelay) {
  digitalWrite(LedAzul, LED_OFF);  // Turn off blue LED
  digitalWrite(LedVermelho, LED_OFF);   // Turn off red LED
  digitalWrite(LedVerde, LED_ON);  // Turn on green LED
  digitalWrite(Rele, LOW);        // Unlock door!
  delay(setDelay);                 // Hold door lock open for given seconds
  digitalWrite(Rele, HIGH);       // Relock door


  //Mant�m acesso liberado at� acionar o sensor de porta
  while (digitalRead(PortaAberta)) digitalWrite(Rele, LOW);
  digitalWrite(Rele, HIGH);
  while (digitalRead(PortaAberta)) digitalWrite(LedVerde, LOW);
  digitalWrite(LedVerde, HIGH);
  lcd.clear();  // limpa a tela
}
///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  digitalWrite(LedAzul, LED_OFF);   // Make sure blue LED is off
  digitalWrite(LedVermelho, LED_ON);     // Turn on red LED
  delay(1000);
}



///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if (!mfrc522.PICC_IsNewCardPresent()) {  //If a new PICC placed to RFID reader continue
    return 0;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {  //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("UID do chip lido:"));
  for (uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
    lcd.setCursor(0, 2);
    lcd.print("USUARIO  AUTORIZADO?");
  }
  Serial.println("");
  mfrc522.PICC_HaltA();  // Stop reading
  return 1;
}


void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("Versao do software MFRC522: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (desconhecido),provavelmente um clone chines?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("ALERTA: Falha na comunicacao, o modulo MFRC522 esta conectado corretamente?"));
    Serial.println(F("SISTEMA ABORTADO: Verifique as conexoes."));
    // Visualize system is halted
    digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
    digitalWrite(LedAzul, LED_OFF);   // Make sure blue LED is off
    digitalWrite(LedVermelho, LED_ON);     // Turn on red LED
    while (true)
      ;  // do not go further
  }
}


///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(LedVermelho, LED_OFF);   // Make sure red LED is off
  digitalWrite(LedVerde, LED_ON);  // Make sure green LED is on
  digitalWrite(LedAzul, LED_OFF);  // Make sure blue LED is off
  delay(200);
  digitalWrite(LedVermelho, LED_OFF);    // Make sure red LED is off
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  digitalWrite(LedAzul, LED_ON);    // Make sure blue LED is on
  delay(200);
  digitalWrite(LedVermelho, LED_ON);     // Make sure red LED is on
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  digitalWrite(LedAzul, LED_OFF);   // Make sure blue LED is off
  delay(200);
}


//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn() {
  digitalWrite(LedAzul, LED_ON);    // Blue LED ON and ready to read card
  digitalWrite(LedVermelho, LED_OFF);    // Make sure Red LED is off
  digitalWrite(LedVerde, LED_OFF);  // Make sure Green LED is off
  digitalWrite(Rele, HIGH);        // Make sure Door is Locked
}


//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID(uint8_t number) {
  uint8_t start = (number * 4) + 2;          // Figure out starting position
  for (uint8_t i = 0; i < 4; i++) {          // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);  // Assign values read from EEPROM to array
  }
}


///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID(byte a[]) {
  if (!findID(a)) {                    // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);      // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = (num * 4) + 6;     // Figure out where the next slot starts
    num++;                             // Increment the counter by one
    EEPROM.write(0, num);              // Write the new count to the counter
    for (uint8_t j = 0; j < 4; j++) {  // Loop 4 times
      EEPROM.write(start + j, a[j]);   // Write the array values to EEPROM in the right position
    }
    EEPROM.commit();
    successWrite();
    Serial.println(F("ID adicionado na EEPROM com sucesso"));
  } else {
    failedWrite();
    Serial.println(F("Erro! Tem alguma coisa errada com o ID do chip ou problema na EEPROM"));
  }
}


///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID(byte a[]) {
  if (!findID(a)) {  // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite();   // If not
    Serial.println(F("Erro! Tem alguma coisa errada com o ID do chip ou problema na EEPROM"));
  } else {
    uint8_t num = EEPROM.read(0);  // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;                  // Figure out the slot number of the card
    uint8_t start;                 // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;               // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0);  // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT(a);            // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;                                                  // Decrement the counter by one
    EEPROM.write(0, num);                                   // Write the new count to the counter
    for (j = 0; j < looping; j++) {                         // Loop the card shift times
      EEPROM.write(start + j, EEPROM.read(start + 4 + j));  // Shift the array values to 4 places earlier in the EEPROM
    }
    for (uint8_t k = 0; k < 4; k++) {  // Shifting loop
      EEPROM.write(start + j + k, 0);
    }
    EEPROM.commit();
    successDelete();
    Serial.println(F("ID removido da EEPROM com sucesso"));
  }
}


///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo(byte a[], byte b[]) {
  if (a[0] != 0)                     // Make sure there is something in the array first
    match = true;                    // Assume they match at first
  for (uint8_t k = 0; k < 4; k++) {  // Loop 4 times
    if (a[k] != b[k])                // IF a != b then set match = false, one fails, all fail
      match = false;
  }
  if (match) {    // Check to see if if match is still true
    return true;  // Return true
  } else {
    return false;  // Return false
  }
}


///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT(byte find[]) {
  uint8_t count = EEPROM.read(0);         // Read the first Byte of EEPROM that
  for (uint8_t i = 1; i <= count; i++) {  // Loop once for each EEPROM entry
    readID(i);                            // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard)) {     // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;  // The slot number of the card
      break;     // Stop looking we found it
    }
  }
}


///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID(byte find[]) {
  uint8_t count = EEPROM.read(0);         // Read the first Byte of EEPROM that
  for (uint8_t i = 1; i <= count; i++) {  // Loop once for each EEPROM entry
    readID(i);                            // Read an ID from EEPROM, it is stored in storedCard[4]
    if (checkTwo(find, storedCard)) {     // Check to see if the storedCard read from EEPROM
      return true;
      break;  // Stop looking we found it
    } else {  // If not, return false
    }
  }
  return false;
}


///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  digitalWrite(LedAzul, LED_OFF);   // Make sure blue LED is off
  digitalWrite(LedVermelho, LED_OFF);    // Make sure red LED is off
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is on
  delay(200);
  digitalWrite(LedVerde, LED_ON);  // Make sure green LED is on
  delay(200);
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(LedVerde, LED_ON);  // Make sure green LED is on
  delay(200);
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(LedVerde, LED_ON);  // Make sure green LED is on
  delay(200);
}


///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  digitalWrite(LedAzul, LED_OFF);   // Make sure blue LED is off
  digitalWrite(LedVermelho, LED_OFF);    // Make sure red LED is off
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(LedVermelho, LED_ON);  // Make sure red LED is on
  delay(200);
  digitalWrite(LedVermelho, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(LedVermelho, LED_ON);  // Make sure red LED is on
  delay(200);
  digitalWrite(LedVermelho, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(LedVermelho, LED_ON);  // Make sure red LED is on
  delay(200);
}


///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
  digitalWrite(LedAzul, LED_OFF);   // Make sure blue LED is off
  digitalWrite(LedVermelho, LED_OFF);    // Make sure red LED is off
  digitalWrite(LedVerde, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(LedAzul, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(LedAzul, LED_OFF);  // Make sure blue LED is off
  delay(200);
  digitalWrite(LedAzul, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(LedAzul, LED_OFF);  // Make sure blue LED is off
  delay(200);
  digitalWrite(LedAzul, LED_ON);  // Make sure blue LED is on
  delay(200);
}


////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster(byte test[]) {
  if (checkTwo(test, masterCard))
    return true;
  else
    return false;
}


bool monitorBotaoWipeutton(uint32_t interval) {
  uint32_t now = (uint32_t)millis();
  while ((uint32_t)millis() - now < interval) {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (digitalRead(BotaoWipe) != LOW)
        return false;
    }
  }
  return true;
}