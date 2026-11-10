#include Servo.h

#define NUM_SERVOS 4

 ============================================================
 PIN ASSIGNMENTS
 ============================================================

Servo servos[NUM_SERVOS];

int servoPins[NUM_SERVOS] = { 6, 11, 10, 9 };
int potPins[NUM_SERVOS]   = {A0, A1, A2, A3 };

const int toggleButton = 2;
const int ledPin       = 4;
const int pumpButton = 3; 

const int signalPin = 12;

 ============================================================
 SERVO ASSIGNMENTS

   Index 0 = Base      (rotates whole arm)
   Index 1 = Wrist
   Index 2 = Elbow
   Index 3 = Shoulder
 ============================================================

int servoMin[NUM_SERVOS] = {  0,   0,   0,   0 };
int servoMax[NUM_SERVOS] = {180, 180, 180, 180 };

 ============================================================
 POSITIONS
 ============================================================

int defaultPositions[NUM_SERVOS] = {90, 0, 0, 0};

#define MAX_PHASES 8
int autoPhase1[NUM_SERVOS] = { 140, 55, 32, 74}; pickup
int autoPhase2[NUM_SERVOS] = { 141, 55, 32, 16}; lift up
int autoPhase3[NUM_SERVOS] = { 36, 55, 32, 16};  rotate and put object down

int hardcodedPhases[] = {autoPhase1, autoPhase2, autoPhase3}; 
const int NUM_HARDCODED_PHASES = 3;

int recordedPhases[MAX_PHASES][NUM_SERVOS];
int recordedPhaseCount = 0;

 ============================================================
 PAUSE DURATIONS
 ============================================================

unsigned long phasePauseMS[] = {3000, 0, 3000};
                               ^         ^
                            phase 1   phase 3

 ============================================================
 CALIBRATION MODE
 ============================================================

const bool CALIBRATION_MODE = false;

 ============================================================
 RUNTIME STATE
 ============================================================

int currentPositions[NUM_SERVOS] = {0, 0, 0, 0};
int targetPositions[NUM_SERVOS]  = {0, 0, 0, 0};

int potBaseline[NUM_SERVOS]   = {0, 0, 0, 0};
int servoBaseline[NUM_SERVOS] = {0, 0, 0, 0};

bool manualMode      = false;
bool lastButtonState = HIGH;

unsigned long pressStartTime      = 0;
const unsigned long LONG_PRESS_MS = 1000;

bool pumpState = false; 
bool lastPumpButtonState = HIGH; 

const int stepDelay = 20;
unsigned long lastStepTime = 0;

 States MOVING, PAUSING
enum AutoState { MOVING, PAUSING };
AutoState autoState  = MOVING;
bool autoRunning     = false;
int  activeServo     = 0;
int  activePhase     = 0;
unsigned long pauseStartTime = 0;

bool movingToDefault = false;

 ============================================================
 HELPERS
 ============================================================

int clamp(int value, int minVal, int maxVal) {
  return max(minVal, min(maxVal, value));
}

int getPhase(int index) {
  return recordedPhaseCount  0  recordedPhases[index]  hardcodedPhases[index];
}

int numPhases() {
  return recordedPhaseCount  0  recordedPhaseCount  NUM_HARDCODED_PHASES;
}

void loadPhase(int phase) {
  int p = getPhase(phase);
  for (int i = 0; i  NUM_SERVOS; i++) {
    targetPositions[i] = clamp(p[i], servoMin[i], servoMax[i]);
  }
  activeServo = 0;
  Serial.print(Auto loading phase );
  Serial.println(phase);
}

 ============================================================
 SETUP
 ============================================================

void setup() {

  Serial.begin(9600);
  pinMode(toggleButton, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(signalPin, OUTPUT);
   pinMode(pumpButton, INPUT_PULLUP); 

  digitalWrite(signalPin, LOW);  start with pump off

  delay(500);

  if (CALIBRATION_MODE) {

    Serial.println(CALIBRATION MODE all servos at 90.);

    for (int i = 1; i  NUM_SERVOS; i++) {
      servos[i].attach(servoPins[i]);
      servos[i].write(0);
    }
    servos[1].write(90);
    return;
  }

  for (int i = 0; i  NUM_SERVOS; i++) {
    currentPositions[i] = clamp(defaultPositions[i], servoMin[i], servoMax[i]);
    targetPositions[i]  = currentPositions[i];
    servos[i].attach(servoPins[i]);
    servos[i].write(currentPositions[i]);
  }

  lastStepTime = millis();
  startAutomatic();
}

 ============================================================
 MAIN LOOP
 ============================================================

void loop() {

  if (CALIBRATION_MODE) return;

  handleButton();
  handlePumpButton();
  updateServos();

  if (movingToDefault) {
    checkDefaultComplete();
    return;
  }

  if (manualMode) {
    digitalWrite(ledPin, HIGH);
    runManualMode();
  } else {
    digitalWrite(ledPin, LOW);
    runAutomaticMode();
  }
}

 ============================================================
 POSITION RECORDING
 ============================================================

void recordCurrentPosition() {

  if (!manualMode) return;

  if (recordedPhaseCount = MAX_PHASES) {
    Serial.println(Record phase slots full.);
    return;
  }

  for (int i = 0; i  NUM_SERVOS; i++) {
    recordedPhases[recordedPhaseCount][i] = currentPositions[i];
  }

  Serial.print(Recorded phase );
  Serial.print(recordedPhaseCount);
  Serial.print( );
  for (int i = 0; i  NUM_SERVOS; i++) {
    Serial.print(currentPositions[i]);
    if (i  NUM_SERVOS - 1) Serial.print(, );
  }
  Serial.println();

  recordedPhaseCount++;
}

 ============================================================
 DEFAULT RETURN
 ============================================================

void moveToDefault() {

  movingToDefault = true;

   Start with SHOULDER FIRST (index 3)
  activeServo = 3;

   Set all targets
  for (int i = 0; i  NUM_SERVOS; i++) {
    targetPositions[i] = clamp(defaultPositions[i], servoMin[i], servoMax[i]);
  }

  Serial.println(Moving to default);
}

void checkDefaultComplete() {

  if (currentPositions[activeServo] != targetPositions[activeServo]) return;

   shoulder  base  elbow  wrist 
  if (activeServo == 3) activeServo = 0;    after shoulder  base
  else if (activeServo == 0) activeServo = 1;
  else if (activeServo == 1) activeServo = 2;
  else if (activeServo == 2) {
    
     DONE
    movingToDefault = false;
    Serial.println(Default reached.);

    if (!manualMode) {
      startAutomatic();
    } else {
      snapshotPotsForManual();
    }

    return;
  }
}

 ============================================================
C MODE    STATE MACHINE

 MOVING   steps activeServo toward target one degree at a time.
           When activeServo finishes, advances to next servo.
           When all servos finish a phase, checks pause duration.
           If pause  0, switches to PAUSING state.
           If pause = 0, loads next phase immediately.
           After last phase, loops back to phase 0.

 PAUSING  waits for phasePauseMS[completedPhase] to elapse,
           then loads the next phase and returns to MOVING.
 ============================================================

void startAutomatic() {
  autoRunning  = true;
  autoState    = MOVING;
  activePhase  = 0;
  activeServo  = 0;
  loadPhase(activePhase);
}

void runAutomaticMode() {

  if (!autoRunning) return;

   ---- PAUSING state ----
  if (autoState == PAUSING) {
    if (millis() - pauseStartTime = phasePauseMS[activePhase]) {
       Pause over, advance to next phase
      activePhase = (activePhase + 1) % numPhases();
      loadPhase(activePhase);
      autoState = MOVING;
      Serial.print(Auto pause done, moving to phase );
      Serial.println(activePhase);
    }
    return;
  }

   ---- MOVING state ----
   Check if current active servo has reached its target
  if (currentPositions[activeServo] != targetPositions[activeServo]) return;

   This servo is done, advance to next servo
  activeServo++;

   If all servos in this phase are done
  if (activeServo = NUM_SERVOS) {

    Serial.print(Auto phase );
    Serial.print(activePhase);
    Serial.println( complete.);

     ---- Signal control ----
    if (activePhase == 0) {         After phase 1
      digitalWrite(signalPin, HIGH);
      Serial.println(Signal HIGH (after phase 1));
    }
    if (activePhase == 2) {         After phase 3
      digitalWrite(signalPin, LOW);
      Serial.println(Signal LOW (after phase 3));
    }

    if (phasePauseMS[activePhase]  0) {
       Enter pause before loading next phase
      autoState      = PAUSING;
      pauseStartTime = millis();
      Serial.print(Auto pausing for );
      Serial.print(phasePauseMS[activePhase]);
      Serial.println(ms.);
    } else {
       No pause, load next phase immediately
      activePhase = (activePhase + 1) % numPhases();
      loadPhase(activePhase);
    }
  }
}

 ============================================================
 MANUAL MODE
 ============================================================

void snapshotPotsForManual() {
  for (int i = 0; i  NUM_SERVOS; i++) {
    int raw        = analogRead(potPins[i]);
    potBaseline[i] = map(raw, 0, 1023, 0, 180);
    servoBaseline[i] = currentPositions[i];
  }
  Serial.println(Manual ready. Long press to record a phase.);
}

void runManualMode() {

  for (int i = 0; i  NUM_SERVOS; i++) {

    int potValue = analogRead(potPins[i]);
    targetPositions[i] = map(potValue, 0, 1023, 0, 180);
  }
}

 ============================================================
 SERVO MOTION ENGINE
 ============================================================

void updateServos() {

  if (millis() - lastStepTime  stepDelay) return;
  lastStepTime = millis();

  for (int i = 0; i  NUM_SERVOS; i++) {

     In auto MOVING state, only step the active servo
    if (!manualMode && autoRunning && autoState == MOVING && !movingToDefault && i != activeServo) continue;

     During default return, only step the active servo
    if (movingToDefault && i != activeServo) continue;

    if (currentPositions[i]  targetPositions[i]) {
      currentPositions[i]++;
      servos[i].write(currentPositions[i]);
    } else if (currentPositions[i]  targetPositions[i]) {
      currentPositions[i]--;
      servos[i].write(currentPositions[i]);
    }
  }
}

 ============================================================
 BUTTON
 Short press  toggle manualauto
 Long press   record position (manual mode only)
 ============================================================

void handleButton() {

  bool buttonState = digitalRead(toggleButton);

  if (lastButtonState == HIGH && buttonState == LOW) {
    pressStartTime = millis();
  }

  if (lastButtonState == LOW && buttonState == HIGH) {

    unsigned long pressDuration = millis() - pressStartTime;

    if (pressDuration = LONG_PRESS_MS) {
      recordCurrentPosition();
    } else {
      manualMode  = !manualMode;
      autoRunning = false;

      Serial.print(Mode );
      Serial.println(manualMode  MANUAL  AUTOMATIC);

      digitalWrite(signalPin, LOW);
      moveToDefault();
    }

    delay(300);
  }

  lastButtonState = buttonState;
}

void handlePumpButton() {

  bool buttonState = digitalRead(pumpButton);

  if (lastPumpButtonState == HIGH && buttonState == LOW) {

    pumpState = !pumpState;

    digitalWrite(signalPin, pumpState  HIGH  LOW);

    Serial.print(Pump );
    Serial.println(pumpState  INHALE  EXHALE);

    delay(200);
  }

  lastPumpButtonState = buttonState;
}