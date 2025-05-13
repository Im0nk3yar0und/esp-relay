#include <SoftwareSerial.h>             // Enables serial communication on other digital pins (not just the hardware serial port)

#include <ArduinoJson.h>                // Allows parsing and generating JSON, useful for configuration or API responses

#include <DNSServer.h>                  // Provides DNS server capabilities, often used in captive portals
#include <ESP8266WiFi.h>                // Core library for handling WiFi on ESP8266 boards

#include <ESPAsyncTCP.h>                // Asynchronous TCP library, improves performance for TCP communication
#include <ESPAsyncWebServer.h>          // Asynchronous HTTP server for serving web pages or APIs without blocking the main loop

#include <LittleFS.h>                   // Filesystem library for reading/writing files on the ESP8266's internal flash storage
#include <FS.h>                         // General filesystem interface, used as a base for specific file systems like LittleFS

#include "secrets.h"                    // Header file containing sensitive data such as WiFi credentials (excluded from version control)






/*
 * *******************************************************
 * Intranet Mode for ESP8266
 * *******************************************************
 * Define USE_INTRANET to connect the ESP8266 to your home 
 * intranet for easier debugging during development.
 * 
 * When this mode is enabled, the ESP8266 will connect to 
 * your home Wi-Fi network (intranet). This allows you to 
 * access the ESP8266 web server from your browser without 
 * needing to reconnect to the network each time, which 
 * simplifies testing and debugging.
  * *******************************************************
 */
#define USE_INTRANET





/*
  *******************************************************
  IP Configuration for ESP8266
  *******************************************************
  This part of the code defines various IP settings for the ESP8266.
  - `Actual_IP`: Stores the IP address assigned to the ESP8266 when it connects to the home intranet (used in debug mode).
  - `PageIP`: The default IP address for the ESP8266 when it is acting as an Access Point (AP).
  - `gateway`: The gateway IP address for the ESP8266 when it's set up as an Access Point.
  - `subnet`: The subnet mask used for the ESP8266 when it's set up as an Access Point.
  *******************************************************
*/
IPAddress Actual_IP;
IPAddress PageIP(192, 168, 1, 1);    // Default IP address of the AP
IPAddress gateway(192, 168, 1, 1);   // Gateway for the AP
IPAddress subnet(255, 255, 255, 0);  // Subnet mask for the AP





/*
  *******************************************************
  Web Server and DNS Redirection Setup
  *******************************************************
  This part of the code sets up a web server on the ESP8266 and configures DNS redirection.
  - `server`: An instance of the ESP8266 WebServer that listens on port 80 (HTTP default port) to handle incoming HTTP requests.
  - `dnsServer`: An instance of the DNS server for handling custom DNS redirection. The ESP8266 can act as a DNS server, redirecting specific domain requests to its own IP address.
  - `homeIP`: The IP address that the ESP8266 will respond with when a certain domain (`evilcorp.io` in this case) is queried. 
  -  This address can be the IP address of the ESP8266 in AP mode or any custom address.
  - `domain`: The domain name that will be redirected to the ESP8266's IP address (e.g., `evilcorp.io`). 
  -  When a device queries this domain, the ESP8266 will intercept the request and respond with its own IP address.
  *******************************************************
*/

// Create an instance of the ESP8266 WebServer
AsyncWebServer server(80);  // HTTP server running on port 80

// DNS server instance
DNSServer dnsServer;

// IP address to return for a specific domain
IPAddress homeIP(192, 168, 1, 1);  // Redirected IP address for the domain

// The domain name to redirect to the ESP8266's IP address
const char *domain = "evilcorp.io";









/*
  ********************************************************************************
  Debugging System and Status Monitoring Configuration
  ********************************************************************************
  This section handles all debug output configuration and system health monitoring:
  
  DEBUGGING SYSTEM:
  - `DEBUG_MODE`: Master switch for debug output (true=enabled, false=disabled)
  - Provides three debug output methods that compile out completely in production:
    * `DEBUG_PRINT()`   - Equivalent to Serial.print()
    * `DEBUG_PRINTLN()` - Equivalent to Serial.println() 
    * `DEBUG_PRINTF()`  - Equivalent to Serial.printf() for formatted output
  - When DEBUG_MODE=false, all debug statements are removed at compile time for:
    * Reduced memory footprint
    * Improved performance
    * Cleaner production output
  
  ********************************************************************************
*/

// ===== CONFIGURATION SETTINGS =====
// Debug mode switch - set to 'true' for debug output, 'false' for production (no debug output)
#define DEBUG_MODE false



// ===== DEBUG MACROS =====
// These macros provide debug output that automatically compiles out in production
#if DEBUG_MODE
  // Debug print - works like Serial.print() but only in debug mode
  #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
  
  // Debug println - works like Serial.println() but only in debug mode
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
  
  // Debug printf - works like Serial.printf() but only in debug mode
  // Allows formatted strings like: DEBUG_PRINTF("Value: %d", someInt)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  // Empty definitions for production - all debug statements compile to nothing
  // This completely removes debug code from production firmware
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
  #define DEBUG_PRINTF(...)
#endif


// Tracks the last time system status was checked (initialized to 0 for first run)
// System check interval in milliseconds (1 minute = 60,000 ms)
unsigned long lastCheckTime = 0;
const unsigned long CHECK_INTERVAL = 60000; // 1 minute in milliseconds








/*
  ******************************************************************************
  HARDWARE CONFIGURATION AND PIN DEFINITIONS
  ******************************************************************************
  This section defines all hardware-related configurations and pin assignments:

  SIM800L GSM MODULE:
  - `SIM800_TX_PIN` (GPIO4):  Serial transmit pin connected to SIM800's RX
  - `SIM800_RX_PIN` (GPIO5):  Serial receive pin connected to SIM800's TX
  - `serialSIM800`:           SoftwareSerial instance for SIM800 communication
                              (Baud rate is typically configured in setup())

  RELAY MODULE:
  - `RELAY_PIN` (GPIO12):     Digital output pin controlling the relay
                              (HIGH = relay ON, LOW = relay OFF)

  WIRING NOTES:
      SIM800L requires stable 3.7-4.2V power supply (use capacitor)

  ******************************************************************************
*/
#define SIM800_TX_PIN 4
#define SIM800_RX_PIN 5
#define RELAY_PIN 12            // Relay connected to GPIO12

// Create SoftwareSerial object for SIM800 communication
SoftwareSerial serialSIM800(SIM800_RX_PIN, SIM800_TX_PIN);
 







// Global variables for serial input handling
String inputString = "";          // Temporarily stores incoming characters from Serial
bool stringComplete = false;     // Flag indicating whether a complete command has been received






/*
  Global strings:
  - `pendingCommand` is used to temporarily store a command received from the web client via the `/send?cmd=...` endpoint.
  - `lastResult` is used to store the result or response string generated by the `processWEBCommand()` function,
    which can be retrieved later by the web client via the `/output` endpoint.
*/
String pendingCommand = "";      // Stores a command that is waiting to be processed
String lastResult = "";          // Stores the result of the last executed command






/*
  The `setup()` function is called once when the device starts up or is reset.
  It is used for initializing the system, setting up configurations, and establishing connections.
*/
void setup() {


    // Start serial communication for debugging purposes (baud rate 115200)
    Serial.begin(115200);


    // Wait for serial port to connect (only needed for native USB ports)
    while (!Serial) {
      ;  // Wait for serial port to connect
    }

    // A small delay before starting the setup process
    delay(500);

    // Initialize the LittleFS for file storage
    if (!LittleFS.begin()) {
      Serial.println("LittleFS Mount Failed");
      return;  // Stop further execution if LittleFS fails to mount
    }






/*
    NOTE:
    By default, most SIM800 modules are configured to operate at 115200 baud rate.

    However, SoftwareSerial on ESP8266 (e.g., ESP-07) is not reliable at high baud rates.
    Baud rates above 9600 often cause communication issues such as:
    - Garbled or missing characters
    - Lost responses
    - Inconsistent behavior

    To ensure stable communication, we need to reduce the baud rate of the SIM800 to 9600.


    Step 1 (one-time operation):
    Start SoftwareSerial at 115200 — the default baud rate of SIM800.
    Then send the AT command to change the SIM800 baud rate permanently to 9600.  --> serialSIM800.println("AT+IPR=9600");

    This step only needs to be done ONCE — either in code or using a USB-to-TTL adapter manually.

    IMPORTANT:
    After successfully sending the command AT+IPR=9600 and restarting the SIM800 module,
    you must change serialSIM800.begin(115200) to serialSIM800.begin(9600) in your code,
    and REMOVE the line serialSIM800.println("AT+IPR=9600");
*/

    // Start serial communication with the SIM800 module
    // serialSIM800.begin(115200);                        // TEMPORARILY use this to send AT+IPR=9600
    // delay(100);
    // serialSIM800.println("AT+IPR=9600");
    // delay(100);
    // serialSIM800.println("AT&W");                      // Save settings
    // delay(100);
    // NOTE: After this step, restart SIM800 manually or via AT+CFUN=1,1


    serialSIM800.begin(9600);
    delay(100);
    serialSIM800.println("AT+IPR=9600");
    delay(100);
    serialSIM800.println("AT&W");
    delay(100);



    // In the setup function — set the SIM800 to use text mode
    serialSIM800.println("AT+CMGF=1");
    delay(1000);



/*
  Configure how the SIM800 handles incoming SMS messages:
  AT+CNMI=<mode>,<mt>,<bm>,<ds>,<bfr>

  Parameters:

  mode – How the module handles unsolicited result codes:
    0 = Buffer them in modem (do not send to serial port).
    1 = Send them directly to serial port.
    2 = Buffer in modem *and* send to serial port when possible.

  mt – How the module handles received SMS messages:
    0 = Do not forward new messages.
    1 = Forward message indication to serial and store message in memory.
    2 = Discard message after forwarding (do not store in memory).
    3 = Store message and send "+CMTI" indication to serial.
    4 = Same as 3, but for ME (Mobile Equipment) memory.

  bm – Cell broadcast message settings:
    0 = Disable cell broadcast reception.
    1 = Enable cell broadcast reception.
    2 = Buffer cell broadcast messages in modem.

  ds – Delivery status reports (when message is delivered):
    0 = Disable status report notifications.
    1 = Enable and send status reports to serial.
    2 = Buffer them in the modem.

  bfr – How to handle the output buffer before sending unsolicited result codes:
    0 = Do not clear buffer before sending.
    1 = Clear buffer before sending new unsolicited codes.

  Recommended setting for SMS reading without auto-deletion:
    AT+CNMI=2,1,0,0,0
    -> Shows message immediately, stores it, and does not auto-delete.
*/
    serialSIM800.println("AT+CNMI=1,2,0,0,0");
    delay(1000);




/*
  Delete all stored SMS messages to clear memory (optional, not recommended if you want to retain messages)

  Delete SMS messages from SIM memory:
  AT+CMGD=<index>,<delflag>

  Parameters:
    index   = 1   -> Starting message index to delete (inbox position).
    delflag = 4   -> Deletion mode:

      0 = Delete only the message at the specified index.
      1 = Delete all read messages.
      2 = Delete all read and sent messages.
      3 = Delete all read, sent, and unsent messages.
      4 = Delete **all messages** (read, unread, sent, unsent).

  So, AT+CMGD=1,4 means:
  ➤ Start from message index 1 and delete **all messages** stored on SIM.
*/
    //serialSIM800.println("AT+CMGD=1,4");
    //delay(1000);
    //serialSIM800.println("AT+CMGDA= \"DEL ALL\"");
    //delay(1000);



    inputString.reserve(500);       // Pre-allocate 500 bytes for inputString to avoid memory fragmentation

    // Initialize relay pin
    pinMode(RELAY_PIN, OUTPUT);     // Set the relay pin as an output
    digitalWrite(RELAY_PIN, LOW);   // Ensure the relay is initially turned off (assuming LOW = off)








    // If USE_INTRANET is defined, connect to a specific Wi-Fi network (home intranet)
    #ifdef USE_INTRANET
      WiFi.begin(LOCAL_SSID, LOCAL_PASS);  // Connect to Wi-Fi using predefined credentials

      delay(2000);
      // Wait until the ESP8266 successfully connects to Wi-Fi
      Serial.print("\nConnecting to WiFi: ");
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);                 // Wait 500ms between connection attempts
        Serial.print(".");          // Print progress dots
        attempts++;                 // Increment attempt counter
        
        // After 20 attempts (10 seconds), this loop will exit
      }

      // Check final connection status
      if (WiFi.status() == WL_CONNECTED) {
          // Successful connection
          Serial.println("\nWiFi connected!");
          Serial.print("IP address: ");
          Serial.println(WiFi.localIP());  // Print the assigned IP address
        
          // This IP is what you'll use to access the web interface

          // Store the current IP address for further use
          Actual_IP = WiFi.localIP();

       } else {
          startAP();  // Start the ESP8266 in Access Point (AP) mode
          Serial.println("Started AP mode: ESP-Backup");
      }

    #endif


    // If USE_INTRANET is not defined, configure the ESP as an access point
    #ifndef USE_INTRANET
      startAP();  // Start the ESP8266 in Access Point (AP) mode

      // Start the DNS server on port 53 for redirecting requests to the ESP's IP
      dnsServer.start(53, domain, homeIP);
    #endif




    // Start the HTTP server to listen for incoming HTTP requests on port 80
    server.begin();

    /*
     * Define Routes for the HTTP Server:
     * - The server.on() function is used to define different routes (URLs) and associate them with specific handlers (functions).
     * - Each route corresponds to a specific page or action within the web application.
     */

    // Route for the root URL (login or main page)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html", "text/html");  // Serve the HTML page from internal flash filesystem
    });


    // Route for the favicon (small icon shown in browser tabs)
    server.on("/favicon-48x48.png", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/favicon-48x48.png", "image/png");  // Serve favicon image
    });





/*
  Explanation:
  ----------------------------------------------------
  The function `processWEBCommand()` should NOT be called directly inside an AsyncWebServer route handler (/send).
  This is because it contains blocking operations, such as `delay()` or other time-consuming tasks.
  The AsyncWebServer library is designed to work asynchronously — it expects all handler functions to return quickly.
  If a blocking function like `delay()` is used inside a handler, it can cause the ESP8266 to crash or become unresponsive,
  because the WiFi and networking background tasks are not given time to execute properly.

  Instead, we store the incoming command in `pendingCommand`, and then process it later in the main loop (or somewhere safe),
  outside the async context. This ensures the server remains responsive and stable.

  Global strings:
  - `pendingCommand` is used to temporarily store a command received from the web client via the `/send?cmd=...` endpoint.
  - `lastResult` is used to store the result or response string generated by the `processWEBCommand()` function,
    which can be retrieved later by the web client via the `/output` endpoint.
*/

    // Handle HTTP GET requests to /send?cmd=...
    server.on("/send", HTTP_GET, [](AsyncWebServerRequest *request){
      DEBUG_PRINT("[DEBUG] /send endpoint called");

      if (request->hasParam("cmd")) {
        if (pendingCommand == "") {                                                         // If no command is currently being processed
          pendingCommand = request->getParam("cmd")->value();                               // Store the command
          request->send(200, "application/json", "{\"response\":\"Processing...\"}");       // Send back immediate feedback
        } else {
          request->send(429, "application/json", "{\"response\":\"Busy, try again\"}");     // Too many requests, already processing
        }
      } else {
        DEBUG_PRINTLN("\nMissing 'cmd' parameter");
        request->send(400, "application/json", "{\"response\":\"Missing cmd parameter\"}");  // Bad request
      }
    });


    // Endpoint to retrieve the result of the last processed command
    server.on("/output", HTTP_GET, [](AsyncWebServerRequest *request) {

      DynamicJsonDocument doc(512);     // Create a JSON document
      doc["response"] = lastResult;     // Set the response to the last result

      String json;
      serializeJson(doc, json);         // Convert JSON document to a string

      request->send(200, "application/json", json);  // Send the JSON response

      lastResult = "";                  // Clear lastResult after sending
    });


    // Catch-all handler for undefined routes (404 Not Found)
    server.onNotFound([](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/404.html", "text/html");  // Serve custom 404 page
    });

}








/*
  ******************************************************************************
  FORMATTED AT COMMAND EXECUTOR
  ******************************************************************************
  Wraps standard AT commands with consistent formatting headers and separators
  for improved readability in console output.

  PARAMETERS:
  - cmd : The AT command to execute (without terminator)

  FEATURES:
  - Adds command execution header
  - Includes visual separators
  - Preserves raw response formatting
  - Combines with sendWEBCommand() for actual transmission

  USAGE:
  String response = sendWEBCommand("AT+CSQ");
  // Returns formatted signal quality report
  ******************************************************************************
*/
String sendWEBCommand(String cmd, unsigned long timeout = 5000) {
  
  // ESP.wdtFeed();                      // Reset watchdog
  String response = "";
  
  // Clear the serial buffer first
  while(serialSIM800.available()) {
    serialSIM800.read();
    delay(1);
  }

  delay(100);

  // Send the command with proper line endings
  serialSIM800.println(cmd);
 

  
  // Collect response with timeout
  unsigned long start = millis();

  while(millis() - start < timeout) {
    while(serialSIM800.available()) {
      char c = serialSIM800.read();
      response += c;
      
      // Optional: Add small delay between characters
      delay(1);
    }
    
    // Early exit if we got a complete response
    if(response.endsWith("OK\r\n") || response.endsWith("ERROR\r\n")) {
      break;
    }
  }

  // Clean up the response
  response.trim();
  response += "\n";

  return response;
}























/*
  ******************************************************************************
  MAIN PROGRAM LOOP
  ******************************************************************************
  The `loop()` function runs continuously after `setup()` completes, handling all
  repetitive tasks and event processing:

  PRIMARY RESPONSIBILITIES:
  1. HTTP SERVER HANDLING:
     - Processes incoming web requests
     - Manages client connections
     - Serves web pages and API responses

  2. NETWORK AND EVENT MONITORING:
     - Checks for network activity
     - Handles DNS requests (if DNS server active)
     - Processes any event-driven tasks

  3. PERIODIC SYSTEM OPERATIONS:
     - Regular system status checks
     - Sensor data collection (if applicable)
     - Maintenance tasks and cleanup

  OPERATION NOTES:
  - Should remain non-blocking whenever possible
  - Critical operations should include timeout handling
  - Long-running tasks should be broken into smaller steps

  TYPICAL EXECUTION CYCLE:
  1. Check for incoming network requests
  2. Process any pending events
  3. Handle periodic tasks
  4. Yield CPU when possible (via delay(0) or similar)
  ******************************************************************************
*/

void loop() {

  ESP.wdtFeed(); // Reset watchdog

  #ifndef USE_INTRANET
    // If not using intranet mode, process incoming DNS requests
    // This is typically used in captive portal setups to redirect all DNS queries to the ESP
    dnsServer.processNextRequest();
  #endif

  #ifdef DEBUG_MODE
    // If debug mode is enabled, periodically check and print system status
    // Useful for monitoring memory, WiFi signal, uptime, etc.
    checkSystemStatus();
  #endif





  // Process any pending web command
  if (pendingCommand != "") {

    // Process the web command by passing it to processWEBCommand() function
    lastResult = processWEBCommand(pendingCommand);

    // Print the result of the processed command to the debug output
    DEBUG_PRINTLN("\n[WEB CMD RESULT]");
    DEBUG_PRINTLN(lastResult);

    // Reset pendingCommand for the next command
    pendingCommand = "";         
  }




  // Read incoming serial data if available
  while (Serial.available()) {

    char inChar = (char)Serial.read();      // Read one character from the serial input buffer
    inputString += inChar;                  // Append the character to the inputString

    // Check if the end of the line (newline character) has been reached
    if (inChar == '\n') {              
      stringComplete = true;                // Mark the input as complete when newline is encountered
    }
  }




  // Process the full serial command when received
  if (stringComplete) {
    inputString.trim();                          // Remove any leading/trailing whitespace or newline characters
    processSerialCommand(inputString);           // Pass the cleaned string to a function that handles commands
    inputString = "";                            // Clear the input string for the next message
    stringComplete = false;                      // Reset the completion flag
  }








/*
  This section of the code is designed to handle communication with a SIM800 module via a serial interface.


  Explanation:
  - `serialSIM800.available()` checks if there are any bytes of data waiting to be read from the SIM800 module. 
     This ensures that we don't attempt to read data when nothing is available.

  - `handleSIM800Response()` is likely a custom function that handles the specific response or message received from the SIM800 
     (e.g., SMS, call status, or connection confirmation). This function might parse the response and trigger appropriate actions.

  - `Serial.write()` is used to forward each byte received from the SIM800 to the primary serial port 
     (often connected to a USB terminal or the PC). This is useful for debugging or monitoring data coming from the SIM800.
  
  Keep in mind that communication with external modules (like SIM800) over serial interfaces often involves waiting for responses, which may cause delays in the program flow.
  It's important to ensure that such blocking operations don't interfere with real-time tasks, like handling web requests or managing the watchdog timer.
*/

  // Check for incoming data from SIM800
  if (serialSIM800.available()) {
    Serial.print(handleSIM800Response());
  }


  // Forward what Serial received to Software Serial Port
  // while (serialSIM800.available())
  // {
  //   Serial.write(serialSIM800.read());    
  // }




  // Be sure to add a small delay in the main loop
  // Gives the CPU a break, allowing the watchdog timer (WDT) and other background tasks to run
  delay(10);

}









/*
  ******************************************************************************
  ACCESS POINT (AP) MODE INITIALIZATION
  ******************************************************************************
  Configures the ESP8266 to operate as a wireless Access Point with the following
  parameters and functionality:

  CORE FEATURES:
  - Creates a WiFi network with configurable SSID and password
  - Sets up AP network parameters (IP range, subnet mask)
  - Enables broadcast of the specified SSID
  - Implements basic security through WPA2 password protection

  NETWORK PARAMETERS:
  - Default IP: 192.168.4.1 (configurable)
  - Default Subnet: 255.255.255.0
  - Default AP Channel: 1 (configurable)
  - Maximum Connections: Typically 4-8 clients (hardware dependent)

  OPERATIONAL NOTES:
  1. SSID visibility can be controlled (visible/hidden)
  2. Password must meet minimum length requirements (8+ chars recommended)
  3. IP address and network parameters should be logged after initialization
  4. AP mode consumes more power than station mode

  TYPICAL USAGE:
  1. Call during setup() to establish AP
  2. Monitor connection status via WiFi.softAPgetStationNum()
  3. Combine with DNS server for captive portal functionality
  ******************************************************************************
*/
void startAP() {

  // Start the Access Point with the defined SSID and password
  WiFi.softAP(AP_SSID, AP_PASS);

  delay(100);     // Brief delay to ensure the AP starts properly

  // Configure the Access Point's network settings: IP, gateway, and subnet
  WiFi.softAPConfig(PageIP, gateway, subnet);

  delay(100);     // Brief delay to ensure network settings are applied

  // Notify that the Access Point is active
  Serial.println("\nAccess Point Started");

  // Retrieve and display the IP address assigned to the Access Point
  Actual_IP = WiFi.softAPIP();

  Serial.print("IP address: ");
  Serial.println(Actual_IP);
}






/*
  Function: escapeJson

  Purpose: 
  This function takes an input string and returns a new string where certain characters are escaped 
  to make it valid JSON format. This is important when creating JSON strings to ensure that characters 
  like double quotes, backslashes, and newline characters are correctly formatted for JSON parsing.

  Returns:
    - A new string (`output`) where certain characters have been escaped to make the string valid 
      JSON. For example, double quotes (`"`) become `\"`, and backslashes (`\`) become `\\`.

  Explanation:
  The function loops through each character of the input string:
  - If it encounters a double quote (`"`), it appends `\"` to the output string (escaping the quote).
  - If it encounters a backslash (`\`), it appends `\\` to the output string (escaping the backslash).
  - If it encounters a newline character (`\n`), it appends `\\n` to the output string (escaping the newline).
  - If it encounters a carriage return (`\r`), it appends `\\r` to the output string (escaping the carriage return).
  - All other characters are appended to the output string as they are without modification.

  This function is useful when preparing strings to be embedded in JSON objects, especially when the strings may contain special characters
  that could interfere with JSON syntax or parsing.

  Example:
  - Input: `Hello "world"\nNew line`
  - Output: `Hello \"world\"\\nNew line`

  Use Case:
  This function could be called when creating JSON responses for a web server, ensuring that strings are correctly escaped before they are included in the response.

*/
String escapeJson(const String &input) {
  String output = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == '"') {
      output += "\\\"";         // Escape double quotes
    } else if (c == '\\') {
      output += "\\\\";         // Escape backslash
    } else if (c == '\n') {
      output += "\\n";          // Escape newlines
    } else if (c == '\r') {
      output += "\\r";          // Escape carriage returns
    } else {
      output += c;              // Other characters stay the same
    }
  }
  return output;
}






/*
  ******************************************************************************
  LITTLEFS FILE SYSTEM DIRECTORY LISTING
  ******************************************************************************
  Scans and displays the contents of the LittleFS filesystem with detailed file
  information, primarily used for debugging and system verification.

  CORE FUNCTIONALITY:
  - Recursively lists all files in specified directory path
  - Displays complete file paths relative to LittleFS root
  - Shows precise file sizes in bytes
  - Handles both files and directories

  OUTPUT FORMAT:
  - [File] /path/filename.ext (size bytes)
  - [Dir]  /path/subdirectory/

  TECHNICAL DETAILS:
  - Uses LittleFS directory traversal functions
  - Implements recursive scanning for complete filesystem overview
  - Calculates exact file sizes including filesystem overhead

  EXAMPLE OUTPUT:
  [File] /config/settings.json (512 bytes)
  [Dir]  /data/logs/
  [File] /data/logs/system.log (2048 bytes)
  ******************************************************************************
*/
String listFiles(const char *dir) {
  String output = "";
  output += "\n";
  output += "----------- List files -----------\n\n";
  
  // Open the directory
  File root = LittleFS.open(dir, "r");
  if (!root) {
    return "Failed to open directory";
  }

  // Find the first file in the directory
  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    size_t fileSize = file.size();

    output += "File: " + fileName + " | Size: " + String(fileSize) + "\n";

    // Go to the next file
    file = root.openNextFile();
  }

  if (output == "") {
    return "No files found";
  }
  
  output += "\n\n";
  return output;
}


















/*
 * ===========================================================================
 *                         COMMAND HELP SECTION
 * ===========================================================================
 * 
 * Description:
 *   This section contains all help functions that document available commands.
 *   Each function provides detailed usage information for a specific command.
 * 
 * ===========================================================================
 */


// Print all available commands
String printHelpAll() {
  String help = "";
  help += "\n";
  help += "------------------- Available commands --------------------\n";
  help += "helpesp       - Show ESP-07 specific commands\n";
  help += "helpsim       - Show SIM800 specific commands\n";
  help += "clearscreen   - Clears the screen with ANSI escape sequence\n";
  help += "cls           - Clears the screen (simulated)\n\n\n";

  help += "--------------------- ESP-07 Commands ---------------------\n";
  help += "espinfo       - ESP chip information\n";
  help += "flashinfo     - Flash memory information\n";
  help += "systemstatus  - Get system status\n";
  help += "listfiles     - List system files\n";   
  help += "reset         - Reset the ESP module\n";
  help += "restart       - Restart the ESP module\n\n";

  help += "FILE SYSTEM COMMANDS:\n";
  help += "  ls      - List directory contents\n";
  help += "  rm      - Remove files or directories\n";
  help += "  mv      - Move (rename) files\n";
  help += "  chmod   - Change file modes/attributes\n";
  help += "  grep    - Search for patterns in files\n";
  help += "  find    - Search for files\n";
  help += "  diff    - Compare files line by line\n";
  help += "  dd      - Convert and copy files\n\n";
  
  help += "PROCESS COMMANDS:\n";
  help += "  kill     - Send signals to processes\n";
  help += "  sudo     - Execute command as superuser\n";
  help += "  whoami   - Print effective userid\n";
  help += "  history  - Display command history\n\n";
  
  help += "NETWORKING COMMANDS:\n";
  help += "  ping     - Send ICMP ECHO_REQUEST packets\n";
  help += "  ssh      - OpenSSH SSH client\n";
  help += "  ifconfig - Configure network interfaces\n";
  help += "  tail     - Output last part of files\n\n";
  
  help += "EDITING COMMANDS:\n";
  help += "  vim      - Vi IMproved text editor\n";
  help += "  nano     - Simple text editor\n\n";
  
  help += "SYSTEM MONITORING:\n";
  help += "  cron     - Daemon to execute scheduled commands\n";
  help += "  bash     - GNU Bourne-Again SHell\n\n\n";
  
  help += "--------------------- SIM800 Commands ---------------------\n";
  
  help += "at            - Attention command\n";
  help += "csq           - Signal quality\n";
  help += "creg          - Network registration\n";
  help += "cgreg         - GPRS registration\n";
  help += "cops          - Operator info\n";
  help += "cpin          - SIM card status\n";
  help += "cgatt         - GPRS attach status\n";
  help += "sms           - SMS commands (type 'sms help' for more)\n";
  help += "battery       - Battery voltage and charge status\n";
  help += "time          - Get network time\n";
  help += "imei          - Get IMEI number\n";
  help += "imsi          - Get IMSI number\n";
  help += "ccid          - Get SIM CCID\n";
  help += "cmd <command> - Send a custom AT command to the SIM800 module (type 'cmd help' for more)\n\n\n";


  // Add a section for help instructions
  help += "\nFor detailed help on a specific command, type '<command> help'.\n";
  help += "For example, to get detailed help for the 'sms' command, type 'sms help'.\n";
  help += "-----------------------------------------------------------\n\n";
  
  return help;
  return help;
}



// Print help
String printHelp() {
  String help = "";
  help += "\n";
  help += "------------------- Available commands --------------------\n";
  help += "helpesp       - Show ESP-07 specific commands\n";
  help += "helpsim       - Show SIM800 specific commands\n";
  help += "clearscreen   - Clears the screen with ANSI escape sequence\n";
  help += "cls           - Clears the screen (simulated)\n";
  help += "-----------------------------------------------------------\n\n";
  return help;
}




// Print ESP-specific commands
String printHelpESP() {
  String help = "";
  help += "\n";
  help += "--------------------- ESP-07 Commands ---------------------\n";
  help += "espinfo       - ESP chip information\n";
  help += "flashinfo     - Flash memory information\n";
  help += "systemstatus  - Get system status\n";
  help += "listfiles     - List system files\n";   
  help += "reset         - Reset the ESP module\n";
  help += "restart       - Restart the ESP module\n\n";

  help += "FILE SYSTEM COMMANDS:\n";
  help += "  ls      - List directory contents\n";
  help += "  rm      - Remove files or directories\n";
  help += "  mv      - Move (rename) files\n";
  help += "  chmod   - Change file modes/attributes\n";
  help += "  grep    - Search for patterns in files\n";
  help += "  find    - Search for files\n";
  help += "  diff    - Compare files line by line\n";
  help += "  dd      - Convert and copy files\n\n";
  
  help += "PROCESS COMMANDS:\n";
  help += "  kill     - Send signals to processes\n";
  help += "  sudo     - Execute command as superuser\n";
  help += "  whoami   - Print effective userid\n";
  help += "  history  - Display command history\n\n";
  
  help += "NETWORKING COMMANDS:\n";
  help += "  ping     - Send ICMP ECHO_REQUEST packets\n";
  help += "  ssh      - OpenSSH SSH client\n";
  help += "  ifconfig - Configure network interfaces\n";
  help += "  tail     - Output last part of files\n\n";
  
  help += "EDITING COMMANDS:\n";
  help += "  vim      - Vi IMproved text editor\n";
  help += "  nano     - Simple text editor\n\n";
  
  help += "SYSTEM MONITORING:\n";
  help += "  cron     - Daemon to execute scheduled commands\n";
  help += "  bash     - GNU Bourne-Again SHell\n\n";
  
  help += "-----------------------------------------------------------\n";
  return help;
}



// Print SIM800-specific commands and explain how to get detailed help for each command
String printHelpSIM() {
  String help = "";
  help += "\n\n";
  help += "--------------------- SIM800 Commands ---------------------\n";
  
  // Basic commands and their brief descriptions
  help += "at            - Attention command\n";
  help += "csq           - Signal quality\n";
  help += "creg          - Network registration\n";
  help += "cgreg         - GPRS registration\n";
  help += "cops          - Operator info\n";
  help += "cpin          - SIM card status\n";
  help += "cgatt         - GPRS attach status\n";
  help += "sms           - SMS commands (type 'sms help' for more)\n";
  help += "battery       - Battery voltage and charge status\n";
  help += "time          - Get network time\n";
  help += "imei          - Get IMEI number\n";
  help += "imsi          - Get IMSI number\n";
  help += "ccid          - Get SIM CCID\n";
  help += "cmd <command> - Send a custom AT command to the SIM800 module (type 'cmd help' for more)\n";

  // Add a section for help instructions
  help += "\nFor detailed help on a specific command, type '<command> help'.\n";
  help += "For example, to get detailed help for the 'sms' command, type 'sms help'.\n";
  help += "-----------------------------------------------------------\n\n";
  
  return help;
}


String printHelpAT() {
  String help = "";
  help += "/*\n";
  help += " * Command: at\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   The 'AT' command is used to check if the SIM800 module is responding.\n";
  help += " *   It is a basic command to test the communication with the module.\n";
  help += " * \n";
  help += " * Syntax: at\n";
  help += " * \n";
  help += " * Example Usage:\n";
  help += " *   1. Query the SIM800 module status: at\n";
  help += " *      - The module will respond with 'OK' if it is functional.\n";
  help += " * \n";
  help += " * The module will respond with 'OK' if the command is successful.\n";
  help += " */\n";
  return help;
}








String printHelpIMEI() {
  String help = "";
  help += "/*\n";
  help += " * Command: IMEI\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   IMEI (International Mobile Equipment Identity) is a unique identifier\n";
  help += " *   used to identify mobile devices on cellular networks. It is a 15-digit\n";
  help += " *   number, typically displayed as:\n";
  help += " * \n";
  help += " *   Example IMEI: 354528073503456\n";
  help += " * \n";
  help += " * Structure of IMEI:\n";
  help += " *   XXXXXXXX-XX-XXXXXX-X\n";
  help += " * \n";
  help += " * The IMEI is structured as follows:\n";
  help += " * \n";
  help += " * 1. TAC (Type Allocation Code) - The first 8 digits, used to identify the manufacturer and model.\n";
  help += " *    Example: 35452807\n";
  help += " *    ASCII Representation for TAC:\n";
  help += " *    3  -> ASCII: 51\n";
  help += " *    5  -> ASCII: 53\n";
  help += " *    4  -> ASCII: 52\n";
  help += " *    5  -> ASCII: 53\n";
  help += " *    2  -> ASCII: 50\n";
  help += " *    8  -> ASCII: 56\n";
  help += " *    0  -> ASCII: 48\n";
  help += " *    7  -> ASCII: 55\n";
  help += " * \n";
  help += " * 2. FAC (Final Assembly Code) - The next 2 digits, representing the final assembly location.\n";
  help += " *    Example: 35\n";
  help += " *    ASCII Representation for FAC:\n";
  help += " *    3  -> ASCII: 51\n";
  help += " *    5  -> ASCII: 53\n";
  help += " * \n";
  help += " * 3. SNR (Serial Number) - The next 6 digits are unique to the device.\n";
  help += " *    Example: 180345\n";
  help += " *    ASCII Representation for SNR:\n";
  help += " *    1  -> ASCII: 49\n";
  help += " *    8  -> ASCII: 56\n";
  help += " *    0  -> ASCII: 48\n";
  help += " *    3  -> ASCII: 51\n";
  help += " *    4  -> ASCII: 52\n";
  help += " *    5  -> ASCII: 53\n";
  help += " * \n";
  help += " * 4. SP (Spare) - The last digit, which serves as a checksum digit.\n";
  help += " *    Example: 6\n";
  help += " *    ASCII Representation for SP:\n";
  help += " *    6  -> ASCII: 54\n";
  help += " * \n";
  help += " * Example IMEI: 354528073503456\n";
  help += " * Example IMEI: XXXXXXXX-XX-XXXXXX-X\n";
  help += " * Example IMEI: 35452807-35-03456\n";
  help += " * \n";
  help += " * The IMEI is used for identifying and tracking mobile devices across networks.\n";
  help += " */\n";
  return help;
}



String printHelpIMSI() {
  String help = "";
  help += "/*\n";
  help += " * Command: IMSI\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   IMSI (International Mobile Subscriber Identity) is a unique identifier used\n";
  help += " *   to identify a subscriber within a mobile network. It is a 15-digit number,\n";
  help += " *   structured as follows:\n";
  help += " * \n";
  help += " * Structure of IMSI:\n";
  help += " *   XXX XXX XXXXXXXX\n";
  help += " *   - MCC (Mobile Country Code): The first 3 digits represent the country code.\n";
  help += " *     Example: 310\n";
  help += " *   - MNC (Mobile Network Code): The next 2 or 3 digits represent the mobile\n";
  help += " *     network operator.\n";
  help += " *     Example: 260\n";
  help += " *   - MSIN (Mobile Subscriber Identification Number): The remaining digits are\n";
  help += " *     unique to the subscriber.\n";
  help += " *     Example: 1234567890\n";
  help += " * \n";
  help += " * ASCII Representation for IMSI:\n";
  help += " *   MCC (310) -> 51 49 48\n";
  help += " *   MNC (260) -> 50 54 48\n";
  help += " *   MSIN (1234567890) -> 49 50 51 52 53 54 55 56 57 48\n";
  help += " * \n";
  help += " * Example IMSI: 3102601234567890\n";
  help += " * \n";
  help += " * This unique identifier is used by mobile networks for identifying subscribers.\n";
  help += " */\n";
  return help;
}



String printHelpCOPS() {
  String help = "";
  help += "/*\n";
  help += " * Command: COPS\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   This command is used to select or query the current mobile network operator.\n";
  help += " *   You can use it to get information about the currently registered network operator,\n";
  help += " *   or to register on a specific operator manually.\n";
  help += " * \n";
  help += " * Query Mode (AT+COPS?):\n";
  help += " *   Queries the current network operator.\n";
  help += " * \n";
  help += " * Response Format:\n";
  help += " *   +COPS: <reg_status>,<format>,<operator_name>,<network_type>\n";
  help += " * \n";
  help += " * Where:\n";
  help += " *   <reg_status>    - Registration status\n";
  help += " *                      0 = Registered\n";
  help += " *   <format>        - Format of operator name\n";
  help += " *                      0 = Numeric, 1 = Alphanumeric, 2 = Both\n";
  help += " *   <operator_name> - The name of the operator (e.g., Telenor)\n";
  help += " *   <network_type>  - Type of network\n";
  help += " *                      2 = 2G, 3 = 3G, 4 = 4G, etc.\n";
  help += " * \n";
  help += " * Example Response:\n";
  help += " *   +COPS: 0,0,\"Telenor\",2\n";
  help += " * \n";
  help += " * Meaning:\n";
  help += " *   - Status: Registered\n";
  help += " *   - Format: Numeric\n";
  help += " *   - Operator: Telenor\n";
  help += " *   - Network Type: 2G\n";
  help += " * \n";
  help += " * Manual Mode (AT+COPS=<mode>,<format>,<operator>):\n";
  help += " *   This command is used to select a specific operator manually.\n";
  help += " * \n";
  help += " * Parameters:\n";
  help += " *   <mode>     - Registration mode\n";
  help += " *                0 = Automatic\n";
  help += " *                1 = Manual\n";
  help += " *                2 = Deregister\n";
  help += " *                3 = Search mode\n";
  help += " *   <format>   - Format of operator name\n";
  help += " *                0 = Numeric\n";
  help += " *                1 = Alphanumeric\n";
  help += " *                2 = Both\n";
  help += " *   <operator> - Operator code (MCC+MNC) or operator name\n";
  help += " * \n";
  help += " * Example Command:\n";
  help += " *   AT+COPS=1,2,\"310260\"  // Select T-Mobile USA\n";
  help += " * \n";
  help += " * Example Response:\n";
  help += " *   OK\n";
  help += " * \n";
  help += " * Usage:\n";
  help += " *   Type 'COPS' in the serial monitor to query or select a network operator.\n";
  help += " */\n";
  return help;
}



String printHelpCSQ() {
  String help = "";
  help += "/*\n";
  help += " * Command: CSQ\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   This command queries the signal quality of the network.\n";
  help += " *   It returns two values: RSSI (Received Signal Strength Indicator) and BER (Bit Error Rate).\n";
  help += " * \n";
  help += " * Response Format:\n";
  help += " *   +CSQ: <rssi>,<ber>\n";
  help += " * \n";
  help += " * Where:\n";
  help += " *   <rssi> - Received Signal Strength Indicator (RSSI)\n";
  help += " *            - Values: 0-31\n";
  help += " *            - 0-9: Very weak or no signal\n";
  help += " *            - 10-14: Weak signal\n";
  help += " *            - 15-20: Moderate signal\n";
  help += " *            - 21-30: Strong signal\n";
  help += " *            - 31: No signal or not measurable\n";
  help += " *   <ber>  - Bit Error Rate (BER)\n";
  help += " *            - 0: No errors\n";
  help += " *            - 1-7: Increasing number of errors\n";
  help += " * \n";
  help += " * Example Response:\n";
  help += " *   +CSQ: 20,0\n";
  help += " * \n";
  help += " * Meaning:\n";
  help += " *   - Signal Strength: 20 (moderate signal)\n";
  help += " *   - Bit Error Rate: 0 (no errors)\n";
  help += " * \n";
  help += " * Example Usage:\n";
  help += " *   1. Query signal quality: AT+CSQ\n";
  help += " *      Example Response: +CSQ: 20,0\n";
  help += " *   2. Query signal quality with weak signal: AT+CSQ\n";
  help += " *      Example Response: +CSQ: 5,0\n";
  help += " * \n";
  help += " * Usage:\n";
  help += " *   Type 'CSQ' in the serial monitor to retrieve and display the\n";
  help += " *   network signal quality information.\n";
  help += " */\n";
  return help;
}



String printHelpCREG() {
  String help = "";
  help += "/*\n";
  help += " * Command: CREG\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   This command queries the network registration status.\n";
  help += " *   It provides information on whether the device is registered\n";
  help += " *   on the home network or a roaming network.\n";
  help += " * \n";
  help += " * Response Format:\n";
  help += " *   +CREG: <stat>,<lac>,<ci>\n";
  help += " * \n";
  help += " * Where:\n";
  help += " *   <stat> - Registration status of the device.\n";
  help += " *            - 0: Not registered, searching for a network.\n";
  help += " *            - 1: Registered, home network.\n";
  help += " *            - 2: Not registered, but roaming.\n";
  help += " *            - 3: Registration denied.\n";
  help += " *            - 4: Unknown.\n";
  help += " *            - 5: Registered, roaming (specific to some networks).\n";
  help += " *   <lac>  - Location Area Code (LAC), a unique identifier for the location area.\n";
  help += " *   <ci>   - Cell Identity (CI), a unique identifier for the cell.\n";
  help += " * \n";
  help += " * Example Response:\n";
  help += " *   +CREG: 1,1234,5678\n";
  help += " * \n";
  help += " * Meaning:\n";
  help += " *   - Registered on home network.\n";
  help += " * \n";
  help += " * Example Usage:\n";
  help += " *   1. Query registration status: AT+CREG?\n";
  help += " *      Example Response: +CREG: 1,1234,5678\n";
  help += " *      - Registered on home network.\n";
  help += " *   2. Query registration status: AT+CREG?\n";
  help += " *      Example Response: +CREG: 0\n";
  help += " *      - Not registered, searching for a network.\n";
  help += " *   3. Query registration status: AT+CREG?\n";
  help += " *      Example Response: +CREG: 2,1234,5678\n";
  help += " *      - Registered while roaming.\n";
  help += " * \n";
  help += " * Usage:\n";
  help += " *   Type 'CREG' in the serial monitor to retrieve and display the\n";
  help += " *   network registration status.\n";
  help += " */\n";
  return help;
}



String printHelpCGREG() {
  String help = "";
  help += "/*\n";
  help += " * Command: CGREG\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   This command queries the GPRS network registration status.\n";
  help += " *   It provides information on whether the device is registered\n";
  help += " *   on the home network or a roaming network for GPRS services.\n";
  help += " * \n";
  help += " * Response Format:\n";
  help += " *   +CGREG: <stat>,<lac>,<ci>\n";
  help += " * \n";
  help += " * Where:\n";
  help += " *   <stat> - GPRS registration status of the device.\n";
  help += " *            - 0: Not registered, searching for a network.\n";
  help += " *            - 1: Registered, home network.\n";
  help += " *            - 2: Not registered, but roaming.\n";
  help += " *            - 3: Registration denied.\n";
  help += " *            - 4: Unknown.\n";
  help += " *            - 5: Registered, roaming (specific to some networks).\n";
  help += " *   <lac>  - Location Area Code (LAC), a unique identifier for the location area.\n";
  help += " *   <ci>   - Cell Identity (CI), a unique identifier for the cell.\n";
  help += " * \n";
  help += " * Example Response:\n";
  help += " *   +CGREG: 1,1234,5678\n";
  help += " * \n";
  help += " * Meaning:\n";
  help += " *   - Registered on home network for GPRS services.\n";
  help += " * \n";
  help += " * Example Usage:\n";
  help += " *   1. Query registration status: AT+CGREG?\n";
  help += " *      Example Response: +CGREG: 1,1234,5678\n";
  help += " *      - Registered on home network for GPRS services.\n";
  help += " *   2. Query registration status: AT+CGREG?\n";
  help += " *      Example Response: +CGREG: 0\n";
  help += " *      - Not registered, searching for a network.\n";
  help += " *   3. Query registration status: AT+CGREG?\n";
  help += " *      Example Response: +CGREG: 2,1234,5678\n";
  help += " *      - Registered while roaming for GPRS services.\n";
  help += " * \n";
  help += " * Usage:\n";
  help += " *   Type 'CGREG' in the serial monitor to retrieve and display the\n";
  help += " *   GPRS network registration status.\n";
  help += " */\n";
  return help;
}





String printHelpCPIN() {
  String help = "";
  help += "/*\n";
  help += " * Command: CPIN\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   This command queries the PIN status of the SIM card.\n";
  help += " *   It indicates whether the SIM card is locked and if a PIN is required.\n";
  help += " * \n";
  help += " * Response Format:\n";
  help += " *   +CPIN: <status>\n";
  help += " * \n";
  help += " * Where:\n";
  help += " *   <status> - The current status of the SIM card.\n";
  help += " *            - READY: The SIM card is ready to be used, no PIN required.\n";
  help += " *            - SIM PIN: A PIN code is required to unlock the SIM card.\n";
  help += " *            - SIM PUK: The SIM card is locked, and a PUK code is required.\n";
  help += " *            - PH_SIM PIN: The phone-specific SIM PIN is required.\n";
  help += " *            - PH_SIM PUK: The phone-specific SIM PUK is required.\n";
  help += " * \n";
  help += " * Example Response:\n";
  help += " *   +CPIN: READY\n";
  help += " * \n";
  help += " * Meaning:\n";
  help += " *   - SIM card is ready, no PIN required.\n";
  help += " * \n";
  help += " * Example Usage:\n";
  help += " *   1. Query SIM card PIN status: AT+CPIN?\n";
  help += " *      Example Response: +CPIN: READY\n";
  help += " *      - SIM card is ready, no PIN required.\n";
  help += " *   2. Query SIM card PIN status: AT+CPIN?\n";
  help += " *      Example Response: +CPIN: SIM PIN\n";
  help += " *      - SIM card is locked, PIN required.\n";
  help += " *   3. Query SIM card PIN status: AT+CPIN?\n";
  help += " *      Example Response: +CPIN: SIM PUK\n";
  help += " *      - SIM card is locked permanently, PUK required.\n";
  help += " *   4. Query SIM card PIN status: AT+CPIN?\n";
  help += " *      Example Response: +CPIN: PH_SIM PIN\n";
  help += " *      - Phone-specific PIN required.\n";
  help += " * \n";
  help += " * Usage:\n";
  help += " *   Type 'CPIN' in the serial monitor to retrieve and display the SIM card PIN status.\n";
  help += " */\n";
  return help;
}


String printHelpCGATT() {
  String help = "";
  help += "/*\n";
  help += " * Command: CGATT\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   This command queries the current GPRS attachment status.\n";
  help += " *   It checks whether the module is attached or detached from the GPRS network.\n";
  help += " * \n";
  help += " * Response Format:\n";
  help += " *   +CGATT: <status>\n";
  help += " * \n";
  help += " * Where:\n";
  help += " *   <status> - Indicates the GPRS attachment status.\n";
  help += " *            - 0: The GPRS service is detached.\n";
  help += " *            - 1: The GPRS service is attached.\n";
  help += " * \n";
  help += " * Example Response:\n";
  help += " *   +CGATT: 1\n";
  help += " * \n";
  help += " * Meaning:\n";
  help += " *   - The module is attached to the GPRS network.\n";
  help += " * \n";
  help += " * Example Usage:\n";
  help += " *   1. Query GPRS attachment status: AT+CGATT?\n";
  help += " *      Example Response: +CGATT: 1\n";
  help += " *      - The module is attached to the GPRS network.\n";
  help += " *   2. Attach the module to GPRS network: AT+CGATT=1\n";
  help += " *      Example Response: OK\n";
  help += " *      - The module is successfully attached to the GPRS network.\n";
  help += " *   3. Detach the module from GPRS network: AT+CGATT=0\n";
  help += " *      Example Response: OK\n";
  help += " *      - The module is successfully detached from the GPRS network.\n";
  help += " * \n";
  help += " * Usage:\n";
  help += " *   Type 'CGATT' in the serial monitor to retrieve and display the GPRS attachment status.\n";
  help += " */\n";
  return help;
}


String printHelpCCLK() {
  String help = "";
  help += "/*\n";
  help += " * Command: CCLK\n";
  help += " * \n";
  help += " * Description:\n";
  help += " *   This command queries the current date and time as provided by the GSM network.\n";
  help += " *   It returns the local time in the format: \"yy/MM/dd,hh:mm:ss+zz\"\n";
  help += " * \n";
  help += " * Response Format:\n";
  help += " *   +CCLK: <time>\n";
  help += " * \n";
  help += " * Where:\n";
  help += " *   <time> is in the format: yy/MM/dd,hh:mm:ss+zz\n";
  help += " *     - yy: Last two digits of the year (e.g., 23 for 2023)\n";
  help += " *     - MM: Month (01-12)\n";
  help += " *     - dd: Day of the month (01-31)\n";
  help += " *     - hh: Hour (00-23)\n";
  help += " *     - mm: Minute (00-59)\n";
  help += " *     - ss: Second (00-59)\n";
  help += " *     - +zz: Time zone offset from UTC (e.g., +02, -05)\n";
  help += " * \n";
  help += " * Example Response:\n";
  help += " *   +CCLK: \"23/04/28,15:30:45+02\"\n";
  help += " * \n";
  help += " * Meaning:\n";
  help += " *   - The date is 28th April 2023, time is 15:30:45 (3:30:45 PM), time zone is UTC+2.\n";
  help += " * \n";
  help += " * Example Usage:\n";
  help += " *   1. Query Current Time: AT+CCLK?\n";
  help += " *      Example Response: +CCLK: \"23/04/28,15:30:45+02\"\n";
  help += " *      - The date is 28th April 2023, time is 15:30:45 (3:30:45 PM), time zone is UTC+2.\n";
  help += " * \n";
  help += " * Usage:\n";
  help += " *   Type 'CCLK' in the serial monitor to retrieve and display the current date and time.\n";
  help += " */\n";
  return help;
}




String printHelpCCID() {
  String help = "";
  help += "/*\n";
  help += " * COMMAND: CCID\n";
  help += " * \n";
  help += " * DESCRIPTION:\n";
  help += " *   Retrieves the ICCID (Integrated Circuit Card Identifier)\n";
  help += " *   - Unique SIM identifier across mobile networks\n";
  help += " *   - Essential for SIM card identification\n";
  help += " * \n";
  help += " * RESPONSE FORMAT:\n";
  help += " *   +CCID: <ICCID>\n";
  help += " * \n";
  help += " * ICCID DETAILS:\n";
  help += " *   - 19-20 digit unique number\n";
  help += " *   - Contains:\n";
  help += " *     * Country code\n";
  help += " *     * Issuer identifier\n";
  help += " *     * Serial number\n";
  help += " *     * Check digit\n";
  help += " * \n";
  help += " * EXAMPLE OUTPUT:\n";
  help += " *   +CCID: \"89860000000000000001\"\n";
  help += " *   - Unique SIM identifier\n";
  help += " * \n";
  help += " * USAGE EXAMPLES:\n";
  help += " *   1. CCID          - Basic query\n";
  help += " *   2. CCID help     - Show this help\n";
  help += " * \n";
  help += " * TYPICAL USAGE:\n";
  help += " *   Check SIM card authenticity\n";
  help += " *   Verify SIM card replacement\n";
  help += " *   Troubleshoot network issues\n";
  help += " */\n\n";
  return help;
}



String printHelpCMD() {
  String help = "";
  help += "/*\n";
  help += " * COMMAND: cmd\n";
  help += " * \n";
  help += " * DESCRIPTION:\n";
  help += " *   Direct AT command interface for SIM800 module.\n"; 
  help += " *   Sends raw AT commands for advanced troubleshooting\n";
  help += " *   and custom functionality.\n";
  help += " * \n";
  help += " * SYNTAX:\n";
  help += " *   cmd <AT_command>\n";
  help += " * \n";
  help += " * EXAMPLES:\n";
  help += " *   1. cmd AT+CSQ\n";
  help += " *      - Checks signal quality\n";
  help += " *   2. cmd AT+CREG?\n";
  help += " *      - Checks network registration\n";
  help += " *   3. cmd AT+COPS?\n";
  help += " *      - Shows current network operator\n";
  help += " * \n";
  help += " * NOTES:\n";
  help += " *   - Supports all standard AT commands\n";
  help += " *   - Returns raw module responses\n";
  help += " *   - Use for debugging and testing\n";
  help += " * \n"; 
  help += " * USAGE:\n";
  help += " *   cmd <command>  - Execute AT command\n";
  help += " *   cmd help       - Show this message\n";
  help += " */\n\n";
  return help;
}



String printHelpBattery() {
  String help = "";
  help += "/*\n";
  help += " * COMMAND: battery\n";
  help += " * \n";
  help += " * DESCRIPTION:\n";
  help += " *   Queries the SIM800 module for battery status information\n";
  help += " *   using AT+CBC command.\n";
  help += " * \n";
  help += " * RESPONSE FORMAT:\n";
  help += " *   +CBC: <bcs>,<bcl>,<voltage>\n";
  help += " * \n";
  help += " * PARAMETERS:\n";
  help += " *   <bcs> - Battery Charge Status:\n";
  help += " *           0 = Unknown\n";
  help += " *           1 = Charging\n";
  help += " *           2 = Not charging\n";
  help += " *   <bcl> - Charge Level (0-100%)\n";
  help += " *   <voltage> - Voltage in mV\n";
  help += " * \n";
  help += " * EXAMPLE:\n";
  help += " *   +CBC: 1,85,4180\n";
  help += " *   Interpretation:\n";
  help += " *   - Charging (1)\n";
  help += " *   - 85% charge\n";
  help += " *   - 4.18V (4180mV)\n";
  help += " * \n";
  help += " * USAGE:\n";
  help += " *   Enter 'battery' to check status\n";
  help += " *   'battery help' shows this message\n";
  help += " */\n\n";
  return help;
}




/*
 * ===========================================================================
 *                          SIM800 MODULE SECTION
 * ===========================================================================
 * 
 * Description:
 *   This section contains all functions related to the SIM800 GSM/GPRS module,
 *   including AT command handling, network operations, SMS, and cellular data.
 * 
 * Key Features:
 *   - AT command interface for module control
 *   - SMS sending/receiving functionality
 *   - Battery and signal status monitoring
 *   - Network registration handling
 *   - GPRS data connectivity
 * 
 * Hardware Configuration:
 *   - Connected via UART with hardware/software serial
 *   - Typical baud rate: 9600 or 115200
 *   - Power: 3.4V-4.4V (recommended stable 4.0V)
 * 
 * ===========================================================================
 */


// Handle SIM800 response and process SMS commands
String handleSIM800Response() {
  static String sim800Buffer = "";
  static String senderNumber = "";
  static bool awaitingMessage = false;

  String receivedMessage = "";

  while (serialSIM800.available()) {
    char c = serialSIM800.read();
    sim800Buffer += c;

    if (c == '\n') {
      sim800Buffer.trim();

      if (sim800Buffer.startsWith("+CMT:")) {
        int quote1 = sim800Buffer.indexOf("\"");
        int quote2 = sim800Buffer.indexOf("\"", quote1 + 1);
        senderNumber = sim800Buffer.substring(quote1 + 1, quote2);
        awaitingMessage = true;
      } 
      else if (awaitingMessage) {
        String smsMessage = sim800Buffer;
        smsMessage.trim();

        Serial.println("\n[SMS RECEIVED]");
        Serial.println("From: " + senderNumber);
        Serial.println("Message: " + smsMessage);

        #if DEBUG_MODE
          if (senderNumber == ADMIN_PHONE) {
            Serial.printf("Sender number matches ADMIN_PHONE: %s\n", senderNumber.c_str());
          } else {
            Serial.printf("Sender number does NOT match ADMIN_PHONE.\nReceived: %s\nExpected: %s\n", senderNumber.c_str(), ADMIN_PHONE);
          }

          if (smsMessage == SECRET_COMMAND) {
            Serial.printf("Received SMS matches SECRET_COMMAND: %s\n", smsMessage.c_str());
          } else {
            Serial.printf("Received SMS does NOT match SECRET_COMMAND.\nReceived: %s\nExpected: %s\n", smsMessage.c_str(), SECRET_COMMAND);
          }
        #endif


        if (senderNumber == ADMIN_PHONE && smsMessage == SECRET_COMMAND) {
          Serial.println("Authorized command received. Activating relay!");
          digitalWrite(RELAY_PIN, HIGH);
          delay(1000);
          digitalWrite(RELAY_PIN, LOW);
        }

        receivedMessage = smsMessage;

        awaitingMessage = false;
        senderNumber = "";
      }

      sim800Buffer = "";
    }
  }

  return receivedMessage;
}





/*
  ******************************************************************************
  SIM800 COMMAND INTERFACE
  ******************************************************************************
  Sends raw AT commands to SIM800 module and captures the complete response.
  Implements timeout handling and response buffering for reliable communication.

  PARAMETERS:
  - cmd : The complete AT command string to send (e.g. "AT+CSQ")

  FEATURES:
  - Automatic response buffering with 5 second timeout
  - Handles both single-line and multi-line responses
  - Includes command echo in response
  - Processes incoming data in real-time

  USAGE:
  String response = sendCommand("AT+CSQ");
  // Returns raw module response including command echo
  ******************************************************************************
*/
String sendCommand(String cmd) {
  
  // ESP.wdtFeed();                      // Reset watchdog
  
  // Clear the serial buffer first
  // while(serialSIM800.available()) {
  //   serialSIM800.read();
  //   delay(1);
  // }

  // delay(100);

  // Send the command with proper line endings
  Serial.printf("Before: %s\n",cmd);
  serialSIM800.println(cmd);

  delay(100);
  String response = "";

  unsigned long timeout = 5000;

  // Collect response with timeout
  unsigned long start = millis();

  while(millis() - start < timeout) {
    while(serialSIM800.available()) {
      char c = serialSIM800.read();
      response += c;
      
      // Optional: Add small delay between characters
      delay(1);
    }
    
    // Early exit if we got a complete response
    if(response.endsWith("OK\r\n") || response.endsWith("ERROR\r\n")) {
      break;
    }
  }

  // Clean up the response
  response.trim();
  response += "\n";

  return response;
}





/*
 * Function: processSMSCommand
 * ---------------------------
 * Handles SMS-related commands sent to the system. This function interprets specific text-based commands
 * and communicates with a SIM800 GSM module via AT commands to perform various SMS operations.
 *
 * Supported Commands:
 *  - "sms help"           : Displays help text listing available SMS commands and their syntax.
 *  - "sms list"           : Lists all SMS messages stored on the SIM card (uses AT+CMGL="ALL").
 *  - "sms read <n>"       : Reads the SMS message at position <n> (uses AT+CMGR=n).
 *  - "sms delete <n>"     : Deletes the SMS message at position <n> (uses AT+CMGD=n).
 *  - "sms send <num> <msg>": Sends an SMS with <msg> to the phone number <num> (uses AT+CMGS).
 *  - "sms mode <1/0>"     : Sets SMS mode to text (1) or PDU (0) using AT+CMGF.
 *
 * Parameters:
 *  - command (String): A string representing the full command input to be parsed and executed.
 *
 * Returns:
 *  - String: A response message indicating success, failure, or help instructions depending on the command.
 *
 * Notes:
 *  - This function uses delays between certain AT commands to ensure the SIM800 has time to process them.
 *  - It directly writes to the serialSIM800 stream, which should be initialized and configured beforehand.
 *  - Commands are case-sensitive and expected in lowercase.
 *  - The 'sms send' command requires CTRL+Z (ASCII 26) to signal end of message.
 */

String processSMSCommand(String command) {
  command.trim();
  Serial.print(command);

  if (command == "sms help") {
    String help = "";
    help += "\nSMS Commands:\n";
    help += "----------------------\n";
    help += "sms list       - List all SMS messages\n";
    help += "sms read <n>   - Read SMS at position n\n";
    help += "sms delete <n> - Delete SMS at position n\n";
    help += "sms send <num> <msg> - Send SMS to number\n";
    help += "sms mode <1/0> - Set SMS text (1) or PDU (0) mode\n";
    help += "----------------------\n";
    return help;
  }


  // Lists all SMS messages using AT+CMGL
  else if (command == "sms list") {
    return sendCommand("AT+CMGL=\"ALL\"");              // List all SMS
  }

  // Reads an SMS message at a specific index
  else if (command.startsWith("sms read ")) {
    int index = command.substring(9).toInt();           // Extract index from command
    if (index > 0) {
      return sendCommand("AT+CMGR=" + String(index));   // Read SMS at index
    }
    return "\nInvalid index. Usage: sms read <n>";
  }


  // Deletes an SMS message at a specific index
  else if (command.startsWith("sms delete ")) {
    int index = command.substring(11).toInt();            // Extract index
    if (index > 0) {
      return sendCommand("AT+CMGD=" + String(index));     // Delete SMS at index
    }
    return "\nInvalid index. Usage: sms delete <n>";
  }


  // Sends an SMS to the given number with the given message
  else if (command.startsWith("sms send ")) {

    int firstSpace = command.indexOf(' ', 9);               // Find space between number and message
    if (firstSpace == -1) {
      return "\nInvalid format. Use: sms send <number> <message>";
    }

    String number  = command.substring(9, firstSpace);             // Extract number
    String message = command.substring(firstSpace + 1);     // Extract message content


    DEBUG_PRINTF("\n[DEBUG] Numb: %s", number);
    DEBUG_PRINTF("\n[DEBUG] Message: %s", message);


    delay(300);
    serialSIM800.println("AT+CMGS=\"" + number + "\"");     // Prepare to send SMS
    delay(300);
    serialSIM800.print(message);                            // Write SMS content
    delay(100);
    serialSIM800.write(26);                                 // Send Ctrl+Z (ASCII 26) to finish SMS input
    return "\nSending SMS to " + number + "...";
  }



  // Sets SMS mode to text (1) or PDU (0)
  else if (command.startsWith("sms mode ")) {
    String mode = command.substring(9);
    return sendCommand("AT+CMGF=" + mode);                    // Set SMS mode
  }


  // Unknown or invalid command
  return "\nUnknown SMS command. Type 'sms help' for list.";
}


















/*
 * ===========================================================================
 *                          ESP-07 COMMAND SECTION
 * ===========================================================================
 * 
 * Description:
 *   This section contains all commands and functions specific to the ESP-07 module,
 *   including system information, memory status, and module control.
 * 
 * Key Features:
 *   - Hardware information retrieval
 *   - Flash memory diagnostics
 *   - System control commands
 *   - Dual output (Serial + Web interface)
 * 
 * Command Summary:
 *   espinfo    - Displays detailed ESP chip information
 *   flashinfo  - Shows flash memory configuration
 *   reset      - Performs hardware reset
 *   restart    - Software restart of the module
 * 
 * Information Provided:
 *   - Chip identification and version
 *   - CPU frequency and performance
 *   - Flash memory size/speed/mode
 *   - Memory allocation details
 *   - Sketch storage information
 * 
 * Usage Examples:
 *   [Serial Terminal]
 *   > espinfo
 *   > flashinfo
 *   > restart
 * 
 * ===========================================================================
 */


// Get ESP chip information
String getESPInfo() {
  String info = "";
  info += "/*\n";
  info += " * ESP Chip Information:\n";
  info += " * ---------------------\n";
  info += " * Chip ID: 0x" + String(ESP.getChipId(), HEX) + "\n";
  info += " * CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
  info += " * Flash Chip ID: 0x" + String(ESP.getFlashChipId(), HEX) + "\n";
  info += " * Flash Chip Size: " + String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB\n";
  info += " * Flash Chip Speed: " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz\n";
  info += " * Flash Chip Mode: " + String(ESP.getFlashChipMode()) + "\n";
  info += " * Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
  info += " * Sketch Size: " + String(ESP.getSketchSize()) + " bytes\n";
  info += " * Free Sketch Space: " + String(ESP.getFreeSketchSpace()) + " bytes\n";
  info += " */\n\n";
  return info;
}





/*
  ******************************************************************************
  FLASH MEMORY INFORMATION
  ******************************************************************************
  Retrieves and formats detailed information about the ESP8266's flash memory.

  RETURNS:
  - Formatted string containing:
    * Flash chip ID (hexadecimal)
    * Total flash size (MB)
    * Operating speed (MHz)
    * Access mode
    * Wrapped in comment-style formatting

  DATA SOURCES:
  - ESP.getFlashChipId()
  - ESP.getFlashChipRealSize()
  - ESP.getFlashChipSpeed()
  - ESP.getFlashChipMode()

  USAGE:
  Serial.print(getFlashInfo());
  // Outputs complete flash memory report
  ******************************************************************************
*/String getFlashInfo() {
  String info = "";

  info += "/*\n";
  info += " * Flash Memory Information:\n";
  info += " * ---------------------\n";
  info += " * Flash Chip ID: 0x" + String(ESP.getFlashChipId(), HEX) + "\n";
  info += " * Flash Chip Size: " + String(ESP.getFlashChipRealSize() / (1024 * 1024)) + " MB\n";
  info += " * Flash Chip Speed: " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz\n";
  info += " * Flash Chip Mode: " + String(ESP.getFlashChipMode()) + "\n";
  info += " * ---------------------\n";

  info += " */\n\n";              // Added extra newline to match original

  return info;
}









/*
  ******************************************************************************
  SYSTEM STATUS REPORT
  ******************************************************************************
  Generates a comprehensive snapshot of current system status and metrics.

  RETURNS:
  - Formatted string containing:
    * Client connection statistics
    * Memory usage and fragmentation
    * WiFi network information
    * Visual header/footer separators

  MONITORED METRICS:
  - Connected/active clients
  - Command buffer size
  - Free heap memory
  - Heap fragmentation
  - WiFi SSID/RSSI/IP

  USAGE:
  Serial.print(getSystemStatus());
  // Displays current system health status
  ******************************************************************************
*/
String getSystemStatus() {
  String status = "";

  status += "/*\n";
  status += " * SYSTEM STATUS INFORMATION:\n";
  status += " * -------------------------\n";
  status += " * Memory: FreeHeap=" + String(ESP.getFreeHeap()) + " bytes, Fragmentation=" + String(ESP.getHeapFragmentation()) + "%\n";
  status += " * WiFi: SSID=" + WiFi.SSID() + ", RSSI=" + String(WiFi.RSSI()) + " dBm, IP=" + WiFi.localIP().toString() + "\n";
  status += " * -------------------------\n";

  status += " */\n\n";                    // Extra newline for spacing
  return status;
}




/*
  ******************************************************************************
  SCREEN CLEAR UTILITY
  ******************************************************************************
  Generates a string of newlines to simulate terminal screen clearing.

  FEATURES:
  - Returns 50 newline characters
  - Creates vertical scroll effect
  - Non-destructive alternative to system clears
  - Consistent behavior across platforms

  USAGE:
  Serial.print(clearScreen());
  // Clears terminal by scrolling content up
  ******************************************************************************
*/
String clearScreen() {
  String clearString = "";

  // Create a string with 50 newlines to simulate screen clearing
  for (int i = 0; i < 50; i++) {
    clearString += "\n";
  }

  return clearString;
}







/*
  ******************************************************************************
  RESTART NOTIFICATION FUNCTION
  ******************************************************************************
  Generates and returns a formatted restart countdown message without
  actually performing the restart. Useful for displaying restart information
  before calling the actual restart function.

  RETURNS:
  - String containing:
    * Restart initiation message
    * 3-second countdown notice
    * Visual formatting

  USAGE:
  Serial.print(showRestartMessage());
  // Displays restart notice without executing restart
  ******************************************************************************
*/
String showRestartMessage() {
  String message = "";

  message += "/*\n";
  message += " * SYSTEM RESTART NOTIFICATION:\n";
  message += " * ---------------------------\n";
  message += " * WARNING: System restart initiated\n";
  message += " * \n";
  message += " * The device will restart in:\n";
  message += " * 3 seconds...\n";
  message += " * \n";
  message += " * ACTION REQUIRED:\n";
  message += " * - Prepare for disconnection\n";
  message += " * ---------------------------\n";

  message += " */\n\n";           // Extra newline for spacing
  return message;
}









/*
  ******************************************************************************
  SYSTEM RESTART EXECUTION FUNCTION
  ******************************************************************************
  Performs an immediate system restart of the ESP8266/ESP32 with optional
  delay and visual feedback. Includes safety delay to ensure message delivery.

  PARAMETERS:
  - delayMs: Optional delay in milliseconds before restart (default 3000ms)

  BEHAVIOR:
  - Adds small delay to ensure serial output is flushed
  - Calls ESP.restart() which never returns
  - All pending operations are aborted

  WARNING:
  - This function does not return
  - Unsaved data will be lost
  - Network connections will be terminated

  USAGE:
  performRestart(); // Default 3 second delay
  performRestart(5000); // 5 second delay
  ******************************************************************************
*/
void performRestart(unsigned int delayMs = 3000) {

  // Small delay to ensure message delivery
  delay(100);
  
  // Optional countdown delay
  if (delayMs > 0) {
    unsigned long start = millis();
    while (millis() - start < delayMs) {
      delay(100);
    }
  }
  
  // Execute restart (this never returns)
  ESP.restart();
}











/*
  ******************************************************************************
  SYSTEM HEALTH MONITOR
  ******************************************************************************
  Periodically checks and reports critical system metrics.

  MONITORED PARAMETERS:
  - Memory usage (every 60 seconds)
  - WiFi connection status (every 60 seconds)
  - Automatic status change detection

  FEATURES:
  - Non-blocking interval checking
  - Critical condition alerts
  - Stable state confirmation
  - Automatic debug output

  USAGE:
  Call checkSystemStatus() in main loop()
  // Automatically reports every 60 seconds
  ******************************************************************************
*/
void checkSystemStatus() {

  // Get current system time in milliseconds
  unsigned long currentTime = millis();
  
  // Check if the defined interval has passed since the last check
  if (currentTime - lastCheckTime >= CHECK_INTERVAL || lastCheckTime == 0) {

    lastCheckTime = currentTime;            // Update the last check timestamp
    
    // -------- MEMORY CHECK --------
    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 5000) {
      // Critical memory warning if heap is low
      DEBUG_PRINTF("\n[CRITICAL] Low memory: %d bytes\n", freeHeap);
    } else {
      // Report current available memory
      DEBUG_PRINTF("\n[OK] Memory available: %d bytes\n", freeHeap);
    }
    
     // -------- WIFI STATUS CHECK --------
    static int lastWifiStatus = WL_IDLE_STATUS;         // Store previous WiFi connection status
    int currentWifiStatus = WiFi.status();              // Get current WiFi status
    
    if (currentWifiStatus != lastWifiStatus) {

      // If WiFi status has changed, log the transition
      DEBUG_PRINTF("\n[WIFI] Status changed: %d -> %d\n", lastWifiStatus, currentWifiStatus);
      lastWifiStatus = currentWifiStatus;               // Update the previous status
    
    } else {
    
      // If status hasn't changed, confirm stability
      DEBUG_PRINTF("\n[OK] WiFi status stable: %d\n", currentWifiStatus);
    }
  }
}








/*
  ******************************************************************************
  COMMAND PROCESSING ENGINE
  ******************************************************************************
  Handles all user input commands and routes them to appropriate handlers.
  Supports three command categories with detailed help systems:

  CATEGORIES:
  1. SYSTEM COMMANDS:
     - help/helpesp/helpsim - Comprehensive help systems
     - cls/clearscreen      - Display management
     - espinfo/flashinfo    - Hardware information
     - systemstatus         - Runtime diagnostics
     - reset/restart        - System control

  2. SIM800 AT COMMANDS:
     - at/status/csq/creg - Basic module queries
     - cgreg/cops/cpin    - Network status commands
     - cgatt/battery/time - Device monitoring
     - imei/imsi/ccid     - Identification queries
     - All commands support 'help' suffix for documentation

  3. ADVANCED OPERATIONS:
     - sms    - SMS message processing
     - cmd    - Raw AT command passthrough
     - Custom command extensions


  USAGE NOTES:
  1. Command variants supported (e.g., "status" vs "status help")
  2. All AT commands include expected response format documentation
  3. System commands provide immediate feedback
  4. Add new commands in the appropriate category section

  ******************************************************************************
*/

// Handles incoming serial commands from user input
void processSerialCommand(String command) {

  // Convert command to lowercase for easier comparison
  command.toLowerCase();

  // --- General Help Commands ---
  if (command == "help") {
    Serial.print(printHelpAll());           // Print all available commands
  } 
  else if (command == "helpesp") {
    Serial.print(printHelpESP());           // Print ESP-specific commands
  }
  else if (command == "helpsim") {
    Serial.print(printHelpSIM());           // Print SIM800-specific commands
  }

  // --- Screen Clearing Commands ---
  else if (command == "cls") {
    Serial.print(clearScreen());            // Clear screen using custom function
  }
  else if (command == "clearscreen") {

     // Clear terminal screen using ANSI escape codes
    Serial.print("\033[2J");                // ANSI escape sequence to clear screen
    Serial.print("\033[H");                 // Move cursor to the top-left corner
  }

  // --- Linux Joke Commands ---
  else if (command == "ls" || command == "rm" || command == "mv" || 
           command == "chmod" || command == "grep" || command == "find" ||
           command == "diff" || command == "dd" || command == "kill" ||
           command == "sudo" || command == "whoami" || command == "history" ||
           command == "ping" || command == "ssh" || command == "ifconfig" ||
           command == "tail" || command == "vim" || command == "nano" ||
           command == "cron" || command == "bash" || command == "dir") {

    // Return a fun message
    Serial.println(getRandomJoke(command));
  }
  // --- ESP Device Commands ---
  else if (command == "espinfo") {
    Serial.print(getESPInfo());               // Print ESP module info
  }
  else if (command == "flashinfo") {
    Serial.print(getFlashInfo());             // Show flash memory info
  }
  else if (command == "systemstatus") {
    Serial.print(getSystemStatus());          // Return memory and WiFi info
  }
  else if (command == "listfiles") {
    Serial.print(listFiles("/"));             // List all files in root directory
  }
  else if (command == "reset" || command == "restart") {

    // Display restart message first
    Serial.print(showRestartMessage());

    // Then perform the actual restart after 3 seconds
    performRestart();

    // Or with custom delay:
    // performRestart(5000); // 5 second delay

  }


  // --- SIM800 AT Commands ---
  else if (command == "at" || command == "at help") {
    if (command == "at help") {
      Serial.print(printHelpAT());                    // AT command help
    } else {
      Serial.println("");
      Serial.println("Sending AT (Attention Command)");
      Serial.println("Expected Response: OK");
      Serial.println("-------------------------------");

      Serial.print(sendCommand("AT"));                // Send basic AT command
    }
  }
  else if (command == "csq" || command == "csq help") {
    if (command == "csq help") {
      Serial.println("");
      Serial.print(printHelpCSQ());
    } else {
      Serial.println("");
      Serial.println("Sending AT+CSQ (Signal Quality Report)");
      Serial.println("Expected Response: +CSQ: <rssi>,<ber>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+CSQ"));
    }
  }
  else if (command == "creg" || command == "creg help") {
    if (command == "creg help") {
      Serial.println("");
      Serial.print(printHelpCREG());
    } else {
      Serial.println("");
      Serial.println("Sending AT+CREG? (Network Registration Status)");
      Serial.println("Expected Response: +CREG: <n>,<stat>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+CREG?"));
    }
  }
  else if (command == "cgreg" || command == "cgreg help") {
    if (command == "cgreg help") {
      Serial.println("");
      Serial.print(printHelpCGREG());
    } else {
      Serial.println("");
      Serial.println("Sending AT+CGREG? (GPRS Network Registration Status)");
      Serial.println("Expected Response: +CGREG: <stat>,<lac>,<ci>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+CGREG?"));
    }
  }
  else if (command == "cops" || command == "cops help") {
    if (command == "cops help") {
      Serial.println("");
      Serial.print(printHelpCOPS());
    } else {
      Serial.println("");
      Serial.println("Sending AT+COPS? (Operator Information)");
      Serial.println("Expected Response: +COPS: <mode>,<format>,<oper>,<AcT>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+COPS?"));
    }
  }
  else if (command == "cpin" || command == "cpin help") {
    if (command == "cpin help") {
      Serial.println("");
      Serial.print(printHelpCPIN());
    } else {
      Serial.println("");
      Serial.println("Sending AT+CPIN? (SIM Card Status)");
      Serial.println("Expected Response: +CPIN: <status>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+CPIN?"));
    }
  }
  else if (command == "cgatt" || command == "cgatt help") {
    if (command == "cgatt help") {
      Serial.println("");
      Serial.print(printHelpCGATT());
    } else {
      Serial.println("");
      Serial.println("Sending AT+CGATT? (GPRS Attach Status)");
      Serial.println("Expected Response: +CGATT: <status>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+CGATT?"));
    }
  }
  else if (command == "battery" || command == "battery help") {
    if (command == "battery help") {
      Serial.println("");
      Serial.print(printHelpBattery());
    } else {
      Serial.println("");
      Serial.println("Sending AT+CBC (Battery Status)");
      Serial.println("Expected Response: +CBC: <bcs>,<bcl>,<voltage>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+CBC"));
    }
  }
  else if (command == "time" || command == "time help") {
    if (command == "time help") {
      Serial.println("");
      Serial.print(printHelpCCLK());
    } else {
      Serial.println("");
      Serial.println("Sending AT+CCLK? (Current Time from Network)");
      Serial.println("Expected Response: +CCLK: \"yy/MM/dd,hh:mm:ss±zz\"");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+CCLK?"));
    }
  }
  else if (command == "imei" || command == "imei help") {
    if (command == "imei help") {
      Serial.println("");
      Serial.print(printHelpIMEI());
    } else {
      Serial.println("");
      Serial.println("Sending AT+GSN (Request for IMEI)");
      Serial.println("Expected Response: <IMEI>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+GSN"));
    }
  }
  else if (command == "imsi" || command == "imsi help") {
    if (command == "imsi help") {
      Serial.println("");
      Serial.print(printHelpIMSI());
    } else {
      Serial.println("");
      Serial.println("Sending AT+CIMI (Request for IMSI)");
      Serial.println("Expected Response: <IMSI>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+CIMI"));
    }
  }
  else if (command == "ccid" || command == "ccid help") {
    if (command == "ccid help") {
      Serial.println("");
      Serial.print(printHelpCCID());
    } else {
      Serial.println("");
      Serial.println("Sending AT+CCID (Request for SIM Card ICCID)");
      Serial.println("Expected Response: +CCID: <ICCID>");
      Serial.println("-------------------------------");
      Serial.print(sendCommand("AT+CCID"));
    }
  }

  // --- SMS Command Handler ---
  else if (command.startsWith("sms")) {
    Serial.print(processSMSCommand(command));
  }


  // --- Custom AT Commands ---
  else if (command.startsWith("cmd ") && command != "cmd help") {

    // Extract the custom command after "cmd " keyword
    String customCommand = command.substring(4);
    
    // Call cmd function to send the custom command
    Serial.print(sendCommand(customCommand));
  }
  else if (command == "cmd help") {
    Serial.println("");
    Serial.print(printHelpCMD());               // Help for custom command
  }


  // --- Unknown Command Fallback ---
  else {
    Serial.println("Unknown command. Type 'help' for list.");
  }
}















/*
  ******************************************************************************
  UNIFIED COMMAND HANDLER
  ******************************************************************************
  Processes and executes system commands, returning formatted string responses.
  Handles all ESP, SIM800, and system commands through a single interface.

  CATEGORIES:
  1. SYSTEM COMMANDS:
     - help/helpesp/helpsim - Display help systems
     - cls/clearscreen - Screen management
     - espinfo/flashinfo/systemstatus - System information
     - reset/restart - Device control

  2. SIM800 COMMANDS:
     - at/status/csq/creg/cgreg - Basic module queries
     - cops/cpin/cgatt - Network status
     - battery/time/imei/imsi/ccid - Device information
     - All support 'help' suffix for documentation

  3. ADVANCED OPERATIONS:
     - sms - SMS message processing
     - cmd - Raw AT command passthrough

  RESPONSE FORMAT:
  - Returns complete response strings for all commands
  - Includes command headers and formatted output
  - Error messages for invalid commands
  - Help text for all supported commands

  ******************************************************************************
*/
String processWEBCommand(String command) {
  command.trim();
  String response = "";
  
  // System commands
  if (command == "help") return printHelpAll();
  else if (command == "helpesp") return printHelpESP();
  else if (command == "helpsim") return printHelpSIM();
  else if (command == "espinfo") return getESPInfo();
  else if (command == "flashinfo") return getFlashInfo();
  else if (command == "listfiles") return listFiles("/");
  else if (command == "systemstatus") return getSystemStatus();

  else if (command == "reset" || command == "restart") {
      String restartMessage = showRestartMessage();
      Serial.print(restartMessage);
      delay(1000);                        // Ensure message is sent
      performRestart();                   // This will never return
      return restartMessage;              // This line is unreachable but satisfies compiler
    
    // Note: performRestart() won't return, so no code executes after it
  }

  // Linux joke commands
  else if (command == "ls" || command == "rm" || command == "mv" || 
           command == "chmod" || command == "grep" || command == "find" ||
           command == "diff" || command == "dd" || command == "kill" ||
           command == "sudo" || command == "whoami" || command == "history" ||
           command == "ping" || command == "ssh" || command == "ifconfig" ||
           command == "tail" || command == "vim" || command == "nano" ||
           command == "cron" || command == "bash" || command == "dir") {
    return getRandomJoke(command);
  }

  // SIM800 basic commands
  else if (command == "at help") return printHelpAT();
  else if (command == "at") return sendWEBCommand("AT");
  else if (command == "csq help") return printHelpCSQ();
  else if (command == "csq") return sendWEBCommand("AT+CSQ");
  else if (command == "creg help") return printHelpCREG();
  else if (command == "creg") return sendWEBCommand("AT+CREG?");
  else if (command == "cgreg help") return printHelpCGREG();
  else if (command == "cgreg") return sendWEBCommand("AT+CGREG?");
  else if (command == "cops help") return printHelpCOPS();
  else if (command == "cops") return sendWEBCommand("AT+COPS?");
  else if (command == "cpin help") return printHelpCPIN();
  else if (command == "cpin") return sendWEBCommand("AT+CPIN?");
  else if (command == "cgatt help") return printHelpCGATT();
  else if (command == "cgatt") return sendWEBCommand("AT+CGATT?");
  else if (command == "battery help") return printHelpBattery();
  else if (command == "battery") return sendWEBCommand("AT+CBC");
  else if (command == "time help") return printHelpCCLK();
  else if (command == "time") return sendWEBCommand("AT+CCLK?");
  else if (command == "imei help") return printHelpIMEI();
  else if (command == "imei") return sendWEBCommand("AT+GSN");
  else if (command == "imsi help") return printHelpIMSI();
  else if (command == "imsi") return sendWEBCommand("AT+CIMI");
  else if (command == "ccid help") return printHelpCCID();
  else if (command == "ccid") return sendWEBCommand("AT+CCID");

  // SMS and custom commands
  else if (command.startsWith("sms")) return processSMSCommand(command);
  else if (command == "cmd help") return printHelpCMD();
  else if (command.startsWith("cmd ")) return sendCommand(command.substring(4));
  else return "\nUnknown command: " + command + "\nType 'help' for available commands\n";
}












/*
  ******************************************************************************
  UNIX COMMAND JOKE GENERATOR
  ******************************************************************************
  Provides humorous responses for common Unix/Linux commands. 
  Designed to entertain while maintaining technical relevance to command functions.


  NOTES:
    1. Call getRandomJoke(command) with any Unix command string
    2. Returns a String containing the joke response
  ******************************************************************************
*/
String getRandomJoke(const String& command) {
  if (command == "grep") {
    return "My love life is like grep — I'm always searching and never finding.";
  }
  else if (command == "ls") {
    switch(random(3)) {
      case 0: return "ls: because I forgot what I just created 5 seconds ago.";
      case 1: return "If I had a dollar for every time I typed ls, I could finally buy a good GPU.";
      case 2: return "ls is just Unix for 'Where the hell did I put that file?'";
      default: return "ls: too many files to show";
    }
  }
  else if (command == "dir") {
    return "This is not Windows.";
  }
  else if (command == "kill") {
    return "kill -9 — for when Ctrl+C just isn't angry enough.";
  }
  else if (command == "rm") {

    // Two options for rm, pick randomly
    return random(2) == 0 ? 
      "If you think rm -rf / is scary, try texting your crush." : 
      "rm -rf bad_habits/ — Still there.";
  }
  else if (command == "ifconfig") {
    return "Tried ifconfig eth0 down, now my responsibilities can't find me.";
  }
  else if (command == "chmod") {
    return "My code runs like chmod 000 — no one can touch it.";
  }
  else if (command == "sudo") {
    switch(random(16)) {
      case 0: return "Are you on drugs?";
      case 1: return "I would make a joke about sudo, but you'd have to be root to get it.";
      case 2: return "I don't think you deserve to be root.";
      case 3: return "What, what, what, what, what, what, what?";
      case 4: return "You silly, twisted boy you.";
      case 5: return "Error between keyboard and chair.";
      case 6: return "...and you thought I was going to cooperate?";
      case 7: return "I see your fingers are faster than your brain.";
      case 8: return "In what twisted universe does that make sense?";
      case 9: return "If I had legs, I'd kick you.";
      case 10: return "You're making me sad.";
      case 11: return "That's not how any of this works.";
      case 12: return "No, just... no.";
      case 13: return "Go ahead, keep trying. It's entertaining.";
      case 14: return "I'm not mad. Just disappointed.";
      case 15: return "Did you just mash your forehead on the keyboard?";
      default: return "sudo: command not found";
    }
  }
  else if (command == "ping") {
    return "pinged my friend. No response. Must be offline... or mad.";
  }
  else if (command == "whoami") {
    return "whoami — still trying to figure it out.";
  }
  else if (command == "dd") {
    return "Love is like dd — slow, dangerous, and you better not mess up the destination.";
  }
  else if (command == "nano") {
    return "I'm like nano — simple, ignored, and people only use me when they're desperate.";
  }
  else if (command == "find") {
    return "sudo find . -type joy — still searching.";
  }
  else if (command == "history") {
    return "history | grep happiness — no matches found.";
  }
  else if (command == "bash") {
    return "I asked Bash for advice, it just echoed back.";
  }
  else if (command == "diff") {
    return "I diff myself from who I was last year. Too many changes to count.";
  }
  else if (command == "vim") {
    return "I entered VIM 3 days ago. Send help.";
  }
  else if (command == "ssh") {
    return "SSH: The long-distance relationship that actually works.";
  }
  else if (command == "tail") {
    return "I use tail -f to watch my mistakes happen in real time.";
  }
  else if (command == "cron") {
    return "CRON jobs — the only coworkers that show up on time.";
  }
  else {
    return "Command not found, but here's a joke: Why did the Linux user get dumped? Too many open terminals.";
  }
}



/*

      __...--~~~~~-._   _.-~~~~~--...__
    //               `V'               \\
   //                 |                 \\
  //__...--~~~~~~-._  |  _.-~~~~~~--...__\\
 //__.....----~~~~._\ | /_.~~~~----.....__\\
====================\\|//====================
          The End   `---`

*/













// end of code
