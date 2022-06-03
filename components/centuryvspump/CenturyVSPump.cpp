#include "CenturyVSPump.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome
{
    namespace century_vs_pump
    {

        //////////////////////////////////////////////////////////////////////////////////////////////
        //
        //  CenturyVSPump implementation
        //
        /////////////////////////////////////////////////////////////////////////////////////////////

        static const char *const TAG = "century_vs_pump";

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::setup()
        {
#ifdef MODBUS_ENABLE_SWITCH
            enabled_switch_ = new CenturyPumpEnabledSwitch();
            enabled_switch_->set_name(name_ + " MODBUS enabled");
            App.register_switch(enabled_switch_);
#endif
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::loop()
        {
            // Incoming data to process?
            if (!response_queue_.empty())
            {
                auto &message = response_queue_.front();
                if (message != nullptr)
                    process_modbus_data_(message.get());
                response_queue_.pop();
            }
            else
            {
                // all messages processed send pending commmands
                send_next_command_();
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::update()
        {
            // Request status & pump RPM
#ifdef MODBUS_ENABLE_SWITCH
            if (enabled_switch_ == nullptr)
                return;
            if (enabled_switch_->state == 0)
                return;
#endif
            ESP_LOGV(TAG, "Updating pump component");
            for (auto item : items_)
                queue_command_(item->create_command());
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        /// called when a modbus response was parsed without errors
        void CenturyVSPump::on_modbus_data(const std::vector<uint8_t> &data)
        {
            ESP_LOGV(TAG, "Pump got data");
            auto &current_command = this->command_queue_.front();
            if (current_command != nullptr)
            {
                current_command->payload_ = data;
                this->response_queue_.push(std::move(current_command));
                ESP_LOGV(TAG, "Pump response queued");
                command_queue_.pop_front();
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        /// called when a modbus error response was received
        void CenturyVSPump::on_modbus_error(uint8_t function_code, uint8_t exception_code)
        {
            ESP_LOGV(TAG, "Received modbus error");
            auto &current_command = this->command_queue_.front();
            if (current_command != nullptr)
            {
                ESP_LOGD(TAG, "Modbus error, so removing current command (%02X) from queue", current_command->function_);
                command_queue_.pop_front();
            }
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::dump_config()
        {
            ESP_LOGCONFIG(TAG, "CenturyVSPump:");
            ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::queue_command_(const CenturyPumpCommand &command)
        {
#ifdef MODBUS_ENABLE_SWITCH
            if (enabled_switch_ == nullptr)
                return;
            if (enabled_switch_->state == 0)
                return;
#endif
            command_queue_.push_back(make_unique<CenturyPumpCommand>(command));
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::process_modbus_data_(const CenturyPumpCommand *response)
        {
            // Ensure function matches ...
            if (response->payload_[0] != response->function_)
            {
                ESP_LOGW(TAG, "Payload data function mismatch (%X != %X), ignoring", response->payload_[0], response->function_);
                return;
            }

            // Ensure ACK was OK
            if (response->payload_[1] != 0x10)
            {
                ESP_LOGW(TAG, "Function %X NACK with %X, ignoring", response->function_, response->payload_[0]);
                return;
            }

            // Pass to handler function
            std::vector<uint8_t> data(response->payload_.begin() + 2, response->payload_.end());
            response->on_data_func_(this, data);
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyVSPump::send_next_command_()
        {
            uint32_t last_send = millis() - this->last_command_timestamp_;
            if ((last_send > this->command_throttle_) && !waiting_for_response() && !command_queue_.empty())
            {
                auto &command = command_queue_.front();

                if (command->send_countdown < 1)
                {
                    ESP_LOGD(TAG, "Pump command %02X no response received - removed from send queue", command->function_);
                    command_queue_.pop_front();
                }
                else
                {
                    ESP_LOGV(TAG, "Sending command with function %02X", command->function_);
                    command->send();
                    this->last_command_timestamp_ = millis();
                }
            }
            return true;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        bool CenturyPumpCommand::send()
        {
            std::vector<uint8_t> cmd;
            cmd.push_back(pump_->get_address());
            cmd.push_back(function_);
            cmd.push_back(0x20);
            cmd.insert(cmd.end(), payload_.begin(), payload_.end());
            pump_->send_raw(cmd);
            this->send_countdown--;
            return true;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_status_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump, bool running)> on_status_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x43; // Pump status
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> data)
            {
                ESP_LOGD(TAG, "Got status command reply %02X", data[0]);

                if (data[0] == 0x00)
                    on_status_func(pump, false);
                else if (data[0] == 0x0B)
                    on_status_func(pump, true);
            };
            return cmd;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_read_sensor_command(CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t scale, std::function<void(CenturyVSPump *pump, uint16_t value)> on_value_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x45; // Read sensor
            cmd.payload_.push_back(page);
            cmd.payload_.push_back(address);
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> data)
            {
                // Always going to have at least 1 byte of sensor data
                uint16_t value = (uint16_t)data[2];
                if (data.size() == 4)
                {
                    // But sometimes, we get two bytes
                    value |= (uint16_t)data[3] << 8;
                }
                // Scale the value
                value /= scale;
                ESP_LOGD(TAG, "Read value %d from page %d, addr %d", value, page, address);
                on_value_func(pump, value);
            };
            return cmd;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_run_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump)> on_confirmation_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x41; // Go
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> data)
            {
                ESP_LOGD(TAG, "Confirmed pump running");
                on_confirmation_func(pump);
            };
            return cmd;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_stop_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump)> on_confirmation_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x42; // Stop
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> data)
            {
                ESP_LOGD(TAG, "Confirmed pump stopped");
                on_confirmation_func(pump);
            };
            return cmd;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_set_demand_command(CenturyVSPump *pump, uint16_t demand, std::function<void(CenturyVSPump *pump)> on_confirmation_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x44;      // Set demand
            cmd.payload_.push_back(0); // Mode (0=Speed, 1=Torque, 2=Reserved, 3=Reserved)
            demand *= 4;               // Scaling
            cmd.payload_.push_back(demand & 0xff);
            cmd.payload_.push_back(demand >> 8);
            cmd.on_data_func_ = [=](CenturyVSPump *pump, const std::vector<uint8_t> data)
            {
                ESP_LOGD(TAG, "Set demand comfirmed");
                on_confirmation_func(pump);
            };
            return cmd;
        }
    }
}