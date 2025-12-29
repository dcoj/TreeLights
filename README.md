# Christmas Tree Lights Controller

ESP8266-based Christmas lights controller with Home Assistant integration and REST API.

## Hardware

- ESP8266 (D1 Mini Pro)
- L298N Motor Driver
- Christmas lights with reverse polarity wiring
- Push button (connected to D7)

## Features

- 8 light modes (All On, Alternate Flash, Fade All, Fade Alternate, Twinkle, Chase, Meteor, Music Sync)
- Adjustable brightness (0-255)
- Adjustable animation speed (0.1x to 5.0x)
- Physical button control
- Telnet control interface
- REST API
- Home Assistant MQTT auto-discovery
- Over-the-air (OTA) updates

## Configuration

1. Copy `src/secrets.example.h` to `src/secrets.h`
2. Update with your WiFi credentials
3. In `main.cpp`, update the MQTT server settings:
   ```cpp
   const char *mqtt_server = "homeassistant.local"; // Your Home Assistant IP/hostname
   const int mqtt_port = 1883;
   const char *mqtt_user = "";     // MQTT username (if required)
   const char *mqtt_password = ""; // MQTT password (if required)
   ```

## REST API

The controller provides a web interface at `http://christmas-lights.local/` (or the IP address).

### Endpoints

- **GET /** - Web interface with API documentation
- **GET /status** - Get current status (JSON)
  ```json
  {
    "mode": 0,
    "mode_name": "All On",
    "brightness": 255,
    "speed": 1.0,
    "state": "on"
  }
  ```

- **POST /state?value=[on|off]** - Turn lights on/off
  ```bash
  curl -X POST "http://christmas-lights.local/state?value=on"
  ```

- **POST /mode?value=[0-7]** - Set light mode (0=All On, 1=Alternate Flash, etc.)
  ```bash
  curl -X POST "http://christmas-lights.local/mode?value=2"
  ```

- **POST /brightness?value=[0-255]** - Set maximum brightness
  ```bash
  curl -X POST "http://christmas-lights.local/brightness?value=128"
  ```

- **POST /speed?value=[0.1-5.0]** - Set animation speed multiplier
  ```bash
  curl -X POST "http://christmas-lights.local/speed?value=2.0"
  ```

## Home Assistant Integration

The controller automatically publishes MQTT discovery messages to Home Assistant. Once configured, three entities will appear:

1. **Light: Christmas Lights** - On/off control and brightness
2. **Select: Christmas Lights Mode** - Choose animation mode
3. **Number: Christmas Lights Speed** - Adjust animation speed (0.1x to 5.0x)

### MQTT Topics

State topics:
- `homeassistant/light/christmas_lights/state`
- `homeassistant/select/christmas_lights_mode/state`
- `homeassistant/number/christmas_lights_speed/state`

Command topics:
- `homeassistant/light/christmas_lights/set`
- `homeassistant/select/christmas_lights_mode/set`
- `homeassistant/number/christmas_lights_speed/set`

### Example Home Assistant Configuration

The devices will auto-discover, but you can also create automations:

```yaml
automation:
  - alias: "Christmas Lights Evening"
    trigger:
      platform: sun
      event: sunset
    action:
      - service: light.turn_on
        target:
          entity_id: light.christmas_lights
        data:
          brightness: 255
      - service: select.select_option
        target:
          entity_id: select.christmas_lights_mode
        data:
          option: "Fade All"
```

## Telnet Control

Connect via telnet to control the lights:

```bash
telnet christmas-lights.local
```

Commands:
- **R** - Reset controller
- **C** - Close telnet connection
- **M** - Cycle to next mode
- **?** - Show menu
- **1-8** - Select specific mode

## Light Modes

1. **All On** - Both sets rapidly alternate to appear all on
2. **Alternate Flash** - Alternate between set A and set B
3. **Fade All** - Smooth fade up and down
4. **Fade Alternate** - Fade while alternating between sets
5. **Twinkle** - Random twinkling effect
6. **Chase** - Light chasing pattern
7. **Meteor** - Meteor shower effect
8. **Music Sync** - Pulsing pattern (simulate music sync)

## Building and Uploading

```bash
# Build
pio run

# Upload via OTA (after first upload)
pio run -t upload

# Upload via USB
pio run -t upload --upload-port /dev/cu.usbserial-*
```

## Pin Configuration

- **D1** (GPIO5) - L298N IN1 (Set A)
- **D2** (GPIO4) - L298N IN2 (Set B)
- **D5** (GPIO14) - L298N ENA (PWM brightness)
- **D7** (GPIO13) - Mode button (pull-up)

## Troubleshooting

- If MQTT doesn't connect, verify your Home Assistant's MQTT broker is running
- Check the IP address in Serial Monitor or your router's DHCP table
- For OTA updates to work, ensure the ESP8266 is on the same network
- If animations are too fast/slow, adjust the speed multiplier via API or Home Assistant
