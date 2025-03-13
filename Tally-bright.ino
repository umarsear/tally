/*
 * TALLY Bright Firmware Version 1.1
 * 
 * This firmware implements a tally light system for video production,
 * supporting preview and program modes with WiFi configuration capabilities.
 * It follows the Osee GoStream protocol for communication.
 */

#include <WebServer.h>
#include <NetWizard.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <Wifi.h>
#include "ESP32_WS2812_Lib.h"

#define LEDS_COUNT 8
#define LEDS_PIN   4
#define CHANNEL    0

/* Configuration structure to store device settings in EEPROM  */
struct Config {
    char mixer_host[40];           /* IP address of the video mixer */
    char device_name[40];          /* Friendly name for this tally device */
    int mixer_port;                /* Network port for mixer communication */
    int device_id;                 /* Unique identifier for this tally device */
    int device_mode;               /* 0 - WiFi Only 1 - GoStream 2 - Atem mini (not implemented) */
    int device_brightness;         /* LED brightness */
};

/* Global variables */
Config config;                      /* Device configuration */
WiFiClient client;                  /* WiFi client instance */
WebServer server(80);               /* Web server for configuration interface */

/* Tally state tracking variables  */
int super_source_1;                 /* First super source index */
int super_source_2;                 /* Second super source index */
int pgmIndex=-1;                    /* Program index */
int pvwIndex=-1;                    /* Preview index */
int super_source_index=5;           /* the super source input of the mixer - 5 for OSEE goStream */

/* Status flags */
bool camera_live = false;           /* Camera is currently live */
bool camera_preview = false;        /* Camera is in preview */
bool WiFi_connected = false;        /* WiFi connection status */
char device_state[5]="OFF";         /* Device status String */
String input_buffer = "";           /* Buffer for incoming network data */

/* Initialize NetWizard for WiFi configuration */
NetWizard NW(&server);

ESP32_WS2812 strip = ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);

/* Constants  */
const int CONFIG_VERSION = 1;
const char app_name[20] = "Tally Bright";
const char app_version[5] = "1.1";
const int EEPROM_SIZE = 512;

/* CRC16 lookup table for GoStream protocol  */
const uint16_t crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

/* Setup configuration parameters for NetWizard interface  */
NetWizardParameter nw_header(&NW, NW_HEADER, "Mixer Configuration");
NetWizardParameter nw_divider1(&NW, NW_DIVIDER);
NetWizardParameter nw_mixer_host(&NW, NW_INPUT, "Mixer host", "", "192.168.1.1");
NetWizardParameter nw_mixer_port(&NW, NW_INPUT, "Mixer port", "19010", "19010");
NetWizardParameter nw_device_name(&NW, NW_INPUT, "Device name", "", "Camera 1");
NetWizardParameter nw_device_id(&NW, NW_INPUT, "Device id", "", "1");
NetWizardParameter nw_device_mode(&NW, NW_INPUT, "Device mode", "1", "1");
NetWizardParameter nw_device_brightness(&NW, NW_INPUT, "Brightness", "50", "50");

void showRing(byte r,byte g, byte b, byte brightness){
  strip.setAllLedsColor(r, g, b);
  strip.setBrightness(brightness);
  strip.show();
  Serial.printf("Red %d Green %d Blue %d Brightness %d\n",r,g,b,brightness);
}

/**
 * Initialize the EEPROM to factory default
*/
void eraseConfig() {
  EEPROM.begin(EEPROM_SIZE); 
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0); 
  }
  EEPROM.commit(); 
}

/**
 * Saves the current configuration to EEPROM
*/

void saveConfig() {
    if (config.device_brightness<10) {
      config.device_brightness=10;
    } else if (config.device_brightness>255) {
      config.device_brightness=255;
    }
    Serial.printf("Saving Config\n Mixer address %s:%d\n Device name:%s, ID:%d, mode:%d brightness:%d",config.mixer_host,config.mixer_port,config.device_name,config.device_id,config.device_mode,config.device_brightness);
    
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, CONFIG_VERSION);
    EEPROM.put(sizeof(int), config);
    EEPROM.commit();
    EEPROM.end();
}

/**
 * Loads configuration from EEPROM
 * @return true if configuration was loaded successfully, false otherwise
 */

bool loadConfig() {
    EEPROM.begin(EEPROM_SIZE);
    int version;
    EEPROM.get(0, version);
    
    if (version == CONFIG_VERSION) {
        EEPROM.get(sizeof(int), config);
        EEPROM.end();
        if (config.device_brightness<10) {
          config.device_brightness=10;
        }
        return true;
    }
    EEPROM.end();
    return false;
}

void defaultConfig(){
  /* Set default configuration if none exists  */
  strcpy(config.mixer_host, "192.168.1.100");
  strcpy(config.device_name, "CAMERA 1");
  config.mixer_port = 19010;
  config.device_id = 0;
  config.device_mode = 1;
  config.device_brightness=50;
}

void configWizard(){
  nw_mixer_port.setValue(String(config.mixer_port));
  nw_mixer_host.setValue(config.mixer_host);
  nw_device_id.setValue(String(config.device_id+1));
  nw_device_mode.setValue(String(config.device_mode));
  nw_device_brightness.setValue(String(config.device_brightness));
  nw_device_name.setValue(config.device_name);
}
void printConfig() {
  Serial.println("Loaded configuration");
  Serial.printf("Local IP: %s\n", NW.localIP().toString().c_str());
  Serial.printf("Gateway IP: %s\n", NW.gatewayIP().toString().c_str());
  Serial.printf("Subnet mask: %s\n", NW.subnetMask().toString().c_str());
  Serial.printf("Server host: %s\n", config.mixer_host);
  Serial.printf("Device name: %s\n", config.device_name);
  Serial.printf("Device ID: %d\n", config.device_id);
  Serial.printf("Device mode: %d\n", config.device_mode);
  Serial.printf("Device brightness: %d\n", config.device_brightness);
}

/**
 * Calculate CRC16 Modbus checksum
 * @param data Pointer to data buffer
 * @param length Length of data
 * @return Calculated CRC16 value
 */
uint16_t calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc16_table[index];
    }
    return crc;
}

/**
 * Send message with CRC according to Osee GoStream protocol
 * Protocol structure:
 * - header: U16 => 0xA6 0xEB
 * - protoid: U8 => currently 0
 * - length: U16 => Total length of data to follow, including crc
 * - command: GoStreamCommand
 * - crc: U16
 */
void sendGoStreamPacket(const String& message) {
    size_t messageLength = message.length();
    uint16_t totalLength = messageLength + 2;  /* Add 2 for CRC bytes */
    /* Create buffer for complete packet */
    uint8_t* packet = new uint8_t[3 + 2 + messageLength + 2];
    size_t packetIndex = 0;
    /* Add header bytes */
    packet[packetIndex++] = 0xEB;  /* header 16 bit low byte */
    packet[packetIndex++] = 0xA6;  /* header 16 bit high byte */
    packet[packetIndex++] = 0x00;  /* protocol id - 0 */
    /* Add length bytes (little-endian) */
    packet[packetIndex++] = totalLength & 0xFF;
    packet[packetIndex++] = (totalLength >> 8) & 0xFF;
    /* Add message content */
    memcpy(packet + packetIndex, message.c_str(), messageLength);
    packetIndex += messageLength;
    /* Calculate CRC on the entire packet up to this point (excluding CRC bytes) */
    uint16_t crc = calculateCRC16(packet, packetIndex);
    /* Add CRC bytes (little-endian) */
    packet[packetIndex++] = crc & 0xFF;
    packet[packetIndex++] = (crc >> 8) & 0xFF;     
    client.write(packet, packetIndex);
    delete[] packet;                            /* Clean up */
}

/*
  Request status update for a specific parameter
 */
 
void getStatus(String id) {
    JsonDocument doc;
    String JsonString;
    doc["id"] = id;
    doc["type"] = "get";
    serializeJson(doc, JsonString);
    sendGoStreamPacket(JsonString);
}

/*
  Initializes tally status variables to default states
*/

void initializeStatus() {
    showRing(255,255,255,10);
    super_source_1 = -1;
    super_source_2 = -1;
    pgmIndex = -1;
    pvwIndex = -1;
    getStatus("pvwIndex");
    getStatus("pgmIndex");
    getStatus("superSourceSource1");
    getStatus("superSourceSource2");
}

void showStatus() {
  bool device_in_super_source=(super_source_1 == config.device_id || super_source_2 == config.device_id); /* check if the device is in super source view */
  camera_live=false;
  camera_preview=false;
  if (device_in_super_source) {
    Serial.printf("Device is in super source view\n");
  }
  /* first check if the device is in program mode */
  if ((pgmIndex==config.device_id) || (pgmIndex==super_source_index & device_in_super_source)) {
    strcpy(device_state,"LIVE");
    camera_live=true;
    showRing(255,0,0,config.device_brightness);
  /* if device is not in program mode, check if it is in preview mode - program mode overides preview mode */
    } else if ((pvwIndex==config.device_id) || (pvwIndex==super_source_index & device_in_super_source)) {
    strcpy(device_state,"PRVW");
    camera_preview=true;
    showRing(0,255,0,config.device_brightness);
  }

  /* if camera is neither in program mode, nor in preview mode, we set the status to off and turn of the LED ring */
  if (!camera_live & !camera_preview) {
    showRing(0,0,0,0);
    strcpy(device_state,"OFF");
  }
}
/**
 * Handles parsed JSON data and updates tally state
 */
void handleJsonData(const char* jsonString) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    bool statusChanged = false;
    byte value;

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }
    if (doc.containsKey("id")) {
        const char* command = doc["id"];
        Serial.print("Received command: ");
        Serial.println(command);
        /* Handle various command types */
        if (strcmp(command, "superSourceSource1") == 0) {
            value = doc["value"][0];
            if (super_source_1 != value) {
                super_source_1 = value;
                statusChanged = true;
            }
        }
        if (strcmp(command, "superSourceSource2") == 0) {
            value = doc["value"][0];
            if (super_source_2 != value) {
                super_source_2 = value;
                statusChanged = true;
            }
        }
        if (strcmp(command, "pvwIndex") == 0) {
            value = doc["value"][0];
            if (pvwIndex != value) {
                pvwIndex = value;
                statusChanged = true;
            }
        }
        if (strcmp(command, "pgmIndex") == 0) {
            value = doc["value"][0];
            if (pgmIndex != value) {
                pgmIndex = value;
                statusChanged = true;
            }
        }
    }
    if (statusChanged) {
        showStatus();
    }
}
/*
 * Processes incoming JSON data from the buffer
 * Handles message framing and extraction
 */
void processJsonBuffer(String& buffer) {
    while (buffer.length() > 2) {
        /* Search for start of message (null byte + '{')  */
        int startPos = -1;
        for (size_t i = 0; i < buffer.length() - 1; i++) {
            if ((uint8_t)buffer[i] == 0x00 && buffer[i + 1] == '{') {
                startPos = i;
                break;
            }
        }
        if (startPos == -1) {
            /* No valid start sequence found, preserve last byte */
            if (buffer.length() > 1) {
                buffer = buffer.substring(buffer.length() - 1);
            }
            return;
        }
        /* Look for message end ('}' followed by '\n') */
        int endPos = -1;
        for (size_t i = startPos + 2; i < buffer.length(); i++) {
            if ((buffer[i] == '}') && (buffer[i + 1] == '\n')) {
                endPos = i + 1;
                break;
            }
        }
        if (endPos == -1) {
            /* Incomplete message, wait for more data */
            return;
        }
        /* Extract and process JSON message */
        String jsonStr = buffer.substring(startPos + 1, endPos);
        handleJsonData(jsonStr.c_str());
        /* Remove processed data from buffer */
        buffer = buffer.substring(endPos + 1);
    }
    /* Buffer overflow protection*/
    if (buffer.length() > 8192) {
        Serial.println("Buffer overflow, clearing...");
        buffer = "";
    }
}

/*
  Device power up or restart initialization
 */

void setup(void) {
    Serial.begin(115200);
    Serial.printf("Starting %s version %d",app_name,app_version);
    /* LED ring initialization - White at 20% brightness */
    strip.begin();
    showRing(255,255,255,50);  
    /* load configuration from EEPROM, revert to default configuration on failure */
    if (!loadConfig()) {
      defaultConfig();
    }
    /* Configure the setup wizard and show the device configuration over the serial port  */
    configWizard();
    printConfig();
    /* set the netwizard to blocking, configure the callback routines. */
    Serial.printf("Starting netWizard\n");
    NW.setStrategy(NetWizardStrategy::BLOCKING);

    /* network connection status callback */  
    NW.onConnectionStatus([](NetWizardConnectionStatus status) {
        String status_str = "";
        switch (status) {
            case NetWizardConnectionStatus::DISCONNECTED:
                status_str = "Disconnected";
                WiFi_connected = false;
                break;
            case NetWizardConnectionStatus::CONNECTING:
                status_str = "Connecting";
                WiFi_connected = false;
                break;
            case NetWizardConnectionStatus::CONNECTED:
                WiFi_connected = true;
                status_str = "Connected";
                break;
            case NetWizardConnectionStatus::CONNECTION_FAILED:
                status_str = "Connection Failed";
                WiFi_connected = false;
                break;
            case NetWizardConnectionStatus::CONNECTION_LOST:
                status_str = "Connection Lost";
                WiFi_connected = false;
                break;
            case NetWizardConnectionStatus::NOT_FOUND:
                status_str = "Not Found";
                WiFi_connected = false;
                break;
            default:
                status_str = "Unknown";
                WiFi_connected = false;
        }
        Serial.printf("NW connection status changed: %s\n", status_str);
        if (status == NetWizardConnectionStatus::CONNECTED) {
            Serial.printf("Local IP: %s\n", NW.localIP().toString().c_str());
            Serial.printf("Gateway IP: %s\n", NW.gatewayIP().toString().c_str());
            Serial.printf("Subnet mask: %s\n", NW.subnetMask().toString().c_str());
        }
    });
  
    /* portal status change callback */  
    NW.onPortalState([](NetWizardPortalState state) {
        String state_str = "";
        switch (state) {
            case NetWizardPortalState::IDLE:
                state_str = "Idle";
                break;
            case NetWizardPortalState::CONNECTING_WIFI:
                state_str = "Connecting to WiFi";
                break;
            case NetWizardPortalState::WAITING_FOR_CONNECTION:
                state_str = "Waiting for Connection";
                break;
            case NetWizardPortalState::SUCCESS:
                state_str = "Success";
                break;
            case NetWizardPortalState::FAILED:
                state_str = "Failed";
                break;
            case NetWizardPortalState::TIMEOUT:
                state_str = "Timeout";
                break;
            default:
                state_str = "Unknown";
        }
        Serial.printf("NW portal state changed: %s\n", state_str);
    });

    /* network wizard configuration callback - we configure and save the application configuration to EEPROM*/  
    NW.onConfig([&]() {
        Serial.println("NW onConfig Received");
        String mixer_host = nw_mixer_host.getValueStr();
        if (mixer_host.length() <= 40) {
            mixer_host.toCharArray(config.mixer_host, mixer_host.length() + 1);
        } else {
            mixer_host.toCharArray(config.mixer_host, 41);
        }
        /* take the configuration from the captive portal and save store them in the EEPROM */
        config.mixer_port = nw_mixer_port.getValue().toInt();
        config.device_id = nw_device_id.getValue().toInt()-1;
        config.device_mode = nw_device_mode.getValue().toInt();
        config.device_brightness = nw_device_brightness.getValue().toInt();
        saveConfig(); /* save config to EEPROM */
        return true;
    });
     /* Start NetWizard with default AP credentials */
    NW.autoConnect("Tally Bright Setup", "");
    /* Check configuration status */
    if (NW.isConfigured()) {
        showRing(0,0,0,config.device_brightness);
        Serial.println("Device is configured");
    } else {
        Serial.println("Device is not configured!");
        showRing(255,255,255,config.device_brightness);
    }
    /* Setup web server endpoints */
    setupWebServer();
    server.begin();                  /* Start web server */
}

/**
 * Configure web server endpoints
 */

void setupWebServer() {
    /* Root endpoint */
    server.on("/", HTTP_GET, []() {
      JsonDocument doc;
      String JsonString;
      doc["app_name"] = app_name;
      doc["app_version"] = app_version;
      doc["manufacturer"] = "makerUSA";
      doc["webstore"]="https://makerusa.net";
      serializeJson(doc, JsonString);
      server.send(200, "application/json", JsonString);
    });
    /* Version endpoint */
    server.on("/version", HTTP_GET, []() {
      JsonDocument doc;
      String JsonString;
      doc["app_name"] = app_name;
      doc["app_version"] = app_version;
      serializeJson(doc, JsonString);
      server.send(200, "application/json", JsonString);
    });
    /* Reset endpoint */
    server.on("/reset", HTTP_GET, []() {
      JsonDocument doc;
      String JsonString;
      if (server.hasArg("confirm") && server.arg("confirm")=="YES") {
        doc["response"] = "resetting WiFi configuration";
        serializeJson(doc, JsonString);
        server.send(200, "application/json", JsonString);
        //server.send(200, "text/plain", "Executing device configuration reset");
        NW.reset();
      }
        doc["error"] = "missing confirmation";
        serializeJson(doc, JsonString);
        server.send(200, "application/json", JsonString);
    });
    /* Mixer configuration endpoint */
    server.on("/mixer", HTTP_GET, []() {
        JsonDocument doc;
        String JsonString;
        doc["Mixer_host"] = config.mixer_host;
        doc["Mixer_port"] = config.mixer_port;
        serializeJson(doc, JsonString);
        server.send(200, "application/json", JsonString);
    });
    /* Device configuration endpoint */
    server.on("/device", HTTP_GET, []() {
        JsonDocument doc;
        String JsonString;
        doc["Device_name"] = config.device_name;
        doc["Device_id"] = config.device_id+1;
        serializeJson(doc, JsonString);
        server.send(200, "application/json", JsonString);
    });
    /* Full configuration endpoint */
    server.on("/config", HTTP_GET, []() {
        bool new_config = false;
        if (server.hasArg("mixer_host")) {
          server.arg("mixer_host").toCharArray(config.mixer_host, 40);
          new_config = true;
        }
        if (server.hasArg("mixer_port")) {
          config.mixer_port = server.arg("mixer_port").toInt();
          new_config = true;
        }
        if (server.hasArg("device_name")) {
          server.arg("device_name").toCharArray(config.device_name, 40);
          new_config = true;
        }
        if (server.hasArg("device_id")) {
          config.device_id = server.arg("device_id").toInt()-1;
          new_config = true;
        }       
        if (server.hasArg("device_mode")) {
          config.device_mode = server.arg("device_mode").toInt();
          new_config = true;
        }
        if (server.hasArg("device_brightness")) {
          config.device_brightness = server.arg("device_brightness").toInt();
          new_config = true;
        }
        if (new_config) {
          saveConfig();
        }
        JsonDocument doc;
        String JsonString;
        doc["app_name"] = app_name;
        doc["app_version"] = app_version;
        doc["Mixer_host"] = config.mixer_host;
        doc["Mixer_port"] = config.mixer_port;
        doc["Device_name"] = config.device_name;
        doc["Device_id"] = config.device_id+1;
        doc["Device_brightness"] = config.device_brightness;
        doc["Device_mode"] = config.device_mode;
        serializeJson(doc, JsonString);
        server.send(200, "application/json", JsonString);
    });
    /* Live status toggle endpoint */
    server.on("/program", HTTP_GET, []() {
        if (server.hasArg("state")) {
          if (server.arg("state")=="on") {
            pgmIndex=config.device_id;
          } else {
            pgmIndex=-1;
          }
        } else {
          pgmIndex = (pgmIndex == config.device_id) ? -1 : config.device_id;
        }
        showStatus();
        JsonDocument doc;
        String JsonString;
        doc[config.device_name] = device_state;
        serializeJson(doc, JsonString);
        server.send(200, "application/json", JsonString);
    });
    /* Preview status toggle endpoint */
    server.on("/preview", HTTP_GET, []() {
        if (server.hasArg("state")) {
          if (server.arg("state")=="on") {
            pvwIndex=config.device_id;
          } else {
            pvwIndex=-1;
          }
        } else {
          pvwIndex = (pvwIndex == config.device_id) ? -1 : config.device_id;
        }
        showStatus(); 
        JsonDocument doc;
        String JsonString;
        doc[config.device_name] = device_state;
        serializeJson(doc, JsonString);
        server.send(200, "application/json", JsonString);
    });
    /* Status endpoint */
    server.on("/status", HTTP_GET, []() {
        JsonDocument doc;
        String JsonString;
        doc["Camera_Live"] = camera_live;
        doc["Camera_preview"] = camera_preview;
        serializeJson(doc, JsonString);
        server.send(200, "application/json", JsonString);
    });
    /* Reboot endpoint */
    server.on("/boot", HTTP_GET, []() {
      JsonDocument doc;
        String JsonString;
        doc["response"] = "Rebooting device";
        serializeJson(doc, JsonString);
        server.send(200, "application/json", JsonString);
        delay(100);
        // server.send(200, "text/plain", "rebooting device");
        ESP.restart();
    });
}

/*
  Main program loop 
*/

void loop(void) {
    /* Handle system tasks */
    server.handleClient();
    NW.loop();
   
    /* Check WiFi connection */
    if (!WiFi_connected) {
        Serial.println("WiFi connection lost");
        delay(3000);
        ESP.restart();
    }
    /*  If configuration mode is set to 1 (Osee GoStream), connect to the mixter using the TCP protocol */
    if (config.device_mode==1 && !client.connected()) {    
        showRing(0,0,255,config.device_brightness);
        Serial.printf("Connecting to mixer %s:%d\n", config.mixer_host,config.mixer_port);
        
        if (!client.connect(config.mixer_host, config.mixer_port)) {
            delay(1000);     
            showRing(0,0,0,0);
            delay(1000);           
            return;
        }
       /* If the TCP connection has been established cycle through Red, Green, Blue and off once to indicate the connection establishment */
        showRing(255,0,0,config.device_brightness);
        delay(500);
        showRing(0,255,0,config.device_brightness);
        delay(500);
        showRing(0,0,255,config.device_brightness);
        delay(500);
        showRing(0,0,0,config.device_brightness);

        Serial.printf("Connected to mixer %s:%d\n",config.mixer_host,config.mixer_port);
        initializeStatus(); /* initialize the local status and request the mixture to send the it's current status*/
    }
    /* if the device mode is set to 1 (Osee GoStream) then listen for incoming packets while the connection is alive */
    if (config.device_mode==1) {
      /* Process incoming data */
      while (client.available()) {
        char c = client.read();
        input_buffer += c;
      }
      /* Handle received data */
      if (input_buffer.length() > 0) {
        processJsonBuffer(input_buffer);
      }
    }  
    delay(100);
}
