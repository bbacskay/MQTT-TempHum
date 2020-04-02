#include "DHT.h"
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#include "config.h"

#define VERSION "0.1.0"

char mqttDeviceName[30];
char mqttTopicValue[40];
char mqttCmdOffset[40];
char mqttCmdInterval[40];
char mqttTopicCheckIn[40];
char mqttTopicLwt[40];

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
int interval = 1;

bool first = true;
bool boot = true;


void setup()
{
  Serial.begin(115200);
  sprintf(mqttDeviceName, "%S_%d", MQTT_DEVICENAME, ESP.getChipId());
  
  Serial.print("Devicename:");
  Serial.println(mqttDeviceName);
  
	sprintf(mqttTopicValue, "%s%s/value", MQTT_TOPIC, mqttDeviceName);
	sprintf(mqttCmdOffset, "%s%s/%s", MQTT_TOPIC, mqttDeviceName, MQTT_CMD_OFFSET);
	sprintf(mqttCmdInterval, "%s%s/%s", MQTT_TOPIC, mqttDeviceName, MQTT_CMD_INTERVAL);
  sprintf(mqttTopicCheckIn, "%s%s/%s", MQTT_TOPIC, mqttDeviceName, MQTT_TOPIC_CHECKIN);
  sprintf(mqttTopicLwt, "%s%s/%s", MQTT_TOPIC, mqttDeviceName, MQTT_TOPIC_LASTWILL);

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
				// In the first cycle fill the array with the same value
				unsigned char i;
				for (i=0;i++;i<SHORTAVGNUM)
				{
					temperatures[i] = t;
					//Serial.println(i);
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
		if ( millis() - lastSend > interval * 1000 ) {
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

/*
	h = 50;
	t = 23.5;
*/

	t += tempOffset;

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");

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
			interval = intPayload;
			} else {
			interval = 1;
		}
	}


}
