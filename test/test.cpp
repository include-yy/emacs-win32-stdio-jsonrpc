#include "jsonrpc.hpp"
#include <iostream>
#include <string>

std::unique_ptr<jsonrpc::Conn> server;

int main() {
    return 0;
}

//int main() {
//    // register methods
//    server.register_method("add", [](const jsonrpc::json& params) -> jsonrpc::json {
//        if (!params.is_array() || params.size() < 2) {
//            throw std::invalid_argument("Expected array with 2 numbers");
//        }
//        return params[0].get<double>() + params[1].get<double>();
//        });
//    server.register_method("echo", [](const jsonrpc::json& params) -> jsonrpc::json {
//        return params;
//        });
//    // start server
//    std::cerr << "[Server] Starting..." << std::endl;
//    server.start();
//
//    // Windows message loop
//    MSG msg;
//    while (GetMessage(&msg, NULL, 0, 0)) {
//        if (msg.message == WM_JSONRPC_MESSAGE) {
//            server.process_queue();
//        }
//        else {
//            TranslateMessage(&msg);
//            DispatchMessage(&msg);
//        }
//    }
//    return 0;
//}