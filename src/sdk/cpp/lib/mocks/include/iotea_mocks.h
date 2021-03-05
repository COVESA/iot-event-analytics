#ifndef MOCK_IOTEA_H_
#define MOCK_IOTEA_H_

#include <string>

#include "gmock/gmock.h"

#include "iotea.hpp"


namespace iotea {
namespace mock {
namespace core {

class Publisher : public iotea::core::Publisher {
    public:
        MOCK_METHOD(void, Publish, (const std::string& topic, const std::string& data), (override));
        MOCK_METHOD(std::string, GetIngestionTopic, (), (const, override));
        MOCK_METHOD(std::string, GetNamespace, (), (const, override));
};

} // namespace core
} // namespace mock
} // namespace iotea

#endif // MOCK_IOTEA_H_

