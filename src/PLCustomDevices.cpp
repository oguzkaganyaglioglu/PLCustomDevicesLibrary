#include <Arduino.h>
#include <PLCustomDevices.h>

PLCustomDevices::PLCustomDevices(WiFiClient *wifiClient, const char *mqttHost, uint16_t *port)
{
    setMqttCallback(NULL);
    PubSubClient tmqttClient(*wifiClient);
    this->host = mqttHost;
    this->port = port;
    this->wifiClient = wifiClient;
    this->mqttClient = &tmqttClient;
}

PLCustomDevices::~PLCustomDevices()
{
    this->mqttClient->~PubSubClient();
    this->doc.clear();
    free(responseBuffer);
}

void PLCustomDevices::init()
{
    mqttClient->setBufferSize(JSON_BUFFER);
    mqttClient->setServer(host, *port);
    mqttClient->setCallback([this](char *topic, byte *payload, uint16_t length)
                            { this->callback(topic, payload, length); });
}

void PLCustomDevices::setAuthInfos(const char *deviceID, const char *mqttUsername, const char *mqttPass)
{
    this->clientID = deviceID;
    this->username = mqttUsername;
    this->pass = mqttPass;
}

void PLCustomDevices::callback(char *topic, uint8_t *message, uint16_t length)
{
    Serial.println("NEW MESSAGE");
    DeserializationError error = deserializeJson(doc, message, length);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    commandAvailable = true;
    if (customCallback)
    {
        customCallback(topic, message, length);
        if (!noAutoResponse)
        {
            sendResponse();
        }
    }
}

bool PLCustomDevices::sendResponse()
{
    commandAvailable = false;
    serializeJson(doc, responseBuffer);
    Serial.println(String(clientID) + "/o");
    bool result = mqttClient->publish((String(clientID) + "/o").c_str(), responseBuffer);
    memset(responseBuffer, 0, sizeof(responseBuffer));
    doc.clear();
    return result;
}

PLCustomDevices &PLCustomDevices::setMqttCallback(CUSTOM_CALLBACK_SIGNATURE, bool noResponse)
{
    this->noAutoResponse = noResponse;
    this->customCallback = customCallback;
    return *this;
}

void PLCustomDevices::connect(bool waitUntilConnect = false)
{
    static unsigned long lastCall;
    static bool useDelay = false;
    if (!waitUntilConnect && useDelay)
    {
        if (millis() - lastCall > 5000)
        {
            useDelay = false;
        }
        else
        {
            return;
        }
    }
    do
    {
        if (!mqttClient->connected())
        {
            Serial.print("Attempting MQTT connection...");
            Serial.printf("HOST: %s \nPORT: %u", host, *port);
            if (mqttClient->connect(clientID, username, pass))
            {
                Serial.println("connected");
                mqttClient->subscribe((String(clientID) + "/i").c_str());
            }
            else
            {
                Serial.print("failed, rc=");
                Serial.print(mqttClient->state());
                Serial.println(" try again in 5 seconds");
                // Wait 5 seconds before retrying
                if (waitUntilConnect)
                {
                    delay(5000);
                }
                else
                {
                    lastCall = millis();
                    useDelay = true;
                }
            }
        }
    } while (!mqttClient->connected() && waitUntilConnect);
}

void PLCustomDevices::loop()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        connect();
        if (!mqttClient->loop())
        {
            mqttClient->connect(clientID, username, pass);
        }
    }
}