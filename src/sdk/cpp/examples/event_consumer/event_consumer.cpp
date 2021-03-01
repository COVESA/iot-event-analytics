#include <csignal>
#include <initializer_list>
#include <memory>

#include "iotea.hpp"
#include "nlohmann/json.hpp"
#include "logging.hpp"
#include "mqtt_client.hpp"
#include "schema.hpp"

using json = nlohmann::json;

const std::string SERVER_ADDRESS("tcp://localhost:1883");

using namespace iotea::core;

class EventConsumer : public Talent {
   private:
    struct ProviderTalent {
        Callee Multiply;
        Callee Fib;
    } provider_talent;

   public:
    explicit EventConsumer(std::shared_ptr<Publisher> publisher)
        : Talent("event_consumer", publisher) {
        provider_talent.Multiply = CreateCallee("provider_talent", "multiply");
        provider_talent.Fib = CreateCallee("provider_talent", "fibonacci");
    }

    void OnEvent(const Event& event, EventContext context) override {
        if (event.GetType() == "kuehlschrank") {
            auto args =
                json{event.GetValue().get<int>(), json{{"factor", event.GetValue().get<int>()}, {"unit", "thing"}}};

            auto t = provider_talent.Multiply.Call(args, context);

            context.Gather([](std::vector<std::pair<json, EventContext>> replies) {
                log::Info() << "Multiply result: " << replies[0].first.dump(4);
            }, {t});

            auto s = provider_talent.Fib.Call(args, context);

            context.Gather([](std::vector<std::pair<json, EventContext>> replies) {
                log::Info() << "Fibonacci result: " << replies[0].first.dump(4);
            }, {s});
        } else if (event.GetType() == "blob") {
            log::Info() << "Currently at " << event.GetValue().dump() << " dingdings";
        }
    }

    schema::rules_ptr OnGetRules() const override {
        return OrRules({AndRules({GreaterThan("temp", 2, "kuehlschrank"), LessThan("temp", 10, "kuehlschrank")}),
                        OrRules({IsSet("dingdings", "blob")})});
    }
};

static std::shared_ptr<MqttClient> client = std::make_shared<MqttClient>(SERVER_ADDRESS, "event_consumer");

void signal_handler(int signal) { client->Stop(); }

int main(int argc, char* argv[]) {
    auto talent = std::make_shared<EventConsumer>(client);
    client->RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);

    client->Run();

    return 0;
}
