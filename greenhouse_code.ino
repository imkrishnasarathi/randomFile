#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

#define WIFI_SSID     "SR_COM_LAB_WIFI"
#define WIFI_PASSWORD "bms@admin25" 
#define SERVO_PIN D4
#define CITY "bardhaman"
#define COUNTRY "india"

Servo ventServo;
ESP8266WebServer server(80);

bool autoMode = true;
float openTempThreshold = 30.0;
float closeTempThreshold = 28.0;
bool ventIsOpen = false;

float intTemp = 25.0, intHum = 60.0;
float extTemp = 0, extRain = 0;

unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_INTERVAL = 1800000UL; 

unsigned long lastInternalUpdate = 0;
const unsigned long INTERNAL_UPDATE_INTERVAL = 5000; 

// Forward declarations
void handleRoot();
void handleOpen();
void handleClose();
void handleToggleAuto();
void handleSetThresholds();
void handleGetData();
void fetchWeather();
void moveVent(bool open);
void checkAutoControl();

void setup() {
  Serial.begin(9600);
  ventServo.attach(SERVO_PIN);
  ventServo.write(0); // start closed

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // Web server routes
  server.on("/", handleRoot);
  server.on("/open", handleOpen);
  server.on("/close", handleClose);
  server.on("/toggleauto", handleToggleAuto);
  server.on("/setthresholds", handleSetThresholds);
  server.on("/getdata", handleGetData);

  server.begin();

  // Initial weather fetch
  fetchWeather();
  lastWeatherUpdate = millis();
}

void loop() {
  server.handleClient();

  // Update weather every WEATHER_INTERVAL
  if (millis() - lastWeatherUpdate > WEATHER_INTERVAL) {
    fetchWeather();
    lastWeatherUpdate = millis();
  }

  // Update internal temp/humidity every 5 seconds with small random changes
  if (millis() - lastInternalUpdate > INTERNAL_UPDATE_INTERVAL) {
    float tempChange = random(-5, 6) / 100.0; // ±0.05 max
    float humChange = random(-5, 6) / 100.0;  // ±0.05 max

    intTemp += tempChange;
    intHum += humChange;

    // Clamp internal temperature between 15 and 30
    if (intTemp < 15.0) intTemp = 15.0;
    if (intTemp > 30.0) intTemp = 30.0;

    // Clamp internal humidity between 0 and 100%
    if (intHum < 0.0) intHum = 0.0;
    if (intHum > 100.0) intHum = 100.0;

    lastInternalUpdate = millis();
  }
}

// --- Web handlers ---

void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Greenhouse Vent Control</title>
    <style>
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        background: #f5f7fa;
        color: #333;
        margin: 0;
        padding: 20px;
      }
      h1 {
        text-align: center;
        color: #2c3e50;
      }
      .container {
        max-width: 500px;
        margin: 20px auto;
        background: #fff;
        box-shadow: 0 4px 8px rgba(0,0,0,0.1);
        padding: 20px 30px;
        border-radius: 8px;
      }
      .status-row {
        display: flex;
        justify-content: space-between;
        margin: 10px 0;
        font-weight: 600;
      }
      .buttons {
        text-align: center;
        margin: 20px 0;
      }
      button {
        background-color: #3498db;
        color: white;
        border: none;
        padding: 12px 25px;
        margin: 5px 10px;
        border-radius: 5px;
        font-size: 1rem;
        cursor: pointer;
        transition: background-color 0.3s ease;
      }
      button:hover {
        background-color: #2980b9;
      }
      .toggle-auto {
        background-color: #27ae60;
      }
      .toggle-auto:hover {
        background-color: #1e8449;
      }
      label {
        display: block;
        margin: 15px 0 5px;
        font-weight: 600;
      }
      input[type=number] {
        width: 100%;
        padding: 8px 10px;
        font-size: 1rem;
        border: 1.5px solid #ccc;
        border-radius: 5px;
        box-sizing: border-box;
      }
      .update-btn {
        width: 100%;
        background-color: #e67e22;
        margin-top: 20px;
      }
      .update-btn:hover {
        background-color: #d35400;
      }
      #status {
        margin-top: 20px;
        text-align: center;
        font-weight: 700;
        color: #2c3e50;
        min-height: 24px;
      }
    </style>
    <script>
      function sendCommand(cmd) {
        fetch(cmd)
          .then(response => response.text())
          .then(data => {
            document.getElementById('status').innerText = data;
            updatePage();
          });
      }

      function toggleAutoMode() {
        fetch('/toggleauto')
          .then(response => response.text())
          .then(data => {
            document.getElementById('status').innerText = data;
            updatePage();
          });
      }

      function updateThresholds() {
        let openTemp = document.getElementById('openTemp').value;
        let closeTemp = document.getElementById('closeTemp').value;
        fetch(`/setthresholds?open=` + openTemp + `&close=` + closeTemp)
          .then(response => response.text())
          .then(data => {
            document.getElementById('status').innerText = data;
            updatePage();
          });
      }

      function updatePage() {
        fetch('/getdata')
          .then(r => r.json())
          .then(d => {
            document.getElementById('intTemp').innerText = d.intTemp + ' °C';
            document.getElementById('intHum').innerText = d.intHum + ' %';
            document.getElementById('extTemp').innerText = d.extTemp + ' °C';
            document.getElementById('extRain').innerText = d.extRain + ' mm';
            document.getElementById('autoMode').innerText = d.autoMode ? 'ON' : 'OFF';
            document.getElementById('ventState').innerText = d.ventIsOpen ? 'Open' : 'Closed';
            document.getElementById('openTemp').value = d.openTempThreshold;
            document.getElementById('closeTemp').value = d.closeTempThreshold;
          });
      }

      setInterval(updatePage, 10000);
      window.onload = updatePage;
    </script>
  </head>
  <body>
    <div class="container">
      <h1>Greenhouse Vent Control</h1>
      
      <div class="status-row"><div>Internal Temp:</div><div id="intTemp">--</div></div>
      <div class="status-row"><div>Internal Humidity:</div><div id="intHum">--</div></div>
      <div class="status-row"><div>External Temp:</div><div id="extTemp">--</div></div>
      <div class="status-row"><div>Rain Today:</div><div id="extRain">--</div></div>
      <div class="status-row"><div>Auto Mode:</div><div id="autoMode">--</div></div>
      <div class="status-row"><div>Vent State:</div><div id="ventState">--</div></div>

      <div class="buttons">
        <button onclick="sendCommand('/open')">Open Vent</button>
        <button onclick="sendCommand('/close')">Close Vent</button>
        <button class="toggle-auto" onclick="toggleAutoMode()">Toggle Auto Mode</button>
      </div>

      <h3>Configure Auto Mode Thresholds (°C)</h3>
      <label for="openTemp">Open Vent if Temp ≥=</label>
      <input type="number" id="openTemp" step="0.1" min="15" max="50" />
      
      <label for="closeTemp">Close Vent if Temp ≤=</label>
      <input type="number" id="closeTemp" step="0.1" min="15" max="50" />
      
      <button class="update-btn" onclick="updateThresholds()">Update Thresholds</button>

      <p id="status">Status: Ready</p>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}


void handleOpen() {
  moveVent(true);
  ventIsOpen = true;
  server.send(200, "text/plain", "Vent opened");
}

void handleClose() {
  moveVent(false);
  ventIsOpen = false;
  server.send(200, "text/plain", "Vent closed");
}

void handleToggleAuto() {
  autoMode = !autoMode;
  server.send(200, "text/plain", String("Auto mode ") + (autoMode ? "enabled" : "disabled"));
}

void handleSetThresholds() {
  if (!server.hasArg("open") || !server.hasArg("close")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }

  float openVal = server.arg("open").toFloat();
  float closeVal = server.arg("close").toFloat();

  if (openVal >= closeVal) {
    openTempThreshold = openVal;
    closeTempThreshold = closeVal;
    checkAutoControl();
    server.send(200, "text/plain", "Thresholds updated");
  } else {
    server.send(400, "text/plain", "Invalid thresholds: open must be >= close");
  }
}

void handleGetData() {
  String json = "{";
  json += "\"intTemp\":" + String(intTemp, 1) + ",";
  json += "\"intHum\":" + String(intHum, 1) + ",";
  json += "\"extTemp\":" + String(extTemp, 1) + ",";
  json += "\"extRain\":" + String(extRain, 1) + ",";
  json += "\"autoMode\":" + String(autoMode ? "true" : "false") + ",";
  json += "\"ventIsOpen\":" + String(ventIsOpen ? "true" : "false") + ",";
  json += "\"openTempThreshold\":" + String(openTempThreshold, 1) + ",";
  json += "\"closeTempThreshold\":" + String(closeTempThreshold, 1);
  json += "}";

  server.send(200, "application/json", json);
}

// --- Weather fetch ---

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping weather fetch.");
    return;
  }

  WiFiClient client;
  HTTPClient http;

  // Step 1: Get latitude and longitude
  String geoUrl = "http://geocoding-api.open-meteo.com/v1/search?name=" + String(CITY) + "&country=" + String(COUNTRY);
  http.begin(client, geoUrl);
  int geoCode = http.GET();

  if (geoCode == 200) {
    String geoPayload = http.getString();
    http.end();

    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, geoPayload);
    if (err) {
      Serial.print("Geo JSON parse error: ");
      Serial.println(err.c_str());
      return;
    }

    float lat = doc["results"][0]["latitude"];
    float lon = doc["results"][0]["longitude"];

    // Step 2: Request current weather + hourly precipitation
    String weatherUrl = "http://api.open-meteo.com/v1/forecast?latitude=" + String(lat) +
                        "&longitude=" + String(lon) +
                        "&current_weather=true&hourly=precipitation";
    http.begin(client, weatherUrl);
    int weatherCode = http.GET();

    if (weatherCode == 200) {
      String payload = http.getString();
      http.end();

      Serial.println("Weather payload:");
      Serial.println(payload);

      StaticJsonDocument<20 * 1024> wdoc;
      DeserializationError werr = deserializeJson(wdoc, payload);
      if (werr) {
        Serial.print("Weather JSON parse error: ");
        Serial.println(werr.c_str());
        return;
      }

      extTemp = wdoc["current_weather"]["temperature"].as<float>();

      // Calculate today's total rain
      String currentTime = wdoc["current_weather"]["time"].as<String>();
      String todayDate = currentTime.substring(0, 10);

      JsonArray times = wdoc["hourly"]["time"];
      JsonArray precip = wdoc["hourly"]["precipitation"];

      float totalRain = 0.0;
      for (size_t i = 0; i < times.size(); i++) {
        String t = times[i].as<const char*>();
        if (t.startsWith(todayDate)) {
          totalRain += precip[i].as<float>();
        }
      }
      extRain = totalRain;

      Serial.printf("Weather: %.2f°C, Rain Today: %.2f mm\n", extTemp, extRain);

      // Run auto control after updating weather data
      checkAutoControl();
    } else {
      Serial.printf("Weather HTTP error: %d\n", weatherCode);
      http.end();
    }
  } else {
    Serial.printf("Geo HTTP error: %d\n", geoCode);
    http.end();
  }
}

// --- Servo control ---

void moveVent(bool open) {
  ventServo.write(open ? 90 : 0);
}

// --- Auto control logic ---

void checkAutoControl() {
  if (!autoMode) return;

  if (extTemp >= openTempThreshold) {
    if (!ventIsOpen) {
      moveVent(true);
      ventIsOpen = true;
      Serial.println("Auto: Vent opened");
    }
  } else if (extTemp <= closeTempThreshold) {
    if (ventIsOpen) {
      moveVent(false);
      ventIsOpen = false;
      Serial.println("Auto: Vent closed");
    }
  }
}
