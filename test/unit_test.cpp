#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "jsonrpc.hpp"

using namespace jsonrpc;
using json = nlohmann::json;

#define CHECK_JSONRPC_EXCEPTION(statement, expected_code)            \
    try {                                                            \
        statement;                                                   \
        CHECK_MESSAGE(false, "Should have thrown JsonRpcException"); \
    } catch (const JsonRpcException& e) {                            \
        CHECK(e.code == expected_code);                              \
    } catch (...) {                                                  \
        CHECK_MESSAGE(false, "Threw wrong type of exception");       \
    }

TEST_CASE("Error Serialization/Deserialization") {
    SUBCASE("Edge Case: Error missing 'code'") {
        json j = {
            {"message", "Missing code"}
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Error>(), spec::kInternalError);
    }

    SUBCASE("Edge Case: Error 'code' is string") {
        json j = {
            {"code", "123"}, // Must be int.
            {"message", "Bad code type"}
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Error>(), spec::kInternalError);
    }

    SUBCASE("Edge Case: Error missing 'message'") {
        json j = {
            {"code", -32000}
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Error>(), spec::kInternalError);
    }
}


TEST_CASE("Request Serialization/Deserialization") {

    SUBCASE("Round-Trip: Valid Request with Array Params") {
        Request original{ 1, "add", json::array({1, 2}) };

        // 1. Struct -> JSON
        json j = original;
        CHECK(j["jsonrpc"] == "2.0");
        CHECK(j["method"] == "add");
        CHECK(j["id"] == 1);

        // 2. JSON -> Struct
        Request restored = j;
        CHECK(restored.id == 1);
        CHECK(restored.method == "add");
        CHECK(restored.params.is_array());
    }

    SUBCASE("Round-Trip: Valid Notification (No ID)") {
        Request original{ std::nullopt, "log", nullptr };

        json j = original;
        CHECK_FALSE(j.contains("id"));

        Request restored = j;
        CHECK(restored.id.has_value() == false);
        CHECK(restored.method == "log");
    }

    SUBCASE("Edge Case: Invalid Params Type (String)") {
        json j = {
            {"jsonrpc", "2.0"},
            {"method", "foo"},
            {"params", "I am a string"} // Invalid string
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Request>(), spec::kInvalidRequest);
    }

    SUBCASE("Edge Case: Invalid ID Type (String)") {
        json j = {
            {"jsonrpc", "2.0"},
            {"method", "foo"},
            {"id", "123"} // Support int id only
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Request>(), spec::kInvalidRequest);
    }
}

TEST_CASE("Request Advanced Validation") {
    SUBCASE("Edge Case: Request missing 'method'") {
        json j = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"params", json::array()}
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Request>(), spec::kInvalidRequest);
    }

    SUBCASE("Edge Case: Request 'method' is not string") {
        json j = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", 123} // method must be string.
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Request>(), spec::kInvalidRequest);
    }

    SUBCASE("Edge Case: Request params is null (Valid)") {
        json j = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "foo"},
            {"params", nullptr}
        };
        Request r = j;
        CHECK(r.params == nullptr);
    }
}

TEST_CASE("Response Serialization/Deserialization") {

    SUBCASE("Round-Trip: Success Response") {
        Response original = Response::make_success(42, "OK");

        json j = original;
        CHECK(j["id"] == 42);
        CHECK(j.contains("result"));
        CHECK_FALSE(j.contains("error"));

        Response restored = j;
        CHECK(restored.id == 42);
        CHECK(std::holds_alternative<json>(restored.content));
        CHECK(std::get<json>(restored.content) == "OK");
    }

    SUBCASE("Round-Trip: Error Response") {
        Response original = Response::make_error(42, spec::kMethodNotFound, "No such method");

        json j = original;
        CHECK(j["id"] == 42);
        CHECK(j.contains("error"));
        CHECK_FALSE(j.contains("result"));

        Response restored = j;
        CHECK(restored.id == 42);
        CHECK(std::holds_alternative<Error>(restored.content));

        auto& err = std::get<Error>(restored.content);
        CHECK(err.code == spec::kMethodNotFound);
        CHECK(err.message == "No such method");
    }

    SUBCASE("Edge Case: Response with BOTH result and error") {
        json j = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"result", "ok"},
            {"error", {{"code", 0}, {"message", ""}}}
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Response>(), spec::kInvalidRequest);
    }

    SUBCASE("Edge Case: Response with NEITHER result nor error") {
        json j = {
            {"jsonrpc", "2.0"},
            {"id", 1}
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Response>(), spec::kInvalidRequest);
    }

    SUBCASE("Edge Case: Response with Null ID (Should fail at struct level)") {
        json j = {
            {"jsonrpc", "2.0"},
            {"id", nullptr},
            {"error", {{"code", -32600}, {"message", "error"}}}
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Response>(), spec::kInvalidRequest);
    }
    SUBCASE("Edge Case: Response with no ID") {
        json j = {
            {"jsonrpc", "2.0"},
            {"error", {{"code", -32600}, {"message", "error"}}}
        };
        CHECK_JSONRPC_EXCEPTION(j.get<Response>(), spec::kInvalidRequest);
    }
}

TEST_CASE("Parser Behavior") {

    SUBCASE("Parse Valid Request") {
        json j = { {"jsonrpc", "2.0"}, {"method", "add"}, {"id", 1} };
        auto msg = Parser::parse(j);
        CHECK(std::holds_alternative<Request>(msg));
    }

    SUBCASE("Parse Valid Response") {
        json j = { {"jsonrpc", "2.0"}, {"result", "ok"}, {"id", 1} };
        auto msg = Parser::parse(j);
        CHECK(std::holds_alternative<Response>(msg));
    }

    SUBCASE("Parse Fatal Error (ID is null)") {
        json j = {
            {"jsonrpc", "2.0"},
            {"id", nullptr},
            {"error", {{"code", -32700}, {"message", "Parse error"}}}
        };

        auto msg = Parser::parse(j);

        CHECK(std::holds_alternative<Error>(msg));
        CHECK(std::get<Error>(msg).code == -32700);
    }

    SUBCASE("Parse Garbage (Missing method and id)") {
        json j = { {"jsonrpc", "2.0"}, {"foo", "bar"} };
        CHECK_JSONRPC_EXCEPTION(Parser::parse(j), spec::kInvalidRequest);
    }
}

TEST_CASE("Parser Advanced Boundaries") {
    SUBCASE("Parse Invalid: id is null but has result (Ambiguous)") {
        json j = {
            {"jsonrpc", "2.0"},
            {"id", nullptr},
            {"result", "Should not happen"}
        };
        CHECK_JSONRPC_EXCEPTION(Parser::parse(j), spec::kInvalidRequest);
    }

    SUBCASE("Parse Invalid: Not an object (Batch request?)") {
        // Batch is not supported.
        json j = json::array();
        CHECK_JSONRPC_EXCEPTION(Parser::parse(j), spec::kInvalidRequest);
    }
}
