#ifndef HERBSTLUFT_SIGNAL_H
#define HERBSTLUFT_SIGNAL_H

#include <functional>
#include <stdexcept>
#include <vector>

class Signal {
public:
    Signal() = default;
    virtual ~Signal() = default;

    // connect signal to anonymous/top-level method
    void connect(std::function<void()> slot) {
        slots_0arg_.push_back(slot);
    }

    // connect signal to object method
    template<typename Owner>
    void connect(Owner* owner, void(Owner::*slot)()) {
        slots_0arg_.push_back(std::bind(slot, owner));
    }

    // connect signal to slot
    void connect(const Signal& slot) {
        slots_0arg_.push_back([&slot]() { slot.emit(); });
        // WARNING: above lambda must not be replaced with a
        // bind function:
        //
        //      slots_0arg_.push_back(std::bind(&Signal::emit, slot));
        //
        // because Signal::emit is virtual, so the wrong function
        // may be called!
    }

    // emit the signal
    // instantly calls all receiving slots
    virtual void emit() const {
        for (const auto& s : slots_0arg_) {
            s();
        }
    }

protected:
    std::vector<std::function<void()>> slots_0arg_;
};

template<typename T>
class Signal_ : public Signal {
public:
    using Signal::connect;
    void connect(std::function<void(T)> slot) {
        slots_1arg_.push_back(slot);
    }
    template<typename Owner>
    void connect(Owner* owner, void(Owner::*slot)(T)) {
        slots_1arg_.push_back(std::bind(slot, owner, std::placeholders::_1));
    }
    void connect(const Signal_<T>& slot) {
        slots_1arg_.push_back([&slot](T data){ slot.emit(data); });
    }
    void emit() const override {
        throw new std::invalid_argument("emit() called without data argument");
    }
    void emit(const T& data) const {
        Signal::emit();
        for (const auto& s : slots_1arg_) {
            s(data);
        }
    }
private:
    std::vector<std::function<void(T)>> slots_1arg_;
};

#endif
