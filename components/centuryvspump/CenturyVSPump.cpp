#include "CenturyVSPump.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <memory>

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
            if (this->request_active_ || this->command_queue_.empty() ||
                millis() - this->last_command_timestamp_ <= this->command_throttle_)
                return;

            const std::vector<uint8_t> pdu = this->command_queue_.front()->build_pdu();
            if (!this->queue_pdu(pdu))
                return;

            this->request_active_ = true;
            ESP_LOGV(TAG, "Queued command with function %02X", this->command_queue_.front()->function_);
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
            this->command_queue_.push_back(std::make_unique<CenturyPumpCommand>(command));
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        /// Every Century function code (0x41-0x45) is in the user-defined range, so all of this pump's
        /// traffic arrives here. response_pdu is the whole PDU: [function][ACK][data...].
        void CenturyVSPump::on_custom_response(std::span<const uint8_t>,
                                               std::span<const uint8_t> response_pdu,
                                               modbus::ResponseStatus status)
        {
            if (!this->has_active_request_())
            {
                ESP_LOGW(TAG, "Response for an untracked request, ignoring");
                return;
            }

            std::unique_ptr<CenturyPumpCommand> command = std::move(this->command_queue_.front());
            this->command_queue_.pop_front();
            this->request_active_ = false;

            // Modbus-level failure: response_pdu is empty and the exception code is in status.
            if (!modbus::succeeded(status))
            {
                ESP_LOGW(TAG, "Function %02X returned modbus exception %02X", command->function_,
                         static_cast<uint8_t>(status.value()));
                return;
            }

            if (response_pdu.size() < 2)
            {
                ESP_LOGW(TAG, "Short response for function %02X, ignoring", command->function_);
                return;
            }

            // Application-level ACK from the pump itself (not a modbus exception).
            if (response_pdu[1] != CENTURY_ACK)
            {
                ESP_LOGW(TAG, "Function %02X NACK with %02X, ignoring", command->function_, response_pdu[1]);
                return;
            }

            // Drop the function code and ACK byte; hand the rest to the command-specific handler.
            std::vector<uint8_t> data(response_pdu.begin() + 2, response_pdu.end());
            command->on_data_func_(this, data);
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        /// No matching reply arrived. Returning true asks the hub to re-queue the same frame; the hub
        /// does not bound retries, so the countdown here is what stops them.
        bool CenturyVSPump::on_no_response(std::span<const uint8_t>)
        {
            if (!this->has_active_request_())
                return false;

            auto &command = this->command_queue_.front();
            if (command->send_countdown > 1)
            {
                command->send_countdown--;
                ESP_LOGD(TAG, "Pump command %02X no response - retrying", command->function_);
                return true;
            }

            ESP_LOGD(TAG, "Pump command %02X no response received - removed from send queue", command->function_);
            this->command_queue_.pop_front();
            this->request_active_ = false;
            return false;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        /// Accepted, then dropped before it ever reached the wire.
        void CenturyVSPump::on_not_sent(std::span<const uint8_t>)
        {
            if (!this->has_active_request_())
                return;

            ESP_LOGD(TAG, "Pump command %02X was never sent - removed from send queue",
                     this->command_queue_.front()->function_);
            this->command_queue_.pop_front();
            this->request_active_ = false;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPump::on_sent(std::span<const uint8_t>)
        {
            if (!this->has_active_request_())
                return;
            this->last_command_timestamp_ = millis();
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        std::vector<uint8_t> CenturyPumpCommand::build_pdu() const
        {
            std::vector<uint8_t> pdu;
            pdu.reserve(2 + this->payload_.size());
            pdu.push_back(this->function_);
            pdu.push_back(this->ack_);
            pdu.insert(pdu.end(), this->payload_.begin(), this->payload_.end());
            return pdu;
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyPumpCommand::create_status_command(CenturyVSPump *pump, std::function<void(CenturyVSPump *pump, bool running)> on_status_func)
        {
            CenturyPumpCommand cmd = {};
            cmd.pump_ = pump;
            cmd.function_ = 0x43; // Pump status
            cmd.on_data_func_ = [on_status_func](CenturyVSPump *pump, const std::vector<uint8_t> data)
            {
                if (data.empty())
                {
                    ESP_LOGW(TAG, "Pump status response contained no data");
                    return;
                }
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
            cmd.on_data_func_ = [page, address, scale, on_value_func](CenturyVSPump *pump, const std::vector<uint8_t> data)
            {
                // Guard against short/truncated responses before indexing.
                if (data.size() < 3)
                {
                    ESP_LOGW(TAG, "Sensor response too short for page %d addr %d: %zu bytes", page, address, data.size());
                    return;
                }

                // Always going to have at least 1 byte of sensor data
                uint16_t value = (uint16_t)data[2];
                if (data.size() >= 4)
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
            cmd.on_data_func_ = [on_confirmation_func](CenturyVSPump *pump, const std::vector<uint8_t> data)
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
            cmd.on_data_func_ = [on_confirmation_func](CenturyVSPump *pump, const std::vector<uint8_t> data)
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
            cmd.on_data_func_ = [on_confirmation_func](CenturyVSPump *pump, const std::vector<uint8_t> data)
            {
                ESP_LOGD(TAG, "Set demand comfirmed");
                on_confirmation_func(pump);
            };
            return cmd;
        }
    }
}
