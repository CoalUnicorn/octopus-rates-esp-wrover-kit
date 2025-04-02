# Octopus Energy Rate Display

## Overview
The Octopus Energy Rate Display project is an Arduino-based application that fetches and displays electricity unit rates from the Octopus Energy API. It utilizes an LCD screen to present the current rate, a bar chart of rates, and the next six rates in a user-friendly format.

## Features
- Connects to Wi-Fi to fetch electricity rates.
- Displays the current rate on an LCD screen.
- Shows a bar chart representing the unit rates for the current day.
- Lists the next 12 rates with their valid time ranges.
- After 16:00 it will fetch toorrows rates and display barchart for them

## Hardware Requirements
- **Microcontroller**: ESP32 (or compatible)
- **LCD Display**: WROVER_KIT_LCD
- **Wi-Fi Module**: Integrated in ESP32
- **Power Supply**: USB or battery

## Software Requirements
- Arduino IDE
- Libraries:
  - `WiFi.h`
  - `HTTPClient.h`
  - `time.h`
  - `WROVER_KIT_LCD` (from [CoalUnicorn/WROVER_KIT_LCD](https://github.com/CoalUnicorn/WROVER_KIT_LCD))
  - `ArduinoJson.h`

### Installation of WROVER_KIT_LCD Library
1. Go to the [WROVER_KIT_LCD GitHub repository](https://github.com/CoalUnicorn/WROVER_KIT_LCD).
2. Click on the green "Code" button and select "Download ZIP".
3. Unzip the downloaded folder and rename it to `WROVER_KIT_LCD`.
4. Place the `WROVER_KIT_LCD` folder in your Arduino libraries folder (usually located at `~/Arduino/libraries/`).
5. Restart the Arduino IDE to recognize the new library.

## Installation
1. Clone the repository or download the source code.
2. Open the project in the Arduino IDE.
3. Install the required libraries via the Library Manager.
4. Update the Wi-Fi credentials and Octopus Energy API key in the ```arduino_secrets.h```.
5. Upload the code to your ESP32 microcontroller.


## API Configuration
Create arduino_secrets.h

```cpp
#define SECRET_SSID "SSID";
#define SECRET_PASS "PASSWORD";

// Octopus Energy API configuration
#define API_KEY "sk_live_"; // Replace the placeholder with your actual Octopus Energy API key.
#define ACCOUNT_NUMBER ""; // Enter your Octopus Energy account number.
#define MPAN ""; // Your Meter Point Administration Number.
#define METER_SERIAL ""; // Your meter's serial number.
```


## Troubleshooting
- Ensure that the ESP32 is connected to a stable Wi-Fi network.
- Check the API key and account details for correctness.
- Monitor the Serial output for any error messages during execution.

## License
This project is licensed under the MIT License. See the LICENSE file for more details.

## Acknowledgments
- Thanks to the Octopus Energy API for providing the data. 
[Use this link to sign up to Octopus and save on your energy bill.](https://share.octopus.energy/great-owl-393)
- Special thanks to the Arduino community for their support and resources.