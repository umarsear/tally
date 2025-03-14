# **Tally Bright**  

Tally Bright is a feedback light solution designed for use with **OSee GoStream Video Mixer** and **Bitfocus Companion**. It provides clear visual status updates for live and preview camera feeds.  

## **Modes of Operation**  

Tally Bright supports two operational modes:  

1. **GoStream Mode**  
2. **Independent Mode**  

### **GoStream Mode**  

In **GoStream Mode**, the device connects to the **OSee GoStream Video Mixer** and automatically updates based on the associated camera’s status:  

- If the camera is in **preview** or **live**, the tally light updates accordingly.  
- It supports **super source status**, meaning if the assigned camera is part of a **super source**, the tally light will indicate whether **super source** is in **preview** or **live**.  

### **Independent Mode**  

**Independent Mode** allows the tally light to function without GoStream integration.  

- In this mode, GoStream-specific functionality is **disabled**.  
- The light can only be controlled via its **web interface**.  
- It is designed for **local networks only**, using an **unsecured HTTP connection** on **port 80**.  

## **API Endpoints**  

The device provides a **RESTful HTTP API** for control and status monitoring. All responses are returned in **JSON format**.  

| Endpoint      | Description |
|--------------|------------|
| `/`          | The root returns device information, including version number. |
| `/version`   | Returns the application name and version. |
| `/reset`     | Resets the device to factory defaults. Requires a `confirm=YES` parameter to prevent accidental resets. |
| `/mixer`     | Returns the current mix configuration. |
| `/device`    | Returns the device configuration. |
| `/config`    | Returns the full configuration. Supports parameters to modify specific settings. |
| `/program`   | Toggles the **program** status. Accepts an optional `state` parameter (`on` or `off`). |
| `/preview`   | Toggles the **preview** status. Accepts an optional `state` parameter (`on` or `off`). |
| `/status`    | Returns the current **light status**. |
| `/boot`      | Reboots the device. |

## **Hardware Compatibility**  

- This firmware has been tested with:  
  - **ESP32-C3**  
  - **WS2812B RGB LED**  
- Fully assembled product available at:  
  [Tally Bright – MakerUSA](https://makerusa.net/product/tally-bright-wireless-tally-light-for-osee-gostream-and-bitfocus-companion/)  

## **Dependencies**  

Tally Bright relies on the following libraries:  

- **[NetWizard](https://github.com/ayushsharma82/NetWizard)**  
- **WiFi** – by Ivan Grokhotov  
- **WebServer** – by Ivan Grokhotov  
- **EEPROM**  
- **[Arduino JSON](https://arduinojson.org)**  
- **[ESP32_WS2812_Lib](https://github.com/Zhentao-Lin/ESP32_WS2812_Lib)**  
