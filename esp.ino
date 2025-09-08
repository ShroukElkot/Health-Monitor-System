#include <Wire.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"
#include <SoftwareSerial.h>

// Firebase credentials
#define API_KEY "AIzaSyCmBB0qzFZoQj-N77UHjn_AUra2HXnbmak"
#define DATABASE_URL "https://lab6-a9984-default-rtdb.firebaseio.com/"

// Wi-Fi credentials
#define ssid "WE26E651"
#define pass "NadaHelal147"


// RX, TX = D5 (GPIO14), D6 (GPIO12)
SoftwareSerial mySerial(14, 12);  // mySerial RX, TX

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupok = false;
//String heartRateStr = "";String incomingStr = "";
unsigned long sendDataPrevMillis = 0;
int heartRate = 0;
bool ledState = false;

const int ledPin = LED_BUILTIN;  // Built-in LED pin (usually GPIO2 on ESP8266)



void setup() {
  Serial.begin(9600);    // Initialize UART at 9600 baud rate
  Serial.println("ESP8266 Initialized and Waiting for Data...");
  
  //pinMode(ledPin, OUTPUT);             // Set the LED pin as an output
  Serial.println("ESP Ready to Receive from Tiva C...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
  Serial.println(WiFi.localIP());

    // Firebase setup
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase SignUp OK");
    signupok = true;
  } else {
    Serial.println("Firebase SignUp Failed");
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

}

void loop() {
  if (Firebase.ready() && signupok) {
    // Read from Tiva C
    if (Serial.available()) {
      String message = Serial.readStringUntil('\n');
      message.trim();
      
      Serial.println("Received: " + message);

      // Parse heart rate data (expecting format "HR:1234")
      if (message.startsWith("HR:")) {
        heartRate = message.substring(3).toInt();
        Serial.print("Parsed HR: ");
        Serial.println(heartRate);
        
        // Toggle LED for visual feedback
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
        
        // Send to Firebase
        if (Firebase.RTDB.setInt(&fbdo, "health/pulse", heartRate)) {
          Serial.println("Firebase: Data sent successfully");
        } else {
          Serial.println("Firebase: " + fbdo.errorReason());
        }
      }
    }
  }
  
  // Small delay to prevent watchdog reset
  delay(10);
}
