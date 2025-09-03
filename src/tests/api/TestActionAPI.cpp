#include "../TestFramework.h"
#include "../../api/ActionAPI.h"
#include "../../api/ActionSchemas.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace mixmind::tests {

// ============================================================================
// Action API Tests
// ============================================================================

class ActionAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        testEnv = &getTestEnvironment();
        
        // Create mock interfaces (would be actual implementations in real tests)
        session = std::make_shared<MockISession>();
        transport = std::make_shared<MockITransport>();
        trackManager = std::make_shared<MockITrack>();
        clipManager = std::make_shared<MockIClip>();
        pluginHost = std::make_shared<MockIPluginHost>();
        automation = std::make_shared<MockIAutomation>();
        renderService = std::make_shared<MockIRenderService>();
        mediaLibrary = std::make_shared<MockIMediaLibrary>();
        audioProcessor = std::make_shared<MockIAudioProcessor>();
        asyncService = std::make_shared<MockIAsyncService>();
        ossServices = std::make_shared<MockOSSServiceRegistry>();
        
        actionAPI = std::make_shared<api::ActionAPI>(
            session, transport, trackManager, clipManager, pluginHost,
            automation, renderService, mediaLibrary, audioProcessor,
            asyncService, ossServices
        );
        
        // Initialize the API
        auto future = actionAPI->initialize();
        ASSERT_TRUE(TestUtils::waitForResult(future));
        auto result = future.get();
        ASSERT_TRUE(result.hasValue()) << "Failed to initialize ActionAPI: " << result.getErrorMessage();
    }
    
    void TearDown() override {
        if (actionAPI) {
            auto future = actionAPI->shutdown();
            TestUtils::waitForResult(future);
            actionAPI.reset();
        }
    }
    
    TestEnvironment* testEnv;
    
    // Mock interfaces
    std::shared_ptr<MockISession> session;
    std::shared_ptr<MockITransport> transport;
    std::shared_ptr<MockITrack> trackManager;
    std::shared_ptr<MockIClip> clipManager;
    std::shared_ptr<MockIPluginHost> pluginHost;
    std::shared_ptr<MockIAutomation> automation;
    std::shared_ptr<MockIRenderService> renderService;
    std::shared_ptr<MockIMediaLibrary> mediaLibrary;
    std::shared_ptr<MockIAudioProcessor> audioProcessor;
    std::shared_ptr<MockIAsyncService> asyncService;
    std::shared_ptr<MockOSSServiceRegistry> ossServices;
    
    std::shared_ptr<api::ActionAPI> actionAPI;
};

// ============================================================================
// Mock Interface Definitions (would be in separate file normally)
// ============================================================================

class MockISession : public core::ISession {
public:
    MOCK_METHOD(core::AsyncResult<core::VoidResult>, createSession, 
        (const std::string&, core::SampleRate, int32_t), (override));
    MOCK_METHOD(core::AsyncResult<core::VoidResult>, loadSession, (const std::string&), (override));
    MOCK_METHOD(core::AsyncResult<core::VoidResult>, saveSession, 
        (std::optional<std::string>), (override));
    MOCK_METHOD(core::AsyncResult<core::Result<SessionInfo>>, getSessionInfo, (), (const, override));
    // ... other methods would be mocked here
};

class MockITransport : public core::ITransport {
public:
    MOCK_METHOD(core::AsyncResult<core::VoidResult>, play, (std::optional<core::TimePosition>), (override));
    MOCK_METHOD(core::AsyncResult<core::VoidResult>, stop, (), (override));
    MOCK_METHOD(core::AsyncResult<core::VoidResult>, pause, (), (override));
    MOCK_METHOD(bool, isPlaying, (), (const, override));
    MOCK_METHOD(bool, isRecording, (), (const, override));
    MOCK_METHOD(bool, isPaused, (), (const, override));
    MOCK_METHOD(core::TimePosition, getCurrentPosition, (), (const, override));
    MOCK_METHOD(core::TimeDuration, getLength, (), (const, override));
    MOCK_METHOD(double, getTempo, (), (const, override));
    MOCK_METHOD(bool, isLooping, (), (const, override));
    // ... other methods would be mocked here
};

// Similar mock definitions for other interfaces...
class MockITrack : public core::ITrack { /* ... */ };
class MockIClip : public core::IClip { /* ... */ };
class MockIPluginHost : public core::IPluginHost { /* ... */ };
class MockIAutomation : public core::IAutomation { /* ... */ };
class MockIRenderService : public core::IRenderService { /* ... */ };
class MockIMediaLibrary : public core::IMediaLibrary { /* ... */ };
class MockIAudioProcessor : public core::IAudioProcessor { /* ... */ };
class MockIAsyncService : public core::IAsyncService { /* ... */ };
class MockOSSServiceRegistry : public services::OSSServiceRegistry { /* ... */ };

// ============================================================================
// Action Registration Tests
// ============================================================================

MIXMIND_TEST_F(ActionAPITest, RegisterCustomAction) {
    api::ActionDefinition customAction;
    customAction.name = "test.customAction";
    customAction.category = "test";
    customAction.description = "A custom test action";
    customAction.jsonSchema = json{
        {"type", "object"},
        {"properties", {
            {"value", {"type", "number"}},
            {"message", {"type", "string"}}
        }},
        {"required", {"value"}}
    };
    customAction.handler = [](const json& params, const api::ActionContext& context) -> api::ActionResult {
        auto value = params["value"].get<double>();
        auto message = params.value("message", "default");
        
        if (value > 10.0) {
            return api::ActionResult::createError("Value too large");
        }
        
        return api::ActionResult::createSuccess("Action executed", json{
            {"processedValue", value * 2},
            {"processedMessage", message + " (processed)"}
        });
    };
    
    auto result = actionAPI->registerAction(customAction);
    EXPECT_TRUE(result.hasValue()) << "Failed to register action: " << result.getErrorMessage();
    
    // Verify action is registered
    auto actions = actionAPI->getRegisteredActions();
    EXPECT_TRUE(std::find(actions.begin(), actions.end(), "test.customAction") != actions.end());
    
    // Verify action definition can be retrieved
    auto actionDef = actionAPI->getActionDefinition("test.customAction");
    ASSERT_TRUE(actionDef.has_value());
    EXPECT_EQ(actionDef->name, "test.customAction");
    EXPECT_EQ(actionDef->category, "test");
}

MIXMIND_TEST_F(ActionAPITest, UnregisterAction) {
    // Register a test action first
    api::ActionDefinition testAction;
    testAction.name = "test.temporary";
    testAction.handler = [](const json&, const api::ActionContext&) -> api::ActionResult {
        return api::ActionResult::createSuccess();
    };
    
    auto registerResult = actionAPI->registerAction(testAction);
    ASSERT_TRUE(registerResult.hasValue());
    
    // Verify it's registered
    auto actions = actionAPI->getRegisteredActions();
    EXPECT_TRUE(std::find(actions.begin(), actions.end(), "test.temporary") != actions.end());
    
    // Unregister it
    auto unregisterResult = actionAPI->unregisterAction("test.temporary");
    EXPECT_TRUE(unregisterResult.hasValue());
    
    // Verify it's no longer registered
    actions = actionAPI->getRegisteredActions();
    EXPECT_TRUE(std::find(actions.begin(), actions.end(), "test.temporary") == actions.end());
}

// ============================================================================
// Action Execution Tests
// ============================================================================

MIXMIND_TEST_F(ActionAPITest, ExecuteValidAction) {
    // Register a simple test action
    api::ActionDefinition testAction;
    testAction.name = "test.simple";
    testAction.jsonSchema = json{
        {"type", "object"},
        {"properties", {
            {"input", {"type", "string"}}
        }},
        {"required", {"input"}}
    };
    testAction.handler = [](const json& params, const api::ActionContext& context) -> api::ActionResult {
        auto input = params["input"].get<std::string>();
        return api::ActionResult::createSuccess("Action completed", json{
            {"output", "Processed: " + input}
        });
    };
    
    auto registerResult = actionAPI->registerAction(testAction);
    ASSERT_TRUE(registerResult.hasValue());
    
    // Execute the action
    json parameters = json{{"input", "test data"}};
    auto future = actionAPI->executeAction("test.simple", parameters);
    ASSERT_TRUE(TestUtils::waitForResult(future));
    
    auto result = future.get();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "Action completed");
    EXPECT_EQ(result.data["output"].get<std::string>(), "Processed: test data");
    EXPECT_FALSE(result.actionId.empty());
}

MIXMIND_TEST_F(ActionAPITest, ExecuteActionWithInvalidParameters) {
    // Register a test action with strict schema
    api::ActionDefinition testAction;
    testAction.name = "test.strict";
    testAction.jsonSchema = json{
        {"type", "object"},
        {"properties", {
            {"requiredNumber", {"type", "number", "minimum", 0, "maximum", 100}}
        }},
        {"required", {"requiredNumber"}}
    };
    testAction.handler = [](const json&, const api::ActionContext&) -> api::ActionResult {
        return api::ActionResult::createSuccess();
    };
    
    auto registerResult = actionAPI->registerAction(testAction);
    ASSERT_TRUE(registerResult.hasValue());
    
    // Try to execute with invalid parameters
    json invalidParameters = json{{"requiredNumber", -5}}; // Below minimum
    auto future = actionAPI->executeAction("test.strict", invalidParameters);
    ASSERT_TRUE(TestUtils::waitForResult(future));
    
    auto result = future.get();
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorCode.empty());
}

MIXMIND_TEST_F(ActionAPITest, ExecuteNonExistentAction) {
    json parameters = json{{"test", "value"}};
    auto future = actionAPI->executeAction("nonexistent.action", parameters);
    ASSERT_TRUE(TestUtils::waitForResult(future));
    
    auto result = future.get();
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, "ACTION_NOT_FOUND");
}

// ============================================================================
// Schema Validation Tests
// ============================================================================

MIXMIND_TEST_F(ActionAPITest, ValidateActionParameters) {
    json validSchema = json{
        {"type", "object"},
        {"properties", {
            {"name", {"type", "string", "minLength", 1}},
            {"age", {"type", "number", "minimum", 0, "maximum", 150}}
        }},
        {"required", {"name"}}
    };
    
    // Test valid data
    json validData = json{{"name", "John", "age", 25}};
    auto result = api::ActionAPI::validateJson(validData, validSchema);
    EXPECT_TRUE(result.hasValue());
    
    // Test invalid data - missing required field
    json invalidData1 = json{{"age", 25}};
    result = api::ActionAPI::validateJson(invalidData1, validSchema);
    EXPECT_FALSE(result.hasValue());
    
    // Test invalid data - wrong type
    json invalidData2 = json{{"name", 123}};
    result = api::ActionAPI::validateJson(invalidData2, validSchema);
    EXPECT_FALSE(result.hasValue());
    
    // Test invalid data - out of range
    json invalidData3 = json{{"name", "John", "age", 200}};
    result = api::ActionAPI::validateJson(invalidData3, validSchema);
    EXPECT_FALSE(result.hasValue());
}

MIXMIND_TEST_F(ActionAPITest, GetValidationErrors) {
    json schema = json{
        {"type", "object"},
        {"properties", {
            {"value", {"type", "number", "minimum", 0}}
        }},
        {"required", {"value"}}
    };
    
    json invalidData = json{{"value", -5}};
    auto errors = api::ActionAPI::getValidationErrors(invalidData, schema);
    
    EXPECT_FALSE(errors.empty());
    // Check that error message contains relevant information
    bool hasMinimumError = false;
    for (const auto& error : errors) {
        if (error.find("minimum") != std::string::npos) {
            hasMinimumError = true;
            break;
        }
    }
    EXPECT_TRUE(hasMinimumError);
}

// ============================================================================
// Action Discovery Tests
// ============================================================================

MIXMIND_TEST_F(ActionAPITest, GetActionsByCategory) {
    // Register actions in different categories
    api::ActionDefinition action1;
    action1.name = "transport.play";
    action1.category = "transport";
    action1.handler = [](const json&, const api::ActionContext&) -> api::ActionResult {
        return api::ActionResult::createSuccess();
    };
    
    api::ActionDefinition action2;
    action2.name = "transport.stop";
    action2.category = "transport";
    action2.handler = [](const json&, const api::ActionContext&) -> api::ActionResult {
        return api::ActionResult::createSuccess();
    };
    
    api::ActionDefinition action3;
    action3.name = "track.create";
    action3.category = "track";
    action3.handler = [](const json&, const api::ActionContext&) -> api::ActionResult {
        return api::ActionResult::createSuccess();
    };
    
    actionAPI->registerAction(action1);
    actionAPI->registerAction(action2);
    actionAPI->registerAction(action3);
    
    // Test category filtering
    auto transportActions = actionAPI->getActionsByCategory("transport");
    EXPECT_EQ(transportActions.size(), 2);
    EXPECT_TRUE(std::find(transportActions.begin(), transportActions.end(), "transport.play") != transportActions.end());
    EXPECT_TRUE(std::find(transportActions.begin(), transportActions.end(), "transport.stop") != transportActions.end());
    
    auto trackActions = actionAPI->getActionsByCategory("track");
    EXPECT_EQ(trackActions.size(), 1);
    EXPECT_TRUE(std::find(trackActions.begin(), trackActions.end(), "track.create") != trackActions.end());
    
    // Test categories list
    auto categories = actionAPI->getCategories();
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), "transport") != categories.end());
    EXPECT_TRUE(std::find(categories.begin(), categories.end(), "track") != categories.end());
}

MIXMIND_TEST_F(ActionAPITest, SearchActions) {
    // Register some test actions
    api::ActionDefinition action1;
    action1.name = "audio.volume.set";
    action1.description = "Set audio volume level";
    action1.handler = [](const json&, const api::ActionContext&) -> api::ActionResult {
        return api::ActionResult::createSuccess();
    };
    
    api::ActionDefinition action2;
    action2.name = "audio.mute.toggle";
    action2.description = "Toggle mute state";
    action2.handler = [](const json&, const api::ActionContext&) -> api::ActionResult {
        return api::ActionResult::createSuccess();
    };
    
    api::ActionDefinition action3;
    action3.name = "track.volume.set";
    action3.description = "Set track volume";
    action3.handler = [](const json&, const api::ActionContext&) -> api::ActionResult {
        return api::ActionResult::createSuccess();
    };
    
    actionAPI->registerAction(action1);
    actionAPI->registerAction(action2);
    actionAPI->registerAction(action3);
    
    // Search by name
    auto volumeActions = actionAPI->searchActions("volume");
    EXPECT_EQ(volumeActions.size(), 2);
    
    // Search by description
    auto audioActions = actionAPI->searchActions("audio");
    EXPECT_GE(audioActions.size(), 1);
    
    // Case insensitive search
    auto muteActions = actionAPI->searchActions("MUTE");
    EXPECT_GE(muteActions.size(), 1);
}

// ============================================================================
// Batch Operations Tests
// ============================================================================

MIXMIND_TEST_F(ActionAPITest, BatchExecution) {
    // Register multiple test actions
    api::ActionDefinition incrementAction;
    incrementAction.name = "test.increment";
    incrementAction.jsonSchema = json{
        {"type", "object"},
        {"properties", {{"value", {"type", "number"}}}},
        {"required", {"value"}}
    };
    incrementAction.handler = [](const json& params, const api::ActionContext&) -> api::ActionResult {
        auto value = params["value"].get<double>();
        return api::ActionResult::createSuccess("Incremented", json{{"result", value + 1}});
    };
    
    api::ActionDefinition multiplyAction;
    multiplyAction.name = "test.multiply";
    multiplyAction.jsonSchema = json{
        {"type", "object"},
        {"properties", {{"value", {"type", "number"}}, {"factor", {"type", "number"}}}},
        {"required", {"value", "factor"}}
    };
    multiplyAction.handler = [](const json& params, const api::ActionContext&) -> api::ActionResult {
        auto value = params["value"].get<double>();
        auto factor = params["factor"].get<double>();
        return api::ActionResult::createSuccess("Multiplied", json{{"result", value * factor}});
    };
    
    actionAPI->registerAction(incrementAction);
    actionAPI->registerAction(multiplyAction);
    
    // Execute batch
    std::vector<std::pair<std::string, json>> batch = {
        {"test.increment", json{{"value", 5}}},
        {"test.multiply", json{{"value", 3}, {"factor", 4}}}
    };
    
    auto future = actionAPI->executeActionBatch(batch);
    ASSERT_TRUE(TestUtils::waitForResult(future));
    
    auto results = future.get();
    ASSERT_EQ(results.size(), 2);
    
    EXPECT_TRUE(results[0].success);
    EXPECT_EQ(results[0].data["result"].get<double>(), 6.0);
    
    EXPECT_TRUE(results[1].success);
    EXPECT_EQ(results[1].data["result"].get<double>(), 12.0);
}

// ============================================================================
// Performance Tests
// ============================================================================

MIXMIND_TEST_F(ActionAPITest, ActionExecutionPerformance) {
    // Register a simple action
    api::ActionDefinition simpleAction;
    simpleAction.name = "perf.simple";
    simpleAction.jsonSchema = json{{"type", "object"}};
    simpleAction.handler = [](const json&, const api::ActionContext&) -> api::ActionResult {
        return api::ActionResult::createSuccess("OK");
    };
    
    actionAPI->registerAction(simpleAction);
    
    constexpr int32_t iterations = 1000;
    json params = json::object();
    
    BENCHMARK_TEST("Simple Action Execution", {
        auto future = actionAPI->executeAction("perf.simple", params);
        TestUtils::waitForResult(future);
        auto result = future.get();
        EXPECT_TRUE(result.success);
    }, iterations / 10); // Reduce iterations for async operations
}

MIXMIND_TEST_F(ActionAPITest, SchemaValidationPerformance) {
    json schema = json{
        {"type", "object"},
        {"properties", {
            {"name", {"type", "string"}},
            {"value", {"type", "number", "minimum", 0}}
        }},
        {"required", {"name", "value"}}
    };
    
    json validData = json{{"name", "test", "value", 42}};
    
    constexpr int32_t iterations = 10000;
    
    BENCHMARK_TEST("JSON Schema Validation", {
        auto result = api::ActionAPI::validateJson(validData, schema);
        EXPECT_TRUE(result.hasValue());
    }, iterations);
}

} // namespace mixmind::tests