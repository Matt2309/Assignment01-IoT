#include <LiquidCrystal_I2C.h>
#include <avr/sleep.h>

const int greenLedPins[] = {12, 11, 10, 9};
const int redLedPin = 8;
const int btn[] = {2, 3, 4, 5};
const int potPin = A0;

boolean giocoAvviato = false; //appena agigutnbe


boolean btn1pressed = false;
const int NUM_DIGITS = 4;
int sequence[NUM_DIGITS]; //Sequenza di cifre {4, 1, 3, 2}
const int NUM_greenLed =  sizeof(greenLedPins) / sizeof(greenLedPins[0]);
const int NUM_btn =  sizeof(btn) / sizeof(btn[0]);
int difficultyLevel = 1;
int potValue = 0;
unsigned long startTime;
const int maxTime = 10000;
volatile boolean wakeUpFlag = true;  //serve per sapere se è stato premuto il pulsante per risveglio da deep sleeping

LiquidCrystal_I2C lcd(0x27, 16, 2);


void initHardware() {
  Serial.begin(9600);
  //inizializzo LCD
  lcd.init();
  lcd.backlight();
  //inizializzazione led
  //verdi
  for (int i = 0; i < NUM_greenLed; i++) {
    pinMode(greenLedPins[i], OUTPUT);
  }
  //rosso
  pinMode(redLedPin, OUTPUT);
  //inzializzazione pulsanti
  for (int i = 0; i < NUM_btn ; i++) {
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

void allGreenLedsOff() {
  for (int i = 0; i < NUM_greenLed; i++) {
    digitalWrite(greenLedPins[i], LOW);
  }
  Serial.println("tutti led verdi spenti");
}

void fadeRedLed(int fadeDelay) {
  //fade in
  for (int brightness  = 0; brightness  <= 255; brightness += 5)
    analogWrite(redLedPin, brightness );
  delay(fadeDelay);
  //fade out
  for (int brightness  = 255; brightness  >= 0; brightness  -= 5 ) {
    analogWrite(redLedPin, brightness );
    delay(fadeDelay);
  }
}

void welcomeMessage() {
  lcd.setCursor(0, 0);
  lcd.print("Welcome to TOS!");
  lcd.setCursor(0, 1);
  lcd.print("Press B1 to start");
}

boolean pulsantePremuto(int pin) {
  if (digitalRead(pin) == HIGH) {
    //btn premuto in tempo, avvio gioco
    return true;
  }
  return false;
}

void enterDeepSleepUntilB1() {
  if(pulsantePremuto(btn[0]) == true ){
    //il pulsante é stato premuto durante il timer quindi si passa al gioco
    btn1pressed = true;
    startTime = millis(); // reset del timer
    giocoAvviato = true;
    Serial.println("Pulsante premuto!");
  }

  //se sono passati piu di 10 sec e non é stato premuto btn entro in deep sleep
  //millis ritorna numero millisecondi passati dall avvio del programma
  if (!btn1pressed && millis() - startTime >= maxTime) {
    Serial.println("letsgooo sleep");
    wakeUpFlag = false;
    enterDeepSleep();
  }
}

void enterDeepSleep() {
  // 1. Pulizia fisica delle periferiche
  lcd.noDisplay();
  digitalWrite(redLedPin, LOW);
  allGreenLedsOff();
  
  // *** DEBUG: Stampa un messaggio molto presto per sapere che siamo arrivati qui ***
  Serial.println("Tentativo di Deep Sleep...");
  Serial.flush(); // Garantisce che il buffer sia vuoto ORA.

  // 2. Disabilita Serial e ADC prima di impostare lo sleep
  
  // Disabilita la Serial Communication (RX e TX)
  // Questo spegne gli interrupt seriali che potrebbero risvegliare la CPU.
  UCSR0B &= ~(_BV(RXEN0) | _BV(TXEN0)); 
  
  // Disabilita l'ADC (Analog-to-Digital Converter)
  ADCSRA &= ~_BV(ADEN); 
  
  // 3. Setup Interrupt per il risveglio
  attachInterrupt(digitalPinToInterrupt(btn[0]), wakeUp, RISING); 
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  
  // 4. Sequenza atomica
  noInterrupts(); // Blocca tutti gli interrupt
  interrupts();   // Riabilita (questo deve avvenire prima di sleep_cpu())
  sleep_cpu();    // LA CPU SI FERMA QUI!
  
  // *** L'ESECUZIONE RIPRENDE QUI DOPO IL RISVEGLIO ***
  
  // 5. Riattiva le periferiche
  sleep_disable();
  detachInterrupt(digitalPinToInterrupt(btn[0])); 
  
  // Riabilita Serial e ADC
  UCSR0B |= _BV(RXEN0) | _BV(TXEN0); // Riabilita Serial
  ADCSRA |= _BV(ADEN); // Riabilita l'ADC

  Serial.println("RISVEGLIO AVVENUTO! Esecuzione nel loop..."); 
}

//isr, interrupt service routine
void wakeUp() {
  noInterrupts(); // Disabilita gli interrupt all'interno della ISR per sicurezza.
  wakeUpFlag = true;
  interrupts(); // Riabilita gli interrupt
}


void setup() {
  initHardware();
  welcomeMessage();
  generateSequence();
}

void loop() {
  // A. GESTIONE DEL RISVEGLIO / INIZIALIZZAZIONE STATO
  if (wakeUpFlag) {
    Serial.println("Reset dello stato: Avvio o Risveglio dal sonno.");
    // ... (Logica di reset invariata: lcd.display(), fadeRedLed, reset variabili, delay(100))
    lcd.display();
    fadeRedLed(20); 
    giocoAvviato = false; 
    btn1pressed = false;
    startTime = millis(); 
    wakeUpFlag = false; 
    
    // Attesa di rilascio del pulsante per evitare il rimbalzo
    Serial.println("Attendendo rilascio pulsante...");
    while (digitalRead(btn[0]) == HIGH) {
        delay(1); 
    }
    Serial.println("Pulsante rilasciato. Inizio ciclo di 10s.");
    
    return; // Torna subito all'inizio del loop
  }

  // --- STATI PRINCIPALI ---

  if (!giocoAvviato) {
    // STATO 1: ATTESA O SONNO (Timer di 10 secondi in corso)
    
    // 1. Check rapido del pulsante: AVVIO GIOCO
    if (digitalRead(btn[0]) == HIGH) {
        Serial.println("Pulsante premuto in tempo! Avvio Gioco.");
        giocoAvviato = true;
        btn1pressed = true;
        delay(50); 
        // Non usare return; qui, permetti all'esecuzione di cadere nel blocco "Game is running"
    }

    // 2. Check del timeout: DEEP SLEEP
    if (!btn1pressed && millis() - startTime >= maxTime) {
        Serial.print("Tempo T scaduto. Inizio Deep Sleep.");
        enterDeepSleep();
        // L'esecuzione riprenderà dal blocco di gestione del risveglio (A)
    }
  } else {
    // STATO 2: GIOCO AVVIATO
    Serial.println("Game is running - Gioco attivo indefinitamente.");
    delay(2000);
    // ... logica del gioco ...
  }
}