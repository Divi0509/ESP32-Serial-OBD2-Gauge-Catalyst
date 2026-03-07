//------------ Check new firmware update from web flasher --------------------
/*manifest.json
{
    "name": "PrintpooP",
    "version": "1.3.2",
    "new_install_prompt_erase": true,
    "new_install_improv_wait_time": 0,
    "builds": [
      {
        "chipFamily": "ESP32",
        "parts": [
          { "path": "bootloader.bin", "offset": 4096 },
          { "path": "partitions.bin", "offset": 32768},
          { "path": "boot_app0.bin", "offset": 57344 },
          { "path": "printpoop24_app.ino.bin", "offset": 65536 }
        ]
      }
    ]
}*/
/*
String current_version = "";    // current firmware version
bool firmware_checked = false;  //check firmware flag
String firmware_manifest = "";
//------------------------------------------------
void setVersion(const String ver, String manifest) {
  current_version = ver;
  firmware_manifest = manifest;
}

//function return firmware version to integer number
int versionToNumber(String version) {
  int major = version.substring(0, version.indexOf('.')).toInt();
  version = version.substring(version.indexOf('.') + 1);
  int minor = version.substring(0, version.indexOf('.')).toInt();
  version = version.substring(version.indexOf('.') + 1);
  int patch = version.toInt();
  // Combine the parts into a single integer for comparison
  return major * 10000 + minor * 100 + patch;
}
*/
//----------------------------------------
// check a new firmware version available on github
/*
bool newFirmwareAvailable() {
  Serial.print(F("\nChecking firmware updates\nCurrent version: "));
  Serial.println(current_version);
  // fetch the latest version from manifest.json
  String latestURL =
String("https://vaandcob.github.io/webpage/firmware/printpoop/") +
firmware_manifest; HTTPClient http; String payload = "";

  // Serial.print(F("Fetching manifest from: "));
  // Serial.println(latestURL);

  http.begin(latestURL);              // Specify the target server
  int httpResponseCode = http.GET();  // Send the GET request

  if (httpResponseCode > 0) {  // Check if the request was successful
    // Handle potential redirection
    if (httpResponseCode == HTTP_CODE_MOVED_PERMANENTLY || httpResponseCode ==
HTTP_CODE_FOUND) { String newUrl = http.header("Location"); http.end(); // End
the current connection before starting a new one http.begin(newUrl); // Begin
new request with the new URL httpResponseCode = http.GET();  // Send the GET
request to the new URL

      if (httpResponseCode > 0) {
        payload = http.getString();  // Get payload from redirected URL
      } else {
        http.end();
        return false;  // Failed after redirect
      }
    } else {
      payload = http.getString();  // Get payload from original URL
    }

    // --- ArduinoJson Parsing ---
    if (!payload.isEmpty()) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        http.end();
        return false;
      }

      // Check if the "version" key exists
      if (!doc["version"].isNull()) {
        const char* latestVersion_cstr = doc["version"];    // Get as const
char* String latestVersion = String(latestVersion_cstr);  // Convert to Arduino
String Serial.printf("Latest firmware: %s\n", latestVersion); firmware_checked =
true;

        // version comparision
        if (versionToNumber(latestVersion) > versionToNumber(current_version)) {
          Serial.println(F("-> New firmware version available"));
          http.end();
          return true;
        } else {
          Serial.println(F("-> Firmware is up to date"));
          http.end();
          return false;
        }
      } else {
        Serial.println(F("Manifest JSON does not contain 'version' key."));
        http.end();
        return false;
      }
    } else {
      Serial.println(F("Received empty payload."));
      http.end();
      return false;
    }
    // --- End ArduinoJson Parsing ---
  } else {  // Initial HTTP Request failed
    Serial.print(F("Initial HTTP Request failed. Error code: "));
    Serial.println(httpResponseCode);
  }
  http.end();    // Close connection
  return false;  // Default return if something went wrong or no new version
}
*/