#include "Meo3_Registration.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "cJSON.h"
#include <cstring>
#include <vector>

static const char* TAG = "MeoRegistration";

static const uint16_t MEO_REG_LISTEN_PORT = 8091;     // TCP Port ESP32 lắng nghe
static const uint16_t MEO_REG_DISCOVERY_PORT = 8901;  // UDP Port Gateway lắng nghe
static const char*    MEO_REG_DISCOVERY_MAGIC = "MEO3_DISCOVERY_V1";

MeoRegistrationClient::MeoRegistrationClient()
    : _port(MEO_REG_DISCOVERY_PORT),
      _logger(nullptr) {}

MeoRegistrationClient::~MeoRegistrationClient() {}

void MeoRegistrationClient::setGateway(const char* host, uint16_t port) {
    if (host) _gatewayHost = host;
    _port = port;
}

void MeoRegistrationClient::setLogger(MeoLogFunction logger) {
    _logger = logger;
}

void MeoRegistrationClient::_log(const char* level, const char* msg) {
    // Log ra ESP logger chuẩn
    if (strcmp(level, "ERROR") == 0) ESP_LOGE(TAG, "%s", msg);
    else if (strcmp(level, "WARN") == 0) ESP_LOGW(TAG, "%s", msg);
    else ESP_LOGI(TAG, "%s", msg);

    // Gọi callback người dùng nếu có
    if (_logger) {
        _logger(level, msg);
    }
}

bool MeoRegistrationClient::registerIfNeeded(const MeoDeviceInfo& devInfo,
                                             const MeoFeatureRegistry& features,
                                             std::string& deviceIdOut,
                                             std::string& transmitKeyOut) {
    if (!deviceIdOut.empty() && !transmitKeyOut.empty()) {
        return true; // Đã có thông tin
    }

    // Kiểm tra Wifi (đơn giản bằng cách check interface default)
    esp_netif_t* netif = esp_netif_get_default_netif();
    if (!netif) {
        _log("ERROR", "No default netif (WiFi not initialized?)");
        return false;
    }
    
    // 1) Send broadcast
    if (!_sendBroadcast(devInfo, features)) {
        _log("ERROR", "Failed to send registration broadcast");
        return false;
    }

    // 2) Listen on TCP 8091
    std::string responseJson;
    if (!_waitForRegistrationResponse(responseJson)) {
        _log("ERROR", "Did not receive registration response (timeout)");
        return false;
    }

    // 3) Parse response
    return _parseRegistrationResponse(responseJson, deviceIdOut, transmitKeyOut);
}

bool MeoRegistrationClient::_sendBroadcast(const MeoDeviceInfo& devInfo,
                                           const MeoFeatureRegistry& features) {
    // --- Lấy thông tin IP và MAC ---
    esp_netif_t* netif = esp_netif_get_default_netif();
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
             
    char ipStr[16];
    esp_ip4addr_ntoa(&ip_info.ip, ipStr, sizeof(ipStr));

    // --- Tạo JSON bằng cJSON ---
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "magic", MEO_REG_DISCOVERY_MAGIC);
    // Lưu ý: devInfo.model và manufacturer phải là std::string hoặc const char*
    cJSON_AddStringToObject(root, "model", devInfo.model.c_str()); 
    cJSON_AddStringToObject(root, "manufacturer", devInfo.manufacturer.c_str());
    cJSON_AddNumberToObject(root, "connectionType", static_cast<int>(devInfo.connectionType));
    cJSON_AddStringToObject(root, "mac", macStr);
    cJSON_AddStringToObject(root, "ip", ipStr);
    cJSON_AddNumberToObject(root, "listen_port", MEO_REG_LISTEN_PORT);

    cJSON *events = cJSON_CreateArray();
    for (const auto& e : features.eventNames) {
        cJSON_AddItemToArray(events, cJSON_CreateString(e.c_str()));
    }
    cJSON_AddItemToObject(root, "featureEvents", events);

    cJSON *methods = cJSON_CreateArray();
    for (const auto& kv : features.methodHandlers) {
        cJSON_AddItemToArray(methods, cJSON_CreateString(kv.first.c_str()));
    }
    cJSON_AddItemToObject(root, "featureMethods", methods);

    char *jsonString = cJSON_PrintUnformatted(root);
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        _log("ERROR", "Unable to create UDP socket");
        cJSON_Delete(root);
        free(jsonString);
        return false;
    }

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(MEO_REG_DISCOVERY_PORT);
    // Tính toán địa chỉ broadcast: (IP | ~SubnetMask)
    // Nhưng đơn giản nhất là gửi tới 255.255.255.255
    dest_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    int err = sendto(sock, jsonString, strlen(jsonString), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    
    if (err < 0) {
        _log("ERROR", "Error occurred during UDP sending");
    } else {
        std::string msg = "Sent discovery broadcast to 255.255.255.255:" + std::to_string(MEO_REG_DISCOVERY_PORT);
        _log("INFO", msg.c_str());
    }

    // Cleanup
    close(sock);
    cJSON_Delete(root);
    free(jsonString); // cJSON_PrintUnformatted cấp phát malloc

    return (err >= 0);
}

bool MeoRegistrationClient::_waitForRegistrationResponse(std::string& responseJson) {
    // Sửa thành:
int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) return false;

    // Reuse address để tránh lỗi bind khi restart nhanh
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(MEO_REG_LISTEN_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        _log("ERROR", "Unable to bind TCP port 8091");
        close(sock);
        return false;
    }

    if (listen(sock, 1) < 0) {
        close(sock);
        return false;
    }

    _log("INFO", "Listening for response on TCP 8091...");

    // Sử dụng select để set timeout cho accept (15 giây)
    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec = 15;
    timeout.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    int activity = select(sock + 1, &readfds, NULL, NULL, &timeout);

    if (activity <= 0) {
        _log("WARN", "Timeout waiting for gateway connection");
        close(sock);
        return false;
    }

    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int client_sock = accept(sock, (struct sockaddr *)&source_addr, &addr_len);
    
    if (client_sock < 0) {
        close(sock);
        return false;
    }

    _log("INFO", "Gateway connected!");

    // Set timeout cho receive (5 giây)
    struct timeval recv_timeout;
    recv_timeout.tv_sec = 5;
    recv_timeout.tv_usec = 0;
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

    // Đọc dữ liệu
    char rx_buffer[128];
    responseJson = "";
    bool complete = false;

    // Vòng lặp đọc đơn giản (như logic Arduino cũ: đọc đến khi gặp \n hoặc hết data)
    while (true) {
        int len = recv(client_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            break; // Error or Timeout
        }
        if (len == 0) {
            break; // Connection closed
        }
        
        rx_buffer[len] = 0; // Null terminate
        
        // Kiểm tra ký tự newline trong buffer vừa nhận
        char* newline = strchr(rx_buffer, '\n');
        if (newline) {
            *newline = 0; // Cắt chuỗi tại \n
            responseJson += rx_buffer;
            complete = true;
            break;
        } else {
            responseJson += rx_buffer;
        }
    }

    close(client_sock);
    close(sock);

    if (complete) {
        std::string msg = "Received: " + responseJson;
        _log("DEBUG", msg.c_str());
        return true;
    } else {
        _log("ERROR", "Incomplete response or timeout reading data");
        return false;
    }
}

bool MeoRegistrationClient::_parseRegistrationResponse(const std::string& json,
                                                       std::string& deviceIdOut,
                                                       std::string& transmitKeyOut) {
    cJSON *root = cJSON_Parse(json.c_str());
    if (root == NULL) {
        _log("ERROR", "Failed to parse registration JSON");
        return false;
    }

    cJSON *jDevId = cJSON_GetObjectItem(root, "device_id");
    cJSON *jKey   = cJSON_GetObjectItem(root, "transmit_key");

    bool success = false;
    if (cJSON_IsString(jDevId) && cJSON_IsString(jKey)) {
        deviceIdOut = jDevId->valuestring;
        transmitKeyOut = jKey->valuestring;
        success = true;
    } else {
        _log("ERROR", "JSON missing device_id or transmit_key");
    }

    cJSON_Delete(root);
    return success;
}