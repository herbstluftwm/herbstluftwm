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

    // connect signal to anonymous/top-level method
    void connect(std::function<void(T)> slot) {
        slots_.push_back(slot);
    }

    // connect signal to object method
    template<typename Owner>
    void connect(Owner* owner, void(Owner::*slot)(T)) {
        slots_.push_back(std::bind(slot, owner, std::placeholders::_1));
    }

    // connect signal to slot
    void connect(const Signal_<T>& slot) {
        slots_.push_back(std::bind(&Signal_<T>::emit, slot, std::placeholders::_1));
    }

    // emit the signal
    // instantly calls all receiving slots
    void emit(const T& data) const {
        for (const auto& s : slots_) s(data);
    }
private:
    std::vector<std::function<void(T)>> slots_;
};

/* void-specialization for parameter-less signals */
template<>
class Signal_<void> : public Signal {
public:
    void connect(std::function<void()> slot) {
        slots_.push_back(slot);
    }
    template<typename Owner>
    void connect(Owner* owner, void(Owner::*slot)()) {
        slots_.push_back(std::bind(slot, owner));
    }
    void connect(const Signal_<void>& slot) {
        slots_.push_back(std::bind(&Signal_<void>::emit, slot));
    }
    void emit() const {
        for (const auto& s : slots_) s();
    }
private:
    std::vector<std::function<void()>> slots_;
};

#endif
