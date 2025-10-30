#include <LiquidCrystal_I2C.h>
#include <avr/sleep.h>

const int greenLedPins[] = {12, 11, 10, 9};
const int redLedPin = 8;
const int btn[] = {2, 3, 4, 5};
const int potPin = A0;

const int NUM_DIGITS = 4;
int sequence[NUM_DIGITS]; //Sequenza di cifre {4, 1, 3, 2}
const int NUM_greenLed =  sizeof(greenLedPins)/ sizeof(greenLedPins[0]);
const int NUM_btn =  sizeof(btn)/ sizeof(btn[0]);
int difficultyLevel = 1;
int potValue = 0;
unsigned long startTime;
const int maxTime = 10000;
volatile boolean wakeUpFlag = false;  //serve per sapere se è stato premuto il pulsante per risveglio da deep sleeping

LiquidCrystal_I2C lcd(0x27, 16, 2);
 

void initHardware(){
  Serial.begin(9600);
  //inizializzo LCD
  lcd.init();
  lcd.backlight();
  //inizializzazione led
  //verdi
  for(int i = 0; i < NUM_greenLed; i++){
    pinMode(greenLedPins[i], OUTPUT);
  }
  //rosso
  pinMode(redLedPin, OUTPUT);
  //inzializzazione pulsanti
  for(int i = 0; i < NUM_btn ; i++){
    pinMode(btn[i], INPUT);
  }
  //mi salvo il momento di inizio per i 10 sec della deep sleeping
  startTime = millis(); //funzione per salvare il momento di inzio
  Serial.println("inizializzazione fatta");
}

// Funzione per generare un numero di 4 cifre distinte (1-4)
void generateSequence() {
  int number[] = {1, 2, 3, 4};
  for (int i = 0; i < NUM_DIGITS; i++) {
    // Semplice shuffle in loco
    int j = random(i, NUM_DIGITS);
    int temp = number[i];
    number[i] = number[j];
    number[j] = temp;
    sequence[i] = number[i]; // La sequenza contiene i numeri 1, 2, 3, 4 rimescolati
  }
}

// Funzione per stampare la sequenza sull'LCD
void displaySequence() {
  char seqStr[5] = "";
  for (int i = 0; i < NUM_DIGITS; i++) {
    seqStr[i] = sequence[i] + '0';
  }
  seqStr[NUM_DIGITS] = '\0';

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sequence:");
  lcd.setCursor(6, 1);
  lcd.print(seqStr);
}

void allGreenLedsOff(){
  for(int i = 0; i < NUM_greenLed; i++){
      digitalWrite(greenLedPins[i], LOW);
  }
  Serial.println("tutti led verdi spenti");
}

void fadeRedLed(int fadeDelay){
  //fade in
  for(int brightness  = 0; brightness  <= 255; brightness += 5)
  analogWrite(redLedPin, brightness );
  delay(fadeDelay);
  //fade out
  for(int brightness  = 255; brightness  >= 0; brightness  -= 5 ){
    analogWrite(redLedPin, brightness );
    delay(fadeDelay);
  }
}

void welcomeMessage(){
  lcd.setCursor(0, 0);
  lcd.print("Welcome to TOS!");
  lcd.setCursor(0, 1);
  lcd.print("Press B1 to start");
}

void enterDeepSleepUntilB1(){
  boolean btn1pressed = false;
  if(btn[0] == HIGH){
    //btn premuto in tempo, avvio gioco
    btn1pressed = true;
  }
  //se sono passati piu di 10 sec e non é stato premuto btn entro in deep sleep
  //millis ritorna numero millisecondi passati dall avvio del programma
  if(!btn1pressed && millis() - startTime >= maxTime){
    enterDeepSleep();
  }
}

void enterDeepSleep(){
  Serial.println("tempo scaduto, deep sleeping in avvio");

  lcd.noDisplay();
  digitalWrite(redLedPin, LOW);
  allGreenLedsOff();

// Abilita l'interrupt di risveglio sul Pin 2 (btn[0])
// Usiamo LOW perché il pin è HIGH (pull-up) e va a LOW quando premuto
 attachInterrupt(digitalPinToInterrupt(btn[0]), wakeUp, RISING);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();
  //risveglio
  sleep_disable();
  detachInterrupt(digitalPinToInterrupt(btn[0])); //disattiva intterupt
  //reimposta stato inziale
  lcd.display();
 startTime = millis();
  fadeRedLed(20);
}

//isr, interrupt service routine
void wakeUp(){
  wakeUpFlag = true;
}


void setup() {
  initHardware();
  welcomeMessage();
  generateSequence();
}

void loop() {
  Serial.println(digitalRead(btn[0]));
  delay(1000);
}
