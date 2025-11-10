/* 
AUTHORS:
BOTTEGHI MATTEO     0001129907
MULARONI MATTIA     0001126065
MONTANARI NICOLAS   0001128064 
*/

#include <LiquidCrystal_I2C.h>
#include <avr/sleep.h>

const int greenLedPins[] = {12, 11, 10, 9};
const int redLedPin = 6;
const int btn[] = {2, 3, 4, 5};
const int potPin = A0;

int brightness = 0;
int fadeAmount = 5;

boolean giocoAvviato = false;
boolean btn1pressed = false;

const int NUM_DIGITS = 4;
int sequence[NUM_DIGITS];
const int NUM_greenLed = sizeof(greenLedPins) / sizeof(greenLedPins[0]);
const int NUM_btn = sizeof(btn) / sizeof(btn[0]);

unsigned long previousFadeMillis = 0;
const long fadeInterval = 30; // 30ms between every brightness interval

int difficultyLevel = 1;
float factorF = 0.9;
int potValue = 0;

unsigned long baseTime = 5000;
unsigned long currentTimeLimit;
int score = 0;
bool gameOver = false;

const unsigned long MAX_WAIT_TIME = 10000; // 10 sec
unsigned long sleepStartTime = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);

void updateRedLedFade() {
  if(!giocoAvviato){
    unsigned long currentMillis = millis();
    if (currentMillis - previousFadeMillis >= fadeInterval) {
      previousFadeMillis = currentMillis;
      analogWrite(redLedPin, brightness);
      brightness = brightness + fadeAmount;
      if (brightness <= 0 || brightness >= 255) {
        fadeAmount = -fadeAmount;
      }
    }
  }
}

// Non blocking delay that keep red led and sleep functions updated
void smartDelay(unsigned long ms) {
  unsigned long __start = millis();
  while (millis() - __start < ms) {
    updateRedLedFade();
    waitForSleep();
  }
}

void initHardware() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.init();
  lcd.backlight();

  // green leds
  for (int i = 0; i < NUM_greenLed; i++) pinMode(greenLedPins[i], OUTPUT);
  // red led
  pinMode(redLedPin, OUTPUT);
  // buttons
  for (int i = 0; i < NUM_btn; i++) pinMode(btn[i], INPUT);

  Serial.println("Inizializzazione completata");
}

void wakeupCallback() {
  Serial.println("Wake up");
}

void allGreenLedsOff() {
  for (int i = 0; i < NUM_greenLed; i++) digitalWrite(greenLedPins[i], LOW);
}

void sleep() {
  allGreenLedsOff();
  analogWrite(redLedPin, 0);
  lcd.noBacklight();
  lcd.noDisplay();

  Serial.println("Entro in deep sleep...");
  Serial.flush();

  EIFR = bit(INTF0); //remove previous interrupts

  attachInterrupt(digitalPinToInterrupt(btn[0]), wakeupCallback, RISING);

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  sleep_cpu();
  sleep_disable();
  detachInterrupt(digitalPinToInterrupt(btn[0]));

  lcd.display();
  lcd.backlight();
  Serial.println("Risvegliato!");

  sleepStartTime = millis();
}

void waitForSleep() {
  if (sleepStartTime == 0) {
    sleepStartTime = millis();
  }
  // reset timer if button is clicked
  if (digitalRead(btn[0]) == HIGH) {
    sleepStartTime = 0;
    return;
  }
  // Check if 10sec is over
  if (millis() - sleepStartTime >= MAX_WAIT_TIME) {
    sleep();
    sleepStartTime = 0;
  }
}

void displayAndScroll(const char* text, int row) {
  int textLength = strlen(text);

  lcd.setCursor(0, row);
  lcd.print(text);

  if (textLength > 16) {
    int scrollSteps = textLength - 16;
    smartDelay(800);
    // Scroll left
    for (int i = 0; i < scrollSteps; i++) {
      lcd.scrollDisplayLeft();
      smartDelay(350);
    }
    
    smartDelay(800);
    // Move text on initial position
    for (int i = 0; i < scrollSteps; i++) {
      lcd.scrollDisplayRight();
    }
  }
}

void welcomeMessage() {
  displayAndScroll("Welcome to TOS!", 0);
  displayAndScroll("Press B1 to Start", 1);
}

// Random sequence of 4 number from 1 to 4 without repetitions
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

// Display the random sequence
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

void readDifficultyLevel() {
  potValue = analogRead(potPin);

  if (potValue <= 255) { // easy
    difficultyLevel = 1; factorF = 0.9; baseTime = 8000;
  } else if (potValue <= 510) { // medium
    difficultyLevel = 2; factorF = 0.8; baseTime = 6000;
  } else if (potValue <= 765) { // difficult
    difficultyLevel = 3; factorF = 0.7; baseTime = 4000;
  } else { // extreme
    difficultyLevel = 4; factorF = 0.6; baseTime = 2500;
  }
}

void waitForAllButtonsRelease() {
  bool allReleased = false;
  while (!allReleased) {
    allReleased = true;
    for (int i = 0; i < NUM_btn; i++) {
      if (digitalRead(btn[i]) == HIGH) {
        allReleased = false;
      }
    }
    smartDelay(20); // debounce
  }
}

// Read the player sequence and compare between the one generated
void readSequence() {
  int inputSequence[NUM_DIGITS];
  int index = 0;
  bool lastBtnState[NUM_btn] = {LOW, LOW, LOW, LOW};
  unsigned long startRound = millis();

  // Wait for player to press all buttons
  while (index < NUM_DIGITS) {
    for (int i = 0; i < NUM_btn; i++) {
      int state = digitalRead(btn[i]);

      if (state == HIGH && lastBtnState[i] == LOW) {
        inputSequence[index] = i + 1;
        // turn on the corrisponding led
        digitalWrite(greenLedPins[i], HIGH);
        smartDelay(200);
        digitalWrite(greenLedPins[i], LOW);
        index++;
      }
      lastBtnState[i] = state;
    }
    // If the time limit is passed go into gameOver state
    if (millis() - startRound > currentTimeLimit) {
      gameOver = true;
      return;
    }
  }

  waitForAllButtonsRelease();

  // Compare the sequence
  bool corretta = true;
  for (int i = 0; i < NUM_DIGITS; i++) {
    if (inputSequence[i] != sequence[i]) {
      corretta = false;
      break;
    }
  }

  lcd.clear();
  if (corretta) {
    score += 10;
    lcd.print("GOOD!");
    lcd.setCursor(0, 1);
    lcd.print("Score: ");
    lcd.print(score);
    smartDelay(1500);
    currentTimeLimit *= factorF;
  } else {
    gameOver = true;
  }
}

void gameOverScreen() {
  lcd.clear();
  lcd.print("GAME OVER");
  lcd.setCursor(0, 1);
  lcd.print("Final Score:");
  lcd.print(score);

  analogWrite(redLedPin, 255);
  smartDelay(2000);
  analogWrite(redLedPin, 0);

  giocoAvviato = false;
  score = 0;
  currentTimeLimit = baseTime;
}

void setup() {
  initHardware();
  welcomeMessage();
  randomSeed(analogRead(A1));
}

void loop() {
  if (!giocoAvviato) {
    updateRedLedFade();
    welcomeMessage();
    readDifficultyLevel();

    // If B1 is pressed, then start the game
    if (digitalRead(btn[0]) == HIGH) {
      giocoAvviato = true;
      btn1pressed = true;
      score = 0;
      currentTimeLimit = baseTime;
      sleepStartTime = 0;

      // turn off red led
      analogWrite(redLedPin, 0);

      lcd.clear();
      lcd.print("Go!");
      smartDelay(1000);

      lcd.clear();
      lcd.print("Livello: ");
      lcd.print(difficultyLevel);
      smartDelay(1000);
    }
  }

  if (giocoAvviato) {
    if (gameOver) {
      gameOverScreen();
      welcomeMessage();
      gameOver = false;
    } else {
      generateSequence();
      displaySequence();

      waitForAllButtonsRelease();
      readSequence();

      if (gameOver) {
        gameOverScreen();
        welcomeMessage();
        gameOver = false;
      }
    }
  }
}