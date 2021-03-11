#include <csignal>
#include <initializer_list>
#include <memory>

#include "nlohmann/json.hpp"
#include "iotea.hpp"
#include "logging.hpp"
#include "mqtt_client.hpp"
#include "schema.hpp"

using namespace iotea::core;
using json = nlohmann::json;

static const std::string SERVER_ADDRESS("tcp://localhost:1883");
static const std::string TALENT_NAME("echo_observer");
static const std::string PROVIDER_TALENT_NAME("echo_provider");
static const std::string SUBSCRIBED_ECHO_EVENT(PROVIDER_TALENT_NAME+".echoResponseSent");
static const std::string SUBSCRIBED_COUNT_EVENT(PROVIDER_TALENT_NAME+".echoCount");


class EchoObserver : public Talent {
   public:
    EchoObserver(std::shared_ptr<Publisher> publisher)
        : Talent(TALENT_NAME, publisher) {
    }

    schema::rules_ptr OnGetRules() const override {
        return OrRules({IsSet(SUBSCRIBED_ECHO_EVENT),
                        IsSet(SUBSCRIBED_COUNT_EVENT)});
    }

    void OnEvent(const Event& event, EventContext context) override {
        if (event.GetFeature() == SUBSCRIBED_ECHO_EVENT) {
            auto message = json{event.GetValue().get<std::string>()};
            log::Info() << "Received echo: '" << message << "'";
        } else if (event.GetFeature() == SUBSCRIBED_COUNT_EVENT) {
            auto echoCount = json{event.GetValue().get<unsigned int>()};
            log::Info() << "Received echoCount: " << echoCount;
        } else {
            log::Warn() << "UNKNOWN EVENT RECEIVED";
        }
    }
};

static std::shared_ptr<MqttClient> client = std::make_shared<MqttClient>(SERVER_ADDRESS, TALENT_NAME);

void signal_handler(int signal) { client->Stop(); }

int main(int argc, char* argv[]) {
    auto talent = std::make_shared<EchoObserver>(client);
    client->RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);

    client->Run();

    return 0;
}
