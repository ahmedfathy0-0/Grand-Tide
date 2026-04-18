#include "event-manager.hpp"

namespace our {
    std::unordered_map<std::string, std::vector<EventManager::EventCallback>> EventManager::listeners;

    void EventManager::subscribe(const std::string& eventName, EventCallback callback) {
        listeners[eventName].push_back(callback);
    }

    void EventManager::emit(const std::string& eventName, int payload) {
        if (listeners.find(eventName) != listeners.end()) {
            for (auto& callback : listeners[eventName]) {
                callback(payload);
            }
        }
    }
}
