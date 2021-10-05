// THIS PROGRAM IS ONLY FOR THE WIFI KIT 32 with a built-in OLED
// Jake Jensen, 2021

// Includes and local files
#include <DHTesp.h>
#include <WiFi.h>
#include <Wire.h>
#include "heltec.h"
#include <Ticker.h>

// Global defines, could also be const
#define BUILTIN_LED 25
#define TRIGGER 21
#define ECHO 22

// DHTESP required setup
DHTesp dht;
int dhtPin = 14;

// Threaded tasks for forward declarations
void tempTask(void *pvParameters);
void SonicTask(void *pvParameters);
void OLEDTask(void *pvParameters);
bool getTemperature();
bool GetDistance();
bool UpdateOLED();
void triggerGetTemp();
void TriggerGetDistance();
void TriggerUpdateOLED();
// Tasks and tickers
TaskHandle_t tempTaskHandle = NULL;
TaskHandle_t SonicTaskHandle = NULL;
TaskHandle_t OLEDTaskHandle = NULL;
Ticker tempTicker;
Ticker SonicTicker;
Ticker OLEDTicker;
bool tasksEnabled = false;

// Placeholder and global variables, not atomic yet
String LastSuccessfulDistance = "WAITING FOR INIT";
String LastSuccessfulTemp = "WAITING FOR INIT";
String LastSuccessfulHum = "WAITING FOR INIT";
String LastSuccessfulHI = "WAITING FOR INIT";
String LastSuccessfulDP = "WAITING FOR INIT";
String LastSuccessfulCL = "WAITING FOR INIT";

const char* ssid = "SapphireNet"; //replace "xxxxxx" with your WIFI's ssid
const char* password = "HellFire166"; //replace "xxxxxx" with your WIFI's password

#define USE_STATIC_IP true
#if USE_STATIC_IP
  IPAddress staticIP(192,168,0,83);
  IPAddress gateway(192,168,0,1);
  IPAddress subnet(255,255,255,0);
  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(8,8,4,4);
#endif

#define OLED_UPDATE_INTERVAL 500        // OLED screen refresh interval ms

bool InitSonic() {
  byte resultValue = 0;
	Serial.println("Initializing Sonic");

  // Start task to get temperature
	xTaskCreatePinnedToCore(
			SonicTask,                       /* Function to implement the task */
			"SonicTask ",                    /* Name of the task */
			4000,                           /* Stack size in words */
			NULL,                           /* Task input parameter */
			5,                              /* Priority of the task */
			&SonicTaskHandle,                /* Task handle. */
			1);                             /* Core where the task should run */

  if (SonicTaskHandle == NULL) {
    Serial.println("Failed to start task for sonic update");
    return false;
  } else {
    // Start update of environment data every 20 seconds
    SonicTicker.attach(0.5, TriggerGetDistance);
  }
  return true;
}

bool InitOLED() {
  byte resultValue = 0;
	Serial.println("Initializing OLED");

  // Start task to get temperature
	xTaskCreatePinnedToCore(
			OLEDTask,                       /* Function to implement the task */
			"OLEDTask ",                    /* Name of the task */
			4000,                           /* Stack size in words */
			NULL,                           /* Task input parameter */
			5,                              /* Priority of the task */
			&OLEDTaskHandle,                /* Task handle. */
			1);                             /* Core where the task should run */

  if (OLEDTaskHandle == NULL) {
    Serial.println("Failed to start task for temperature update");
    return false;
  } else {
    // Start update of environment data every 20 seconds
    OLEDTicker.attach(1, TriggerUpdateOLED);
  }
  return true;
}

bool initTemp() {
  byte resultValue = 0;
  // Initialize temperature sensor
	dht.setup(dhtPin, DHTesp::DHT11);
	Serial.println("DHT initiated");

  // Start task to get temperature
	xTaskCreatePinnedToCore(
			tempTask,                       /* Function to implement the task */
			"tempTask ",                    /* Name of the task */
			4000,                           /* Stack size in words */
			NULL,                           /* Task input parameter */
			5,                              /* Priority of the task */
			&tempTaskHandle,                /* Task handle. */
			1);                             /* Core where the task should run */

  if (tempTaskHandle == NULL) {
    Serial.println("Failed to start task for temperature update");
    return false;
  } else {
    // Start update of environment data every 20 seconds
    tempTicker.attach(4, triggerGetTemp);
  }
  return true;
}

void triggerGetTemp() {
  if (tempTaskHandle != NULL) {
	   xTaskResumeFromISR(tempTaskHandle);
  }
}

void TriggerGetDistance() {
  if (SonicTaskHandle != NULL) {
	   xTaskResumeFromISR(SonicTaskHandle);
  }
}

void TriggerUpdateOLED() {
  if (OLEDTaskHandle != NULL) {
	   xTaskResumeFromISR(OLEDTaskHandle);
  }
}

void tempTask(void *pvParameters) {
	Serial.println("tempTask loop started");
	while (1)
  {
    if (tasksEnabled) {
			getTemperature();
		}
		vTaskSuspend(NULL);
	}
}

void SonicTask(void *pvParameters) {
	Serial.println("SonicTask loop started");
	while (1)
  {
    if (tasksEnabled) {
			GetDistance();
		}
		vTaskSuspend(NULL);
	}
}

void OLEDTask(void *pvParameters) {
	Serial.println("OLEDTask loop started");
	while (1)
  {
    if (tasksEnabled) {
			UpdateOLED();
		}
		vTaskSuspend(NULL);
	}
}

bool GetDistance() {
  // Get distance from sensor
  unsigned long EchoDuration;
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER, LOW);
  EchoDuration = pulseIn(ECHO, HIGH);
  float EchoDurationFINAL = (float)EchoDuration * 34.0 / 2000.0;
  String DistanceX = "Distance: ";
  DistanceX.concat(EchoDurationFINAL);

  if (EchoDuration == 0 || EchoDuration > 2000) {
    return false;
  } else {
    LastSuccessfulDistance = DistanceX;
    return true;
  }
}

ComfortState cf;
bool getTemperature() {

  digitalWrite(BUILTIN_LED, LOW);
  TempAndHumidity newValues = dht.getTempAndHumidity();
	if (dht.getStatus() != 0) {
		Serial.println("DHT11 error status: " + String(dht.getStatusString()));
		return false;
	}

	float heatIndex = dht.computeHeatIndex(newValues.temperature, newValues.humidity);
  float dewPoint = dht.computeDewPoint(newValues.temperature, newValues.humidity);
  float cr = dht.getComfortRatio(cf, newValues.temperature, newValues.humidity);

  String comfortStatus;
  switch(cf) {
    case Comfort_OK:
      comfortStatus = "Okay";
      break;
    case Comfort_TooHot:
      comfortStatus = "Too hot";
      break;
    case Comfort_TooCold:
      comfortStatus = "Too cold";
      break;
    case Comfort_TooDry:
      comfortStatus = "Too dry";
      break;
    case Comfort_TooHumid:
      comfortStatus = "Too humid";
      break;
    case Comfort_HotAndHumid:
      comfortStatus = "Hot and humid";
      break;
    case Comfort_HotAndDry:
      comfortStatus = "Hot and dry";
      break;
    case Comfort_ColdAndHumid:
      comfortStatus = "Cold and humid";
      break;
    case Comfort_ColdAndDry:
      comfortStatus = "Cold and dry";
      break;
    default:
      comfortStatus = "Shit's fucked";
      break;
  };

  String TempX = "Temp: ";
  TempX.concat(String((newValues.temperature * 9.0) / 5.0 + 32));
  TempX.concat(" °F");

  String HumX = "Humidity: ";
  HumX.concat(String(newValues.humidity));
  HumX.concat("%");

  String HeatX = "Feels like: ";
  HeatX.concat(String((heatIndex * 9.0) / 5.0 + 32));
  HeatX.concat(" °F");

  String DewX = "Dew point: ";
  DewX.concat((dewPoint * 9.0) / 5.0 + 32);
  DewX.concat(" °F");

  LastSuccessfulTemp = TempX;
  LastSuccessfulHum = HumX;
  LastSuccessfulHI = HeatX;
  LastSuccessfulDP = DewX;
  LastSuccessfulCL = String(comfortStatus);
  
	return true;
}

bool UpdateOLED() {

  Heltec.display->clear();
  Heltec.display->drawString(0, 0, LastSuccessfulTemp);
  Heltec.display->drawString(0, 10, LastSuccessfulHum);
  Heltec.display->drawString(0, 20, LastSuccessfulHI);
  Heltec.display->drawString(0, 30, LastSuccessfulDP);
  Heltec.display->drawString(0, 40, LastSuccessfulCL);
  Heltec.display->drawString(0, 50, LastSuccessfulDistance);
  Heltec.display->display();

  digitalWrite(BUILTIN_LED, HIGH);
  vTaskDelay(100);
  digitalWrite(BUILTIN_LED, LOW);
  return true;
}

void setupWIFI()
{
  Heltec.display->clear();
  Heltec.display->drawString(0, 0, "Connecting...");
  Heltec.display->drawString(0, 10, String(ssid));
  Heltec.display->display();

  WiFi.disconnect(true);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.onEvent(WiFiEvent);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(ssid, password);
#if USE_STATIC_IP
  WiFi.config(staticIP, gateway, subnet);
#endif

  // Wait for 5000ms, if there is no connection, continue down
  // Otherwise the basic functions are not available
  byte count = 0;
  while(WiFi.status() != WL_CONNECTED && count < 10)
  {
    count ++;
    delay(500);
    Serial.print(".");
  }

  Heltec.display->clear();
  if(WiFi.status() == WL_CONNECTED)
    Heltec.display->drawString(0, 0, "Connected");
  else
    Heltec.display->drawString(0, 0, "Connect False");
  Heltec.display->display();
}

void setup()
{
  Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Disable*/, true /*Serial Enable*/);
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT);
  digitalWrite(TRIGGER, LOW); // Put in known state
  digitalWrite(BUILTIN_LED ,HIGH);

    dht.setup(dhtPin, DHTesp::DHT11);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }


  Serial.println("Initialize...");

  setupWIFI();

  initTemp();
  InitSonic();
  InitOLED();
  // Signal end of setup() to tasks
  tasksEnabled = true;
}

void loop()
{
  if (!tasksEnabled) {
    // Wait 2 seconds to let system settle down
    delay(2000);
    tasksEnabled = true;
    if (tempTaskHandle != NULL) {
			vTaskResume(tempTaskHandle);
		}
    if (SonicTaskHandle != NULL) {
			vTaskResume(SonicTaskHandle);
		}
    if (OLEDTaskHandle != NULL) {
			vTaskResume(OLEDTaskHandle);
		}
  }
  yield();
}


void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event)
    {
        case SYSTEM_EVENT_WIFI_READY:               /**< ESP32 WiFi ready */
            break;
        case SYSTEM_EVENT_SCAN_DONE:                /**< ESP32 finish scanning AP */
            break;

        case SYSTEM_EVENT_STA_START:                /**< ESP32 station start */
            break;
        case SYSTEM_EVENT_STA_STOP:                 /**< ESP32 station stop */
            break;

        case SYSTEM_EVENT_STA_CONNECTED:            /**< ESP32 station connected to AP */
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:         /**< ESP32 station disconnected from AP */
            break;

        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:      /**< the auth mode of AP connected by ESP32 station changed */
            break;

        case SYSTEM_EVENT_STA_GOT_IP:               /**< ESP32 station got IP from connected AP */
        case SYSTEM_EVENT_STA_LOST_IP:              /**< ESP32 station lost IP and the IP is reset to 0 */
            break;

        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:       /**< ESP32 station wps succeeds in enrollee mode */
        case SYSTEM_EVENT_STA_WPS_ER_FAILED:        /**< ESP32 station wps fails in enrollee mode */
        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:       /**< ESP32 station wps timeout in enrollee mode */
        case SYSTEM_EVENT_STA_WPS_ER_PIN:           /**< ESP32 station wps pin code in enrollee mode */
            break;

        case SYSTEM_EVENT_AP_START:                 /**< ESP32 soft-AP start */
        case SYSTEM_EVENT_AP_STOP:                  /**< ESP32 soft-AP stop */
        case SYSTEM_EVENT_AP_STACONNECTED:          /**< a station connected to ESP32 soft-AP */
        case SYSTEM_EVENT_AP_STADISCONNECTED:       /**< a station disconnected from ESP32 soft-AP */
        case SYSTEM_EVENT_AP_PROBEREQRECVED:        /**< Receive probe request packet in soft-AP interface */
        case SYSTEM_EVENT_AP_STA_GOT_IP6:           /**< ESP32 station or ap interface v6IP addr is preferred */
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            break;

        case SYSTEM_EVENT_ETH_START:                /**< ESP32 ethernet start */
        case SYSTEM_EVENT_ETH_STOP:                 /**< ESP32 ethernet stop */
        case SYSTEM_EVENT_ETH_CONNECTED:            /**< ESP32 ethernet phy link up */
        case SYSTEM_EVENT_ETH_DISCONNECTED:         /**< ESP32 ethernet phy link down */
        case SYSTEM_EVENT_ETH_GOT_IP:               /**< ESP32 ethernet got IP from connected AP */
        case SYSTEM_EVENT_MAX:
            break;
    }
}