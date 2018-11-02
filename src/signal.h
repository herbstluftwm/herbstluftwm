#ifndef HERBSTLUFT_SIGNAL_H
#define HERBSTLUFT_SIGNAL_H

#include<functional>
#include<vector>

class Signal {
public:
    Signal();
};

template<typename T>
class Signal_ : public Signal {
public:
    void connect(std::function<void(T)> slot) {
        slots_.push_back(slot);
    }
    template<typename Owner>
    void connect(Owner* owner, void(Owner::*slot)(T)) {
        slots_.push_back([owner,slot](const T& data) { (owner ->* slot)(data); });
    }

    void connect(const Signal_<T>& slot) {
        slots_.push_back([slot](const T& data){ slot.emit(data); });
    }
    void emit(const T& data) const {
        for (const auto& s : slots_) s(data);
    }
private:
    std::vector<std::function<void(T)>> slots_;
};

template<>
class Signal_<void> : public Signal {
public:
    void connect(std::function<void()> slot) {
        slots_.push_back(slot);
    }
    void emit() {
        for (const auto& s : slots_) s();
    }
private:
    std::vector<std::function<void()>> slots_;
};

#endif
