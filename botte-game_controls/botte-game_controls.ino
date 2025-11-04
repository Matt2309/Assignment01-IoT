#include <LiquidCrystal_I2C.h>
#include <avr/sleep.h>

const int greenLedPins[] = {12, 11, 10, 9};
const int redLedPin = 8;
const int btn[] = {2, 3, 4, 5};
const int potPin = A0;

boolean giocoAvviato = false;
boolean btn1pressed = false;
volatile boolean wakeUpFlag = true;

const int NUM_DIGITS = 4;
int sequence[NUM_DIGITS];
const int NUM_greenLed = sizeof(greenLedPins) / sizeof(greenLedPins[0]);
const int NUM_btn = sizeof(btn) / sizeof(btn[0]);

int difficultyLevel = 1;
float factorF = 0.9;
int potValue = 0;

unsigned long baseTime = 5000;
unsigned long currentTimeLimit;
int score = 0;
bool gameOver = false;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Inizializza LED, pulsanti e LCD
void initHardware() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // LED verdi
  for (int i = 0; i < NUM_greenLed; i++) pinMode(greenLedPins[i], OUTPUT);
  // LED rosso
  pinMode(redLedPin, OUTPUT);
  // Pulsanti
  for (int i = 0; i < NUM_btn; i++) pinMode(btn[i], INPUT);

  Serial.println("Inizializzazione completata");
}

// Spegne tutti i LED verdi
void allGreenLedsOff() {
  for (int i = 0; i < NUM_greenLed; i++) digitalWrite(greenLedPins[i], LOW);
}

// Fa lampeggiare lentamente il LED rosso in attesa del giocatore
void fadeRedLedWait() {
  static int brightness = 0;
  static int fadeAmount = 5;
  analogWrite(redLedPin, brightness);
  brightness += fadeAmount;

  // Inverte direzione quando raggiunge massimo o minimo
  if (brightness <= 0 || brightness >= 255) fadeAmount = -fadeAmount;
  delay(15);
}

void displayAndScroll(const char* text, int row) {
  int textLength = strlen(text);

  lcd.setCursor(0, row);
  lcd.print(text);

  if (textLength > 16) {
    int scrollSteps = textLength - 16 + 1;
    
    lcd.setCursor(16, row); 

    for (int i = 0; i < scrollSteps; i++) {
      lcd.scrollDisplayLeft();
      delay(350);
    }
    
    for (int i = 0; i < scrollSteps; i++) {
      lcd.scrollDisplayRight();
    }
  }
}

// Mostra il messaggio di benvenuto
void welcomeMessage() {
  lcd.begin(16, 2);
  
  displayAndScroll("Benvenuti a TOS!", 0);
  displayAndScroll("Premi B1 per start", 1);
}

// Crea una sequenza casuale di numeri da 1 a 4 (senza ripetizioni)
void generateSequence() {
  int number[] = {1, 2, 3, 4};
  for (int i = 0; i < NUM_DIGITS; i++) {
    int j = random(i, NUM_DIGITS);
    int temp = number[i];
    number[i] = number[j];
    number[j] = temp;
    sequence[i] = number[i];
  }
}

// Mostra la sequenza sul display LCD
void displaySequence() {
  char seqStr[5] = "";
  for (int i = 0; i < NUM_DIGITS; i++) seqStr[i] = sequence[i] + '0';
  seqStr[NUM_DIGITS] = '\0';

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sequence:");
  lcd.setCursor(6, 1);
  lcd.print(seqStr);
}

// Legge il potenziometro e imposta la difficoltà
void readDifficultyLevel() {
  potValue = analogRead(potPin);

  // 4 intervalli che determinano il livello
  if (potValue <= 255) {
    difficultyLevel = 1;
    factorF = 0.9;
    baseTime = 8000;
  } else if (potValue <= 510) {
    difficultyLevel = 2;
    factorF = 0.8;
    baseTime = 6000;
  } else if (potValue <= 765) {
    difficultyLevel = 3;
    factorF = 0.7;
    baseTime = 4000;
  } else {
    difficultyLevel = 4;
    factorF = 0.6;
    baseTime = 2500;
  }
}

// Legge la sequenza inserita dal giocatore e la confronta con quella corretta
void readSequence() {
  int inputSequence[NUM_DIGITS];
  int index = 0;
  bool lastBtnState[NUM_btn] = {LOW, LOW, LOW, LOW};
  unsigned long startRound = millis();

  // Attende che il giocatore prema i 4 tasti
  while (index < NUM_DIGITS) {
    for (int i = 0; i < NUM_btn; i++) {
      int state = digitalRead(btn[i]);
      
      // Se un pulsante passa da non premuto a premuto
      if (state == HIGH && lastBtnState[i] == LOW) {
        inputSequence[index] = i + 1;
        
        // Accende temporaneamente il LED corrispondente
        digitalWrite(greenLedPins[i], HIGH);
        delay(200);
        digitalWrite(greenLedPins[i], LOW);

        index++;
      }
      lastBtnState[i] = state;
    }

    // Controlla se il tempo limite è scaduto
    if (millis() - startRound > currentTimeLimit) {
      gameOver = true;
      return;
    }
  }

  // Attende che tutti i tasti siano rilasciati prima di verificare
  waitForAllButtonsRelease();

  // Confronta la sequenza digitata con quella corretta
  bool corretta = true;
  for (int i = 0; i < NUM_DIGITS; i++) {
    if (inputSequence[i] != sequence[i]) {
      corretta = false;
      break;
    }
  }

  // Aggiorna il punteggio o termina il gioco
  lcd.clear();
  if (corretta) {
    score += 10;
    lcd.print("BUONO!");
    lcd.setCursor(0, 1);
    lcd.print("Score: ");
    lcd.print(score);
    delay(1500);
    // Riduce il tempo disponibile per il round successivo
    currentTimeLimit *= factorF;
  } else {
    gameOver = true;
  }
}

// Mostra schermata di Game Over e resetta lo stato
void gameOverScreen() {
  lcd.clear();
  lcd.print("GAME OVER");
  lcd.setCursor(0, 1);
  lcd.print("Score: ");
  lcd.print(score);

  // Lampeggia il LED rosso tre volte
  for (int i = 0; i < 3; i++) {
    analogWrite(redLedPin, 255);
    delay(200);
    analogWrite(redLedPin, 0);
    delay(200);
  }

  delay(2000);
  giocoAvviato = false;
  score = 0;
  currentTimeLimit = baseTime;
}

// Attende che tutti i pulsanti siano rilasciati
void waitForAllButtonsRelease() {
  bool allReleased = false;
  while (!allReleased) {
    allReleased = true;
    for (int i = 0; i < NUM_btn; i++) {
      if (digitalRead(btn[i]) == HIGH) {
        allReleased = false;
      }
    }
    delay(20); // piccolo debounce
  }
}

// Impostazioni iniziali
void setup() {
  initHardware();
  welcomeMessage();
  randomSeed(analogRead(A1)); // inizializza il generatore casuale
}

// Ciclo principale del gioco
void loop() {
  // Fase iniziale: attesa che il giocatore inizi
  if (!giocoAvviato) {
    welcomeMessage();
    fadeRedLedWait();           // LED rosso lampeggia dolcemente
    readDifficultyLevel();      // Legge continuamente la difficoltà

    // Se viene premuto B1, inizia la partita
    if (digitalRead(btn[0]) == HIGH) {
      giocoAvviato = true;
      btn1pressed = true;
      score = 0;
      currentTimeLimit = baseTime; // imposta tempo in base alla difficoltà

      lcd.clear();
      lcd.print("Livello: ");
      lcd.print(difficultyLevel);
      delay(1000);

      lcd.clear();
      lcd.print("Vai!");
      delay(1000);

      generateSequence();  // genera la prima sequenza
      displaySequence();   // mostra la sequenza
      delay(1500);

      waitForAllButtonsRelease(); // attende rilascio prima di leggere
      readSequence();             // legge la risposta del giocatore
    }

    return; // torna all'inizio se il gioco non è partito
  }

  // Fase di gioco attivo
  if (gameOver) {
    gameOverScreen();     // mostra schermata di fine gioco
    welcomeMessage();     // torna allo stato iniziale
    gameOver = false;
  } else {
    generateSequence();   // genera nuova sequenza
    displaySequence();
    delay(1000);

    waitForAllButtonsRelease();
    readSequence();

    if (gameOver) {
      gameOverScreen();
      welcomeMessage();
      gameOver = false;
    }
  }
}
