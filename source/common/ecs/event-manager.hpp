#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace our {
    class EventManager {
    public:
        using EventCallback = std::function<void(int)>;

        static void subscribe(const std::string& eventName, EventCallback callback);
        static void emit(const std::string& eventName, int payload = 0);

    private:
        static std::unordered_map<std::string, std::vector<EventCallback>> listeners;
    };
}
