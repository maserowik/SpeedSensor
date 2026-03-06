# Speed Sensor System

## Table of Contents
1. [Purpose](#purpose)
2. [Scope](#scope)
3. [Project Structure](#project-structure)
4. [Prerequisites](#prerequisites)
5. [Hardware](#hardware)
   - [Sensor Specifications](#sensor-specifications)
   - [Voltage Notes](#voltage-notes)
6. [Wiring](#wiring)
   - [Sensor Power](#sensor-power)
   - [Signal Wires](#signal-wires)
   - [Wire Color Reference](#wire-color-reference)
   - [LCD Wiring](#lcd-wiring)
   - [Wemos D1 Mini Wiring](#wemos-d1-mini-wiring)
   - [Pin Summary](#pin-summary)
7. [Sensor Configuration](#sensor-configuration)
8. [Arduino IDE Setup](#arduino-ide-setup)
   - [Arduino Mega 2560](#arduino-mega-2560)
   - [Wemos D1 Mini](#wemos-d1-mini)
9. [Sketch Configuration](#sketch-configuration)
10. [Required Libraries](#required-libraries)
11. [Sensor Identification Test](#sensor-identification-test)
12. [WiFi Access Point](#wifi-access-point)
13. [Webpage](#webpage)
    - [Captive Portal](#captive-portal)
    - [Webpage Field Reference](#webpage-field-reference)
    - [Clear History Button](#clear-history-button)
14. [Serial Monitor Output](#serial-monitor-output)
    - [Startup Sequence](#startup-sequence)
    - [Measurement Output](#measurement-output)
    - [Warning Messages](#warning-messages)
15. [LCD Display Layout](#lcd-display-layout)
16. [How It Works](#how-it-works)
17. [Fault Tolerance](#fault-tolerance)
18. [Troubleshooting](#troubleshooting)
19. [Notes](#notes)
20. [Revision Control](#revision-control)

---

## Purpose

The purpose of this system is to provide an automated way to:
* Detect a moving object passing between two laser sensors
* Measure the elapsed time between sensor triggers
* Calculate and display speed in m/s, km/h, and mph
* Determine the direction of travel (Right to Left or Left to Right)
* Log all results to the Serial Monitor with timestamp and measurement counter
* Optionally display results on a 20x4 I2C LCD
* Broadcast results live to any browser (phone, tablet, laptop) via WiFi using a Wemos D1 Mini

---

## Scope

This system applies to any application requiring non-contact speed measurement using two fixed laser sensors separated by a known distance. The system runs on an **Arduino Mega 2560** with an optional **Wemos D1 Mini** for WiFi output, and is configured for **Keyence LR-TB5000C** sensors operating on a dedicated 24VDC power supply.

---

## Project Structure

The project consists of four files across three separate Arduino IDE sketch folders:

```
Documents/Arduino/
|
+-- Speed_Test/
|   +-- Speed_Test.ino        <- Main sketch -- upload to Arduino Mega 2560
|   +-- README.md             <- This file
|
+-- Wemos_D1_Mini/
|   +-- Wemos_D1_Mini.ino     <- WiFi bridge sketch -- upload to Wemos D1 Mini
|
+-- Sensor_ID_Test/
    +-- Sensor_ID_Test.ino    <- One-time test to identify Left/Right sensors
```

> **Important:** Each sketch folder must have the same name as the .ino file inside it. Arduino IDE will fail to compile if two unrelated .ino files are placed in the same folder.

> **Note:** The README.md is documentation only. Arduino IDE ignores .md files. It does not need to be uploaded anywhere.

---

## Prerequisites

### Hardware & Access
* Arduino Mega 2560
* 2x Keyence LR-TB5000C laser sensors
* Dedicated 24VDC power supply for sensors
* Wemos D1 Mini -- for WiFi phone display
* 20x4 I2C LCD display -- optional
* Connecting wires and M12 sensor cables

### Software
* Arduino IDE installed on PC
* USB cable connected to Arduino Mega
* Serial Monitor set to **115200 baud**
* ESP8266 board package installed in Arduino IDE (required for Wemos D1 Mini)

### Libraries
* `LiquidCrystal I2C` by Frank de Brabander -- install via Arduino IDE Library Manager
* `ArduinoJson` by Benoit Blanchon -- install via Arduino IDE Library Manager
* `Wire` -- built into Arduino IDE, no install needed
* `ESP8266WiFi` -- included automatically when ESP8266 board package is installed
* `ESP8266WebServer` -- included automatically when ESP8266 board package is installed
* `DNSServer` -- included automatically when ESP8266 board package is installed

---

## Hardware

### Sensor Specifications

| Spec | Value |
|------|-------|
| Model | LR-TB5000C |
| Quantity | 2 |
| Role | Right sensor on Pin 2 / Left sensor on Pin 3 |
| Laser Class | Class 2 |
| Detection Distance | 60mm to 5000mm |
| Power Supply | 24VDC dedicated supply |
| Output Type | NPN open collector |
| Output Voltage | 24VDC max, 50mA max |
| Fastest Response Time | 7ms |
| Connector | M12 |
| Output Logic | N.O. / N.C. selectable |

### Voltage Notes

The sensors run on a dedicated 24VDC power supply. Only the black signal wire connects to the Arduino. This is safe because:

* The sensors use NPN open collector output
* When idle the Arduino INPUT_PULLUP holds the signal pin at 5V -- the 24VDC supply never touches the Arduino pin
* When triggered the sensor pulls the black wire LOW toward GND
* Because all grounds are tied together (24VDC supply negative, Arduino GND, Wemos GND) the Arduino pin sees a clean 0V when triggered

> **Critical:** All grounds must be tied together -- 24VDC supply negative, Arduino GND, and Wemos GND must all share a common ground. Without this the signal will be unreliable or incorrect.

---

## Wiring

### Sensor Power

The sensors are powered by a dedicated 24VDC supply. The Arduino has its own separate power supply via USB or barrel jack.

```
24VDC Supply (+) ---- Both sensor Brown wires (VCC)
24VDC Supply (-) ---- Both sensor Blue wires  (GND)
24VDC Supply (-) ---- Arduino GND             <- Common ground -- essential
```

### Signal Wires

Only the black signal wire from each sensor connects to the Arduino. No logic level converter is required because all grounds are tied together and the NPN open collector output only ever pulls the signal to GND -- it never pushes 24V onto the Arduino pin.

```
Right Sensor Black wire ---- Arduino Pin 2
Left  Sensor Black wire ---- Arduino Pin 3
```

### Wire Color Reference

| Wire Color | Function |
|-----------|----------|
| Brown | Power + (24VDC) -- connects to 24VDC supply only |
| Blue | Power - (GND) -- connects to 24VDC supply and common ground |
| Black | Output 1 -- signal wire, connects to Arduino pin |
| White | Output 2 -- not used in this project |

### LCD Wiring

| LCD Pin | Arduino Mega Pin |
|---------|-----------------|
| VCC | 5V |
| GND | GND |
| SDA | Pin 20 |
| SCL | Pin 21 |

> **Note:** On the Arduino Mega, I2C uses pins 20 (SDA) and 21 (SCL). This is different from the Arduino Uno which uses A4/A5.

> **Note:** If the LCD does not appear at startup its I2C address may differ from 0x27. Run the I2C_Scanner sketch to find the correct address and update `LiquidCrystal_I2C lcd(0x27, 20, 4)` in the sketch.

### Wemos D1 Mini Wiring

| Wemos D1 Mini Pin | Arduino Mega Pin |
|-------------------|-----------------|
| RX | Pin 18 (TX1) |
| GND | GND (common ground) |
| 5V | 5V |

### Pin Summary

| Pin | Function |
|-----|----------|
| 2 | Right sensor black wire (signal) |
| 3 | Left sensor black wire (signal) |
| 18 (TX1) | Serial output to Wemos D1 Mini RX |
| 20 | LCD SDA |
| 21 | LCD SCL |

---

## Sensor Configuration

Configure each sensor using its front panel buttons before connecting to the Arduino:

| Setting | Required Value | Reason |
|---------|---------------|--------|
| Output Logic | N.O. (Normally Open) | Pin reads HIGH at idle, LOW when object detected |
| Response Time | Fastest (7ms) | Best timing accuracy for speed measurement |
| Output Mode | Standard | Simple threshold detection |

> **Important:** Both sensors must be configured identically. Mismatched settings will cause inconsistent timing and inaccurate speed readings.

> **Note:** If sensors are set to N.C. (Normally Closed) the logic is inverted. In that case change `FALLING` to `RISING` in the interrupt attachments in the sketch.

---

## Arduino IDE Setup

### Arduino Mega 2560

1. Open Arduino IDE
2. Open `Speed_Test/Speed_Test.ino`
3. Go to Tools -> Board -> Arduino AVR Boards -> **Arduino Mega or Mega 2560**
4. Go to Tools -> Port -> select the correct COM port
5. Click Upload

### Wemos D1 Mini

The Wemos D1 Mini requires the ESP8266 board package to be installed before it appears in the board list.

#### Install ESP8266 Board Package
1. Go to File -> Preferences
2. In Additional Boards Manager URLs add:
   ```
   http://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
3. Click OK
4. Go to Tools -> Board -> Boards Manager
5. Search `esp8266` and click Install
6. Wait for installation to complete

#### Upload Wemos Sketch
1. Open `Wemos_D1_Mini/Wemos_D1_Mini.ino`
2. Go to Tools -> Board -> ESP8266 Boards -> **LOLIN(Wemos) D1 R2 & mini**
3. Go to Tools -> Port -> select the Wemos COM port
4. Click Upload

> **Note:** The Wemos and the Mega use different COM ports. Make sure you select the correct port for whichever board you are currently uploading to.

---

## Sketch Configuration

### Speed_Test.ino (Arduino Mega)

```cpp
const float SENSOR_DISTANCE        = 1.0;       // Distance between sensors in meters
const unsigned long TIMEOUT_US     = 5000000UL; // 5 second timeout if only one sensor fires
const unsigned long MIN_DELTA_US   = 7000;      // 7ms -- LR-TB5000C minimum response time
const unsigned long MIN_TRIGGER_US = 3000;      // 3ms per-sensor debounce
```

> **Important:** `SENSOR_DISTANCE` must be measured accurately. Even a few centimeters of error will affect speed readings, especially at close sensor spacing.

### Wemos_D1_Mini.ino

```cpp
const char* AP_SSID = "SpeedSensor";  // WiFi network name
const char* AP_PASS = "speedsensor";  // WiFi password
#define MAX_HISTORY 20                // Number of readings to store in history
```

---

## Required Libraries

Install via Arduino IDE -> Tools -> Manage Libraries:

| Library | Author | Install Name | Required For |
|---------|--------|-------------|-------------|
| LiquidCrystal I2C | Frank de Brabander | `LiquidCrystal I2C` | LCD display |
| ArduinoJson | Benoit Blanchon | `ArduinoJson` | Wemos JSON data endpoint |
| Wire | Arduino | Built-in | LCD display |
| ESP8266WiFi | ESP8266 Community | Included with ESP8266 board package | Wemos WiFi |
| ESP8266WebServer | ESP8266 Community | Included with ESP8266 board package | Wemos webpage |

---

## Sensor Identification Test

Before running the main sketch it is important to confirm which physical sensor is wired to Pin 2 (Right) and which is wired to Pin 3 (Left). Use the `Sensor_ID_Test.ino` sketch for this.

### How to Run

1. Open `Sensor_ID_Test/Sensor_ID_Test.ino`
2. Select board: Arduino Mega 2560
3. Upload to the Mega
4. Open Serial Monitor at **115200 baud**
5. Block the physically **left sensor** with your hand and watch the output

### Expected Output

```
=================================
  Sensor Identification Test    
  Keyence LR-TB5000C x2         
=================================
Block each sensor one at a time
and watch which pin fires below.
=================================
>>> Pin 3 triggered (currently labeled LEFT)
```

### Interpreting Results

| Serial Monitor Shows | Meaning | Action Required |
|---------------------|---------|-----------------|
| Pin 3 triggered when blocking left sensor | Wiring is correct | None -- upload Speed_Test.ino |
| Pin 2 triggered when blocking left sensor | Left and Right are swapped | Swap pin numbers in Speed_Test.ino |

### Fixing a Swap

If left and right are swapped change these two lines at the top of `Speed_Test.ino`:

```cpp
const int sensorPin1 = 3;  // Right sensor -- swapped
const int sensorPin2 = 2;  // Left sensor  -- swapped
```

Then re-upload `Speed_Test.ino` to the Mega.

---

## WiFi Access Point

The Wemos D1 Mini creates its own WiFi hotspot. No router or existing network is required.

| Setting | Value |
|---------|-------|
| WiFi Network Name | SpeedSensor |
| WiFi Password | speedsensor |
| Browser Address | 4.3.2.1 |

### How to Connect

1. On your device go to WiFi Settings
2. Connect to `SpeedSensor`
3. Enter password `speedsensor`
4. The browser will open automatically via the captive portal
5. If the browser does not open automatically go to `4.3.2.1` manually

> **Note:** The captive portal works the same way as hotel or airport WiFi login pages. Android, iOS, and Windows all detect a new network without internet and automatically redirect to the device page.

---

## Webpage

The Wemos D1 Mini serves a live dashboard at `4.3.2.1`. The page loads once and data updates automatically without reloading or flickering. It uses three endpoints:

| Endpoint | Purpose |
|----------|---------|
| / | Full HTML page -- loads once only |
| /data | JSON data -- polled by JavaScript every second to update readings |
| /clear | Resets the reading history on the Wemos -- called by the Clear button |

### Captive Portal

A captive portal is the mechanism that automatically opens a browser page when you connect to a WiFi network. You have seen it at hotels, airports, and coffee shops — when you connect and a login page appears without you doing anything. The SpeedSensor hotspot works the same way.

There are two pieces working together:

**1. DNS Server**

Normally when a device connects to WiFi it asks a DNS server to translate domain names like `google.com` into IP addresses. The Wemos has no real internet connection so instead it runs its own DNS server that answers every single domain lookup — no matter what domain is asked — with `4.3.2.1`. So when a phone asks "where is google.com?" the Wemos replies "it's at 4.3.2.1".

**2. OS Probe Interception**

Every operating system has a built-in internet check. Right after connecting to any WiFi network the OS quietly sends a test request to a known server expecting a very specific response. If the response comes back wrong the OS knows there is no real internet and automatically opens the browser to the redirected address.

| Operating System | Probe URL | Expected Response |
|-----------------|-----------|-------------------|
| Android | /generate_204 | Empty 204 response |
| iOS / macOS | /hotspot-detect.html | Specific Apple content |
| Windows | /ncsi.txt | "Microsoft NCSI" |

The Wemos intercepts all of these requests and sends a redirect to `http://4.3.2.1/` instead of the expected response. The OS sees the wrong response, recognizes there is no real internet, and automatically opens the browser to the redirected address — which is the Speed Sensor page.

In plain terms: the Wemos pretends to be the entire internet, intercepts the OS internet test, and redirects the browser to itself. The OS thinks it needs to log in to get internet access — exactly like hotel WiFi — and opens the browser automatically.

> **Note:** Captive portal behavior varies slightly by device and OS version. If the browser does not open automatically, navigate to `4.3.2.1` manually in any browser.

### Webpage Field Reference

The following numbered fields appear on the webpage and correspond to the GUI annotation reference image.

| Field # | Field Name | Description |
|---------|-----------|-------------|
| 1 | Page Title & Sensor Model | System name and hardware identifiers shown at the top of the page. |
| 2 | WiFi Connection Info | The SSID and IP address displayed for quick reference. |
| 3 | Status Bar | Green & pulsing = system armed and receiving data. Orange = waiting for Arduino connection. Shows last speed reading when armed. |
| 4 | Latest Reading Label | Section header for the most recent measurement card. |
| 5 | Speed Display | Live speed shown simultaneously in m/s, km/h, and mph. m/s is the raw calculated value. km/h and mph are converted from m/s. |
| 6 | Direction of Travel | Which sensor triggered first. Green arrow = Right, Blue arrow = Left. Shows dashes until first reading arrives. |
| 7 | Clear Button | Resets the reading history stored on the Wemos. History clears within 1 second. Does not affect the Arduino or any Serial output. History is also cleared automatically on Wemos power cycle. |
| 8 | Reading History Table | Log of past readings in order, newest at top. Columns: Reading #, m/s, km/h, mph, Direction. The most recent row is highlighted in blue. Oldest reading is dropped when the table is full. |

### Clear History Button

The **✕ CLEAR** button appears in the top-right corner of the Reading History section. Clicking it sends a request to the `/clear` endpoint on the Wemos which resets `historyCount` and `historyHead` to zero. The table clears within 1 second on the next `/data` poll. This only clears the history stored in the Wemos -- it has no effect on the Arduino, the Serial Monitor output, or the measurement counter.

---

## Serial Monitor Output

Set the Serial Monitor baud rate to **115200** using the dropdown at the bottom right of the Serial Monitor window.

### Startup Sequence

#### No LCD Connected
```
=================================
  Speed Sensor System Starting  
  Keyence LR-TB5000C x2         
=================================
Initializing...
Checking for LCD...No LCD found - Serial only
Warming up sensors...
Checking sensor states...
Sensor states OK (both HIGH at idle)
Sensors ready! Waiting for object...
=================================
```

#### LCD Connected
```
=================================
  Speed Sensor System Starting  
  Keyence LR-TB5000C x2         
=================================
Initializing...
Checking for LCD...LCD detected OK
Warming up sensors...
Checking sensor states...
Sensor states OK (both HIGH at idle)
Sensors ready! Waiting for object...
=================================
```

### Measurement Output

After the startup sequence the Serial Monitor is silent until an object passes through the beams:

```
[00:04]  [#1]  Speed: 2.347 m/s  |  8.45 km/h  |  5.25 mph  |  Direction: Right -> Left
[00:09]  [#2]  Speed: 2.401 m/s  |  8.64 km/h  |  5.37 mph  |  Direction: Right -> Left
[01:32]  [#3]  Speed: 1.893 m/s  |  6.81 km/h  |  4.23 mph  |  Direction: Left -> Right
```

### Warning Messages

```
[WARNING] Right sensor (LR-TB5000C) reading LOW at idle!
          Check 24VDC power supply and common ground wiring.

[WARNING] Left sensor (LR-TB5000C) reading LOW at idle!
          Check 24VDC power supply and common ground wiring.

[Timeout] Measurement reset -- only one sensor triggered.
```

---

## LCD Display Layout

> **Note:** LCD is currently on hold pending replacement of faulty unit. The Wemos webpage serves as the primary display. LCD code remains in Speed_Test.ino and will activate automatically when a working LCD is connected.

### Boot Screen
```
+--------------------+
|  Speed Sensor Sys  |
|  Keyence LR-T x2   |
|  Warming up ...    |
|                    |
+--------------------+
```

### Ready Screen
```
+--------------------+
|  ** SYSTEM READY **|
|  Waiting for       |
|  object...         |
|                    |
+--------------------+
```

### Measurement Result Screen
```
+--------------------+
|Spd:2.35m/s  8.5k   |
|mph:5.25  R->L      |
|Measurement #:4     |
|Time: 00:32         |
+--------------------+
```

### Timeout Screen
```
+--------------------+
|[Timeout] Reset     |
|One sensor only     |
|Waiting for object  |
|                    |
+--------------------+
```

---

## How It Works

1. **Startup** -- Arduino Mega waits 500ms for the Serial Monitor to connect then immediately prints the startup banner to both Serial and Serial1 (Wemos).

2. **LCD Detection** -- I2C is initialized with a 3ms timeout. If no LCD is found the sketch continues instantly without hanging.

3. **Warm-up** -- A 3-second countdown runs before interrupts are armed. This allows the 24VDC supply to stabilize and the laser beams to settle, preventing false triggers at startup.

4. **Sensor Check** -- Both sensor pins are read before interrupts attach. If either reads LOW at idle a warning is printed.

5. **Detection** -- When an object breaks the first laser beam the NPN output pulls the black wire LOW triggering a falling-edge interrupt which starts a microsecond timer. When the second beam breaks the timer stops and speed is calculated from the known distance divided by elapsed time.

6. **Debounce** -- Each sensor has its own 3ms debounce timer. The overall minimum delta of 7ms matches the LR-TB5000C minimum response time.

7. **Output** -- Direction is determined by which sensor fired first. All Serial, Serial1, and LCD output happens in `loop()` -- never inside the interrupt handler.

8. **WiFi** -- The Mega sends every output line to Serial1 (pin 18). The Wemos D1 Mini receives these lines and parses speed and direction data. A captive portal automatically opens the browser when a device connects to the SpeedSensor hotspot. The page at `4.3.2.1` loads once and JavaScript polls the `/data` endpoint every second to update readings live without reloading the page. A `/clear` endpoint allows the reading history to be reset from the browser.

9. **Timeout** -- If only one sensor fires and 5 seconds pass with no second trigger the system resets automatically.

---

## Fault Tolerance

The Wemos D1 Mini failure cannot affect the Mega in any way.

| Failure Scenario | Effect on Mega | Effect on LCD | Effect on Phone |
|-----------------|---------------|--------------|----------------|
| Wemos powered off | None | None | Page unavailable |
| Wemos crashed | None | None | Page unavailable |
| Wemos not installed | None | None | No WiFi hotspot |
| Phone disconnects | None | None | Reconnect and refresh |
| LCD not connected | None | No display | No effect |

The Mega transmits to Serial1 using fire-and-forget. It never waits for a reply and Serial1 transmission cannot block, delay, or crash the Mega under any circumstances.

---

## Troubleshooting

### Startup Issues

#### Nothing Appears on Serial Monitor
- Verify Serial Monitor baud rate is set to **115200**
- Press the reset button on the Arduino while Serial Monitor is open
- Verify the correct COM port is selected in Arduino IDE

#### Stops at "Checking for LCD..."
- An older version of the sketch without `Wire.setWireTimeout` may be loaded
- Upload the latest version of the sketch

#### Garbled Text on Serial Monitor
- Baud rate mismatch -- set Serial Monitor to **115200**

### Sensor Issues

#### WARNING -- Sensor Reading LOW at Idle
- Sensor may be set to N.C. (Normally Closed) -- change to N.O. on sensor front panel
- Check 24VDC power supply is powered on
- Check that 24VDC supply negative is connected to Arduino GND

#### No Speed Readings After Sensors Connected
- Confirm black signal wire is connected to Arduino pins 2 and 3
- Check sensor output logic is set to N.O.
- Run Sensor_ID_Test.ino to confirm sensors are triggering correctly

#### Arduino Resets or Behaves Erratically
- Check common ground connection between 24VDC supply negative and Arduino GND

### Measurement Issues

#### Timeout Messages Only
- Object is not reaching the second sensor
- Check sensor alignment -- both beams must be in the object path

#### Wrong Direction Reported
- Left and right sensors may be swapped -- run Sensor_ID_Test.ino to verify
- Swap pin numbers in Speed_Test.ino if needed

#### Inaccurate Speed Readings
- Measure `SENSOR_DISTANCE` precisely in meters and update the sketch
- Verify both sensors are set to the same response time (7ms fastest)

### LCD Issues

#### LCD Not Detected at Startup
- LCD may be faulty -- run I2C_Scanner.ino to test
- Run I2C scanner to verify correct I2C address
- Update `LiquidCrystal_I2C lcd(0x27, 20, 4)` with correct address if needed

### WiFi / Wemos Issues

#### SpeedSensor WiFi Network Not Appearing
- Verify Wemos D1 Mini is powered
- Verify `Wemos_D1_Mini.ino` was uploaded successfully
- Verify correct board (LOLIN Wemos D1 R2 & mini) was selected during upload

#### Browser Does Not Open Automatically After Connecting
- Captive portal behavior varies by device -- wait a few seconds after connecting
- Disable mobile data on the device to prevent it switching back to cellular
- If it still does not open, navigate to `4.3.2.1` manually in any browser

#### Page Not Loading at 4.3.2.1
- Confirm the device is connected to SpeedSensor WiFi and not your regular network
- Some phones switch back to cellular -- disable mobile data temporarily

#### Clear Button Does Not Clear History
- Check the device is still connected to the SpeedSensor hotspot
- Refresh the page and try again -- the table clears within 1 second of clicking

#### Page Shows "Waiting for Arduino..."
- Check the wire between Mega pin 18 (TX1) and Wemos RX
- Check common GND between Mega and Wemos
- Verify Speed_Test.ino is running on the Mega

#### Wemos Compilation Error -- ArduinoJson.h Not Found
- ArduinoJson library not installed
- Go to Arduino IDE -> Tools -> Manage Libraries
- Search `ArduinoJson` by Benoit Blanchon and install it

#### Wemos Board Not Showing in Arduino IDE
- ESP8266 board package not installed -- see Arduino IDE Setup section

---

## Notes

* The sketch operates fully with or without the LCD or Wemos connected.
* Sensors run on their own dedicated 24VDC supply. Only the black signal wire connects to the Arduino.
* No logic level converter is required because the NPN open collector output only pulls the signal to GND and all grounds are tied together.
* `micros()` rollover occurs approximately every 70 minutes. This is handled safely using unsigned long subtraction and does not affect operation.
* All Serial output is kept out of the interrupt service routine for stability.
* The measurement counter resets on power cycle or Arduino reset.
* Silence on the Serial Monitor after the ready message is correct -- it means the system is armed and waiting.
* The Wemos stores a rolling history of readings in memory. These are lost if the Wemos is power cycled or if the Clear button is used.
* The webpage at 4.3.2.1 loads once and updates live without reloading. No flicker.
* The captive portal opens the browser automatically on Android, iOS, macOS, and Windows when connecting to the SpeedSensor hotspot. If it does not trigger automatically navigate to 4.3.2.1 manually.
* The reading history table shows Reading #, m/s, km/h, mph, and Direction only. Timestamp is not shown on the webpage -- it is available on the Serial Monitor output only.

---

## Revision Control

All changes to this documentation must be reviewed and approved prior to release.

**Last Updated:** March 2026
**Version:** 2.4

---

## License

Free to use and modify for personal and educational projects.
