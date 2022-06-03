#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"

#include <queue>
#include <list>

// #define MODBUS_ENABLE_SWITCH

/*
    I know there are multiple classes in this file, but I wanted to keep them in one place
    so it's easier for other folk to integrate into their ESPHome environment

    This component piggybacks on the modbus driver, replacing logically a modbus_controller
    since the controller has a hardwired view of what the protocol looks like and this protocol
    uses user defined functions.
*/

namespace esphome
{
    using namespace modbus;

    namespace century_vs_pump
    {

        class CenturyVSPump;
        class CenturyVSPumpSensor;

        /////////////////////////////////////////////////////////////////////////////////////////////////
        class CenturyPumpCommand
        {
        public:
            static const uint8_t MAX_SEND_REPEATS = 5;
            CenturyVSPump *pump_{};
            uint8_t function_{};
            uint8_t ack_{0x20};
            std::vector<uint8_t> payload_ = {};
            std::function<void(CenturyVSPump *pump, const std::vector<uint8_t> &data)> on_data_func_;
            // limit the number of repeats
            uint8_t send_countdown{MAX_SEND_REPEATS};

            bool send();

            static CenturyPumpCommand create_status_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump, bool running)> on_status_func);
            static CenturyPumpCommand create_read_sensor_command(CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t scale, std::function<void(CenturyVSPump *pump, uint16_t value)> on_value_func);
            static CenturyPumpCommand create_run_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump)> on_confirmation_func);
            static CenturyPumpCommand create_stop_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump)> on_confirmation_func);
            static CenturyPumpCommand create_set_demand_command(CenturyVSPump *pump, uint16_t demand, std::function<void(CenturyVSPump *pump)> on_confirmation_func);
        };

        /////////////////////////////////////////////////////////////////////////////////////////////////
        class CenturyPumpItemBase
        {
        public:
            CenturyPumpItemBase() : pump_(nullptr) {}
            CenturyPumpItemBase(CenturyVSPump *pump) : pump_(pump) {}
            virtual CenturyPumpCommand create_command() = 0;

            void set_pump(CenturyVSPump *pump) { pump_ = pump; }

        protected:
            CenturyVSPump *pump_;
        };

/////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef MODBUS_ENABLE_SWITCH
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
#endif

        /////////////////////////////////////////////////////////////////////////////////////////////////
        //
        //  To work successfully, this component needs modification to the ESPHome modbus.cpp file which
        //  can be found in the `modbus` component in https://github.com/gazoodle/esphome branch
        //  "Feature-Request-#1725"
        //
        class CenturyVSPump : public PollingComponent,
                              public ModbusDevice
        {
        public:
            CenturyVSPump() {}

            uint8_t get_address() const { return this->address_; }

            void loop() override;
            void setup() override;
            void update() override;
            void dump_config() override;

            /// called when a modbus response was parsed without errors
            void on_modbus_data(const std::vector<uint8_t> &data) override;
            /// called when a modbus error response was received
            void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;
            /// Registers an item with the controller. Called by esphomes code generator
            void add_item(CenturyPumpItemBase *item) { items_.push_back(item); }
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
#ifdef MODBUS_ENABLE_SWITCH
            CenturyPumpEnabledSwitch *enabled_switch_{nullptr};
#endif
        };

    }
}
