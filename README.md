# Smart Christmas Lights Controller

ESP8266-based Christmas lights controller with Home Assistant integration and REST API.

## Hardware

- ESP8266 (D1 Mini Pro)
- L298N Motor Driver
- Christmas lights with reverse polarity wiring (Check your existing PSU is 32v like this one: [Amazon](https://www.amazon.de/-/en/Pack-32V-3-6W-Power-Supply/dp/B0FKTHBZC2?crid=17AQG18E51KJC)
- Push button (connected to D2 to toggle setting like the original)

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

1. Copy `./src/secrets.example.h` to `./src/secrets.h` and update WIFI and MQTT options.
2. Copy `./config.sample.ini` to `./config.ini` and update chip connection options.

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

# Upload
pio run -t upload
```

## Pin Configuration

- **D5** - L298N IN1 (Set A)
- **D6** - L298N IN2 (Set B)
- **D7** - L298N ENA (PWM brightness)
- **D2** - Mode button (pull-up)

## Troubleshooting

- If MQTT doesn't connect, verify your Home Assistant's MQTT broker is running
- Check the IP address in Serial Monitor or your router's DHCP table
- For OTA updates to work, ensure the ESP8266 is on the same network.
- When running from USB power some Clone 8266s' have bad power managment components and can't reliably power the wifi chip.

## Schematic

![](./pcb/christmas.svg)

- L298N servo driver powers the LED string (~€2-5)
- 5V Step down is used for Driver and 8266 power (Don't connect to USB at the same time!)
- 100µF electrolytic capacitor (power supply filtering)
- 0.1µF ceramic capacitor (high freq filtering and smoothing of PWM)

- VCC should be around 30 - 32V
- A 0.1Ω 1W resistor could be added and connected to A1 for current sensing.
