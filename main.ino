#include <SevSegShift.h>
#include <Wire.h>
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include <Button.h>

static unsigned long g_timer = 0;
int g_setTemperature = 95;
int MIN_TEMP = 15;
int MAX_TEMP = 100;
int STRIDE = 5;
int heaterPin = 2;

// Thermistor setup
float Ro = 10., B =  3950;
float Rseries = 14.4; // Calibration resistance, in kOhm
float To = 288.45; // Calibration temperature, in K
int thermistorPin = 0;

// Rotary encoder setup
Encoder knob(3, 11); // D3 and D2 reserved for rotary encoder
long g_oldPosition = -999;
const int enterButton = 12; // D8 reserved for rotary encoder button

// 7-segment setup
#define SHIFT_PIN_DS   7 /* Data input PIN */
#define SHIFT_PIN_STCP 9 /* Shift Register Storage PIN */
#define SHIFT_PIN_SHCP 10 /* Shift Register Shift PIN */
SevSegShift sevseg(
  SHIFT_PIN_DS, 
  SHIFT_PIN_SHCP, 
  SHIFT_PIN_STCP, 
  1, /* number of shift registers there is only 1 Shiftregister 
    used for all Segments (digits are on Controller)
    default value = 2 (see SevSegShift example)
    */
  true /* Digits are connected to Arduino directly 
  default value = false (see SevSegShift example)
  */
);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(enterButton, INPUT_PULLUP);
  pinMode(heaterPin, OUTPUT);
  g_oldPosition = knob.read();
  // 7-segment setup
  byte numDigits = 3;
  byte digitPins[] = {6,5,4}; // These are the PINS of the ** Arduino **
  byte segmentPins[] = {0, 2, 4, 6, 7, 1, 3, 5}; // these are the PINs of the ** Shift register **
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_ANODE; // See README.md for options
  bool updateWithDelays = false; // Default 'false' is Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
  bool disableDecPoint = false; // Use 'true' if your decimal point doesn't exist or isn't connected. Then, you only need to specify 7 segmentPins[]
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments,
  updateWithDelays, leadingZeros, disableDecPoint);
  sevseg.setNumber(88,0);
  sevseg.setBrightness(50);
  sevseg.refreshDisplay();
  Serial.println("Ready!");
}

int read_rotary_encoder() {
  // Responde con:
  // -1 si se rota contra-reloj
  // 0 si no hay rotación
  // 1 si se rota reloj
  if (knob.read() == g_oldPosition) {
    return 0;
  }
  long currentPosition = knob.read();
  if (currentPosition != g_oldPosition) {
    if (currentPosition >= g_oldPosition+4)  {
      g_oldPosition = currentPosition;
      return 1;
    }
    else if (currentPosition <= g_oldPosition-4) {
      g_oldPosition = currentPosition;
      return -1;
    }
    else {
      return 0;
    }
  }
}

float read_instant_therm_resistance(float referenceResistance = Ro, float referenceVoltage = 3.3) {
  float readVoltage = analogRead(thermistorPin)*5./1023;
  return referenceResistance * (referenceVoltage/readVoltage - 1);
}

float read_average_therm_resistance(float referenceResistance = Ro, float referenceVoltage = 3.3, int windowSize = 200) {
  float average = 0;
  for (int i=0; i <=windowSize; i++) {
    average += read_instant_therm_resistance(referenceResistance, referenceVoltage);
  }
  return average/windowSize;
}

float tempRolling[] = {50,50};
float read_therm_temperature(float calibrationTemp = To, float calibrationRes = Rseries, float thermistorConstant = B) {
  float inverseTemperature = (1/calibrationTemp) + (1/thermistorConstant)*log(read_average_therm_resistance()/calibrationRes);
  float measuredTemperature = (tempRolling[0]+tempRolling[1]+(1/inverseTemperature))/3;
  tempRolling[0] = tempRolling[1];
  tempRolling[1] = 1/inverseTemperature;
  return measuredTemperature;
}

void set_temperature(int selectedTemp) {
  if (selectedTemp > 95) {selectedTemp = 95;};
  g_setTemperature = selectedTemp;
  sevseg.setChars("SEt");
  unsigned long startTime = millis();
  while (millis()>startTime+4000) {
    sevseg.refreshDisplay();
  }
  sevseg.setBrightness(50);
}

void enter_temp_select() {
  int hoverTemperature = g_setTemperature;
  sevseg.setBrightness(100);
  sevseg.setNumber(hoverTemperature, 0);
  unsigned long lastInteraction = millis();
  unsigned int timeout = 10*1000;
  while (millis()<lastInteraction+timeout) {
      sevseg.refreshDisplay();
        switch (read_rotary_encoder()) {
          case -1:
            hoverTemperature += -1;
            if (hoverTemperature <= MIN_TEMP) {
              hoverTemperature = MIN_TEMP;
            }
            lastInteraction = millis();
            sevseg.setNumber(hoverTemperature, 0);
            break;
          case +1:
            hoverTemperature += +1;
            if (hoverTemperature >= MAX_TEMP) {
              hoverTemperature = MAX_TEMP;
            }
            lastInteraction = millis();
            sevseg.setNumber(hoverTemperature, 0);
            break;
          }
    if (digitalRead(enterButton)==LOW) {
      set_temperature(hoverTemperature);
      digitalWrite(heaterPin, HIGH);
      break;
    }
  }
  sevseg.setBrightness(50);
}

void loop() {
  if (g_timer % 500 == 0) {
    //Serial.println((String)"Read resistance:\t"+read_average_therm_resistance());
    float celsiusCurrentTemp = read_therm_temperature()-273.15;
    //Serial.println((String)"Read temperature:\t"+(celsiusCurrentTemp));
    sevseg.setNumberF(celsiusCurrentTemp,1);
    if (celsiusCurrentTemp >= g_setTemperature) {
      digitalWrite(heaterPin, LOW);
    }
    else if (celsiusCurrentTemp <= g_setTemperature-STRIDE) {
      digitalWrite(heaterPin, HIGH);
    }
  }
  if (read_rotary_encoder() != 0) {
    enter_temp_select();
  }
  sevseg.refreshDisplay();
  g_timer = millis();
}
