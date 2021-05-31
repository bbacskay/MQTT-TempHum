#include <DHT.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

#include "config.h"

#define TEMPHUM_VERSION "1.1.1"
#define TEMPHUM_VERSION_FULL TEMPHUM_VERSION " " ARDUINO_BOARD

#define MQTT_COMPONENT "sensor"

char mqttDeviceName[30];
char mqttTopicValue[80];
char mqttCmdOffset[80];
char mqttCmdInterval[80];
char mqttTopicCheckIn[80];
char mqttTopicLwt[80];

char charPayload[50];

WiFiClient wifiClient;

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

// Initialize MQTT client
PubSubClient client(wifiClient);

int status = WL_IDLE_STATUS;
unsigned long lastSend;
unsigned long lastMeasured;

float t, h;

float temperatures[SHORTAVGNUM];
unsigned char tempAvgPos;
float avgTemp;

float tempOffset;
unsigned int interval = 1;

bool first = true;
bool boot = true;


void sendDiscovery() {
    /*
      Buffer size is calculated with ArduinoJson Assistant
      https://arduinojson.org/v6/assistant/
    */

    const size_t capacity = JSON_OBJECT_SIZE(5)  +  // device
                            JSON_OBJECT_SIZE(10) +  // root
                            446;
                            
    char uniqueIdTemp[50];
    char uniqueIdHum[50];
    char mqttDiscoveryTempTopic[80];
    char mqttDiscoveryHumTopic[80];
    
    char nameTemp[80];
    char nameHum[80];


    sprintf(uniqueIdTemp, "%s-temperature", mqttDeviceName);
    sprintf(uniqueIdHum,  "%s-humidity", mqttDeviceName);

    sprintf(mqttDiscoveryTempTopic, "%s/%s/%s/config", MQTT_DISCOVERY_BASE_TOPIC, MQTT_COMPONENT, uniqueIdTemp);
    sprintf(mqttDiscoveryHumTopic, "%s/%s/%s/config", MQTT_DISCOVERY_BASE_TOPIC, MQTT_COMPONENT, uniqueIdHum);

    sprintf(nameTemp, "%s Temperature", mqttDeviceName);
    sprintf(nameHum , "%s Humidity"   , mqttDeviceName);

    #ifdef DEBUG
      Serial.print("mqttDeviceName: ");
      Serial.println(mqttDeviceName);
      Serial.print("ChipID:");
      Serial.println(ESP.getChipId());
      Serial.print("uniqueIdTemp: ");
      Serial.println(uniqueIdTemp);

      Serial.printf("SendDiscovery - capacity: %i\n", capacity);
    #endif

    DynamicJsonDocument doc(capacity);

    // Create Temperature sensor discovery
    doc["unique_id"] = uniqueIdTemp;  //"ESP8266_2352108-temperature";

    JsonObject device = doc.createNestedObject("device");
    device["identifiers"] = ESP.getChipId();
    device["model"] = "MQTT-TempHum DHT22";
    device["manufacturer"] = "Balu";
    device["name"] = mqttDeviceName; //"ESP8266_2352108";
    device["sw_version"] = TEMPHUM_VERSION_FULL;

    doc["device_class"] = "temperature";
    doc["name"] = nameTemp;
    doc["unit_of_measurement"] = "°C";
    doc["value_template"] = "{{ value_json.temperature | is_defined }}";
    doc["state_topic"] = mqttTopicValue;
    doc["availability_topic"] = mqttTopicLwt;
    doc["payload_available"] = "Online";
    doc["payload_not_available"] = "Offline";

    int jsonBufferSize = measureJson(doc)+1;
    char b[jsonBufferSize];

    #ifdef DEBUG
      Serial.printf("Discover JSON buffer size: %i\n", jsonBufferSize);
      serializeJson(doc, Serial);
      Serial.printf("\n");
    #endif

    serializeJson(doc, b, jsonBufferSize);

    client.beginPublish(mqttDiscoveryTempTopic,jsonBufferSize-1,true);
    client.write((uint8_t*)b,jsonBufferSize-1);
    client.endPublish();

    // Create Humidity sensor discovery

    // Reset the JSON doc
    doc.clear();
    device.clear();

    doc["unique_id"] = uniqueIdHum;  //"ESP8266_2352108-humidity";

    device = doc.createNestedObject("device");
    device["identifiers"] = ESP.getChipId();
    device["model"] = "MQTT-TempHum DHT22";
    device["manufacturer"] = "Balu";
    device["name"] = mqttDeviceName; //"ESP8266_2352108";
    device["sw_version"] = TEMPHUM_VERSION_FULL;

    doc["device_class"] = "humidity";
    doc["name"] = nameHum;
    doc["unit_of_measurement"] = "%";
    doc["value_template"] = "{{ value_json.humidity | is_defined }}";
    doc["state_topic"] = mqttTopicValue;
    doc["availability_topic"] = mqttTopicLwt;
    doc["payload_available"] = "Online";
    doc["payload_not_available"] = "Offline";

    memset(b, 0, sizeof(b));
    jsonBufferSize = measureJson(doc)+1;
    serializeJson(doc, b, jsonBufferSize);
    
    #ifdef DEBUG
      Serial.printf("Humidity discovery\n");
      Serial.printf("Discover JSON buffer size: %i\n", jsonBufferSize);
      serializeJson(doc, Serial);
      Serial.printf("\n");
    #endif

    client.beginPublish(mqttDiscoveryHumTopic,jsonBufferSize-1,true);
    client.write((uint8_t*)b,jsonBufferSize-1);
    client.endPublish();
   
  }

void setup()
{
  Serial.begin(115200);
  sprintf(mqttDeviceName, "%s_%d", MQTT_DEVICENAME, ESP.getChipId());
  
  #ifdef DEBUG
  delay(10000);
  #endif

  Serial.print("Devicename:");
  Serial.println(mqttDeviceName);
  
	sprintf(mqttTopicValue, "%s/%s/value", MQTT_TOPIC, mqttDeviceName);
	sprintf(mqttCmdOffset, "%s/%s/%s", MQTT_TOPIC, mqttDeviceName, MQTT_CMD_OFFSET);
	sprintf(mqttCmdInterval, "%s/%s/%s", MQTT_TOPIC, mqttDeviceName, MQTT_CMD_INTERVAL);
  sprintf(mqttTopicCheckIn, "%s/%s/%s", MQTT_TOPIC, mqttDeviceName, MQTT_TOPIC_CHECKIN);
  sprintf(mqttTopicLwt, "%s/%s/%s", MQTT_TOPIC, mqttDeviceName, MQTT_TOPIC_LASTWILL);

  #ifdef DEBUG
  Serial.printf("mqttTopicValue (%d) : %s\n", strlen(mqttTopicValue), mqttTopicValue);
  Serial.printf("mqttCmdOffset (%d) : %s\n", strlen(mqttCmdOffset), mqttCmdOffset);
  Serial.printf("mqttCmdInterval (%d) : %s\n", strlen(mqttCmdInterval), mqttCmdInterval);
  Serial.printf("mqttTopicCheckIn (%d) : %s\n", strlen(mqttTopicCheckIn), mqttTopicCheckIn);
  Serial.printf("mqttTopicLwt (%d) : %s\n", strlen(mqttTopicLwt), mqttTopicLwt);
  #endif

  dht.begin();
  delay(10);

  InitWiFi();
  
  client.setServer( MQTT_BROKER, 1883 );
	client.setCallback(mqttCallback);
  lastSend = 0;
	lastMeasured = 0;
	tempAvgPos = 0;

}

void loop()
{

  if ( !client.connected() ) {
    reconnect();
  }

  if ( millis() - lastMeasured > 1000 ) { // Update in every 1 seconds
    getTemperatureAndHumidityData();
		if (!isnan(t)) {
			if (first == true) {
        #ifdef DEBUG
        Serial.println("First cycle");
        #endif
				// In the first cycle fill the array with the same value
				unsigned char i;
				for (i=0;i<SHORTAVGNUM;i++)
				{
					temperatures[i] = t;
				}
				first = false;
			} else {
				temperatures[tempAvgPos] = t;
				
				tempAvgPos++;
				if (tempAvgPos >= SHORTAVGNUM) {
					tempAvgPos = 0;
				}
			}
			
			
			// Calculate the average temperature
			avgTemp = CalculateAvgTemp();
			
		}
		
		
    lastMeasured = millis();
  }
	
	// Check if any reads failed and exit early (to try again).
	if (isnan(h) || isnan(t)) {
		Serial.println("Failed to read from DHT sensor!");
	} else {
		if (( millis() - lastSend > interval * 1000 ) && (!first)) {
		sendData();
	
		lastSend = millis();
		}		
		
	}


  client.loop();
}



void getTemperatureAndHumidityData()
{
  Serial.println("Collecting temperature data.");
	
	t = NAN;
	h = NAN;

  // Reading temperature or humidity takes about 250 milliseconds!
  h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  t = dht.readTemperature();

  #ifdef TEST
  // Test data
	h = 50;
	t = 23.5;
  #endif

	t += tempOffset;

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" *C ");

}

float CalculateAvgTemp()
{
	float TempVal;
	unsigned char i;
	
	TempVal = 0;
	
	for(i=0;i<SHORTAVGNUM;i++)
	{
		TempVal += temperatures[i];
	}
	TempVal /= SHORTAVGNUM;
	
	return TempVal;
}


void sendData() {
	String temperature = String(avgTemp, 1);   // convert float to string with 1 decimal
	String humidity = String(h);


	// Just debug messages
	Serial.print( "Sending temperature and humidity : [" );
	Serial.print( temperature );
  Serial.print( "," );
	Serial.print( humidity );
	Serial.print( "]   -> " );

	// Prepare a JSON payload string
	String payload = "{";
		payload += "\"temperature\":"; payload += temperature; payload += ",";
		payload += "\"humidity\":"; payload += humidity;
	payload += "}";

	// Send payload
	char attributes[100];
	payload.toCharArray( attributes, 100 );
	//client.publish( "v1/devices/me/telemetry", attributes );
	client.publish( mqttTopicValue, attributes );
	Serial.println( attributes );
}

void InitWiFi()
{
// We start by connecting to a WiFi network
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // attempt to connect to WiFi network
  WiFi.mode(WIFI_STA);
	WiFi.hostname(mqttDeviceName);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Hostname: ");
  Serial.println(WiFi.hostname());
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to AP");
    }
    Serial.print("Connecting to MQTT Broker ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect(mqttDeviceName, MQTT_USERNAME, MQTT_PASSWORD, mqttTopicLwt, 0, true, "Offline") ) {
      Serial.println( "[DONE]" );

      client.publish(mqttTopicLwt, "Online", true);

      // Once connected, publish an announcement...
      if(boot == true)
      {
        client.publish(mqttTopicCheckIn,"Rebooted");
        boot = false;
      }
      if(boot == false)
      {
        client.publish(mqttTopicCheckIn,"Reconnected"); 
      }

      // Send discovery for homeassistant
      sendDiscovery();

			// Subscribe to command topics
			client.subscribe(mqttCmdOffset);
			client.subscribe(mqttCmdInterval);
			
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}

/************************** MQTT CALLBACK ***********************/
void mqttCallback(char* topic, byte* payload, unsigned int length) {
	Serial.print("Message arrived [");
	String newTopic = topic;
	Serial.print(topic);
	Serial.print("] ");
	payload[length] = '\0';
	String newPayload = String((char *)payload);
	int intPayload = newPayload.toInt();
	Serial.println(newPayload);
	Serial.println();
	newPayload.toCharArray(charPayload, newPayload.length() + 1);

	if (newTopic == mqttCmdOffset)
	{
		float newOffset = newPayload.toFloat();
		if (( newOffset > -10 ) && ( newOffset < 10 )) {
			tempOffset = newOffset;
		} else {
			tempOffset = 0;
		}
	}


	if (newTopic == mqttCmdInterval)
	{
		if (( intPayload > 0 ) && ( intPayload < 600 )) {
			interval = (unsigned int)intPayload;
			} else {
			interval = 1;
		}
	}


}
