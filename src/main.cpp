//v1.0.o SMART IRRIGATION E12 E
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h> // Include the ArduinoJson library
//DIDPLAY
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.in.pool.ntp.org", 5.5*3600,60000);


// User credentials storage
const int EEPROM_SIZE = 512;
#define USERNAME_ADDR 0
#define PASSWORD_ADDR 50
#define KEY_ADDR 100
#define STATUS_ADDR 150
#define LED_PIN 2

// MCB control pin

const int mcbPin = D6;//relay1
const int mcbPin2  = D7;//relay2
const int ledPin2 = D8;//mcb enable//override status 

const int ledPin = D4; // WIFI STATUS LED
#define MOISTURE_SENSOR_PIN D0

#define TRIGGER_PIN 0

const String DEFAULT_SECRET_KEY = "SYSCO"; // Define a default secret key

ESP8266WebServer server(80);
WiFiManager wifiManager;


// Define a structure for the MCB schedule
struct MCB_Schedule {
  int onHour;
  int onMinute;
  int offHour;
  int offMinute;
};

// Schedule for each day of the week (Sunday to Saturday)
MCB_Schedule weeklySchedule[7] = {
  {00, 00, 00, 00}, // Sunday
  {00, 00, 00, 00}, // Monday
  {00, 00, 00, 00}, // Tuesday
  {00, 00, 00, 00}, // Wednesday
  {00, 00, 00, 00}, // Thursday
  {00, 00, 00, 00}, // Friday
  {00, 00, 00, 00}  // Saturday
};

bool overrideSchedule = false;
bool overrideperiod = false;


bool mcbState = false;
bool state = false;
bool sensorFault = false; // Variable to track sensor fault
bool isVerified = false;
bool loggedIn = false;

// Timing for sensor fault alerts
const unsigned long faultCheckInterval = 3600000; // 1 hour
unsigned long lastFaultCheck = 0;

const int scheduleSize = 7; // Number of days in a week
const int scheduleDataSize = scheduleSize * 4 * 5; // Each day has 4 bytes (2 hours + 2 minutes) and there are 7 days
const int scheduleStartAddress = 0; // Starting address in EEPROM for schedule data


// Weekdays array
const char* weekDays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
bool scheduleChanged = false; // Flag to track if schedule has changed

// Web page content in English
//C,U,B,M,I,A(CURRENT, UPGRADE , BASIC , MODIFIED , INCHANCE , ADVANCE VERSION)
//VERSION MODEL SERIAL NUMBER CURRENT UPDATE ::: V_2.0.4c-M2 SN:ABLZx0002C

const char* mainPageEN = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>SMART IRRIGATION SYSTEM - v1.O</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background-color: black;
      color: white;
      margin: 0;
      padding: 0;
      position: relative;
    }
    h1 {
      color: #ccc;
    }
    form {
      display: inline-block;
      margin-top: 20px;
    }
    input {
      padding: 10px;
      margin: 5px;
      border: 1px solid #555;
      background-color: #333;
      color: white;
    }
    .status {
      margin-top: 20px;
    }
    .scrollable-days {
      display: flex;
      flex-direction: column;
      align-items: center;
      margin-top: 20px;
      padding: 10px;
    }
    .duration {
      position: fixed;
      bottom: 10px;
      right: 10px;
      color: white;
      font-size: 18px;
    }
    .datetime {
      position: fixed;
      top: 10px;
      right: 10px;
      color: white;
      font-size: 18px;
    }
    .footer {
      position: fixed;
      bottom: 0;
      left: 0;
      color: white;
      font-size: 14px;
      padding: 20px;
    }
    .config-button {
      position: fixed;
      bottom: 10px;
      right: 100px;
      background-color: #FF5722;
      color: white;
      border: none;
      padding: 10px 20px;
      cursor: pointer;
      font-size: 16px;
      border-radius: 5px;
    }
    .config-button:hover {
      background-color: #E64A19;
    }
      .marquee {
  position: fixed;
  bottom: 0;
  width: 100%;
  overflow: hidden;
  white-space: nowrap;
  background-color: black;
}

.marquee span {
  display: inline-block;
  padding-left: 100%;
  animation: scroll-left 30s linear infinite;
}

@keyframes scroll-left {
  0%   { transform: translateX(100%); }
  100% { transform: translateX(-100%); }
}

  </style>
</head>
<body>
  <h1>SMART IRRIGATION SYSTEM - v1.0</h1>
  <div class="datetime">
        <p id="date">Loading date...</p>
        <p id="time">Loading time...</p>
      </div>
    
      <div class="status">
        <p>[RUN] : SYSTEM Status : <span id="mcbStatus">Unknown</span></p>
        <p>[TIME] : Total Duration: <span id="duration">Unknown</span></p>
        <p>[WiFi] : SSID: <span id="ssid">Unknown</span></p>
      </div>

   <div class="scrollable-days">
        <label for="daySelector">Select a Day:</label>
        <select id="daySelector" onchange="updateTimes(this.value)">
          <option value="0">Sunday</option>
          <option value="1">Monday</option>
          <option value="2">Tuesday</option>
          <option value="3">Wednesday</option>
          <option value="4">Thursday</option>
          <option value="5">Friday</option>
          <option value="6">Saturday</option>
        </select>
    
        <div id="timeInputs">
          <h2>SHOW TIME</h2>
          <label for="onTime">On Time (HH:MM): </label>
          <span id="onTime">00:00</span><br>
          <label for="offTime">Off Time (HH:MM): </label>
          <span id="offTime">00:00</span><br>
        </div>
      </div>

  <form action="/manualOn" method="POST">
    <input type="submit" value="Manually ON">
  </form>
  <form action="/manualOff" method="POST">
    <input type="submit" value="Manually OFF">
  </form>
  <form action="/daily" method="POST">
    <input type="submit" value="Set Daily Timmer">
  </form>
  <form action="/weekly" method="POST">
    <input type="submit" value="Set Weekly Schedule">
  </form>
  <form action="/logout" method="POST">
    <input type="submit" value="Logout">
  </form>
  <form action="/emergency" method="POST">
    <input type="submit" value="Shut-Down!" style="color: red; font-weight: bold;">
  </form>





  <script>
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('mcbStatus').textContent = data.mcbStatus ? 'NORMAL' : 'TRIGGER';
        })
        .catch(error => console.error('Error fetching status:', error));
    }

    function updateDuration() {
      fetch('/duration')
        .then(response => response.json())
        .then(data => {
          document.getElementById('duration').textContent = data.duration;
        })
        .catch(error => console.error('Error fetching duration:', error));
    }

    function updateDateTime() {
      fetch('/dateTime')
        .then(response => response.json())
        .then(data => {
          document.getElementById('date').textContent = data.date;
          document.getElementById('time').textContent = data.time;
        })
        .catch(error => console.error('Error fetching date/time:', error));
    }

    function updateWifiInfo() {
      fetch('/wifiInfo')
        .then(response => response.json())
        .then(data => {
          document.getElementById('ssid').textContent = data.ssid;
        })
        .catch(error => console.error('Error fetching WiFi info:', error));
    }
   
    setInterval(updateStatus, 1000);
    setInterval(updateDuration, 60000);
    setInterval(updateDateTime, 1000);
    setInterval(updateWifiInfo, 10000);

    updateStatus();
    updateDuration();
    updateDateTime();
    updateWifiInfo();


  </script>

  

  <!-- End of Scrollable Days Section -->



  
 

  <script>
        function updateTimes(dayIndex) {
          const schedule = [
            { on: '06:00', off: '07:00' },
            { on: '07:00', off: '08:00' },
            { on: '08:00', off: '09:00' },
            { on: '09:00', off: '10:00' },
            { on: '10:00', off: '11:00' },
            { on: '11:00', off: '12:00' },
            { on: '12:00', off: '13:00' }
          ];
    
          const selectedSchedule = schedule[dayIndex];
          document.getElementById('onTime').textContent = selectedSchedule.on;
          document.getElementById('offTime').textContent = selectedSchedule.off;
        }
      </script>


  <div class="marquee">
  <span>⚡ SYSCO INDUSTRY Embedded & IoT & Automation Products Developments [R&D] V_1.0.0-M1 (Esp8266) SN:SYSCOZ6 ISO-2025 ⚡</span>
</div>

</body>
</html>
)rawliteral";







// Web page content for login

const char* loginPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Automation Login Esp8266</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background-color: black;
      color: white;
      margin: 0;
      padding: 0;
    }
    h1 {
      color: #eee;
      margin-top: 50px;
    }
    form {
      display: inline-block;
      margin: 20px auto;
      padding: 20px;
      background-color: #333;
      border-radius: 8px;
      box-shadow: 0 0 10px rgba(0,0,0,0.5);
    }
    label {
      display: block;
      margin: 10px 0 5px;
      color: #ccc;
    }
    .input-container {
      position: relative;
      margin-bottom: 15px;
    }
    
    input[type="text"],
    input[type="password"] {
      padding: 10px;
      margin: 5px 0 15px;
      width: 100%;
      max-width: 300px;
      border: 1px solid #555;
      border-radius: 4px;
      background-color: #222;
      color: white;
    }
    .eye-icon {
      position: absolute;
      right: 10px;
      top: 50%;
      transform: translateY(-50%);
      cursor: pointer;
      color: #ccc;
    }
    input[type="submit"] {
      padding: 10px 20px;
      border: none;
      border-radius: 4px;
      background-color: #007BFF;
      color: white;
      font-size: 16px;
      cursor: pointer;
    }
    input[type="submit"]:hover {
      background-color: #0056b3;
    }
    .footer {
      position: fixed;
      bottom: 0;
      left: 0;
      color: #ccc;
      font-size: 12px;
      padding: 10px;
      background-color: #222;
      box-shadow: 0 -1px 5px rgba(0,0,0,0.5);
      width: 100%;
    }
  </style>
  <script>
    function toggleVisibility(id) {
      const input = document.getElementById(id);
      const eyeIcon = document.querySelector('#' + id + ' + .eye-icon');
      if (input.type === 'password') {
        input.type = 'text';
        eyeIcon.classList.replace('fa-eye', 'fa-eye-slash');
      } else {
        input.type = 'password';
        eyeIcon.classList.replace('fa-eye-slash', 'fa-eye');
      }
    }
  </script>
</head>
<body>

  <h1>SMART IRRIGATION SYSTEM</h1>

 <form action="/login" method="POST">
    <label for="username">Username:</label>
    <input type="text" id="username" name="username" required>
    
    <label for="password">Password:</label>
    <div class="input-container">
      <input type="password" id="password" name="password" required>
      <i class="eye-icon fas fa-eye" onclick="toggleVisibility('password')"></i>
    </div>
    
    <label for="key">Secret Key:</label>
    <div class="input-container">
      <input type="password" id="key" name="key" required>
      <i class="eye-icon fas fa-eye" onclick="toggleVisibility('key')"></i>
    </div>
    
    <input type="submit" value="Login">
  </form>

   <form action="/register" method="POST">
    <h2>Register</h2>
    <label for="newUsername">Username:</label>
    <input type="text" id="newUsername" name="newUsername" required>
    
    <label for="newPassword">Password:</label>
    <input type="password" id="newPassword" name="newPassword" required>
    
    <input type="submit" value="Register">
  </form>

  <div class="footer">
    📞 [Support] Contact: +91 xxxx xxxx xx  |  Email: @gmail.com |  Location: Ground Floor, - 816107, JHARKHAND.
  </div>

</body>
</html>
)rawliteral";



void handleWifiInfo() {
    String ssid = WiFi.SSID();
    
    String json = "{";
    json += "\"ssid\":\"" + ssid + "\"";
    json += "}";

    server.send(200, "application/json", json);
}
// Function to clear EEPROM data
void clearEEPROM(int startAddress, int length) {
    for (int i = startAddress; i < startAddress + length; i++) {
        EEPROM.write(i, 0xFF); // Write 0xFF to clear the data
    }
    EEPROM.commit(); // Commit changes to EEPROM
}

// Function to save the schedule to EEPROM
void saveScheduleToEEPROM() {
    if (!scheduleChanged) return; // Only save if there is a change

    // Calculate the number of bytes to clear
    int numberOfBytes = scheduleSize * 4; // 4 bytes per schedule entry

    // Clear old data in EEPROM
    clearEEPROM(scheduleStartAddress, numberOfBytes);

    // Save new schedule data
    int address = scheduleStartAddress;
    for (int i = 0; i < scheduleSize; i++) {
        EEPROM.write(address++, weeklySchedule[i].onHour);
        EEPROM.write(address++, weeklySchedule[i].onMinute);
        EEPROM.write(address++, weeklySchedule[i].offHour);
        EEPROM.write(address++, weeklySchedule[i].offMinute);
    }
    EEPROM.commit(); // Commit changes to EEPROM
    scheduleChanged = false; // Reset the flag after saving
}


void loadScheduleFromEEPROM() {
    int address = scheduleStartAddress;
    for (int i = 0; i < scheduleSize; i++) {
        weeklySchedule[i].onHour = EEPROM.read(address++);
        weeklySchedule[i].onMinute = EEPROM.read(address++);
        weeklySchedule[i].offHour = EEPROM.read(address++);
        weeklySchedule[i].offMinute = EEPROM.read(address++);

        // Validate time values
        if (weeklySchedule[i].onHour > 23) weeklySchedule[i].onHour = 0;
        if (weeklySchedule[i].onMinute > 59) weeklySchedule[i].onMinute = 0;
        if (weeklySchedule[i].offHour > 23) weeklySchedule[i].offHour = 0;
        if (weeklySchedule[i].offMinute > 59) weeklySchedule[i].offMinute = 0;
    }
}

String getScheduleAsJSON() {
    loadScheduleFromEEPROM(); // Load the schedule from EEPROM

    StaticJsonDocument<512> doc; // Create a JSON document

    for (int i = 0; i < scheduleSize; i++) {
        String onTime = String(weeklySchedule[i].onHour) + ":" + (weeklySchedule[i].onMinute < 10 ? "0" : "") + String(weeklySchedule[i].onMinute);
        String offTime = String(weeklySchedule[i].offHour) + ":" + (weeklySchedule[i].offMinute < 10 ? "0" : "") + String(weeklySchedule[i].offMinute);
        
        doc[i]["on"] = onTime;
        doc[i]["off"] = offTime;
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse); // Convert the JSON document to a string
    return jsonResponse;
}

// Example function to handle an HTTP request
void handleScheduleRequest() {
    String jsonResponse = getScheduleAsJSON();
    // Send jsonResponse back to the client
}
void updateSchedule() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        StaticJsonDocument<512> doc;

        DeserializationError error = deserializeJson(doc, body);
        if (error) {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        for (int i = 0; i < scheduleSize; i++) {
            weeklySchedule[i].onHour = doc[i]["onHour"];
            weeklySchedule[i].onMinute = doc[i]["onMinute"];
            weeklySchedule[i].offHour = doc[i]["offHour"];
            weeklySchedule[i].offMinute = doc[i]["offMinute"];

            // Save the updated schedule to EEPROM
            int address = scheduleStartAddress + (i * 4); // 4 bytes per schedule entry
            EEPROM.write(address++, weeklySchedule[i].onHour);
            EEPROM.write(address++, weeklySchedule[i].onMinute);
            EEPROM.write(address++, weeklySchedule[i].offHour);
            EEPROM.write(address++, weeklySchedule[i].offMinute);
        }
        EEPROM.commit(); // Ensure the data is saved

        server.send(200, "application/json", "{\"status\":\"Schedule updated\"}");
    } else {
        server.send(400, "application/json", "{\"error\":\"No data received\"}");
    }
}
// Function to print time in HHMM format
void printTimeInHHMM(uint8_t hour, uint8_t minute) {
    Serial.print(hour < 10 ? "0" : ""); // Leading zero for hours
    Serial.print(hour);
    Serial.print(":");
    Serial.print(minute < 10 ? "0" : ""); // Leading zero for minutes
    Serial.print(minute);
}
void printEEPROMContents() {
 int address = scheduleStartAddress;

    Serial.println("EEPROM Contents:");

    for (int i = 0; i < scheduleSize; i++) {
        // Read values from EEPROM
        uint8_t onHour = EEPROM.read(address++);
        uint8_t onMinute = EEPROM.read(address++);
        uint8_t offHour = EEPROM.read(address++);
        uint8_t offMinute = EEPROM.read(address++);

        // Format time in HHMM
        Serial.print("Schedule Entry ");
        Serial.print(i);
        Serial.print(": ON: ");
        printTimeInHHMM(onHour, onMinute);
        Serial.print(", OFF: ");
        printTimeInHHMM(offHour, offMinute);
        Serial.println();
    }
}


void initializeSchedule() {
    for (int i = 0; i < scheduleSize; i++) {
        weeklySchedule[i].onHour = 0;  // Example initial values
        weeklySchedule[i].onMinute = 0;
        weeklySchedule[i].offHour = 0;
        weeklySchedule[i].offMinute = 0;

        int address = scheduleStartAddress + (i * 4);
        EEPROM.write(address++, weeklySchedule[i].onHour);
        EEPROM.write(address++, weeklySchedule[i].onMinute);
        EEPROM.write(address++, weeklySchedule[i].offHour);
        EEPROM.write(address++, weeklySchedule[i].offMinute);
    }
    EEPROM.commit(); // Ensure changes are saved
}
void clearEEPROM() {
    for (int i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
}


// Function to check if the current time is within a schedule entry
bool isRelayOn(int currentHour, int currentMinute) {
    for (int i = 0; i < 7; i++) {
        int onTotalMinutes = weeklySchedule[i].onHour * 60 + weeklySchedule[i].onMinute;
        int offTotalMinutes = weeklySchedule[i].offHour * 60 + weeklySchedule[i].offMinute;
        int currentTotalMinutes = currentHour * 60 + currentMinute;

        // Check if the current time is within the ON and OFF range
        if (currentTotalMinutes >= onTotalMinutes && currentTotalMinutes < offTotalMinutes) {
            return true; // Relay should be ON
        }
    }
    return false; // Relay should be OFF
}
// Function to write a string to EEPROM
void writeString(int addr, String str) {
  int len = str.length();
  EEPROM.write(addr, len);
  for (int i = 0; i < len; i++) {
      EEPROM.write(addr + 1 + i, str[i]);
  }
}

// Function to read a string from EEPROM
String readString(int addr) {
  int len = EEPROM.read(addr);
  String str = "";
  for (int i = 0; i < len; i++) {
      str += char(EEPROM.read(addr + 1 + i));
  }
  return str;
}

// Function to generate a secret key (you can customize this)
String generateSecretKey() {
  return "ABLZx001"; // Replace with your own SECRET KEY
}
// Function to handle login
void handleLogin() {
  String username = server.arg("username");
  String password = server.arg("password");
  String key = server.arg("key");

  // Retrieve stored credentials and key
  String storedUsername = readString(USERNAME_ADDR);
  String storedPassword = readString(PASSWORD_ADDR);
  String storedKey = readString(KEY_ADDR);
  bool storedStatus = EEPROM.read(STATUS_ADDR);

  // Check credentials and key
  if (username == storedUsername && password == storedPassword && key == storedKey) {
      isVerified = storedStatus;
      loggedIn = isVerified; // Set login status based on verification
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "Success");
   
  } else {
       server.send(200, "text/html", "<html><body style=\"background-color: black; color: red;\"><h1>[Login]:Access Deny</h1><a href=\"/\" style=\"color: white;\">Back</a></body></html>");

      
      Serial.println("Invalid login!");
      digitalWrite(ledPin2,LOW);
      delay(100);
      digitalWrite(ledPin2,HIGH);  
      delay(300);     }

}

// Function to handle registration
void handleRegister() {
  String username = server.arg("newUsername");
  String password = server.arg("newPassword");

  // Generate a secret key for new users
  String secretKey = generateSecretKey();

  // Store user credentials and secret key
  writeString(USERNAME_ADDR, username);
  writeString(PASSWORD_ADDR, password);
  writeString(KEY_ADDR, secretKey);
  EEPROM.write(STATUS_ADDR, 0); // Set status as unverified
  EEPROM.commit();

  
    Serial.println(username);
    Serial.println(password);
       
// Send HTML response with black background
  server.send(200, "text/html", "<html><body style=\"background-color: black; color: white;\"><h1>Registration Successful</h1><p>Check your email for a verification SECRET KEY.</p><a href=\"/\" style=\"color: red;\">Back</a></body></html>");
}

//################################ DAY CONTROL #######################
//###############################              #######################
// DURATION CONTROL


int dayOfWeek() {
  timeClient.update(); // Update the time
  int day = timeClient.getDay(); // Get day of the week (0 = Sunday, 1 = Monday, ..., 6 = Saturday)
  return day; // Return the current day index
}
//timmer
void handleRoot2() {
// Get current time
  time_t currentTime = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&currentTime);
  
  char dateTimeStr[30];
  strftime(dateTimeStr, sizeof(dateTimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

// Define the current day index (0 = Sunday, 1 = Monday, ..., 6 = Saturday)
int currentDayIndex = dayOfWeek(); // Get the accurate day of the week

String html ="<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><style>"
              "body { font-family: Arial, sans-serif; background-color: #000000; color: #FF0000; margin: 0; padding: 0; }"
              "h1 { color: #FF0000; text-align: center; margin-top: 10px; }"
              ".container { width: 100%; max-width: 800px; margin: 0 auto; padding: 20px; }"
              ".fixed-day { background-color: #222222; padding: 10px; border: 1px solid #666666; margin-bottom: 20px; }"
              ".scrollable-days { max-height: 300px; overflow-y: auto; border: 1px solid #666666; padding: 10px; }"
              "input[type='text'] { background-color: #333333; color: #FF0000; border: 1px solid #666666; padding: 5px; width: 100px; }"
              "input[type='text']::placeholder { color: #FF0000; }"
              "input[type='submit'] { background-color: #007BFF; color: #FF0000; border: none; padding: 10px 20px; cursor: pointer; margin-top: 10px; }"
              "input[type='submit']:hover { background-color: #0056b3; }"
              ".date-time { position: absolute; top: 10px; right: 20px; color: #ff0000; }"
                "</style><script>"
                "function refreshDateTime() {"
                "  fetch('/getCurrentTime')"
                "    .then(response => response.text())"
                "    .then(data => {"
                "      document.getElementById('currentDateTime').innerText = data;"
                "    });"
                "}"
                "setInterval(refreshDateTime, 1000);"
                "</script></head><body>";
  
  html += "<div class=\"date-time\" id=\"currentDateTime\">" + String(dateTimeStr) + "</div>";
html += "<h1>MCB Daily-Duration Control (Time: 60x24)</h1>";
html += "<div class=\"container\">";
html += "<form action=\"/setTime\" method=\"POST\">";

// Fixed box for current day
html += "<div class=\"fixed-day\">";
html += "<h2>Current Day: " + String(weekDays[currentDayIndex]) + "</h2>";
html += "<label for=\"onTimeCurrent\">On Time (HH:MM): </label>";
html += "<input type=\"text\" id=\"onTimeCurrent\" name=\"onTime" + String(currentDayIndex) + "\" value=\"00:00\" placeholder=\"00:00\"><br>";
html += "<label for=\"offTimeCurrent\">Off Time (HH:MM): </label>";
html += "<input type=\"text\" id=\"offTimeCurrent\" name=\"offTime" + String(currentDayIndex) + "\" value=\"00:00\" placeholder=\"00:00\"><br>";
html += "</div>";

html += "<input type=\"submit\" value=\"SET Timer\">";
html += "</form>";
html += "</div>"; // End of .container
html += "</body></html>";

server.send(200, "text/html", html);
}






// DURATION CONTROL
void handleTimmer(){
  
 if (server.method() == HTTP_POST) {
  for (int i = 0; i < 7; i++) {
    
    sscanf(server.arg("onTime" + String(i)).c_str(), "%d:%d", &weeklySchedule[i].onHour, &weeklySchedule[i].onMinute);
    sscanf(server.arg("offTime" + String(i)).c_str(), "%d:%d", &weeklySchedule[i].offHour, &weeklySchedule[i].offMinute);
  }
    overrideperiod = true;
    overrideSchedule = false;//disable override
    state = false;
    Serial.println("[Time]:Timmer Set !ok! ");
   
  server.send(200, "text/html", "<html><body style=\"background-color: black; color: white;\"><h1>Timmer Set !ok!</h1><a href=\"/\" style=\"color: red;\">Back</a></body></html>");

}
}
// Function to calculate total duration between ON and OFF times
String calculateDuration(MCB_Schedule weeklySchedule) {
  // Convert ON time to minutes since midnight
  int onTimeInMinutes = weeklySchedule.onHour * 60 + weeklySchedule.onMinute;
  // Convert OFF time to minutes since midnight
  int offTimeInMinutes = weeklySchedule.offHour * 60 + weeklySchedule.offMinute;

  // Calculate duration in minutes
  int durationInMinutes = offTimeInMinutes - onTimeInMinutes;

  // Handle cases where OFF time is before ON time (e.g., overnight)
  if (durationInMinutes < 0) {
      durationInMinutes += 24 * 60; // Add 24 hours worth of minutes
  }

  // Convert duration to hours and minutes
  int durationHours = durationInMinutes / 60;
  int durationMinutes = durationInMinutes % 60;

  //water drift out
  float waterlevel = durationInMinutes*3.1; //3.1 LPM base
  
  // Format the duration
  String duration =  String(durationHours) +":"+ String(durationMinutes) +"H:M" + " |" + String(waterlevel)+"L";

  return duration;
}
// Function to handle duration request
void handleDuration() {
  MCB_Schedule todaySchedule = weeklySchedule[timeClient.getDay()];

  // Calculate duration
  String duration = calculateDuration(todaySchedule);

  // Send duration as JSON
  String jsonResponse = "{\"duration\": \"" + duration + "\"}";
  server.send(200, "application/json", jsonResponse);

}


//################################ WEEK CONTROL #######################
// SHEDULE CONTROL
// Helper function to format numbers as two digits
String formatTwoDigits(int number) {
  return (number < 10 ? "0" : "") + String(number);
}
void handleRoot() {
// Load schedule from EEPROM or other storage as needed
   loadScheduleFromEEPROM();

  // Get current time
  time_t currentTime = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&currentTime);
  
  char dateTimeStr[30];
  strftime(dateTimeStr, sizeof(dateTimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

  String html = "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><style>"
                "body { font-family: Arial, sans-serif; background-color: #000000; color: #00FF00; }"
                "h1 { color: #00FF00; }"
                "fieldset { border: 1px solid #444444; padding: 10px; margin-bottom: 10px; }"
                "legend { color: #00FF00; }"
                "input[type='text'] { background-color: #333333; color: #00FF00; border: 1px solid #666666; padding: 5px; }"
                "input[type='text']::placeholder { color: #00FF00; }"
                "input[type='submit'] { background-color: #007BFF; color: #00FF00; border: none; padding: 10px 20px; cursor: pointer; }"
                "input[type='submit']:hover { background-color: #0056b3; }"
                "a { text-decoration: none; color: #00FF00; }"
                "a:hover { text-decoration: underline; }"
                ".date-time { position: absolute; top: 10px; right: 20px; color: #2bff00; }"
                "</style><script>"
                "function refreshDateTime() {"
                "  fetch('/getCurrentTime')"
                "    .then(response => response.text())"
                "    .then(data => {"
                "      document.getElementById('currentDateTime').innerText = data;"
                "    });"
                "}"
                "setInterval(refreshDateTime, 1000);"
                "</script></head><body>";
  
  html += "<div class=\"date-time\" id=\"currentDateTime\">" + String(dateTimeStr) + "</div>";
  html += "<h1>MCB Weekly-Scheduler Control (Time: 7x24)</h1>";
  html += "<form action=\"/setSchedule\" method=\"POST\">";

  for (int i = 0; i < 7; i++) {
      // Format the hours and minutes to always be two digits
      String onHour = formatTwoDigits(weeklySchedule[i].onHour);
      String onMinute = formatTwoDigits(weeklySchedule[i].onMinute);
      String offHour = formatTwoDigits(weeklySchedule[i].offHour);
      String offMinute = formatTwoDigits(weeklySchedule[i].offMinute);

      String onTimeValue = (weeklySchedule[i].onHour >= 0 && weeklySchedule[i].onMinute >= 0) ?
                           onHour + ":" + onMinute : "00:00";
      String offTimeValue = (weeklySchedule[i].offHour >= 0 && weeklySchedule[i].offMinute >= 0) ?
                            offHour + ":" + offMinute : "00:00";

      html += "<fieldset>";
      html += "<legend>" + String(weekDays[i]) + "</legend>";
      html += "<label for=\"onTime" + String(i) + "\">On Time (HH:MM): </label>";
      html += "<input type=\"text\" id=\"onTime" + String(i) + "\" name=\"onTime" + String(i) + "\" value=\"" + onTimeValue + "\" pattern=\"[0-9]{2}:[0-9]{2}\" required placeholder=\"HH:MM\"><br>";
      html += "<label for=\"offTime" + String(i) + "\">Off Time (HH:MM): </label>";
      html += "<input type=\"text\" id=\"offTime" + String(i) + "\" name=\"offTime" + String(i) + "\" value=\"" + offTimeValue + "\" pattern=\"[0-9]{2}:[0-9]{2}\" required placeholder=\"HH:MM\"><br>";
      html += "</fieldset>";
  }

  html += "<input type=\"submit\" value=\"SET SCHEDULE\">";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleGetCurrentTime() {
  timeClient.update(); // Update the time
  time_t currentTime = timeClient.getEpochTime();
  struct tm *timeinfo = localtime(&currentTime);

  char dateTimeStr[30];
  strftime(dateTimeStr, sizeof(dateTimeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

  server.send(200, "text/plain", dateTimeStr); // Send the current time
}
// Handle setting the schedule
bool isValidTime(int hour, int minute) {
  return (hour >= 0 && hour < 24) && (minute >= 0 && minute < 60);
}

void handleSetSchedule() {
if (server.method() == HTTP_POST) {
  for (int i = 0; i < 7; i++) {
    sscanf(server.arg("onTime" + String(i)).c_str(), "%d:%d", &weeklySchedule[i].onHour, &weeklySchedule[i].onMinute);
    sscanf(server.arg("offTime" + String(i)).c_str(), "%d:%d", &weeklySchedule[i].offHour, &weeklySchedule[i].offMinute);
  }
   overrideSchedule = false;
   state=false;
  
  for (int i = 0; i < 7; i++) {
      String onTime = server.arg("onTime" + String(i));
      String offTime = server.arg("offTime" + String(i));
      
      // Extract hours and minutes from the submitted time strings
      int onHour = onTime.substring(0, 2).toInt();
      int onMinute = onTime.substring(3, 5).toInt();
      int offHour = offTime.substring(0, 2).toInt();
      int offMinute = offTime.substring(3, 5).toInt();
      
      // Validate input values
      if (isValidTime(onHour, onMinute) && isValidTime(offHour, offMinute)) {
          // Update the schedule if data is valid
          weeklySchedule[i].onHour = onHour;
          weeklySchedule[i].onMinute = onMinute;
          weeklySchedule[i].offHour = offHour;
          weeklySchedule[i].offMinute = offMinute;
      } else {
          // Handle invalid data (e.g., set to default values or show an error)
          weeklySchedule[i].onHour = 0;
          weeklySchedule[i].onMinute = 0;
          weeklySchedule[i].offHour = 0;
          weeklySchedule[i].offMinute = 0;
      }
  }
  
    scheduleChanged = true; // Set the flag to indicate a change
    saveScheduleToEEPROM(); // Save the updated schedule to EEPROM
   Serial.println("[Schedular]:Schedule Set !ok! ");
   
 server.send(200, "text/html", "<html><body style=\"background-color: black; color: white;\"><h1>Schedule Time Set weekly ok!</h1><a href=\"/\" style=\"color: red;\">Back</a></body></html>");

}
}


// Handle overriding the schedule and timmer @@@
void handleOverride() {
 mcbState = !mcbState;
 digitalWrite(mcbPin,HIGH);delay(100); // all relay off 
 digitalWrite(mcbPin2,HIGH);


overrideSchedule = true;//enable override
overrideperiod = false;//disable timmer
state=false;
Serial.println("[Force Stop]:Schedule Overridden");

server.send(200, "text/html", "<html><body style=\"background-color: black; color: red;\"><h1>[High Alert!]:Schedule Overridden!!!</h1><a href=\"/\" style=\"color: white;\">Back</a></body></html>");

}



//show
void handleDateTime() {
  timeClient.update(); // Update the NTP time

  // Get the current time and date
  String formattedTime = timeClient.getFormattedTime(); // Time in HH:MM:SS
  time_t rawTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime(&rawTime); // Use localtime() if you need local time instead of UTC

  // Format the date as DD/MM/YYYY
  char dateBuffer[11];
  snprintf(dateBuffer, sizeof(dateBuffer), "%02d/%02d/%04d", ptm->tm_mday, ptm->tm_mon + 1, ptm->tm_year + 1900);

  // Create JSON response
  String jsonResponse = "{\"date\": \"" + String(dateBuffer) + "\", \"time\": \"" + formattedTime + "\"}";

  // Send the response
  server.send(200, "application/json", jsonResponse);
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);

  //PROJECT title
  display.println("<<SMART IRRIGATIONa>>");
  // Show IP Address
  IPAddress ip = WiFi.localIP();
  String ipString = ip.toString();
  display.print("Login IP:");
  display.println(ipString);

  //DATE
  // Get current date and time from NTP client
  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawTime = epochTime;
  struct tm * timeInfo = localtime(&rawTime);
  //DAY 
   char dateString[20];
  strftime(dateString, sizeof(dateString), "%a", timeInfo); // e.g., Mon

  //DATE
  char dateString2[15];
  strftime(dateString2, sizeof(dateString2), "%d/%m/%Y", timeInfo); // Date in DD/MM/YYYY format

  
 
 
  display.println(dateString2); // Display the date 
  display.println(dateString);// Display the day
  
  
      
  // Show time
  
  // Get the current time from the NTP client
  String formattedTime = timeClient.getFormattedTime(); // Get time in HH:MM:SS format
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int seconds = timeClient.getSeconds();

  // Convert to 12-hour format with AM/PM
  String ampm = (hours >= 12) ? "PM" : "AM";
  if (hours > 12) hours -= 12;
  if (hours == 0) hours = 12; // Handle midnight as 12 AM

  // Format the time string
  String timeString = String(hours) + ":" + 
                      String(minutes < 10 ? "0" + String(minutes) : String(minutes)) + ":" + 
                      String(seconds < 10 ? "0" + String(seconds) : String(seconds)) + " " + 
                      ampm;

  // Display the time
  
  display.println(timeString);
  // Show MCB status
  display.print("Status: ");
  display.println(digitalRead(mcbPin) ? "NORMAL" : "TRIGGER");


  String duration = calculateDuration(weeklySchedule[timeClient.getDay()]);
  display.print("TIMMER:");
  display.println(duration);

  //

  
  display.display();
}
void LOGO() {
  display.clearDisplay();

  // Set text size to 4 for "ABL"
  display.setTextSize(4); // Larger text size for "ABL"
  display.setTextColor(SSD1306_WHITE);
  
  // Calculate text width and height to center it on the display
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds("ABL", 0, 0, &x1, &y1, &w, &h);

  int16_t x = (display.width() - w) / 2; // Center horizontally
  int16_t y = (display.height() - h) / 2; // Center vertically

  display.setCursor(x, y);
  display.print("ABL");

  display.display();
}



// Setup function
void setup() {
  Serial.begin(115200);
 
  EEPROM.begin(512); // Initialize EEPROM with a size of 512 bytes
  //only required for reset eeprm hardware initilization
  
   // clearEEPROM(); // Only for testing, comment out after initial run
   
    //initializeSchedule(); // Comment out after running once
  
  loadScheduleFromEEPROM(); // Load initial schedule from EEPROM
  printEEPROMContents();
  delay(100);

  pinMode(mcbPin, OUTPUT);
  pinMode(mcbPin2, OUTPUT);
  pinMode(ledPin2, OUTPUT);   
  pinMode(ledPin, OUTPUT);
  //ALL RELAY ARE ENABLE LOW //DISABLE HIGH
  digitalWrite(mcbPin, HIGH);
  digitalWrite(mcbPin2,HIGH);
  digitalWrite(ledPin2,LOW);
  //sensor
  pinMode(MOISTURE_SENSOR_PIN, INPUT);
  // Initialize WiFi Manager
  WiFiManager wifiManager;
  wifiManager.autoConnect("SYSCO-INDUSTRY");//CONFIG CONNECT & reset connect
  pinMode(TRIGGER_PIN, INPUT);

  // Initialize OLED Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Use 0x3C for the OLED I2C address
      Serial.println(F("SSD1306 allocation failed"));
      for(;;);
  }
  display.display();
  delay(1);
  display.clearDisplay();
  display.display();
  LOGO();
  delay(2000);
  display.clearDisplay(); 
  
  // Initialize NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(5.5*3600); // Adjust for your timezone

  // Define web server routes
  //server.on("/", handleRoot);
      server.on("/", HTTP_GET, []() {
      if (!loggedIn) {
          server.send(200, "text/html", loginPage);
          return;
      }
      String pageContent = mainPageEN;
      server.send(200, "text/html", pageContent);
       Serial.println("Login granted!");
  });
server.on("/register", HTTP_POST, []() {
          handleRegister();
          Serial.println("Successfully Registered!");
      });
      server.on("/login", HTTP_POST, []() {
        handleLogin();
        loggedIn = true;
          });
// override control /accessing override on Emergency. ###
  server.on("/manualOn", HTTP_POST, []() {
      digitalWrite(mcbPin, LOW);//ON
      delay(1000);
       digitalWrite(mcbPin2, LOW);//ON
      digitalWrite(ledPin2,HIGH);
      mcbState = true;
      state = true;//disable schedule/overide schedule
      Serial.println("MCB manually turned ON");
      server.send(200, "text/html", "<html><body style=\"background-color: black; color: white;\"><h1>ON</h1><a href=\"/\" style=\"color: red;\">Back</a></body></html>");
 
      overrideSchedule = false;//enable override
      overrideperiod = false;// timmer
  });
  server.on("/manualOff", HTTP_POST, []() {
      digitalWrite(mcbPin, HIGH);//OFF
       digitalWrite(mcbPin2, HIGH);//ON
       digitalWrite(ledPin2,LOW);
      mcbState = false;
      state = false;//enable
      Serial.println("MCB manually turned OFF");
          server.send(200, "text/html", "<html><body style=\"background-color: black; color: white;\"><h1>OFF</h1><a href=\"/\" style=\"color: red;\">Back</a></body></html>");

      overrideSchedule = true;//enable override
      overrideperiod = true;// timmer
     
  });
   server.on("/status", HTTP_GET, []() {
      String status = "{\"mcbStatus\": " + String(mcbState) + "}";
      server.send(200, "application/json", status);
  });
  server.on("/weekly", HTTP_POST, []() {
         handleRoot();
     });
 server.on("/setSchedule", handleSetSchedule);

// logout page ###
  server.on("/logout", HTTP_POST, []() {
      loggedIn = false;
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "");
  });
server.on("/emergency", handleOverride);


//duration
server.on("/daily", HTTP_POST, []() {
         handleRoot2();
     });
server.on("/setTime",handleTimmer);
//server.on("/stop", handlestop);
//duration
server.on("/duration", HTTP_GET, handleDuration);
//show date and time 
server.on("/dateTime", HTTP_GET, handleDateTime);
//refresh
server.on("/getCurrentTime", HTTP_GET, handleGetCurrentTime);
  server.on("/wifiInfo", handleWifiInfo);

//web update save schedule
 server.on("/schedule", HTTP_GET, handleScheduleRequest);
  server.on("/schedule", HTTP_POST, updateSchedule);
  // Start the server
  server.begin();

}
 

int i = 10;
int moistureState=0;
// Main loop function
void loop() {
    server.handleClient();
    server.handleClient();
    timeClient.update();
    updateDisplay();
    unsigned long currentMillis = millis();
          if( digitalRead(TRIGGER_PIN) == LOW ){
        Serial.println("Saved WiFi Erasing...");
      
      wifiManager.resetSettings();
        ESP.restart();
      }
      
    if( WiFi.status() == WL_CONNECTED){
      digitalWrite(ledPin , LOW); delay(200);digitalWrite(ledPin , HIGH);delay(2000);
    }
     
     // MCB control on weekly shedule logic
    if (!overrideSchedule) {
        // Get current time
        int dayOfWeek = timeClient.getDay();
        int currentHour = timeClient.getHours();
        int currentMinute = timeClient.getMinutes();

        MCB_Schedule todaySchedule = weeklySchedule[dayOfWeek];
        if(currentHour == todaySchedule.onHour && currentMinute == todaySchedule.onMinute) {
            digitalWrite(mcbPin, LOW);delay(100);//ON
            digitalWrite(mcbPin2, LOW);delay(100);//ON
            
             digitalWrite(ledPin2,HIGH);    
            mcbState = true;
            Serial.println("[Shedule]:MCB sheduly turned ON");
        } else if (currentHour == todaySchedule.offHour && currentMinute == todaySchedule.offMinute) {
            digitalWrite(mcbPin, HIGH);delay(10);//OFF
            digitalWrite(mcbPin2, HIGH);delay(10);//OFF
                
            digitalWrite(ledPin2,LOW); 
            mcbState = false;
            Serial.println("[Shedule]:MCB sheduly turned OFF");
        }
    }
    //AUTOMATIC SCHEDULING TIME update
 
   int currentHour = timeClient.getHours(); // Replace with actual time retrieval
    int currentMinute = timeClient.getMinutes(); // Replace with actual time retrieval
    // Check relay state
    if(!state){
    if (isRelayOn(currentHour, currentMinute) && !overrideSchedule ) {
        digitalWrite(mcbPin, LOW); // Turn ON relay
        delay(200);
         digitalWrite(mcbPin2, LOW); // Turn ON relay
           overrideSchedule = false;//
           state = true;
    } else {
        digitalWrite(mcbPin, HIGH); // Turn OFF relay
        delay(200);
         digitalWrite(mcbPin2, HIGH); // Turn ON relay
          state = false;
       
    }      
//  // MCB control on  logic duration time
  if (!overrideperiod && !overrideSchedule ) {
        // Get current time
        int dayOfWeek = timeClient.getDay();
        int currentHour = timeClient.getHours();
        int currentMinute = timeClient.getMinutes();

        MCB_Schedule todaySchedule = weeklySchedule[dayOfWeek];
        if (currentHour == todaySchedule.onHour && currentMinute == todaySchedule.onMinute) {
          
           digitalWrite(mcbPin, LOW);delay(100);//ON
           digitalWrite(mcbPin2, LOW);delay(100);//ON
           
           digitalWrite(ledPin2,HIGH); //ON       
            mcbState = true;
            Serial.println("[Period]:MCB Timming ON!");
        } 
        else if (currentHour == todaySchedule.offHour && currentMinute == todaySchedule.offMinute) {
             digitalWrite(mcbPin, HIGH);delay(100);//OFF
                digitalWrite(mcbPin2, HIGH);//OFF
                 
                 digitalWrite(ledPin2,LOW);//OFF
            mcbState = false;
            Serial.println("[Period]:MCB Timming OFF!");
        }
    }
    }

 moistureState = digitalRead(MOISTURE_SENSOR_PIN);//VAL =0


 //LOW when soil is WET and HIGH when DRY
 //OVER SENSOR CONTROL
 /*

  if(moistureState == 0){
          handleOverride();
             Serial.println("[SENSOR]:MCB  TRIGGER STOP !"); 
              mcbState = false;
  }

else {
               overrideperiod = false;//disable timmer
                  Serial.println("[SENSOR]:MCB  DEFAULT STOP !");
                  mcbState = true;
  }
*/
    // Handle sensor fault checks
    if (sensorFault) {
        if (currentMillis - lastFaultCheck >= faultCheckInterval) {
            // Send fault alert
         //   sendEmail("Critical Alert", "Sensor fault detected! Please fix it immediately.", "your_email@example.com");
            lastFaultCheck = currentMillis;
               // digitalWrite(mcbPin, HIGH);delay(100);//OFF
               // digitalWrite(mcbPin2, HIGH);//OFF
                //digitalWrite(ledPin2,LOW);//OFF
                //delay(100);
                //digitalWrite(ledPin2,HIGH);//ON
           /// mcbState = false;
            Serial.println("[Alert]:Fault detected in sensor OFF!"); 
        }
    } else {
        // Check if sensor fault is resolved
        if (i > 0) {
       //     sendEmail("Maintenance Update", "Sensor fault resolved. 😊", "your_email@example.com");
            sensorFault = false;
          //  mcbState = true;
        }
    }
    
 
}

