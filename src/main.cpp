#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <TimeLib.h>
#include <TelnetStream.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <sntp.h>
#include <TZ.h>
#include "secrets.h"

// General Setup
#define TIME_ZONE TZ_Europe_London

// Pin Definitions
#define IN1_PIN D5     // Drives L298N (Light Set A) IN1
#define IN2_PIN D6     // Drives L298N (Light Set B) IN2
#define ENA_PIN D7     // PWM brightness control for L298N ENA
#define MODE_BUTTON D2 // Push button

// FLASH_MAP_SETUP_CONFIG(FLASH_MAP_NO_FS)

// Wi-Fi connection parameters
const char *wifi_ssid = WIFI_SSID;
const char *wifi_password = WIFI_PASS;

// MQTT connection parameters (add these to secrets.h)
const char *mqtt_server = MQTT_SERVER;
const int mqtt_port = 1883;
const char *mqtt_user = MQTT_USER;
const char *mqtt_password = MQTT_PASSWORD;
const char *mqtt_client_id = "christmas-lights";

// MQTT Topics
const char *mqtt_state_topic = "homeassistant/light/christmas_lights/state";
const char *mqtt_command_topic = "homeassistant/light/christmas_lights/set";
const char *mqtt_mode_state_topic = "homeassistant/select/christmas_lights_mode/state";
const char *mqtt_mode_command_topic = "homeassistant/select/christmas_lights_mode/set";
const char *mqtt_speed_state_topic = "homeassistant/number/christmas_lights_speed/state";
const char *mqtt_speed_command_topic = "homeassistant/number/christmas_lights_speed/set";

// Create instances
ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Light modes
enum LightMode
{
  ALL_ON,
  ALTERNATE_FLASH,
  FADE_ALL,
  FADE_ALTERNATE,
  TWINKLE,
  CHASE,
  METEOR,
  MUSIC_SYNC,
  MODE_COUNT
};

const char *modeNames[] = {
    "All On",
    "Alternate Flash",
    "Fade All",
    "Fade Alternate",
    "Twinkle",
    "Chase",
    "Meteor",
    "Music Sync"};

LightMode currentMode = ALL_ON;
bool buttonPressed = false;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200; // Debounce time in milliseconds

// Configurable parameters
int maxBrightness = 255;     // Maximum brightness (0-255)
float speedMultiplier = 1.0; // Speed multiplier (0.1 to 5.0)
bool lightsOn = true;        // Overall on/off state

// Variables for animations
int brightness = 255;
int fadeAmount = 5;
int direction = 1;
unsigned long lastUpdate = 0;
int animationStep = 0;
int twinkleState[10] = {0}; // For twinkle effect

void printModeMenu()
{
  TelnetStream.println("\n=== Christmas Lights Control Menu ===");
  TelnetStream.println("Commands:");
  TelnetStream.println("  R - Reset controller");
  TelnetStream.println("  C - Close telnet connection");
  TelnetStream.println("  M - Cycle to next mode");
  TelnetStream.println("  ? - Show this menu");
  TelnetStream.println("\nLight Modes (press number to select):");

  for (int i = 0; i < MODE_COUNT; i++)
  {
    TelnetStream.print("  ");
    TelnetStream.print(i + 1); // Display 1-based numbering for users
    TelnetStream.print(" - ");
    TelnetStream.println(modeNames[i]);
  }
  TelnetStream.println("=====================================\n");
}

void log(const char *message)
{
  static int i = 0;

  char timeStr[20];
  sprintf(timeStr, "%02d-%02d-%02d %02d:%02d:%02d", year(), month(), day(), hour(), minute(), second());

  TelnetStream.print(i++);
  TelnetStream.print(" ");
  TelnetStream.print(timeStr);
  TelnetStream.print(" log: ");
  TelnetStream.println(message);
}

void connectToWiFi()
{
  Serial.printf("Connecting to '%s'\n", wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  if (WiFi.waitForConnectResult() == WL_CONNECTED)
  {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Connection Failed!");
  }
}

void setUpOverTheAirProgramming()
{
  ArduinoOTA.setHostname("christmas-lights");
  ArduinoOTA.begin();
}

void setDirection(int dir)
{
  // direction: 1=forward (Set A), -1=reverse (Set B), 0=off
  if (dir > 0)
  {
    digitalWrite(IN1_PIN, HIGH);
    digitalWrite(IN2_PIN, LOW);
  }
  else if (dir < 0)
  {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, HIGH);
  }
  else
  {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
  }
}

void setBrightness(int bright)
{
  // brightness: 0-255 (PWM value)
  analogWrite(ENA_PIN, bright);
}

void changeMode(LightMode newMode)
{
  currentMode = newMode;
  char modeMsg[50];
  sprintf(modeMsg, "Mode changed to: %s", modeNames[currentMode]);
  log(modeMsg);

  // Reset animation variables when changing modes
  brightness = 255;
  fadeAmount = 5;
  direction = 1;
  animationStep = 0;
}

void checkModeButton()
{
  // Check if button is pressed (LOW due to pull-up resistor)
  if (digitalRead(MODE_BUTTON) == LOW)
  {
    if (!buttonPressed && (millis() - lastButtonPress > debounceTime))
    {
      buttonPressed = true;
      lastButtonPress = millis();

      // Change to next mode
      LightMode newMode = static_cast<LightMode>((currentMode + 1) % MODE_COUNT);
      changeMode(newMode);
    }
  }
  else
  {
    buttonPressed = false;
  }
}

void allOn()
{
  // Rapidly alternate between both sets to make all lights appear on
  unsigned long currentMillis = millis();
  int interval = (int)(20 / speedMultiplier);
  if (currentMillis - lastUpdate > interval)
  {
    lastUpdate = currentMillis;
    direction = -direction; // Flip between 1 and -1
    setDirection(direction);
    setBrightness(map(10, 0, 255, 0, maxBrightness));
  }
}

void alternateFlash()
{
  // Alternate between set A and set B
  unsigned long currentMillis = millis();
  int interval = (int)(500 / speedMultiplier);
  if (currentMillis - lastUpdate > interval)
  {
    lastUpdate = currentMillis;
    direction = -direction; // Flip between 1 and -1
    setDirection(direction);
    setBrightness(maxBrightness);
  }
}

void fadeAll()
{
  // Fade both sets of lights up and down together
  unsigned long currentMillis = millis();
  int interval = (int)(30 / speedMultiplier);
  if (currentMillis - lastUpdate > interval)
  {
    lastUpdate = currentMillis;

    // Change brightness
    brightness = brightness + fadeAmount;

    // Reverse fade direction when limits are reached
    if (brightness <= 0 || brightness >= maxBrightness)
    {
      fadeAmount = -fadeAmount;
      brightness = constrain(brightness, 0, maxBrightness);

      // Switch direction at the bottom of the fade
      if (brightness <= 0)
      {
        direction = -direction;
      }
    }

    setDirection(direction);
    setBrightness(brightness);
  }
}

void fadeAlternate()
{
  // Fade while alternating between sets
  unsigned long currentMillis = millis();
  int interval = (int)(30 / speedMultiplier);
  if (currentMillis - lastUpdate > interval)
  {
    lastUpdate = currentMillis;

    // Change brightness
    brightness = brightness + fadeAmount;

    // Reverse fade direction when limits are reached
    if (brightness <= 0 || brightness >= maxBrightness)
    {
      fadeAmount = -fadeAmount;
      brightness = constrain(brightness, 0, maxBrightness);

      // Switch between sets at the bottom of the fade
      if (brightness <= 0)
      {
        direction = -direction;
      }
    }

    setDirection(direction);
    setBrightness(brightness);
  }
}

void twinkle()
{
  // Random twinkling effect
  unsigned long currentMillis = millis();
  int interval = (int)(50 / speedMultiplier);
  if (currentMillis - lastUpdate > interval)
  {
    lastUpdate = currentMillis;

    // Randomly decide which set to use
    if (random(10) > 5)
    {
      direction = 1;
    }
    else
    {
      direction = -1;
    }

    // Random brightness
    brightness = random(100, maxBrightness);

    setDirection(direction);
    setBrightness(brightness);
    int delayTime = (int)(random(10, 50) / speedMultiplier);
    delay(delayTime); // Small random delay for twinkling effect
  }
}

void chase()
{
  // Light chasing effect
  unsigned long currentMillis = millis();
  int interval = (int)(100 / speedMultiplier);
  if (currentMillis - lastUpdate > interval)
  {
    lastUpdate = currentMillis;

    animationStep = (animationStep + 1) % 10;

    if (animationStep < 5)
    {
      direction = 1; // Set A
    }
    else
    {
      direction = -1; // Set B
    }

    // Brightness varies with position
    int minBright = maxBrightness * 0.4;
    int maxBright = maxBrightness;
    brightness = minBright + (maxBright - minBright) * sin((PI * animationStep) / 5);

    setDirection(direction);
    setBrightness(brightness);
  }
}

void meteor()
{
  // Meteor shower effect
  unsigned long currentMillis = millis();
  int interval = (int)(50 / speedMultiplier);
  if (currentMillis - lastUpdate > interval)
  {
    lastUpdate = currentMillis;

    // Cycle through animation steps
    animationStep = (animationStep + 1) % 20;

    if (animationStep < 10)
    {
      // Meteor on set A
      direction = 1;
      int stepBright = (animationStep < 5) ? (animationStep * 50) : (255 - (animationStep - 5) * 50);
      brightness = map(stepBright, 0, 255, 0, maxBrightness);
    }
    else
    {
      // Meteor on set B
      direction = -1;
      int step = animationStep - 10;
      int stepBright = (step < 5) ? (step * 50) : (255 - (step - 5) * 50);
      brightness = map(stepBright, 0, 255, 0, maxBrightness);
    }

    setDirection(direction);
    setBrightness(brightness);
  }
}

void musicSync()
{
  // Simulate music sync with pulsing pattern
  unsigned long currentMillis = millis();
  static int pulsePhase = 0;

  int interval = (int)(30 / speedMultiplier);
  if (currentMillis - lastUpdate > interval)
  {
    lastUpdate = currentMillis;

    pulsePhase = (pulsePhase + 1) % 100;

    // Create a pulsing pattern
    int minBright = maxBrightness * 0.4;
    int brightRange = maxBrightness - minBright;
    if (pulsePhase < 50)
    {
      direction = 1;
      brightness = minBright + brightRange * sin((PI * pulsePhase) / 50);
    }
    else
    {
      direction = -1;
      brightness = minBright + brightRange * sin((PI * (pulsePhase - 50)) / 50);
    }

    setDirection(direction);
    setBrightness(brightness);
  }
}

void handleNumericInput(int input)
{
  int modeIndex = input - '1'; // Convert from ASCII to 0-based index
  if (modeIndex >= 0 && modeIndex < MODE_COUNT)
  {
    changeMode(static_cast<LightMode>(modeIndex));
  }
}

// Publish Home Assistant MQTT Discovery messages
void publishHomeAssistantDiscovery()
{
  StaticJsonDocument<768> doc;
  String output;

  // Light entity discovery
  doc.clear();
  doc["name"] = "Christmas Lights";
  doc["unique_id"] = "christmas_lights_main";
  doc["state_topic"] = mqtt_state_topic;
  doc["command_topic"] = mqtt_command_topic;
  doc["brightness_state_topic"] = mqtt_state_topic;
  doc["brightness_command_topic"] = mqtt_command_topic;
  doc["brightness_scale"] = 255;
  doc["on_command_type"] = "brightness";
  doc["schema"] = "json";
  doc["device"]["identifiers"][0] = "christmas_lights_esp8266";
  doc["device"]["name"] = "Christmas Tree Lights";
  doc["device"]["model"] = "ESP8266 + L298N";
  doc["device"]["manufacturer"] = "DIY";

  output = "";
  serializeJson(doc, output);
  mqttClient.publish("homeassistant/light/christmas_lights/config", output.c_str(), true);

  // Mode select entity discovery
  doc.clear();
  doc["name"] = "Christmas Lights Mode";
  doc["unique_id"] = "christmas_lights_mode";
  doc["state_topic"] = mqtt_mode_state_topic;
  doc["command_topic"] = mqtt_mode_command_topic;
  JsonArray options = doc.createNestedArray("options");
  for (int i = 0; i < MODE_COUNT; i++)
  {
    options.add(modeNames[i]);
  }
  doc["device"]["identifiers"][0] = "christmas_lights_esp8266";

  output = "";
  serializeJson(doc, output);
  mqttClient.publish("homeassistant/select/christmas_lights_mode/config", output.c_str(), true);

  // Speed number entity discovery
  doc.clear();
  doc["name"] = "Christmas Lights Speed";
  doc["unique_id"] = "christmas_lights_speed";
  doc["state_topic"] = mqtt_speed_state_topic;
  doc["command_topic"] = mqtt_speed_command_topic;
  doc["min"] = 0.1;
  doc["max"] = 5.0;
  doc["step"] = 0.1;
  doc["mode"] = "slider";
  doc["device"]["identifiers"][0] = "christmas_lights_esp8266";

  output = "";
  serializeJson(doc, output);
  mqttClient.publish("homeassistant/number/christmas_lights_speed/config", output.c_str(), true);

  log("Home Assistant discovery messages published");
}

// Publish MQTT state
void publishMQTTState()
{
  StaticJsonDocument<256> doc;
  doc["state"] = lightsOn ? "ON" : "OFF";
  doc["brightness"] = maxBrightness;

  String output;
  serializeJson(doc, output);
  mqttClient.publish(mqtt_state_topic, output.c_str(), true);
}

void publishMQTTMode()
{
  mqttClient.publish(mqtt_mode_state_topic, modeNames[currentMode], true);
}

void publishMQTTSpeed()
{
  char speedStr[10];
  dtostrf(speedMultiplier, 4, 2, speedStr);
  mqttClient.publish(mqtt_speed_state_topic, speedStr, true);
}

// REST API Handlers
void handleRoot()
{
  String html = "<html><head><title>Christmas Lights Control</title></head><body>";
  html += "<h1>Christmas Lights Controller</h1>";
  html += "<p>Current Mode: <b>" + String(modeNames[currentMode]) + "</b></p>";
  html += "<p>Brightness: <b>" + String(maxBrightness) + "</b></p>";
  html += "<p>Speed: <b>" + String(speedMultiplier) + "</b></p>";
  html += "<p>State: <b>" + String(lightsOn ? "ON" : "OFF") + "</b></p>";
  html += "<h2>API Endpoints:</h2>";
  html += "<ul>";
  html += "<li>GET /status - Get current status</li>";
  html += "<li>POST /mode?value=[0-" + String(MODE_COUNT - 1) + "] - Set mode</li>";
  html += "<li>POST /brightness?value=[0-255] - Set brightness</li>";
  html += "<li>POST /speed?value=[0.1-5.0] - Set speed</li>";
  html += "<li>POST /state?value=[on|off] - Turn on/off</li>";
  html += "</ul>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleStatus()
{
  StaticJsonDocument<256> doc;
  doc["mode"] = currentMode;
  doc["mode_name"] = modeNames[currentMode];
  doc["brightness"] = maxBrightness;
  doc["speed"] = speedMultiplier;
  doc["state"] = lightsOn ? "on" : "off";

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleSetMode()
{
  if (server.hasArg("value"))
  {
    int mode = server.arg("value").toInt();
    if (mode >= 0 && mode < MODE_COUNT)
    {
      changeMode(static_cast<LightMode>(mode));
      publishMQTTMode();
      server.send(200, "application/json", "{\"status\":\"ok\",\"mode\":" + String(mode) + "}");
      return;
    }
  }
  server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid mode\"}");
}

void handleSetBrightness()
{
  if (server.hasArg("value"))
  {
    int bright = server.arg("value").toInt();
    if (bright >= 0 && bright <= 255)
    {
      maxBrightness = bright;
      publishMQTTState();
      server.send(200, "application/json", "{\"status\":\"ok\",\"brightness\":" + String(bright) + "}");
      return;
    }
  }
  server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid brightness (0-255)\"}");
}

void handleSetSpeed()
{
  if (server.hasArg("value"))
  {
    float speed = server.arg("value").toFloat();
    if (speed >= 0.1 && speed <= 5.0)
    {
      speedMultiplier = speed;
      publishMQTTSpeed();
      server.send(200, "application/json", "{\"status\":\"ok\",\"speed\":" + String(speed) + "}");
      return;
    }
  }
  server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid speed (0.1-5.0)\"}");
}

void handleSetState()
{
  if (server.hasArg("value"))
  {
    String state = server.arg("value");
    state.toLowerCase();
    if (state == "on" || state == "off")
    {
      lightsOn = (state == "on");
      publishMQTTState();
      server.send(200, "application/json", "{\"status\":\"ok\",\"state\":\"" + state + "\"}");
      return;
    }
  }
  server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid state (on/off)\"}");
}

// MQTT callback function
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String message;
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  log(("MQTT message received on " + String(topic) + ": " + message).c_str());

  if (String(topic) == mqtt_command_topic)
  {
    // Handle light on/off commands
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (!error)
    {
      if (doc.containsKey("state"))
      {
        String state = doc["state"];
        lightsOn = (state == "ON");
        publishMQTTState();
      }
      if (doc.containsKey("brightness"))
      {
        maxBrightness = doc["brightness"];
        publishMQTTState();
      }
    }
  }
  else if (String(topic) == mqtt_mode_command_topic)
  {
    // Handle mode change
    for (int i = 0; i < MODE_COUNT; i++)
    {
      if (message == modeNames[i])
      {
        changeMode(static_cast<LightMode>(i));
        publishMQTTMode();
        break;
      }
    }
  }
  else if (String(topic) == mqtt_speed_command_topic)
  {
    // Handle speed change
    speedMultiplier = message.toFloat();
    speedMultiplier = constrain(speedMultiplier, 0.1, 5.0);
    publishMQTTSpeed();
  }
}

// Connect to MQTT broker
void connectMQTT()
{
  while (!mqttClient.connected())
  {
    log("Connecting to MQTT...");
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password))
    {
      log("MQTT connected");

      // Subscribe to command topics
      mqttClient.subscribe(mqtt_command_topic);
      mqttClient.subscribe(mqtt_mode_command_topic);
      mqttClient.subscribe(mqtt_speed_command_topic);

      // Publish Home Assistant discovery messages
      publishHomeAssistantDiscovery();

      // Publish initial state
      publishMQTTState();
      publishMQTTMode();
      publishMQTTSpeed();
    }
    else
    {
      log("MQTT connection failed, retrying in 5 seconds");
      delay(5000);
    }
  }
}

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();

  // Maintain MQTT connection
  if (!mqttClient.connected())
  {
    connectMQTT();
  }
  mqttClient.loop();

  // Handle Telnet commands
  int input = TelnetStream.read();
  if (input != -1)
  {
    switch (input)
    {
    case 'R':
      TelnetStream.println("Resetting controller...");
      TelnetStream.stop();
      delay(100);
      ESP.reset();
      break;

    case 'C':
      TelnetStream.println("Closing telnet connection. Bye bye!");
      TelnetStream.flush();
      TelnetStream.stop();
      break;

    case 'M':
      // Change to next mode via Telnet
      changeMode(static_cast<LightMode>((currentMode + 1) % MODE_COUNT));
      break;

    case '?':
      printModeMenu();
      break;

    // Numeric mode selection (1-8)
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
      handleNumericInput(input);
      break;
    }
  }

  // Check for mode button press
  checkModeButton();

  // Only run animations if lights are on
  if (!lightsOn)
  {
    setDirection(0);
    setBrightness(0);
    return;
  }

  // Run the current light mode
  switch (currentMode)
  {
  case ALL_ON:
    allOn();
    break;
  case ALTERNATE_FLASH:
    alternateFlash();
    break;
  case FADE_ALL:
    fadeAll();
    break;
  case FADE_ALTERNATE:
    fadeAlternate();
    break;
  case TWINKLE:
    twinkle();
    break;
  case CHASE:
    chase();
    break;
  case METEOR:
    meteor();
    break;
  case MUSIC_SYNC:
    musicSync();
    break;
  default:
    allOn();
    break;
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.println("Booting...");

  connectToWiFi();
  setUpOverTheAirProgramming();

  configTime(TIME_ZONE, "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < SECS_YR_2000)
  {
    delay(100);
    now = time(nullptr);
  }
  setTime(now);

  TelnetStream.begin();

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(MODE_BUTTON, INPUT_PULLUP); // Set button pin as input with pull-up resistor

  // Initialize twinkle states
  for (int i = 0; i < 10; i++)
  {
    twinkleState[i] = random(0, 255);
  }

  // Setup MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024); // Increase buffer for discovery messages
  connectMQTT();

  // Setup HTTP server
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/mode", HTTP_POST, handleSetMode);
  server.on("/brightness", HTTP_POST, handleSetBrightness);
  server.on("/speed", HTTP_POST, handleSetSpeed);
  server.on("/state", HTTP_POST, handleSetState);
  server.begin();
  log("HTTP server started");

  log("Christmas Lights Controller Ready");
  printModeMenu();
}
