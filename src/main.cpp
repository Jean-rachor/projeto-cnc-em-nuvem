#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Stepper.h>

// RC522 wiring documented for the ESP32 DOIT DevKit V1.
constexpr uint8_t RFID_SS_PIN = 5;
constexpr uint8_t RFID_RST_PIN = 22;
constexpr uint8_t RFID_SCK_PIN = 18;
constexpr uint8_t RFID_MISO_PIN = 19;
constexpr uint8_t RFID_MOSI_PIN = 23;

// Button remains enabled; GPIO 27 is used by the green LED in the gate circuit.
constexpr uint8_t BUTTON_PIN = 13;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 35;

constexpr uint8_t LED_GREEN_PIN = 27;
constexpr uint8_t LED_RED_PIN = 26;
constexpr uint8_t MOTOR_IN1_PIN = 25;
constexpr uint8_t MOTOR_IN2_PIN = 14;
constexpr uint8_t MOTOR_IN3_PIN = 32;
constexpr uint8_t MOTOR_IN4_PIN = 33;
constexpr int STEPS_PER_REVOLUTION = 2048;

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
// Arduino Stepper order for a 28BYJ-48: IN1, IN3, IN2, IN4.
Stepper motor(STEPS_PER_REVOLUTION, MOTOR_IN1_PIN, MOTOR_IN3_PIN,
              MOTOR_IN2_PIN, MOTOR_IN4_PIN);

// Set these bytes to your card UID, then enable authorization.
constexpr bool AUTHORIZED_UID_CONFIGURED = false;
const byte AUTHORIZED_UID[] = {0x00, 0x00, 0x00, 0x00};

bool isAuthorizedCard() {
  if (!AUTHORIZED_UID_CONFIGURED || rfid.uid.size != sizeof(AUTHORIZED_UID)) {
    return false;
  }

  for (byte i = 0; i < rfid.uid.size; ++i) {
    if (rfid.uid.uidByte[i] != AUTHORIZED_UID[i]) {
      return false;
    }
  }
  return true;
}

void printCardUid() {
  Serial.print("UID: ");
  for (byte i = 0; i < rfid.uid.size; ++i) {
    if (i > 0) Serial.print(' ');
    if (rfid.uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();
}

void releaseMotor() {
  digitalWrite(MOTOR_IN1_PIN, LOW);
  digitalWrite(MOTOR_IN2_PIN, LOW);
  digitalWrite(MOTOR_IN3_PIN, LOW);
  digitalWrite(MOTOR_IN4_PIN, LOW);
}

void runMotorCycle() {
  motor.step(STEPS_PER_REVOLUTION);
  delay(1000);
  motor.step(-STEPS_PER_REVOLUTION);
  releaseMotor();
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  pinMode(MOTOR_IN3_PIN, OUTPUT);
  pinMode(MOTOR_IN4_PIN, OUTPUT);

  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
  releaseMotor();

  motor.setSpeed(10);
  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);
  rfid.PCD_Init();

  Serial.println("Leitor RFID pronto. Aproxime uma tag.");
}

void loop() {
  static int lastButtonReading = HIGH;
  static int debouncedButtonState = HIGH;
  static unsigned long lastButtonChangeMs = 0;

  const int buttonReading = digitalRead(BUTTON_PIN);
  const unsigned long now = millis();

  if (buttonReading != lastButtonReading) {
    lastButtonReading = buttonReading;
    lastButtonChangeMs = now;
  }

  if (buttonReading != debouncedButtonState &&
      now - lastButtonChangeMs >= BUTTON_DEBOUNCE_MS) {
    debouncedButtonState = buttonReading;
    if (debouncedButtonState == LOW) {
      Serial.println("BOTAO_PRESSIONADO");
    }
  }

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  printCardUid();
  const bool authorized = isAuthorizedCard();

  if (authorized) {
    Serial.println("Acesso autorizado");
    digitalWrite(LED_GREEN_PIN, HIGH);
    digitalWrite(LED_RED_PIN, LOW);

    Serial.println("Motor: ida e volta");
    runMotorCycle();
  } else {
    Serial.println("Acesso negado");
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_RED_PIN, HIGH);
  }

  delay(2000);
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
