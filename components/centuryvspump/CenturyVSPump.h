#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"

#include <deque>
#include <memory>
#include <span>
#include <vector>

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

        /// Acknowledgement byte the pump returns after the function code on a successful reply.
        static const uint8_t CENTURY_ACK = 0x10;

        //////////////////////////////////////////////////////////////////////////////////////////////////
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

            /// Build this command's PDU: [function][ack_][payload...].
            /// The modbus hub prepends the device address and appends the CRC.
            std::vector<uint8_t> build_pdu() const;

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
        //  This pump speaks a user-defined-function protocol rather than standard modbus registers, so
        //  it subclasses ModbusClientDevice directly and handles its own PDUs. Every function code it
        //  uses (0x41-0x45) falls in the modbus user-defined range, so all of its traffic arrives via
        //  on_custom_response(). No patched modbus component is required - user-defined function
        //  support has been part of mainline ESPHome since esphome/esphome#3461.
        //
        class CenturyVSPump : public PollingComponent,
                              public modbus::ModbusClientDevice
        {
        public:
            CenturyVSPump() {}

            uint8_t get_address() const { return this->address_; }

            void loop() override;
            void setup() override;
            void update() override;
            void dump_config() override;

            /// called when a response to one of our user-defined function codes was parsed.
            /// response_pdu is the whole PDU: [function][ACK][data...]. On failure it is empty and
            /// the modbus exception code is in status.
            void on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                                    modbus::ResponseStatus status) override;
            /// called when no matching response arrived; returns true to ask the hub to retry the frame
            bool on_no_response(std::span<const uint8_t> request_pdu) override;
            /// called when an accepted request was dropped before it reached the wire
            void on_not_sent(std::span<const uint8_t> request_pdu) override;
            /// called when the hub writes the current request to the wire
            void on_sent(std::span<const uint8_t> request_pdu) override;
            /// Registers an item with the controller. Called by esphomes code generator
            void add_item(CenturyPumpItemBase *item) { items_.push_back(item); }
            void queue_command_(const CenturyPumpCommand &cmd);

        private:
            bool has_active_request_() const { return this->request_active_ && !this->command_queue_.empty(); }
            std::deque<std::unique_ptr<CenturyPumpCommand>> command_queue_;
            bool request_active_{false};
            uint32_t last_command_timestamp_{0};
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
