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
static const std::string TALENT_NAME("echo_consumer");
static const std::string PROVIDED_FEATURE_NAME("messageString");
static const std::string PROVIDED_FETAURE_TYPE(schema::DEFAULT_TYPE);
static const std::string CALLED_TALENT_NAME("echo_provider");
static const std::string CALLED_METHOD_NAME("echo");


class EchoConsumer : public Talent {
   private:
    struct EchoProvider {
        Callee echo;
    } echo_provider;

   public:
    explicit EchoConsumer(std::shared_ptr<Publisher> publisher)
        : Talent(TALENT_NAME, publisher) {
        AddOutput(PROVIDED_FEATURE_NAME, schema::Metadata("Message to be forwarded to echo provider", "ONE",
                                                          schema::OutputEncoding(schema::OutputEncoding::Type::String)));
        echo_provider.echo = CreateCallee(CALLED_TALENT_NAME, CALLED_METHOD_NAME);
        schema_.SkipCycleCheckFor({PROVIDED_FETAURE_TYPE+"."+TALENT_NAME+"."+PROVIDED_FEATURE_NAME});
    }

    schema::rules_ptr OnGetRules() const override {
        return OrRules(IsSet(TALENT_NAME+"."+PROVIDED_FEATURE_NAME));
    }

    void OnEvent(const Event& event, EventContext context) override {
        if (event.GetType() == PROVIDED_FETAURE_TYPE) {
            auto message = json{event.GetValue().get<std::string>()};
            log::Info() << "Received message:  '" << message << "'";

            auto t = echo_provider.echo.Call(message, context);

            context.Gather([](std::vector<std::pair<json, EventContext>> replies) {
                    log::Info() << "Received echo:     '" << replies[0].first.dump(4) << "'";
                }, {t});

            log::Info() << "Forwarded message: '" << message << "'";
        } else {
            log::Warn() << "UNKNOWN EVENT RECEIVED";
        }
    }
};

static std::shared_ptr<MqttClient> client = std::make_shared<MqttClient>(SERVER_ADDRESS, TALENT_NAME);

void signal_handler(int signal) { client->Stop(); }

int main(int argc, char* argv[]) {
    auto talent = std::make_shared<EchoConsumer>(client);
    client->RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);

    client->Run();

    return 0;
}
