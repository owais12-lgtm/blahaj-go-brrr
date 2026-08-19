#include "RF24.h"
#include <SPI.h>
#include "esp_bt.h"
#include "esp_wifi.h"

// --- HARDWARE CONFIGURATION ---
// NRF24 Module 1 Pins (shares VSPI bus)
#define NRF24_1_CE_PIN      4
#define NRF24_1_CSN_PIN     5

// NRF24 Module 2 Pins (shares VSPI bus)
#define NRF24_2_CE_PIN      2
#define NRF24_2_CSN_PIN     15

// SPI Clock Speed for NRF24
#define SPI_CLOCK_SPEED     16000000UL

// LED Pin for status indication
const int LED_PIN = 2;

// LED Blink Interval (milliseconds)
const unsigned long LED_BLINK_INTERVAL_MS = 500;

// RF24 Channel Range (0-80)
const uint8_t CHANNEL_MIN = 0;
const uint8_t CHANNEL_MAX = 80;


// --- Global Objects ---
SPIClass *vspi_bus = nullptr;

RF24 radioModule1(NRF24_1_CE_PIN, NRF24_1_CSN_PIN, SPI_CLOCK_SPEED);
RF24 radioModule2(NRF24_2_CE_PIN, NRF24_2_CSN_PIN, SPI_CLOCK_SPEED);

uint8_t currentChannelModule1 = CHANNEL_MIN;
uint8_t currentChannelModule2 = (CHANNEL_MIN + (CHANNEL_MAX - CHANNEL_MIN) / 2);

// --- Function Prototypes ---
void initializeSystemPeripherals();
void setupSPIBus();
void configureNRF24Module(RF24& radio, SPIClass* spiBus, const char* moduleName);
void fastChannelSweep();
void toggleStatusLED();

// --- Setup Function ---
void setup() {
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    initializeSystemPeripherals();
    setupSPIBus();

    bool module1_ok = true;
    if (vspi_bus) {
        configureNRF24Module(radioModule1, vspi_bus, "Module 1");
        if (!radioModule1.isChipConnected()) {
            module1_ok = false;
        }
    } else {
        module1_ok = false;
    }

    bool module2_ok = true;
    if (vspi_bus) {
        configureNRF24Module(radioModule2, vspi_bus, "Module 2");
        if (!radioModule2.isChipConnected()) {
            module2_ok = false;
        }
    } else {
        module2_ok = false;
    }

    if (!module1_ok && !module2_ok) {
        Serial.println("FATAL ERROR: Both NRF24 modules failed. Jammer halted.");
        while (true) {
            digitalWrite(LED_PIN, HIGH); delay(100);
            digitalWrite(LED_PIN, LOW);  delay(100);
        }
    } else if (!module1_ok) {
        Serial.println("WARNING: NRF24 Module 1 failed. Operating with Module 2 only.");
    } else if (!module2_ok) {
        Serial.println("WARNING: NRF24 Module 2 failed. Operating with Module 1 only.");
    }
}

// --- Main Loop Function ---
void loop() {
    fastChannelSweep();
    toggleStatusLED();
}


// --- Helper Functions ---

void initializeSystemPeripherals() {
    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_wifi_disconnect();
}

void setupSPIBus() {
    vspi_bus = new SPIClass(VSPI);
    vspi_bus->begin();
}

void configureNRF24Module(RF24& radio, SPIClass* spiBus, const char* moduleName) {
    if (spiBus == nullptr) {
        return;
    }
    if (!radio.begin(spiBus)) {
        return;
    }

    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);

    uint8_t initialChannel;
    if (strcmp(moduleName, "Module 1") == 0) {
        initialChannel = currentChannelModule1;
    } else {
        initialChannel = currentChannelModule2;
    }
    radio.startConstCarrier(RF24_PA_MAX, initialChannel);
}

void fastChannelSweep() {
    if (radioModule1.isChipConnected()) {
        currentChannelModule1++;
        if (currentChannelModule1 > CHANNEL_MAX) {
            currentChannelModule1 = CHANNEL_MIN;
        }
        radioModule1.setChannel(currentChannelModule1);
    }

    if (radioModule2.isChipConnected()) {
        currentChannelModule2++;
        if (currentChannelModule2 > CHANNEL_MAX) {
            currentChannelModule2 = CHANNEL_MIN;
        }
        radioModule2.setChannel(currentChannelModule2);
    }
    delayMicroseconds(200);
}

void toggleStatusLED() {
    static unsigned long lastToggleTime = 0;
    static bool ledState = false;

    unsigned long currentTime = millis();
    if (currentTime - lastToggleTime >= LED_BLINK_INTERVAL_MS) {
        lastToggleTime = currentTime;
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
}