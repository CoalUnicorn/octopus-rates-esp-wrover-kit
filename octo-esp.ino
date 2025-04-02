#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "WROVER_KIT_LCD.h" // Include the WROVER_KIT_LCD library
#include <ArduinoJson.h>    // Include the ArduinoJson library

#include <arduino_secrets.h>

char ssid[] = SECRET_SSID;
char password[] = SECRET_PASS;
// Replace with your network credentials
// const char* ssid = "";     // Your Wi-Fi SSID
// const char* password = ""; // Your Wi-Fi password

// Octopus Energy API configuration
char apiKey[] = API_KEY;               // Your API key
char accountNumber[] = ACCOUNT_NUMBER; // Your account number
char mpan[] = MPAN;                    // Your MPAN
char meterSerial[] = METER_SERIAL;     // Your meter serial number
const char *baseUrl = "https://api.octopus.energy/v1";

// Define timezone constants
const long timezoneOffset_sec = 3600;              // Offset for BST (UTC+1)
const long gmtOffset_sec = 0;                      // Base offset (UTC)
const int daylightOffset_sec = timezoneOffset_sec; // Daylight saving time adjustment

// NTP server details
const char *ntpServer = "pool.ntp.org";

// Initialize the LCD
WROVER_KIT_LCD tft;

// this only works in portrait mode (orientation=0 or 3)
uint16_t height = tft.height(); // (=320)
uint16_t width = tft.width();   // (=240)

bool tomorrowRatesFetched = false;
bool displayDrawn = false;

// Define new height allocations
uint16_t currentRateHeight = 60;    // Height for current rate display
uint16_t barChartHeight = 130;      // Height for bar chart display
uint16_t nextRatesHeight = 130; // Height for next five rates display

// Define a new color constant for bright red
#define BRIGHT_RED 0xfaea // RGB for bright red

void setup()
{
    Serial.begin(115200); // Start the Serial communication
    delay(1000);          // Wait for a second

    // Initialize the LCD
    tft.begin();
    // Clean LC
    tft.fillRect(0, 0, width, height, WROVER_BLACK);

    // Connect to Wi-Fi
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting...");
    }

    Serial.println("Connected to WiFi!");

    // Initialize NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop()
{

    String lastFetchedDate = ""; // Variable to store the last fetched date
    String unitRatesJson = "";
    String unitRatesTomorrowJson = "";
    // Get the current date
    String currentDate;
    do
    {
        currentDate = getCurrentDate();
        delay(1000); // Wait for a second before checking again
    } while (currentDate <= "1970-01-01");

    String tomorrowDate;
    do
    {
        tomorrowDate = getTomorrowDate();
        delay(1000); // Wait for a second before checking again
    } while (tomorrowDate <= "1970-01-01");

    // Check if the current date has changed
    if (currentDate != lastFetchedDate)
    {
        Serial.println("Current date has changed. Fetching unit rates...");
        lastFetchedDate = currentDate;                 // Update the last fetched date
        unitRatesJson = fetchRateForDate(currentDate); // Fetch unit rates and store the JSON response
        // Serial.println("Unit Rates JSON: " + unitRatesJson); // Print the JSON response

        // reset tomorrow Rates status
        tomorrowRatesFetched = false;
        unitRatesTomorrowJson = "";
    }

    // Check if the current time is after 16:00 (4 PM), the new rates may be available
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_hour >= 16 && !tomorrowRatesFetched)
    {                                                           // Condition to check if it's after 16:00
        unitRatesTomorrowJson = fetchRateForDate(tomorrowDate); // Update tomorrow's rates
        if (!unitRatesTomorrowJson.isEmpty())
        { // Check if the fetch was successful
            // Serial.println("Tomorrow's Unit Rates JSON: " + unitRatesTomorrowJson); // Print the JSON response

            // update tomorrow Rates status, only needs to be updated once
            tomorrowRatesFetched = true;
        }
        else
        {
            Serial.println("Failed to fetch tomorrow's rates."); // Log failure
            tomorrowRatesFetched = false;
        }
    }

    // Check if we draw disply at start and update then only when the current time is at the whole hour or 30-minute mark
    if (!displayDrawn || timeinfo.tm_min == 0 || timeinfo.tm_min == 30)
    {
        // Reduce rates
        unitRatesJson = reduceRatesFromCurrentTime(unitRatesJson);

        displayCurrentRate(unitRatesJson);
        if (tomorrowRatesFetched)
        {
            displayBarChart(unitRatesTomorrowJson, currentRateHeight + barChartHeight); // Set offsetY to currentRateHeight + barChartHeight
            displayNext12RatesText(unitRatesJson, currentRateHeight);                                      // Display next 12 rates as text
        }
        else
        {
            displayBarChart(unitRatesJson, currentRateHeight); // Set offsetY to currentRateHeight
            displayNext12RatesText(unitRatesJson, currentRateHeight + barChartHeight);             // Display next 12 rates as text
        }
        // In the future the delay will be 30 minutes, indicate we drawn the first time.
        displayDrawn = true;
    }

    delay(30000); // Update every 30 seconds
}

String fetchRateForDate(const String &date)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;

        // Step 1: Get the current tariff code
        String tariffUrl = String(baseUrl) + "/accounts/" + accountNumber + "/";
        http.begin(tariffUrl);             // Specify the URL for tariff info
        http.setAuthorization(apiKey, ""); // Set the API key for authorization

        int httpResponseCode = http.GET(); // Make the request

        if (httpResponseCode > 0)
        {
            String payload = http.getString(); // Get the response payload
            // Extract the tariff code from the response
            String tariffCode = extractTariffCode(payload);
            if (tariffCode.length() > 0)
            {
                String productCode = extractProductCode(tariffCode);

                String url = String(baseUrl) + "/products/" + productCode + "/electricity-tariffs/" + tariffCode + "/standard-unit-rates/?period_from=" + date + "T00:00Z&period_to=" + date + "T23:59Z"; // + tomorrowDate + "T00:00Z"; | + date + "T23:59Z";

                // Fetch unit rates
                http.begin(url);                   // Specify the URL for unit rates
                http.setAuthorization(apiKey, ""); // Set the API key for authorization

                int rateResponseCode = http.GET(); // Make the request for unit rates

                if (rateResponseCode > 0)
                {
                    String ratePayload = http.getString(); // Get the response payload
                    // Parse the JSON response directly as an array
                    StaticJsonDocument<1024> doc; // Adjust size as needed
                    DeserializationError error = deserializeJson(doc, ratePayload);
                    if (error)
                    {
                        Serial.print(F("deserializeJson() failed: "));
                        Serial.println(error.f_str());
                        return ""; // Return empty if parsing fails
                    }

                    // Extract the relevant fields
                    String result = "[";
                    JsonArray results = doc["results"].as<JsonArray>(); // Directly access the array
                    int numBars = results.size();

                    // Create a temporary array to hold the JSON objects
                    String tempResults[numBars];

                    for (int i = 0; i < numBars; i++)
                    {
                        JsonObject rate = results[i];
                        float valueIncVat = rate["value_inc_vat"];
                        const char *validFrom = rate["valid_from"];
                        const char *validTo = rate["valid_to"];

                        // Remove the 'Z' from valid_from and valid_to, timezomes are hard
                        String validFromStr(validFrom);
                        String validToStr(validTo);
                        if (validFromStr.endsWith("Z"))
                        {
                            validFromStr.remove(validFromStr.length() - 1); // Remove the last character
                        }
                        if (validToStr.endsWith("Z"))
                        {
                            validToStr.remove(validToStr.length() - 1); // Remove the last character
                        }

                        // Store the JSON object in the temporary array
                        tempResults[i] = String("{\"value_inc_vat\":") + valueIncVat +
                                         String(",\"valid_from\":\"") + validFromStr +
                                         String("\",\"valid_to\":\"") + validToStr + "\"}";
                    }

                    // Reverse the order of the entries
                    for (int i = 0; i < numBars / 2; i++)
                    {
                        String temp = tempResults[i];
                        tempResults[i] = tempResults[numBars - 1 - i];
                        tempResults[numBars - 1 - i] = temp;
                    }

                    // Construct the final result string from the reversed array
                    for (int i = 0; i < numBars; i++)
                    {
                        result += tempResults[i] + ",";
                    }

                    // Remove the last comma and close the JSON array
                    if (result.length() > 1)
                    {
                        result.remove(result.length() - 1); // Remove last comma
                    }
                    result += "]";

                    http.end();    // Free resources
                    return result; // Return the formatted JSON response
                }
                else
                {
                    Serial.println("Error on HTTP request for rates: " + String(rateResponseCode));
                }
            }
            else
            {
                Serial.println("Failed to extract tariff code.");
            }
        }
        else
        {
            Serial.println("Error on HTTP request for tariff info: " + String(httpResponseCode));
        }

        http.end(); // Free resources
    }
    else
    {
        Serial.println("WiFi not connected");
    }
    return ""; // Return empty if not connected or if there was an error
}

String extractTariffCode(String payload)
{
    // Create a JSON document
    StaticJsonDocument<512> doc; // Adjust size as needed

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed in extractTariffCode() : "));
        Serial.println(error.f_str());
        return ""; // Return empty if parsing fails
    }

    // Extract the last tariff code from the JSON
    JsonArray agreements = doc["properties"][0]["electricity_meter_points"][0]["agreements"].as<JsonArray>();
    if (agreements.size() > 0)
    {
        const char *tariffCode = agreements[agreements.size() - 1]["tariff_code"]; // Get the last tariff_code
        return String(tariffCode);                                                 // Return the extracted tariff code
    }

    return ""; // Return empty if no agreements found
}

String extractProductCode(String tariffCode)
{
    // Logic to extract the product code from the tariff code
    // For example, AGILE-24-10-01 from E-1R-AGILE-24-10-01-C
    // plese dont change this to tariffCode.substring(4, 18) as it is incorrect
    return tariffCode.substring(5, 19);
}

String getCurrentDate()
{
    time_t now;
    struct tm timeinfo;
    char buffer[20];

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
    return String(buffer);
}

String getTomorrowDate()
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo); // Convert to local time

    // Increment the day by 1
    timeinfo.tm_mday += 1;

    // Normalize the time structure
    mktime(&timeinfo); // This will handle month and year overflow

    // Format the new date back to a string
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
    return String(buffer);
}

String reduceRatesFromCurrentTime(const String &ratesJson)
{
    // Parse the JSON response
    StaticJsonDocument<1024> doc; // Adjust size as needed
    DeserializationError error = deserializeJson(doc, ratesJson);
    if (error)
    {
        Serial.print(F("deserializeJson() failed in reduceRatesFromCurrentTime(): "));
        Serial.println(error.f_str());
        return ""; // Return empty if parsing fails
    }

    // Get the current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char currentTimeBuffer[20];
    strftime(currentTimeBuffer, sizeof(currentTimeBuffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    String currentTime(currentTimeBuffer);

    // Filter rates based on the current time
    String result = "[";
    JsonArray results = doc.as<JsonArray>();
    for (JsonObject rate : results)
    {
        const char *validFrom = rate["valid_from"];
        const char *validTo = rate["valid_to"];
        // Parse valid_to timestamp
        struct tm validToTime;
        strptime(validTo, "%Y-%m-%dT%H:%M:%S", &validToTime);
        time_t validToTimestamp = mktime(&validToTime);

        // Change the condition to check against validToTimestamp
        if (now <= validToTimestamp)
        {
            // Append to result string
            result += String("{\"value_inc_vat\":") + String(rate["value_inc_vat"].as<float>()) +
                      String(",\"valid_from\":\"") + validFrom +
                      String("\",\"valid_to\":\"") + validTo + "\"},";
        }
    }

    // Remove the last comma and close the JSON array
    if (result.length() > 1)
    {
        result.remove(result.length() - 1); // Remove last comma
    }
    result += "]";

    // Check the length of the filtered rates
    StaticJsonDocument<1024> filteredDoc; // Document for filtered rates
    error = deserializeJson(filteredDoc, result);
    if (error)
    {
        Serial.print(F("deserializeJson() failed in check the length of the filtered rates reduceRatesFromCurrentTime(): "));
        Serial.println(error.f_str());
        return ""; // Return empty if parsing fails
    }

    // // Check if the number of records is less than 16
    // if (filteredDoc.size() < 16) {
    //     String additionalRecords = addRecordsFromTomorrow(20, unitRatesTomorrowJson); // Get additional records
    //     result += additionalRecords.substring(1, additionalRecords.length() - 1); // Append without brackets
    // }
    Serial.println("Filtered Rates JSON: " + result); // Debug print

    return result; // Return the filtered JSON response
}

void displayCurrentRate(const String &ratesJson)
{
    // Parse the filtered JSON response
    StaticJsonDocument<1024> doc; // Adjust size as needed
    DeserializationError error = deserializeJson(doc, ratesJson);
    if (error)
    {
        Serial.print(F("deserializeJson() failed in displayCurrentRate(): "));
        Serial.println(error.f_str());
        return; // Exit if parsing fails
    }
    // Check if there are any valid rates
    JsonArray results = doc.as<JsonArray>();

    if (results.size() > 0)
    {
        // Display the first valid rate
        JsonObject rate = results[0];
        float valueIncVat = rate["value_inc_vat"];
        const char *validFrom = rate["valid_from"];
        const char *validTo = rate["valid_to"];

        // Prepare the display message with rounded value and "/kWh"
        String message = String((int)round(valueIncVat)) + "p/kWh";

        // Determine text color based on valueIncVat
        if (valueIncVat <= 10)
        {
            tft.setTextColor(WROVER_GREEN); // Set text color to green for 10 and below
        }
        else if (valueIncVat <= 20)
        {
            tft.setTextColor(WROVER_YELLOW); // Set text color to yellow for 20 and below
        }
        else
        {
            tft.setTextColor(BRIGHT_RED); // Set text color to bright red for above 20
        }

        // Display the message on the LCD
        tft.fillRect(0, 0, width, currentRateHeight, WROVER_BLACK); // Clear the top section for current rate
        tft.setCursor(35, 5);
        tft.setTextSize(7);                                    // Set text size for the rate value
        tft.print(message.substring(0, message.length() - 5)); // Print the rate value
        tft.setTextSize(3);                                    // Set text size for "/kWh"
        tft.print(message.substring(message.length() - 5));    // Print "/kWh"
    }
    else
    {
        // No valid rates found
        tft.fillRect(0, 0, width, currentRateHeight, WROVER_BLACK); // Clear the top section for current rate
        tft.setCursor(10, 15);
        tft.setTextColor(WROVER_WHITE); // Set text color to white for "No valid rates"
        tft.print("No valid rates");
    }
}

void displayBarChart(const String &ratesJson, int offsetY)
{
    // Parse the JSON response
    StaticJsonDocument<1024> doc; // Adjust size as needed
    DeserializationError error = deserializeJson(doc, ratesJson);
    if (error)
    {
        Serial.print(F("deserializeJson() failed in displayBarChart(): "));
        Serial.println(error.f_str());
        return; // Exit if parsing fails
    }

    // Clear the display for the bar chart
    tft.fillRect(0, currentRateHeight, width, offsetY, WROVER_BLACK); // Clear the bar chart section

    // Get the number of rates
    JsonArray results = doc.as<JsonArray>();
    int numRates = results.size();

    // Ensure a minimum of 18 bars
    const int minBars = 18;
    if (numRates < minBars)
    {
        numRates = minBars; // Set numRates to minBars if fewer rates are available
    }

    // Define bar chart parameters
    int gapSize = 1;                             // Define a minimal gap size
    int barWidth = (width / numRates) - gapSize; // Width of each bar with gap
    int maxBarHeight = barChartHeight;           // Maximum height of the bar (leaving space for labels)
    float maxRate = 0;                           // Variable to find the maximum rate

    // Find the maximum rate for scaling
    for (JsonObject rate : results)
    {
        float valueIncVat = rate["value_inc_vat"];
        if (valueIncVat > maxRate)
        {
            maxRate = valueIncVat;
        }
    }

    // Draw the bars
    for (int i = 0; i < numRates; i++)
    {
        JsonObject rate;
        if (i < results.size())
        {
            rate = results[i]; // Get the rate if available
        }
        else
        {
            // If there are not enough rates, create an empty rate
            rate["value_inc_vat"] = 0; // Set value to 0 for empty bars
        }

        float valueIncVat = rate["value_inc_vat"];
        int barHeight = (int)((valueIncVat / maxRate) * maxBarHeight); // Scale the bar height

        // Calculate the position of the bar with a gap
        int x = i * (barWidth + gapSize);             // Position for the bar with gap
        int y = offsetY + (maxBarHeight - barHeight); // Start from the bottom of the bar chart area

        // Determine the color of the bar
        uint16_t barColor = (valueIncVat < 0) ? WROVER_RED : WROVER_GREEN; // Red for negative, green for positive

        // Draw the bar
        tft.fillRect(x, y, barWidth, barHeight, barColor); // Draw the bar with full width

        // Draw vertical lines and labels for every 12th bar
        if (i % 12 == 0)
        {
            // Draw vertical line
            tft.fillRect(x, offsetY, 2, maxBarHeight, WROVER_WHITE); // Vertical line

            // Extract the valid_from timestamp
            const char *validFrom = rate["valid_from"]; // Get the valid_from field
            String validFromStr(validFrom);             // Convert to String for easier manipulation

            // Parse the hour from the valid_from string
            int hour = validFromStr.substring(11, 13).toInt(); // Extract hour (characters 11-13)

            // Set smaller text size for the hour label
            tft.setTextSize(1); // Set text size to 1 for smaller labels

            // Draw hour label just below the vertical line
            tft.setCursor(x + 5, offsetY); // Position for the label just below the vertical line
            tft.setTextColor(WROVER_WHITE);
            tft.print(hour); // Print the hour
        }
    }

    // Draw horizontal lines and labels
    int lineY;
    tft.setTextSize(2);
    tft.setTextColor(WROVER_WHITE); // Default text color

    // Line at 10
    lineY = offsetY + (maxBarHeight - (10 / maxRate) * maxBarHeight);
    tft.fillRect(0, lineY, width, 2, WROVER_WHITE); // Increased thickness to 2 pixels
    tft.setCursor(5, lineY + 10);                   // Position for the label below the horizontal line
    tft.setTextColor(WROVER_WHITE);                 // Set text color to match the line color
    tft.print("10");

    // Line at 20
    lineY = offsetY + (maxBarHeight - (20 / maxRate) * maxBarHeight);
    tft.fillRect(0, lineY, width, 2, WROVER_YELLOW); // Increased thickness to 2 pixels
    tft.setCursor(5, lineY + 10);                    // Position for the label below the horizontal line
    tft.setTextColor(WROVER_YELLOW);                 // Set text color to match the line color
    tft.print("20");

    // Line at 30
    lineY = offsetY + (maxBarHeight - (30 / maxRate) * maxBarHeight);
    tft.fillRect(0, lineY, width, 2, WROVER_RED); // Increased thickness to 2 pixels
    tft.setCursor(5, lineY + 10);                 // Position for the label below the horizontal line
    tft.setTextColor(WROVER_RED);                 // Set text color to match the line color
    tft.print("30");
}

void displayNext12RatesText(const String &ratesJson, int offsetY)
{
    // Parse the JSON response
    StaticJsonDocument<1024> doc; // Adjust size as needed
    DeserializationError error = deserializeJson(doc, ratesJson);
    if (error)
    {
        Serial.print(F("deserializeJson() failed in displayNextSixRatesText(): "));
        Serial.println(error.f_str());
        return; // Exit if parsing fails
    }

    // Clear the display for the next five rates
    tft.fillRect(0, offsetY, width, nextRatesHeight, WROVER_BLACK); // Clear the next five rates section

    // Get the number of rates
    JsonArray results = doc.as<JsonArray>();
    int numRates = results.size();

    int y = 1; // Offset for second column

    // Display the next five rates, starting from the second rate (index 1)
    for (int i = 1; i < numRates && i < 13; i++)
    { // Start from index 1 and display up to 5 rates
        JsonObject rate = results[i];
        const char *validFrom = rate["valid_from"];
        float valueIncVat = rate["value_inc_vat"]; // Get the rate value

        // Convert valid_from and valid_to to HH:MM format
        String validFromStr(validFrom);

        // Extract hours and minutes
        String fromHour = validFromStr.substring(11, 13);   // HH from valid_from
        String fromMinute = validFromStr.substring(14, 16); // MM from valid_from

        // Prepare the display message
        String message = fromHour + ":" + fromMinute + "|" + String((int)round(valueIncVat)) + "p"; // Include rate value

        // Set text color based on value
        if (valueIncVat <= 10)
        {
            tft.setTextColor(WROVER_GREEN); // Green for 10 and below
        }
        else if (valueIncVat <= 20)
        {
            tft.setTextColor(WROVER_YELLOW); // Yellow for between 20 and 30
        }
        else
        {
            tft.setTextColor(BRIGHT_RED); // Red for above 20
        }

        if (i <= 6)
        {
            // Set cursor position for each rate on left
            tft.setCursor(10, offsetY + ((i - 1) * 20) + 10); // Adjust Y position for each rate
            tft.setTextSize(2);                                                          // Set text size
            tft.print(message);                                                          // Display the time range and rate value
        }
        elsecurrentRateHeight + barChartHeight
        {
            // Set cursor position for each rate on right
            tft.setCursor(130, offsetY + ((y - 1) * 20) + 10); // Adjust Y position for each rate
            tft.setTextSize(2);                                                           // Set text size
            tft.print(message);                                                           // Display the time range and rate value
            y++;
        }
    }
}
