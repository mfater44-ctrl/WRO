#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// WIFI ACCESS POINT SETTINGS
// =====================================================

const char* AP_SSID = "Heritage Guard";
const char* AP_PASSWORD = "00000000";

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

// =====================================================
// PINS
// =====================================================

// Use an ADC1 pin because Wi-Fi prevents reliable ADC2 readings
#define MOISTURE_PIN 34

#define SDA_PIN 21
#define SCL_PIN 22

// =====================================================
// LCD SETTINGS
// =====================================================

// Change 0x27 to 0x3F if necessary
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// BME280
// =====================================================

#define SEALEVELPRESSURE_HPA 1013.25

Adafruit_BME280 bme;
bool bmeFound = false;

// =====================================================
// MOISTURE CALIBRATION
// =====================================================

// Replace these values after calibrating the sensor
int DRY_VALUE = 5000;
int WET_VALUE = 1700;

// =====================================================
// ALERT LEVELS
// =====================================================

const float WALL_WARNING = 40.0;
const float WALL_DANGER = 60.0;

const float AIR_SAFE_MIN = 40.0;
const float AIR_SAFE_MAX = 60.0;
const float AIR_WARNING = 70.0;
const float AIR_DANGER = 75.0;

// =====================================================
// OBJECTS AND VARIABLES
// =====================================================

WebServer server(80);

int moistureRaw = 0;

float wallMoisture = 0.0;
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;
float altitude = 0.0;

unsigned long previousRead = 0;
unsigned long previousLCD = 0;

const unsigned long READ_INTERVAL = 2000;
const unsigned long LCD_INTERVAL = 2500;

byte lcdPage = 0;

// =====================================================
// STATUS FUNCTIONS
// =====================================================

String getWallStatus(float value)
{
  if (value < WALL_WARNING)
  {
    return "SAFE";
  }
  else if (value < WALL_DANGER)
  {
    return "WARNING";
  }
  else
  {
    return "DANGER";
  }
}

String getAirStatus(float value)
{
  if (value >= AIR_SAFE_MIN && value <= AIR_SAFE_MAX)
  {
    return "SAFE";
  }

  if (value > AIR_SAFE_MAX && value < AIR_WARNING)
  {
    return "WARNING";
  }

  if (value >= AIR_WARNING)
  {
    return "DANGER";
  }

  return "DRY AIR";
}

// =====================================================
// READ MOISTURE SENSOR
// =====================================================

void readMoisture()
{
  long total = 0;

  // Average 10 readings for stability
  for (int i = 0; i < 10; i++)
  {
    total += analogRead(MOISTURE_PIN);
    delay(10);
  }

  moistureRaw = total / 10;

  wallMoisture =
      ((float)(DRY_VALUE - moistureRaw) /
       (DRY_VALUE - WET_VALUE)) * 100.0;

  wallMoisture = constrain(wallMoisture, 0.0, 100.0);
}

// =====================================================
// READ BME280
// =====================================================

void readBME()
{
  if (!bmeFound)
  {
    return;
  }

  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
}

// =====================================================
// LCD DISPLAY
// =====================================================

void updateLCD()
{
  lcd.clear();

  if (lcdPage == 0)
  {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature, 1);
    lcd.write(223);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Air Hum: ");
    lcd.print(humidity, 1);
    lcd.print("%");
  }
  else if (lcdPage == 1)
  {
    lcd.setCursor(0, 0);
    lcd.print("Wall: ");
    lcd.print(wallMoisture, 1);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print(getWallStatus(wallMoisture));
    lcd.print(" R:");
    lcd.print(moistureRaw);
  }
  else if (lcdPage == 2)
  {
    lcd.setCursor(0, 0);
    lcd.print("Pres:");
    lcd.print(pressure, 0);
    lcd.print(" hPa");

    lcd.setCursor(0, 1);
    lcd.print("Alt: ");
    lcd.print(altitude, 1);
    lcd.print(" m");
  }
  else
  {
    lcd.setCursor(0, 0);
    lcd.print("WiFi:");
    lcd.print(AP_SSID);

    lcd.setCursor(0, 1);
    lcd.print("192.168.4.1");
  }

  lcdPage++;

  if (lcdPage > 3)
  {
    lcdPage = 0;
  }
}

// =====================================================
// SERIAL MONITOR
// =====================================================

void printReadings()
{
  Serial.println();
  Serial.println("======================================");
  Serial.println(" Heritage Wall Monitoring System");
  Serial.println("======================================");

  Serial.print("Moisture RAW: ");
  Serial.println(moistureRaw);

  Serial.print("Wall Moisture: ");
  Serial.print(wallMoisture, 1);
  Serial.println(" %");

  Serial.print("Wall Status: ");
  Serial.println(getWallStatus(wallMoisture));

  Serial.println("--------------------------------------");

  if (bmeFound)
  {
    Serial.print("Temperature: ");
    Serial.print(temperature, 1);
    Serial.println(" C");

    Serial.print("Air Humidity: ");
    Serial.print(humidity, 1);
    Serial.println(" %");

    Serial.print("Air Status: ");
    Serial.println(getAirStatus(humidity));

    Serial.print("Pressure: ");
    Serial.print(pressure, 1);
    Serial.println(" hPa");

    Serial.print("Approximate Altitude: ");
    Serial.print(altitude, 1);
    Serial.println(" m");
  }
  else
  {
    Serial.println("BME280 NOT FOUND!");
  }

  Serial.println("--------------------------------------");
  Serial.print("WiFi: ");
  Serial.println(AP_SSID);

  Serial.print("Dashboard: http://");
  Serial.println(WiFi.softAPIP());

  Serial.println("======================================");
}

// =====================================================
// WEB PAGE
// =====================================================

String createWebPage()
{
  String wallStatus = getWallStatus(wallMoisture);
  String airStatus = bmeFound ? getAirStatus(humidity) : "SENSOR ERROR";

  String wallColor;
  String airColor;

  if (wallStatus == "SAFE")
  {
    wallColor = "#22c55e";
  }
  else if (wallStatus == "WARNING")
  {
    wallColor = "#f59e0b";
  }
  else
  {
    wallColor = "#ef4444";
  }

  if (airStatus == "SAFE")
  {
    airColor = "#22c55e";
  }
  else if (airStatus == "WARNING")
  {
    airColor = "#f59e0b";
  }
  else
  {
    airColor = "#ef4444";
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport"
        content="width=device-width, initial-scale=1.0">

  <meta http-equiv="refresh" content="5">

  <title>Heritage Guard</title>

  <style>
    * {
      box-sizing: border-box;
    }

    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #eef2f3, #dfe9f3);
      color: #1f2937;
      text-align: center;
      margin: 0;
      padding: 20px;
    }

    .container {
      max-width: 900px;
      margin: auto;
    }

    .header {
      background: #19352d;
      color: white;
      padding: 25px;
      border-radius: 18px;
      margin-bottom: 20px;
    }

    .header h1 {
      margin: 0 0 8px 0;
    }

    .header p {
      margin: 0;
      color: #d1fae5;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
      gap: 18px;
    }

    .card {
      background: white;
      border-radius: 16px;
      padding: 22px;
      box-shadow: 0 5px 18px rgba(0, 0, 0, 0.12);
    }

    .card h2 {
      margin-top: 0;
      font-size: 20px;
      color: #374151;
    }

    .value {
      font-size: 38px;
      font-weight: bold;
      margin: 15px 0;
    }

    .status {
      color: white;
      padding: 10px;
      border-radius: 9px;
      font-size: 18px;
      font-weight: bold;
    }

    .raw {
      color: #6b7280;
      margin-top: 14px;
      font-size: 14px;
    }

    .footer {
      color: #6b7280;
      margin-top: 20px;
      font-size: 14px;
    }
  </style>
</head>

<body>
  <div class="container">

    <div class="header">
      <h1>Heritage Wall Monitoring System</h1>
      <p>Environmental and wall moisture dashboard</p>
    </div>

    <div class="grid">

      <div class="card">
        <h2>Wall Moisture</h2>

        <div class="value">
)rawliteral";

  html += String(wallMoisture, 1);

  html += R"rawliteral(
          %
        </div>

        <div class="status" style="background:
)rawliteral";

  html += wallColor;

  html += R"rawliteral(
        ">
)rawliteral";

  html += wallStatus;

  html += R"rawliteral(
        </div>

        <div class="raw">
          Sensor raw value:
)rawliteral";

  html += String(moistureRaw);

  html += R"rawliteral(
        </div>
      </div>

      <div class="card">
        <h2>Air Humidity</h2>

        <div class="value">
)rawliteral";

  if (bmeFound)
  {
    html += String(humidity, 1);
  }
  else
  {
    html += "--";
  }

  html += R"rawliteral(
          %
        </div>

        <div class="status" style="background:
)rawliteral";

  html += airColor;

  html += R"rawliteral(
        ">
)rawliteral";

  html += airStatus;

  html += R"rawliteral(
        </div>
      </div>

      <div class="card">
        <h2>Temperature</h2>

        <div class="value">
)rawliteral";

  if (bmeFound)
  {
    html += String(temperature, 1);
  }
  else
  {
    html += "--";
  }

  html += R"rawliteral(
          &deg;C
        </div>
      </div>

      <div class="card">
        <h2>Atmospheric Pressure</h2>

        <div class="value">
)rawliteral";

  if (bmeFound)
  {
    html += String(pressure, 1);
  }
  else
  {
    html += "--";
  }

  html += R"rawliteral(
          hPa
        </div>
      </div>

      <div class="card">
        <h2>Approximate Altitude</h2>

        <div class="value">
)rawliteral";

  if (bmeFound)
  {
    html += String(altitude, 1);
  }
  else
  {
    html += "--";
  }

  html += R"rawliteral(
          m
        </div>
      </div>

    </div>

    <div class="footer">
      Connect to Heritage Guard and open 192.168.4.1<br>
      Readings refresh every five seconds.
    </div>

  </div>
</body>
</html>
)rawliteral";

  return html;
}

// =====================================================
// WEB SERVER
// =====================================================

void handleRoot()
{
  server.send(200, "text/html", createWebPage());
}

void handleNotFound()
{
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting Heritage Wall Monitoring System...");

  // Configure moisture sensor
  analogReadResolution(12);
  analogSetPinAttenuation(MOISTURE_PIN, ADC_11db);
  pinMode(MOISTURE_PIN, INPUT);

  // Start I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Start LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Heritage Guard");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // Detect BME280
  if (bme.begin(0x76))
  {
    bmeFound = true;
    Serial.println("BME280 detected at address 0x76");
  }
  else if (bme.begin(0x77))
  {
    bmeFound = true;
    Serial.println("BME280 detected at address 0x77");
  }
  else
  {
    bmeFound = false;
    Serial.println("ERROR: BME280 not detected!");
  }

  // Take the first readings
  readMoisture();
  readBME();

  // Start Wi-Fi access point
  Serial.println();
  Serial.print("Creating WiFi access point: ");
  Serial.println(AP_SSID);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  if (WiFi.softAP(AP_SSID, AP_PASSWORD))
  {
    Serial.println("WiFi access point started.");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", handleRoot);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.println("Web server started.");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Ready");
    lcd.setCursor(0, 1);
    lcd.print("192.168.4.1");
  }
  else
  {
    Serial.println("Failed to start WiFi access point.");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Error");
    lcd.setCursor(0, 1);
    lcd.print("Check ESP32");
  }

  delay(3000);

  previousRead = millis();
  previousLCD = millis();

  updateLCD();

  Serial.println("System Ready!");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  // Keep the website responsive
  server.handleClient();

  unsigned long currentTime = millis();

  // Update sensor readings
  if (currentTime - previousRead >= READ_INTERVAL)
  {
    previousRead = currentTime;

    readMoisture();
    readBME();
    printReadings();
  }

  // Change LCD screen independently
  if (currentTime - previousLCD >= LCD_INTERVAL)
  {
    previousLCD = currentTime;
    updateLCD();
  }
}
