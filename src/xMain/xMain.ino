#include <rtosSerial.h> // https://downloads.arduino.cc/libraries/logs/github.com/HamzaYslmn/Esp32-RTOS-Serial/
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

#define RX_BUFFER_SIZE 2048
#define ESPNOW_CHANNEL 0
const uint8_t peerMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static unsigned long lastRttTime = 0;


String get_mac_address(const esp_now_recv_info_t* info) {
  if (!info) return "";
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
    info->src_addr[0], info->src_addr[1], info->src_addr[2],
    info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  return String(macStr);
}

void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (!data || len <= 0) return;
  int8_t rssi = (info && info->rx_ctrl) ? info->rx_ctrl->rssi : -127;
  String msg = String((const char*)data, len);
  String sender = get_mac_address(info);
  handleEspNow(msg, rssi, sender);
}

void ESPNOW_init(const uint8_t* peer, uint8_t channel) {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
  esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_LORA_250K);
  esp_wifi_set_max_tx_power(84);
  WiFi.setChannel(channel, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { esp_now_deinit(); esp_now_init(); }
  esp_now_register_recv_cb(onDataRecv);
  esp_now_peer_info_t peerInfo = {};
  peerInfo.ifidx = WIFI_IF_STA;
  memcpy(peerInfo.peer_addr, peer, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;
  if (esp_now_is_peer_exist(peer)) esp_now_mod_peer(&peerInfo);
  else esp_now_add_peer(&peerInfo);
}

void writeNow(const String& msg) {
  esp_now_send(peerMac, (const uint8_t*)msg.c_str(), msg.length());
}

void handleEspNow(const String& msg, const int8_t s_latestRSSI, const String& sender) {
  if (!msg.length()) return;
  if (msg.startsWith(">")) {
    writeNow("<" + msg.substring(1));
  } else if (msg.startsWith("<")) {
    unsigned long ts = msg.substring(1).toInt();
    String out = "PING|" + sender + "|" + String(s_latestRSSI) + "|" + String(millis() - ts) + "ms";
    rtosPrintf(out.c_str());
    lastRttTime = millis();
  } else {
    String out = "CHAT|" + sender + "|" + String(s_latestRSSI) + "|" + msg;
    rtosPrintln(out.c_str());
  }
}

void vEspNowTask(void*) {
  rtosSerialInit();
  ESPNOW_init(peerMac, ESPNOW_CHANNEL);
  TickType_t pingTask = xTaskGetTickCount();
  const TickType_t pinginterval = pdMS_TO_TICKS(10 * 1000);
  lastRttTime = millis();
  for (;;) {
    String localIn = rtosRead();
    if (!localIn.isEmpty()) writeNow(localIn);
    if ((xTaskGetTickCount() - pingTask) >= pinginterval) {
      writeNow(">" + String(millis()));
      pingTask = xTaskGetTickCount();
    }
    if ((millis() - lastRttTime) > (pinginterval + 1000)) {
      rtosPrintln("No connection");
      lastRttTime = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  rtosSerialInit();
  xTaskCreate(vEspNowTask, "vEspNowTask", 8192, NULL, 1, NULL);
}

void loop() { delay(1); }