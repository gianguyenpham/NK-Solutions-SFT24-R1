#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""

#define BLYNK_PRINT Serial

#include "DFRobot_AHT20.h"
#include <DFRobot_ENS160.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Protocentral_MAX30205.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "MAX30100_PulseOximeter.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_A_PIN 27 //MAX30102
#define BUTTON_B_PIN 33 // MAX30205
#define BUTTON_C_PIN 15 // Environment

#define REPORTING_PERIOD_MS     1000


#define I2C_COMMUNICATION  // I2C communication. Comment out this line of code if you want to use SPI communication.
DFRobot_ENS160_I2C ENS160(&Wire, /*I2CAddr*/ 0x53);
DFRobot_AHT20 aht20;
MAX30205 tempSensor;
BlynkTimer timer;
PulseOximeter pox;

uint32_t tsLastReport = 0;

bool initialDisplayDoneEnvironment = false;
bool initialDisplayDoneMAX30205 = false;
bool initialDisplayDoneMAX30102 = false;

//Home wifi:
char ssid[] = "Beconec";
char pass[] = "Dmarcom3!@";

//Tab S4 Hotspot:
//char ssid[] = "gnp";
//char pass[] = "12345679";

void onBeatDetected()
{
    Serial.println("Beat!");
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(BUTTON_C_PIN, INPUT_PULLUP);

  // Initialize SSD1306 OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  
  // Initialize MAX30102 sensor
  Serial.print("Initializing pulse oximeter..");

    // Initialize the PulseOximeter instance
    // Failures are generally due to an improper I2C wiring, missing power supply
    // or wrong target chip
    if (!pox.begin()) {
        Serial.println("FAILED");
        for(;;);
    } else {
        Serial.println("SUCCESS");
    }

    // The default current for the IR LED is 50mA and it could be changed
    //   by uncommenting the following line. Check MAX30100_Registers.h for all the
    //   available options.
    // pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);

    // Register a callback for the beat detection
    pox.setOnBeatDetectedCallback(onBeatDetected);


  // Initialize AHT20 sensor
  while (!Serial) {
    // Wait for USB serial port to connect. Needed for native USB port only
  }  
  uint8_t status;
  while ((status = aht20.begin()) != 0) {
    Serial.print("AHT20 sensor initialization failed. error status : ");
    Serial.println(status);
    delay(1000);
  }

  // Initialize ENS160 sensor
  while (NO_ERR != ENS160.begin()) {
    Serial.println("Communication with device failed, please check connection");
    delay(3000);
  }
  Serial.println("Begin ok!");

  // Set power mode
  ENS160.setPWRMode(ENS160_STANDARD_MODE);

  // Set initial temperature and humidity for calibration
  ENS160.setTempAndHum(/*temperature=*/25.0, /*humidity=*/50.0);

  // Initialize MAX30205 sensor
  Wire.begin();

  //scan for temperature in every 30 sec untill a sensor is found. Scan for both addresses 0x48 and 0x49
  while(!tempSensor.scanAvailableSensors()){
    Serial.println("Couldn't find the temperature sensor, please connect the sensor." );
    delay(30000);
  }

  tempSensor.begin();   // set continuos mode, active mode

  // Initialize Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Setup a function to be called every second
  timer.setInterval(1000L, sendSensorDataToBlynk);
}

void loop() {
  Blynk.run();
  timer.run(); // Initiates BlynkTimer
  checkButton();
}

void sendSensorDataToBlynk() {
  // This function is now empty since we are moving Blynk.virtualWrite calls to checkButton
}

void checkButton() {
  if (digitalRead(BUTTON_A_PIN) == LOW) {
    // Button is pressed, execute the main functionality
    if (!initialDisplayDoneMAX30102) {
      // Display initial message
      display.clearDisplay();
      display.setCursor(0, 10);
      display.setTextSize(2);
      display.println(F("MAX30100"));
      display.println(F("ON"));
      display.display();
      delay(2000);

      display.clearDisplay();
      display.setCursor(0, 20);
      display.setTextSize(1);
      display.println(F("SPO2 and Heart Rate"));
      display.println(F("measuring selected!"));
      display.display();
      delay(3000);

      // Display second message
      display.clearDisplay();
      display.setCursor(0, 20);
      display.setTextSize(1);
      display.println(F("Put your finger on"));
      display.println(F("the MAX30100 sensor!"));
      display.display();
      delay(3000);

      // Countdown
      for (int i = 3; i > 0; i--) {
        display.clearDisplay();
        display.setCursor(60, 20);
        display.setTextSize(2);
        display.print(i);
        display.display();
        delay(1000);
      }

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);

      initialDisplayDoneMAX30102 = true; // Set the flag to true after displaying the initial messages
    }

    // Display measurement data on OLED
    pox.update();

    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        display.clearDisplay();
        display.setCursor(0, 20);
        display.setTextSize(1);
        display.print(F("Heart Rate: "));
        display.print(pox.getHeartRate());
        display.print(F(" bpm"));
    
        display.setCursor(0, 30);
        display.print(F("SPO2: "));
        display.print(pox.getSpO2());
        display.print(F(" %"));

        display.display();

        // Send to Blynk
        Blynk.virtualWrite(V0, pox.getHeartRate());
        Blynk.virtualWrite(V1, pox.getSpO2());

        // Also print results to the serial monitor
        Serial.print("Heart rate:");
        Serial.print(pox.getHeartRate());
        Serial.print("bpm / SpO2:");
        Serial.print(pox.getSpO2());
        Serial.println("%");

        tsLastReport = millis();
    }

    

    delay(1000); // Update data every second
  } 
  else if (digitalRead(BUTTON_B_PIN) == LOW) {
    // Button is pressed, execute the main functionality
    if (!initialDisplayDoneMAX30205) {
      // Display initial message
      display.clearDisplay();
      display.setCursor(0, 10);
      display.setTextSize(2);
      display.println(F("MAX30205"));
      display.println(F("ON"));
      display.display();
      delay(2000);

      display.clearDisplay();
      display.setCursor(0, 20);
      display.setTextSize(1);
      display.println(F("Skin temperature"));
      display.println(F("measuring selected!"));
      display.display();
      delay(3000);

      // Display second message
      display.clearDisplay();
      display.setCursor(0, 20);
      display.setTextSize(1);
      display.println(F("Put your finger on"));
      display.println(F("the MAX30205 sensor!"));
      display.display();
      delay(3000);

      // Countdown
      for (int i = 3; i > 0; i--) {
        display.clearDisplay();
        display.setCursor(60, 20);
        display.setTextSize(2);
        display.print(i);
        display.display();
        delay(1000);
      }

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);

      initialDisplayDoneMAX30205 = true; // Set the flag to true after displaying the initial messages
    }

    // Display measurement data on OLED
    float temp = tempSensor.getTemperature(); // read temperature for every 100ms

    display.clearDisplay();
    display.setCursor(0, 20);
    display.println(F("Skin temperature: "));
    display.print(temp, 1);
    display.print(F(" 'C"));
    display.display();

    // Send to Blynk
    Blynk.virtualWrite(V2, temp);

    // Also print results to the serial monitor
    Serial.print(temp ,2);
	  Serial.println("'c" );

    delay(1000); // Update data every second

  } 
  else if (digitalRead(BUTTON_C_PIN) == LOW) {
    // Button is pressed, execute the main functionality
    if (!initialDisplayDoneEnvironment) {
      // Display initial message
      display.clearDisplay();
      display.setCursor(0, 10);
      display.setTextSize(2);
      display.println(F("AHT20 AND"));
      display.println(F("ENS160 ON"));
      display.display();
      delay(2000);

      display.clearDisplay();
      display.setCursor(0, 20);
      display.setTextSize(1);
      display.println(F("Environment"));
      display.println(F("monitoring selected!"));
      display.display();
      delay(3000);

      // Countdown
      for (int i = 3; i > 0; i--) {
        display.clearDisplay();
        display.setCursor(60, 20);
        display.setTextSize(2);
        display.print(i);
        display.display();
        delay(1000);
      }

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.display(); // Ensure initial display buffer is updated

      initialDisplayDoneEnvironment = true; // Set the flag to true after displaying the initial messages
    }

    // Display measurement data on OLED
    if (aht20.startMeasurementReady(/* crcEn = */true)) {
      display.clearDisplay();
      display.setCursor(0, 10);
      display.print("Temperature: ");
      display.print(aht20.getTemperature_C());
      display.println(" C");
      display.print("Humidity: ");
      display.print(aht20.getHumidity_RH());
      display.println(" %RH");

      uint8_t Status = ENS160.getENS160Status();
      uint8_t AQI = ENS160.getAQI();
      uint16_t TVOC = ENS160.getTVOC();
      uint16_t ECO2 = ENS160.getECO2();

      display.print("AQI: ");
      display.println(AQI);
      display.print("TVOC: ");
      display.print(TVOC);
      display.println(" ppb");
      display.print("eCO2: ");
      display.print(ECO2);
      display.println(" ppm");

      display.display(); // Ensure the data is displayed on OLED

      // Send to Blynk
      Blynk.virtualWrite(V3, aht20.getTemperature_C());
      Blynk.virtualWrite(V4, aht20.getHumidity_RH());
      Blynk.virtualWrite(V5, AQI);
      Blynk.virtualWrite(V6, TVOC);
      Blynk.virtualWrite(V7, ECO2);

      // Also print results to the serial monitor
      Serial.print("Temperature: ");
      Serial.print(aht20.getTemperature_C());
      Serial.println(" C");
      Serial.print("Humidity: ");
      Serial.print(aht20.getHumidity_RH());
      Serial.println(" %RH");
      Serial.print("Sensor operating status : ");
      Serial.println(Status);
      Serial.print("AQI: ");
      Serial.println(AQI);
      Serial.print("TVOC: ");
      Serial.print(TVOC);
      Serial.println(" ppb");
      Serial.print("eCO2: ");
      Serial.print(ECO2);
      Serial.println(" ppm");
    }
    delay(1000); // Update data every second   
  } 
  else {
    display.clearDisplay();
    display.setCursor(0, 20);
    display.setTextSize(2);
    display.println(F("Button OFF"));
    display.display();

    initialDisplayDoneEnvironment = false;
    initialDisplayDoneMAX30205 = false;
    initialDisplayDoneMAX30102 = false;
  }
}
