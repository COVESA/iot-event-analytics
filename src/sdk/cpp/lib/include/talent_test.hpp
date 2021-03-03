#ifndef TALENT_TEST_HPP
#define TALENT_TEST_HPP

#include "iotea.hpp"
#include "nlohmann/json.hpp"

#include <functional>
#include <unordered_map>

namespace iotea {
namespace test {

class TestResult {
   private:
    const std::string name_;
    const json actual_value_;
    const int32_t duration_;

   public:
    TestResult(const std::string& name, const json& actual_value, int32_t duration);

    json Json() const;
};

class Test {
   private:
    const std::string name_;
    const json expected_value_;
    const std::function<void(const core::CallContext&)> func_;
    const uint32_t timeout_;

   public:
    Test(const std::string& name, const json& expected_value, const std::function<void(const core::CallContext&)> func, uint32_t timeout);

    void Run(core::CallContext context);

    json Json() const;
};

class TestSetInfo {
   private:
    const std::string name_;
    std::unordered_map<std::string, Test> tests_;

   public:
    explicit TestSetInfo(const std::string& name);

    void AddTest(const std::string& name, const json& exepected_value, const std::function<void(const core::CallContext&)>& func, uint32_t timeout);

    void RunTest(const std::string& name, const core::CallContext& context);

    std::string GetName() const;

    json Json() const;
};

class TalentDependencies {
   private:
    std::unordered_map<std::string, bool> dependencies_;
    size_t n_dep_met_;

   public:
    void Add(const std::string& talent_id);

    void Update(const core::PlatformEvent& event);

    bool Check(const std::string& talent_id) const;

    bool CheckAll() const;

    json Json() const;
};

class TestSetTalent : public core::FunctionTalent {
   private:
    TestSetInfo test_set_info_;
    TalentDependencies dependencies_;

    void Prepare(const json& args, const core::CallContext& context);

    void GetInfo(const json& args, const core::CallContext& context);

    void Run(const json& args, const core::CallContext& context);

   public:
    TestSetTalent(const std::string& name, core::publisher_ptr publisher);

    void OnPlatformEvent(const core::PlatformEvent& event) override;

    void RegisterTest(const std::string& name, const json& expect, const core::Callee callee, const json& args, uint32_t timeout);
};


} // namespace test
} // namespace iotea

#endif // TALENT_TEST_HPP
