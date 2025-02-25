/*
 * ESP8266 UART <-> ESPNOW <-> UART Bridge
 * ESP8266 TX of Serial1 is GPIO2 
 */

#include <ESP8266WiFi.h>
#include <espnow.h>


int gpio0_pin = 16;   //BUTTON
int gpio5_pin = 14;   //BUTTON LED
int gpio6_pin = 12;    //BUZZER
int gpio1_pin = 5;    //PAIRED LED
bool flag_pushed = false;


// new structure for pairing
typedef struct struct_pairing {       
    uint8_t msgType;
    uint8_t espnowChannel; // not used
    uint8_t buffer_length;
    uint8_t buffer[128];
} struct_pairing;

uint8_t PairingAddress[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
enum PairingStatus {NOT_PAIRED, PAIRED, };
enum MessageType {PAIRING_REQUEST, DATA_REQUEST,};
PairingStatus pairingStatus = NOT_PAIRED;
char incomingBuffer[128];
bool incomingPacket = false;
unsigned int lastRequestMillis = 0;

// callback when data is sent from Master to Slave
void OnDataSent(uint8_t *mac_addr, uint8_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  //Serial1.print("Last Packet Sent to: "); Serial1.println(macStr);
  //Serial1.print("Last Packet Send Status: "); Serial1.println(status == 0 ? "Delivery Success" : "Delivery Fail");
  if (status != 0) {
    Serial.print("Packet Sent to: ");
    Serial.print(macStr);
    Serial.println(" Delivery Fail");
    pairingStatus = NOT_PAIRED;
    memset(PairingAddress, 0xFF, sizeof(PairingAddress));
  }
}

// callback when data is recv from Master
void OnDataRecv(uint8_t *mac_addr, uint8_t *data, uint8_t data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  //Serial1.print("Packet Recv from: "); Serial1.println(macStr);

  struct_pairing pairingData;
  memcpy(&pairingData, data, data_len);
  if (pairingData.msgType == PAIRING_REQUEST) {
    int ret;
    ret = esp_now_is_peer_exist(mac_addr);
    Serial1.print("esp_now_is_peer_exist=");
    Serial1.println(ret);
    if (ret < 0) {
      Serial1.println("esp_now_is_peer_exist fail");
      return;
    }
    if (ret > 0) {
      pairingStatus = PAIRED;
      for (int i=0;i<6;i++) PairingAddress[i] = mac_addr[i];
      return;
    }
    
    ret = esp_now_add_peer(mac_addr, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    if (ret != 0) {
      Serial1.println("esp_now_add_peer fail");
      return;
    } else {
      Serial1.println("esp_now_add_peer succes");
      pairingStatus = PAIRED ; 
      for (int i=0;i<6;i++) PairingAddress[i] = mac_addr[i];
    }

  } else {  
    memset(incomingBuffer, 0, sizeof(incomingBuffer));
    incomingPacket = true;
    memcpy(incomingBuffer, pairingData.buffer, pairingData.buffer_length);
  }
}



void setup() {
  delay(1000);
  pinMode(gpio0_pin, INPUT_PULLUP);
  pinMode(gpio5_pin, OUTPUT);
  pinMode(gpio6_pin, OUTPUT);
  pinMode(gpio1_pin, OUTPUT);
  digitalWrite(gpio5_pin, HIGH);
  digitalWrite(gpio6_pin, HIGH);
  digitalWrite(gpio1_pin, HIGH);
  delay(1000);
  digitalWrite(gpio5_pin, LOW);
  digitalWrite(gpio6_pin, LOW);
  digitalWrite(gpio1_pin, LOW);
  
  Serial1.begin(115200); // for Debug print
  Serial.begin(115200); // You can change
  //Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  Serial1.println("ESPNow/Basic/Master Example");
  // This is the mac address of the Master in Station Mode
  Serial1.print("STA MAC: "); Serial1.println(WiFi.macAddress());
  
  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial1.println("Error initializing ESP-NOW");
    return;
  }

  // Set ESP-NOW Role
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

}

void pairing() {
  if (millis() - lastRequestMillis < 1000) return;
  
  Serial1.print("pairingStatus=");
  Serial1.print(pairingStatus);
  Serial1.print(" PairingAddress=");
  for (int i=0;i<sizeof(PairingAddress);i++) {
    Serial1.print(PairingAddress[i], HEX);
    Serial1.print(" ");
  }
  Serial1.println();

  struct_pairing pairingData;
  pairingData.msgType = PAIRING_REQUEST;
  esp_now_send(PairingAddress, (uint8_t *) &pairingData, sizeof(pairingData));
  lastRequestMillis = millis();
}

void loop() {
  // Update pairing
  pairing();

  if(Serial.available()||(digitalRead(gpio0_pin)==LOW)) {
    uint8_t buf[128];
    uint8_t iofs=0;
    while(Serial.available()) {
      buf[iofs++] = Serial.read(); // read char from UART
      buf[iofs] = 0;
    }
    
    Serial.print((char *)buf);
    //if (digitalRead(gpio0_pin)==LOW) Serial.print("button pushed\r\n");
    if (pairingStatus == PAIRED) {
      digitalWrite(gpio1_pin, HIGH);
      struct_pairing pairingData;
      pairingData.msgType = DATA_REQUEST;
      memset(pairingData.buffer, 0, sizeof(pairingData.buffer));
      memcpy(pairingData.buffer, buf, iofs);
      pairingData.buffer_length = iofs;     
      if (digitalRead(gpio0_pin)==HIGH) esp_now_send(PairingAddress, (uint8_t *) &pairingData, sizeof(pairingData));
      else if (!flag_pushed)
      {
        digitalWrite(gpio5_pin, HIGH);
        digitalWrite(gpio6_pin, HIGH);
        struct_pairing pairingData_button;
        pairingData_button.msgType = DATA_REQUEST;
        memset(pairingData_button.buffer, 0, sizeof(pairingData_button.buffer));
        memcpy(pairingData_button.buffer, "FFF", 5);
        pairingData_button.buffer_length = 5;
        esp_now_send(PairingAddress, (uint8_t *) &pairingData_button, sizeof(pairingData_button));    
        delay(1000);
        digitalWrite(gpio6_pin, LOW); 
        delay(3000);
        digitalWrite(gpio5_pin, LOW); 
        memcpy(pairingData_button.buffer, "000", 5);
        esp_now_send(PairingAddress, (uint8_t *) &pairingData_button, sizeof(pairingData_button));
      }
    } else digitalWrite(gpio1_pin, LOW);// end if
  }

  if (incomingPacket) {
    Serial1.print("<--[");
    Serial1.print(incomingBuffer);
    Serial1.println("]");
    Serial.print(incomingBuffer);
    incomingPacket = false;
    if ((incomingBuffer[0]=='F')&&(incomingBuffer[1]=='F')&&(incomingBuffer[2]=='F'))
       {
        //digitalWrite(gpio5_pin, HIGH);
        flag_pushed=true;
       }
    else {
      flag_pushed=false;
      //digitalWrite(gpio5_pin, LOW);
    }
  }
   

}



 
