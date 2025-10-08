# Installation Guide - Water Pressure Controller (WPC)

This guide will walk you through the complete installation process for the Water Pressure Controller project, including the crucial LittleFS filesystem setup.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Step 1: Clone the Repository](#step-1-clone-the-repository)
- [Step 2: Install Arduino IDE and ESP32 Support](#step-2-install-arduino-ide-and-esp32-support)
- [Step 3: Install Required Libraries](#step-3-install-required-libraries)
- [Step 4: Install LittleFS Upload Tool](#step-4-install-littlefs-upload-tool)
- [Step 5: Prepare Configuration Files](#step-5-prepare-configuration-files)
- [Step 6: Upload LittleFS Filesystem](#step-6-upload-littlefs-filesystem)
- [Step 7: Configure Your Settings](#step-7-configure-your-settings)
- [Step 8: Upload the Sketch](#step-8-upload-the-sketch)
- [Step 9: Verify Installation](#step-9-verify-installation)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

Before you begin, make sure you have:
- An ESP32 development board
- USB cable for programming
- WiFi network credentials
- MQTT broker (local or cloud-based)
- Computer with Windows, macOS, or Linux

---

## Step 1: Clone the Repository

Open your terminal or command prompt and run:

```bash
git clone https://github.com/JorgeS15/wpc.git
cd wpc
```

---

## Step 2: Install Arduino IDE and ESP32 Support

### Install Arduino IDE

1. Download Arduino IDE 2.x from [arduino.cc](https://www.arduino.cc/en/software)
2. Install the downloaded package for your operating system
3. Launch Arduino IDE

### Add ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences** (or **Arduino IDE → Settings** on macOS)
3. In the "Additional Board Manager URLs" field, add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click **OK**
5. Go to **Tools → Board → Board Manager**
6. Search for "**ESP32**"
7. Install "**ESP32 by Espressif Systems**" (latest version)
8. Wait for the installation to complete

---

## Step 3: Install Required Libraries

1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries** (or click the Library Manager icon in the sidebar)
3. Search for and install each of the following libraries:

   - **PubSubClient** (by Nick O'Leary) - for MQTT communication
   - **ArduinoJson** (by Benoit Blanchon) - for JSON parsing
   - Any additional libraries that appear as errors when you first try to compile

4. Wait for all installations to complete

---

## Step 4: Install LittleFS Upload Tool

LittleFS is used to store configuration files, web interfaces, and other data on the ESP32's flash memory.

### For Arduino IDE 2.x (Recommended)

#### Option A: Install via Extensions Manager

1. Open Arduino IDE 2.x
2. Click the **Extensions** icon in the left sidebar (puzzle piece icon)
3. Search for "**arduino-littlefs-upload**"
4. Click **Install**
5. Restart Arduino IDE if prompted

#### Option B: Manual Installation

1. Visit the [arduino-littlefs-upload releases page](https://github.com/earlephilhower/arduino-littlefs-upload/releases)
2. Download the latest release (`.vsix` file)
3. In Arduino IDE:
   - Press `Ctrl+Shift+P` (Windows/Linux) or `Cmd+Shift+P` (macOS)
   - Type "**Extensions: Install from VSIX**"
   - Select the downloaded `.vsix` file
4. Restart Arduino IDE

### For Arduino IDE 1.x (Legacy)

1. Download the ESP8266LittleFS tool from [this repository](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin)
2. Extract to your Arduino `tools` folder:
   - Windows: `C:\Users\[Username]\Documents\Arduino\tools\`
   - macOS: `~/Documents/Arduino/tools/`
   - Linux: `~/Arduino/tools/`
3. Restart Arduino IDE

---

## Step 5: Prepare Configuration Files

### Create or Verify the Data Folder

1. Navigate to your WPC sketch folder (where the `.ino` file is located)
2. Check if a `data/` folder exists
3. If it doesn't exist, create it manually

### Data Folder Structure

Your project should look like this:

```
wpc_code/
├── wpc_code.ino
├── data/
│   ├── app.js     
│   ├── index.html       
│   ├── style.css        
│   └── ...
└── other_files.cpp/h
```

---

## Step 6: Upload LittleFS Filesystem

**⚠️ IMPORTANT:** The LittleFS filesystem must be uploaded **BEFORE** or **SEPARATELY** from your sketch.

### Upload Process

1. **Connect your ESP32 board** via USB
2. **Select the correct board**:
   - Go to **Tools → Board**
   - Select your specific ESP32 board (e.g., "ESP32 Dev Module")
3. **Select the correct port**:
   - Go to **Tools → Port**
   - Select the COM port (Windows) or `/dev/ttyUSB*` (Linux) or `/dev/cu.usbserial-*` (macOS)
4. **Set partition scheme** (if available):
   - Go to **Tools → Partition Scheme**
   - Select a scheme with SPIFFS or appropriate storage (e.g., "Default 4MB with spiffs")

### Upload LittleFS

**For Arduino IDE 2.x:**

1. Press `Ctrl+Shift+P` (Windows/Linux) or `Cmd+Shift+P` (macOS)
2. Type "**Upload LittleFS**"
3. Select "**Upload LittleFS to Pico/ESP8266/ESP32**"
4. Wait for the upload to complete (may take several minutes depending on data size)
5. You should see "**LittleFS Image Uploaded**" in the output

**For Arduino IDE 1.x:**

1. Go to **Tools → ESP8266/ESP32 LittleFS Data Upload**
2. Wait for the upload to complete

### Verify Upload

Check the Arduino IDE output window for messages like:
```
LittleFS image created
LittleFS image uploaded
```

---

## Step 7: Configure Your Settings

Before uploading the main sketch, you may need to configure settings. This depends on how the project is set up:

### Option A: Configuration in Data Folder (Recommended)

If using a `config.json` or similar file in the `data/` folder:

1. Open `data/config.json` with a text editor
2. Update the following settings:

```json
{
  "wifi": {
    "ssid": "YourWiFiSSID",
    "password": "YourWiFiPassword"
  },
  "mqtt": {
    "broker": "192.168.1.100",
    "port": 1883,
    "username": "mqtt_user",
    "password": "mqtt_password",
    "topic": "home/water/pressure"
  },
  "sensors": {
    "pressure_calibration": 1.0,
    "temperature_offset": 0.0,
    "flow_sensor_factor": 4.5
  }
}
```

3. Save the file
4. **Re-upload the LittleFS filesystem** (repeat Step 6)

### Option B: Configuration in Code

If settings are hardcoded in the sketch:

1. Open the main `.ino` file
2. Look for configuration constants at the top (e.g., `WIFI_SSID`, `MQTT_BROKER`)
3. Update them with your values
4. Save the file

---

## Step 8: Upload the Sketch

Now that the filesystem is uploaded, upload the main program:

1. **Ensure your ESP32 is still connected**
2. **Close any open Serial Monitors** (important!)
3. Click the **Upload** button (→) in Arduino IDE, or press `Ctrl+U` / `Cmd+U`
4. Wait for compilation and upload to complete
5. You should see "**Done uploading**" when finished

---

## Step 9: Verify Installation

### Open Serial Monitor

1. Go to **Tools → Serial Monitor** or press `Ctrl+Shift+M` / `Cmd+Shift+M`
2. Set the baud rate to **115200** (or match what's in `Serial.begin()` in your code)
3. Press the **RESET** button on your ESP32

### Check for Proper Initialization

You should see output similar to:

```
Starting Water Pressure Controller...
LittleFS mounted successfully
Loading configuration...
Connecting to WiFi: YourSSID
WiFi connected! IP: 192.168.1.XXX
Connecting to MQTT broker...
MQTT connected!
System ready.
```

### Verify Functionality

- ✅ WiFi connection established
- ✅ MQTT connection successful
- ✅ Sensors initialized
- ✅ No error messages in Serial Monitor

---

## Troubleshooting

### LittleFS Upload Fails

**Problem:** "A fatal error occurred: Could not open port"

**Solutions:**
- Close all Serial Monitor windows
- Disconnect and reconnect the ESP32
- Try a different USB cable or port
- Check that no other program is using the COM port

---

**Problem:** "Error: Partition table does not include LittleFS"

**Solutions:**
- Go to **Tools → Partition Scheme**
- Select a partition scheme that includes SPIFFS/LittleFS storage
- Try "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"

---

**Problem:** "Upload failed, error code: 2"

**Solutions:**
- Make sure the correct board is selected in **Tools → Board**
- Ensure the correct port is selected in **Tools → Port**
- Try putting the ESP32 in flash mode manually:
  - Hold **BOOT** button
  - Press **RESET** button
  - Release **RESET** button
  - Release **BOOT** button
  - Try upload again

---

### Files Not Being Read

**Problem:** Code can't read files from LittleFS

**Solutions:**
1. Verify LittleFS was mounted successfully (check Serial Monitor)
2. Check file paths in code - they should start with `/`:
   ```cpp
   File file = LITTLEFS.open("/config.json", "r");
   ```
3. Verify files exist on the filesystem:
   ```cpp
   if (!LITTLEFS.exists("/config.json")) {
       Serial.println("Config file not found!");
   }
   ```
4. Ensure LittleFS is initialized in your code:
   ```cpp
   if (!LITTLEFS.begin()) {
       Serial.println("LittleFS mount failed");
   }
   ```

---

### WiFi Connection Issues

**Problem:** ESP32 can't connect to WiFi

**Solutions:**
- Double-check SSID and password (case-sensitive!)
- Ensure your WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
- Check if MAC address filtering is enabled on your router
- Try moving closer to the WiFi router
- Check Serial Monitor for specific error messages

---

### MQTT Connection Issues

**Problem:** Can't connect to MQTT broker

**Solutions:**
- Verify MQTT broker is running and accessible
- Check firewall settings on the broker
- Confirm broker IP address and port
- Test broker with an MQTT client tool (e.g., MQTT Explorer)
- Verify username/password if authentication is required
- Check Serial Monitor for connection error codes

---

### OTA Update Not Working

**Problem:** Over-The-Air updates fail

**Solutions:**
- Ensure ESP32 is connected to WiFi
- Check that OTA is properly initialized in code
- Verify the device is reachable on the network
- Try increasing the OTA partition size in partition scheme
- Make sure firewall isn't blocking OTA port

---

## Additional Resources

- **Project Repository:** https://github.com/JorgeS15/wpc
- **ESP32 Documentation:** https://docs.espressif.com/projects/arduino-esp32/
- **LittleFS Documentation:** https://github.com/littlefs-project/littlefs
- **PubSubClient (MQTT):** https://pubsubclient.knolleary.net/
- **Home Assistant MQTT:** https://www.home-assistant.io/integrations/mqtt/

---

## Next Steps

After successful installation:

1. **Configure Home Assistant** (if using) to subscribe to your MQTT topics
2. **Set up dashboards** for monitoring pressure, temperature, and flow
3. **Test leak detection** functionality in a safe environment
4. **Configure automation rules** for motor control
5. **Set up regular backups** of your configuration

---

## Support

If you encounter issues not covered in this guide:

1. Check the project's [Issues page](https://github.com/JorgeS15/wpc/issues)
2. Create a new issue with:
   - Your ESP32 board model
   - Arduino IDE version
   - Full Serial Monitor output
   - Steps to reproduce the problem

---

**Happy building! 🚰⚡**
