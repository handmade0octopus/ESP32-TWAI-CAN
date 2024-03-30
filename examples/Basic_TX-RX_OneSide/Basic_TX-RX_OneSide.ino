#include <ESP32-TWAI-CAN.hpp>

// Simple sketch that querries OBD2 over CAN for coolant temperature
// Showcasing simple use of ESP32-TWAI-CAN library driver.

// Default for ESP32
#define CAN_TX		35  //default 5
#define CAN_RX		48  //default 4

//By ESPRESSIF it is recommended to use Tranciever SN65HVD23x.
//To Wake-up/enable the Tranceiver it has pin RS
//RS pin input: 0 = Working mode, 1 = Idly/Standby mode.
#define IDE_RS    36  

CanFrame rxFrame;
uint8_t counter;
void sendObdFrame(uint8_t obdId,uint8_t index) {
	CanFrame obdFrame = { 0 };
	obdFrame.identifier = 1; //This Side ID value
	obdFrame.extd = 0;
	obdFrame.data_length_code = 8;
	obdFrame.data[0] = index;
	obdFrame.data[1] = 1;
	obdFrame.data[2] = obdId;
	obdFrame.data[3] = 0xAA;    // Best to use 0xAA (0b10101010) instead of 0
	obdFrame.data[4] = 0xAA;    // CAN works better this way as it needs
	obdFrame.data[5] = 0xAA;    // to avoid bit-stuffing
	obdFrame.data[6] = 0xAA;
	obdFrame.data[7] = 0xAA;
    // Accepts both pointers and references 
    ESP32Can.writeFrame(obdFrame);  // timeout defaults to 1 ms
}

void setup() {
  counter = 0;
    // Setup serial for debbuging.
  Serial.begin(115200);
    //Enable Tranceiver for Receiving and Transmiting data.
  pinMode(IDE_RS,OUTPUT);
  digitalWrite(IDE_RS,LOW);
    // Set pins
	ESP32Can.setPins(CAN_TX, CAN_RX);
	
    // You can set custom size for the queues - those are default
    ESP32Can.setRxQueueSize(5);
	ESP32Can.setTxQueueSize(5);

    // .setSpeed() and .begin() functions require to use TwaiSpeed enum,
    // but you can easily convert it from numerical value using .convertSpeed()
    // ESP32Can.setSpeed(ESP32Can.convertSpeed(500));

    // You can also just use .begin()..
    // if(ESP32Can.begin()) {
    //     Serial.println("CAN bus started!");
    // } else {
    //     Serial.println("CAN bus failed!");
    // }

    // or override everything in one command;
    // It is also safe to use .begin() without .end() as it calls it internally
    if(ESP32Can.begin(ESP32Can.convertSpeed(500), CAN_TX, CAN_RX, 10, 10)) {
        Serial.println("CAN bus started!");
    } else {
        Serial.println("CAN bus failed!");
    }
}

void loop() {
    static uint32_t lastStamp = 0;
    uint32_t currentStamp = millis();
    
    if(currentStamp - lastStamp > 1000) {   // sends OBD2 request every second
        lastStamp = currentStamp;
        sendObdFrame(5,counter++); // For coolant temperature
    }

    // You can set custom timeout, default is 1000
    if(ESP32Can.readFrame(rxFrame, 1000)) {
        // Comment out if too many frames
        Serial.printf("Received frame: %03X  \r\n", rxFrame.identifier);
        if(rxFrame.identifier == 2) {   // Standard OBD2 frame responce ID
            Serial.printf("packet: %d\r\n", rxFrame.data[0]); // Convert to °C
        }
    }
}