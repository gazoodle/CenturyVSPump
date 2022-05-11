#ifndef _inc_century_vs_pump_h
#define _inc_century_vs_pump_h

#include <esphome.h>
#include <queue>
#include <list>

/*
    I know there are multiple classes in this file, but I wanted to keep them in one place
    so it's easier for other folk to integrate into their ESPHome environment

    This component piggybacks on the modbus driver, replacing logically a modbus_controller
    since the controller has a hardwired view of what the protocol looks like and this protocol
    uses user defined functions.
*/

using namespace esphome;
using namespace sensor;
using namespace switch_;
using namespace modbus;

class CenturyVSPump;

/////////////////////////////////////////////////////////////////////////////////////////////////
class CenturyPumpCommand
{
public:
    CenturyVSPump *pump_{};
    uint8_t function_{};
    uint8_t ack_{0x20};
    std::vector<uint8_t> payload_ = {};
    std::function<void(CenturyVSPump *pump, const std::vector<uint8_t> &data)> on_data_func_;

    bool send();

    static CenturyPumpCommand create_status_command(CenturyVSPump *pump);
    static CenturyPumpCommand create_read_sensor_command(CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t scale, std::function<void(CenturyVSPump *pump, uint16_t value)> on_value_func);
    static CenturyPumpCommand create_run_command(CenturyVSPump *pump);
    static CenturyPumpCommand create_stop_command(CenturyVSPump *pump);
};

/////////////////////////////////////////////////////////////////////////////////////////////////
class CenturyPumpItemBase
{
public:
    CenturyPumpItemBase(CenturyVSPump *pump) : pump_(pump) {}
    virtual CenturyPumpCommand create_command() = 0;

protected:
    CenturyVSPump *pump_;
};

/////////////////////////////////////////////////////////////////////////////////////////////////
class CenturyPumpEnabledSwitch : public esphome::switch_::Switch
{
public:
    void write_state(bool state) override
    {
        enabled_ = state;
        this->publish_state(enabled_);
    }

private:
    bool enabled_{false};
};

/////////////////////////////////////////////////////////////////////////////////////////////////
class CenturyPumpRunSwitch : public CenturyPumpItemBase, public esphome::switch_::Switch
{
public:
    CenturyPumpRunSwitch(CenturyVSPump *pump, const std::string &name) : CenturyPumpItemBase(pump)
    {
        set_name(name);
        App.register_switch(this);
    }

    CenturyPumpCommand create_command() override;
    void write_state(bool state) override;

private:
};

/////////////////////////////////////////////////////////////////////////////////////////////////
class CenturyPumpSensor : public CenturyPumpItemBase, public Sensor
{
public:
    CenturyPumpSensor(CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t scale, const std::string &name, const std::string &unit_of_measurement)
        : CenturyPumpItemBase(pump), page_(page), address_(address), scale_(scale)
    {
        set_name(name);
        set_unit_of_measurement(unit_of_measurement);
        App.register_sensor(this);
    }

    CenturyPumpCommand create_command() override;

private:
    uint8_t page_;
    uint8_t address_;
    uint16_t scale_;
};

/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  To work successfully, this component needs modification to the ESPHome modbus.cpp file which
//  can be found in the PR .... etc etc.
//
class CenturyVSPump : public PollingComponent,
                      public ModbusDevice
{
public:
    CenturyVSPump(Modbus *parent, const char *name, uint8_t address)
    {
        set_update_interval(10000);
        set_parent(parent);
        set_address(address);
        parent->register_device(this);
        name_ = name;
    }

    uint8_t get_address() const { return this->address_; }

    void loop() override;
    void setup() override;
    void update() override;
    void dump_config() override;

    /// called when a modbus response was parsed without errors
    void on_modbus_data(const std::vector<uint8_t> &data) override;
    /// called when a modbus error response was received
    void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;
    void queue_command_(const CenturyPumpCommand &cmd);

protected:
    void process_modbus_data_(const CenturyPumpCommand *response);
    bool send_next_command_();

private:
    std::list<std::unique_ptr<CenturyPumpCommand>> command_queue_;
    std::queue<std::unique_ptr<CenturyPumpCommand>> response_queue_;
    uint32_t last_command_timestamp_;
    uint16_t command_throttle_{10};

public:
    std::string name_;
    std::vector<CenturyPumpItemBase *> items_;
    CenturyPumpRunSwitch *run_switch_{nullptr};
    CenturyPumpSensor *rpm_sensor_{nullptr};
    CenturyPumpEnabledSwitch *enabled_switch_{nullptr};
};

#endif
