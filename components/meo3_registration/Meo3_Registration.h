#ifndef MEO3_REGISTRATION_H
#define MEO3_REGISTRATION_H

#include "Meo3_Type.h"
#include <string>
#include <cstdint>

class MeoRegistrationClient {
public:
    MeoRegistrationClient();
    ~MeoRegistrationClient(); // Thêm destructor để dọn dẹp nếu cần

    // Gateway host/port chỉ dùng nếu muốn gửi unicast (hiện tại logic broadcast không dùng host)
    void setGateway(const char* host, uint16_t port);
    void setLogger(MeoLogFunction logger);

    // Thực hiện đăng ký
    // 1) UDP broadcast IP/MAC/features
    // 2) Listen TCP 8091 đợi phản hồi
    bool registerIfNeeded(const MeoDeviceInfo& devInfo,
                          const MeoFeatureRegistry& features,
                          std::string& deviceIdOut,
                          std::string& transmitKeyOut);

private:
    std::string    _gatewayHost;
    uint16_t       _port;
    MeoLogFunction _logger;

    // Helper functions
    bool _sendBroadcast(const MeoDeviceInfo& devInfo,
                        const MeoFeatureRegistry& features);
    
    bool _waitForRegistrationResponse(std::string& responseJson);
    
    bool _parseRegistrationResponse(const std::string& json,
                                    std::string& deviceIdOut,
                                    std::string& transmitKeyOut);

    // Wrapper để log nội bộ
    void _log(const char* level, const char* msg);
};

#endif // MEO3_REGISTRATION_H