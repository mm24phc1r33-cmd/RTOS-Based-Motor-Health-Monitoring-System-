#include <WiFi.h>
#include <PubSubClient.h>


/* ============================================================
 * WIFI CONFIGURATION
 * ========================================================== */

const char* WIFI_SSID =
    "monika choudhary";

const char* WIFI_PASSWORD =
    "1234512345";


/* ============================================================
 * MQTT CONFIGURATION
 *
 * Public broker for testing.
 * ========================================================== */

const char* MQTT_SERVER =
    "broker.hivemq.com";

const int MQTT_PORT =
    1883;


/* ============================================================
 * MQTT TOPICS
 * ========================================================== */

const char* TOPIC_DATA =
    "industrial/motor/data";

const char* TOPIC_STATE =
    "industrial/motor/state";

const char* TOPIC_FAULT =
    "industrial/motor/fault";

const char* TOPIC_STATUS =
    "industrial/motor/status";


/* ============================================================
 * ESP32 UART2
 *
 * GPIO16 = RX2
 * GPIO17 = TX2
 * ========================================================== */

#define STM32_RX 16
#define STM32_TX 17


HardwareSerial STM32Serial(2);

WiFiClient espClient;

PubSubClient mqttClient(espClient);


/* ============================================================
 * VARIABLES
 * ========================================================== */

String rxBuffer = "";

int temperature = 0;

int humidity = 0;

float current = 0.0;

float vibration = 0.0;

int errorCount = 0;

String motorState = "UNKNOWN";


/* ============================================================
 * WIFI
 * ========================================================== */

void connectWiFi()
{
    Serial.println();

    Serial.println(
        "Connecting to Wi-Fi...");


    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD);


    int attempts = 0;


    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        Serial.print(".");

        attempts++;


        if (attempts > 40)
        {
            Serial.println();

            Serial.println(
                "Wi-Fi connection timeout");

            return;
        }
    }


    Serial.println();

    Serial.println(
        "Wi-Fi Status: CONNECTED");


    Serial.print(
        "IP Address: ");

    Serial.println(
        WiFi.localIP());
}


/* ============================================================
 * MQTT CONNECT
 * ========================================================== */

void connectMQTT()
{
    while (!mqttClient.connected())
    {
        Serial.println(
            "Connecting to MQTT...");


        String clientID =
            "ESP32_Motor_";

        clientID +=
            String((uint32_t)ESP.getEfuseMac(),
                   HEX);


        if (mqttClient.connect(
                clientID.c_str()))
        {
            Serial.println(
                "MQTT: CONNECTED");


            mqttClient.publish(
                TOPIC_STATUS,
                "ESP32_MQTT_GATEWAY_CONNECTED",
                true);
        }

        else
        {
            Serial.print(
                "MQTT connection failed, state=");

            Serial.println(
                mqttClient.state());


            delay(3000);
        }
    }
}


/* ============================================================
 * PARSE STM32 PACKET
 *
 * Example:
 *
 * SENSOR_DATA,TEMP=28,HUM=62,
 * CURRENT=2.50,VIBRATION=1.80,
 * STATE=RUNNING,ERROR=0
 * ========================================================== */

bool parsePacket(String packet)
{
    packet.trim();


    if (!packet.startsWith(
            "SENSOR_DATA"))
    {
        return false;
    }


    int tempIndex =
        packet.indexOf("TEMP=");

    int humIndex =
        packet.indexOf("HUM=");

    int currentIndex =
        packet.indexOf("CURRENT=");

    int vibrationIndex =
        packet.indexOf("VIBRATION=");

    int stateIndex =
        packet.indexOf("STATE=");

    int errorIndex =
        packet.indexOf("ERROR=");


    if (tempIndex < 0 ||
        humIndex < 0 ||
        currentIndex < 0 ||
        vibrationIndex < 0 ||
        stateIndex < 0 ||
        errorIndex < 0)
    {
        return false;
    }


    /* Temperature */

    int tempEnd =
        packet.indexOf(
            ",",
            tempIndex);


    temperature =
        packet.substring(
            tempIndex + 5,
            tempEnd).toInt();


    /* Humidity */

    int humEnd =
        packet.indexOf(
            ",",
            humIndex);


    humidity =
        packet.substring(
            humIndex + 4,
            humEnd).toInt();


    /* Current */

    int currentEnd =
        packet.indexOf(
            ",",
            currentIndex);


    current =
        packet.substring(
            currentIndex + 8,
            currentEnd).toFloat();


    /* Vibration */

    int vibrationEnd =
        packet.indexOf(
            ",",
            vibrationIndex);


    vibration =
        packet.substring(
            vibrationIndex + 10,
            vibrationEnd).toFloat();


    /* State */

    int stateEnd =
        packet.indexOf(
            ",",
            stateIndex);


    if (stateEnd < 0)
    {
        stateEnd =
            packet.length();
    }


    motorState =
        packet.substring(
            stateIndex + 6,
            stateEnd);


    /* Error */

    errorCount =
        packet.substring(
            errorIndex + 6).toInt();


    return true;
}


/* ============================================================
 * PUBLISH MQTT
 * ========================================================== */

void publishMotorData()
{
    char json[300];


    snprintf(
        json,
        sizeof(json),

        "{"
        "\"temperature\":%d,"
        "\"humidity\":%d,"
        "\"current\":%.2f,"
        "\"vibration\":%.2f,"
        "\"state\":\"%s\","
        "\"error\":%d"
        "}",

        temperature,

        humidity,

        current,

        vibration,

        motorState.c_str(),

        errorCount);


    /* Main data topic */

    mqttClient.publish(
        TOPIC_DATA,
        json);


    /* Motor state */

    mqttClient.publish(
        TOPIC_STATE,
        motorState.c_str());


    /* Fault topic */

    if (motorState == "FAULT")
    {
        mqttClient.publish(
            TOPIC_FAULT,
            json);
    }


    Serial.println(
        "MQTT: DATA PUBLISHED");


    Serial.print(
        "JSON: ");

    Serial.println(
        json);
}


/* ============================================================
 * PROCESS STM32 PACKET
 * ========================================================== */

void processPacket(String packet)
{
    Serial.println();

    Serial.println(
        "======================================");

    Serial.println(
        "STM32 PACKET RECEIVED");

    Serial.println(
        packet);


    if (parsePacket(packet))
    {
        Serial.println(
            "PACKET STATUS: VALID");


        Serial.print(
            "Temperature : ");

        Serial.print(
            temperature);

        Serial.println(
            " C");


        Serial.print(
            "Humidity    : ");

        Serial.print(
            humidity);

        Serial.println(
            " %");


        Serial.print(
            "Current     : ");

        Serial.print(
            current,
            2);

        Serial.println(
            " A");


        Serial.print(
            "Vibration   : ");

        Serial.print(
            vibration,
            2);

        Serial.println(
            " g");


        Serial.print(
            "Motor State : ");

        Serial.println(
            motorState);


        Serial.print(
            "Error Count : ");

        Serial.println(
            errorCount);


        publishMotorData();


        /*
         * Send ACK to STM32
         */

        STM32Serial.println(
            "ACK,SENSOR_DATA");


        Serial.println(
            "ESP32 -> STM32 : ACK,SENSOR_DATA");
    }

    else
    {
        Serial.println(
            "PACKET STATUS: INVALID");


        STM32Serial.println(
            "NACK,SENSOR_DATA");
    }


    Serial.println(
        "======================================");
}


/* ============================================================
 * SETUP
 * ========================================================== */

void setup()
{
    Serial.begin(115200);


    /*
     * STM32 UART
     */

    STM32Serial.begin(
        115200,
        SERIAL_8N1,
        STM32_RX,
        STM32_TX);


    Serial.println();

    Serial.println(
        "======================================");

    Serial.println(
        " ESP32 INDUSTRIAL MOTOR MQTT GATEWAY");

    Serial.println(
        " STAGE 15");

    Serial.println(
        "======================================");


    connectWiFi();


    mqttClient.setServer(
        MQTT_SERVER,
        MQTT_PORT);


    connectMQTT();


    Serial.println(
        "ESP32 READY");


    STM32Serial.println(
        "ESP32_READY");
}


/* ============================================================
 * LOOP
 * ========================================================== */

void loop()
{
    /*
     * Wi-Fi reconnect
     */

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "Wi-Fi disconnected");

        connectWiFi();
    }


    /*
     * MQTT reconnect
     */

    if (!mqttClient.connected())
    {
        connectMQTT();
    }


    mqttClient.loop();


    /*
     * Read STM32 UART
     */

    while (STM32Serial.available())
    {
        char c =
            STM32Serial.read();


        if (c == '\n')
        {
            processPacket(
                rxBuffer);


            rxBuffer = "";
        }

        else if (c != '\r')
        {
            rxBuffer += c;


            /*
             * Prevent buffer overflow
             */

            if (rxBuffer.length() > 300)
            {
                rxBuffer = "";
            }
        }
    }


    delay(5);
}
