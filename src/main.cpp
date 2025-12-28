#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <TimeLib.h>
#include <TelnetStream.h>
#include <sntp.h>
#include <TZ.h>
#include "secrets.h"

// General Setup
#define TIME_ZONE TZ_Europe_London

// Pin Definitions
#define IN1_PIN D1     // Drives L298N (Light Set A) IN1
#define IN2_PIN D2     // Drives L298N (Light Set B) IN2
#define ENA_PIN D5     // PWM brightness control for L298N ENA
#define MODE_BUTTON D7 // Push button

// Wi-Fi connection parameters
const char *wifi_ssid = SECRET_SSID;
const char *wifi_password = SECRET_PASS;

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
  if (currentMillis - lastUpdate > 20)
  {
    lastUpdate = currentMillis;
    direction = -direction; // Flip between 1 and -1
    setDirection(direction);
    setBrightness(10);
  }
}

void alternateFlash()
{
  // Alternate between set A and set B
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate > 500)
  {
    lastUpdate = currentMillis;
    direction = -direction; // Flip between 1 and -1
    setDirection(direction);
    setBrightness(255);
  }
}

void fadeAll()
{
  // Fade both sets of lights up and down together
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate > 30)
  {
    lastUpdate = currentMillis;

    // Change brightness
    brightness = brightness + fadeAmount;

    // Reverse fade direction when limits are reached
    if (brightness <= 0 || brightness >= 255)
    {
      fadeAmount = -fadeAmount;
      brightness = constrain(brightness, 0, 255);

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
  if (currentMillis - lastUpdate > 30)
  {
    lastUpdate = currentMillis;

    // Change brightness
    brightness = brightness + fadeAmount;

    // Reverse fade direction when limits are reached
    if (brightness <= 0 || brightness >= 255)
    {
      fadeAmount = -fadeAmount;
      brightness = constrain(brightness, 0, 255);

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
  if (currentMillis - lastUpdate > 50)
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
    brightness = random(100, 255);

    setDirection(direction);
    setBrightness(brightness);
    delay(random(10, 50)); // Small random delay for twinkling effect
  }
}

void chase()
{
  // Light chasing effect
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate > 100)
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
    brightness = 100 + 155 * sin((PI * animationStep) / 5);

    setDirection(direction);
    setBrightness(brightness);
  }
}

void meteor()
{
  // Meteor shower effect
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate > 50)
  {
    lastUpdate = currentMillis;

    // Cycle through animation steps
    animationStep = (animationStep + 1) % 20;

    if (animationStep < 10)
    {
      // Meteor on set A
      direction = 1;
      brightness = (animationStep < 5) ? (animationStep * 50) : (255 - (animationStep - 5) * 50);
    }
    else
    {
      // Meteor on set B
      direction = -1;
      int step = animationStep - 10;
      brightness = (step < 5) ? (step * 50) : (255 - (step - 5) * 50);
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

  if (currentMillis - lastUpdate > 30)
  {
    lastUpdate = currentMillis;

    pulsePhase = (pulsePhase + 1) % 100;

    // Create a pulsing pattern
    if (pulsePhase < 50)
    {
      direction = 1;
      brightness = 100 + 155 * sin((PI * pulsePhase) / 50);
    }
    else
    {
      direction = -1;
      brightness = 100 + 155 * sin((PI * (pulsePhase - 50)) / 50);
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

void loop()
{
  ArduinoOTA.handle();

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

  log("Christmas Lights Controller Ready");
  printModeMenu();
}
