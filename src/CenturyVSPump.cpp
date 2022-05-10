#include "CenturyVSPump.h"

//////////////////////////////////////////////////////////////////////////////////////////////
//
//  CenturyVSPump implementation
//
/////////////////////////////////////////////////////////////////////////////////////////////

static const char *const TAG = "century_pump";

/////////////////////////////////////////////////////////////////////////////////////////////
void CenturyVSPump::setup()
{
    enabled_switch_ = new CenturyPumpEnabledSwitch();
    enabled_switch_->set_name(name_ + " MODBUS enabled");
    App.register_switch(enabled_switch_);

    items_.push_back(rpm_sensor_ = new CenturyPumpSensor(this, 0, 0, 4, name_ + " RPM Sensor", "RPM"));
    items_.push_back(new CenturyPumpSensor(this, 0, 3, 4, name_ + " RPM Demand", "RPM"));
    items_.push_back(run_switch_ = new CenturyPumpRunSwitch(this, name_ + " Run switch"));
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
    if (enabled_switch_ != nullptr)
        if (enabled_switch_->state)
        {
            ESP_LOGV(TAG, "Updating pump component");
            for (auto item : items_)
                queue_command_(item->create_command());
        }
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
        ESP_LOGD(TAG, "Modbus error, so removing current command (%d) from queue", current_command->function_);
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
    if (enabled_switch_ != nullptr)
        if (enabled_switch_->state)
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
    uint32_t last_send = ::millis() - this->last_command_timestamp_;
    if ((last_send > this->command_throttle_) && !waiting_for_response() && !command_queue_.empty())
    {
        auto &command = command_queue_.front();
        command->send();
        this->last_command_timestamp_ = ::millis();
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
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_status_command(CenturyVSPump *pump)
{
    CenturyPumpCommand cmd = {};
    cmd.pump_ = pump;
    cmd.function_ = 0x43; // Pump status
    cmd.on_data_func_ = [](CenturyVSPump *pump, const std::vector<uint8_t> data)
    {
        ESP_LOGD(TAG, "Got status command reply %02X", data[0]);
        if (data[0] == 0x00)
            pump->run_switch_->publish_state(false);
        else if (data[0] == 0x0B)
            pump->run_switch_->publish_state(true);
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
        uint16_t value = ((uint16_t)data[2] | ((uint16_t)data[3] << 8)) / scale;
        ESP_LOGD(TAG, "Read value %d from page %d, addr %d", value, page, address);
        on_value_func(pump, value);
    };
    return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_run_command(CenturyVSPump *pump)
{
    CenturyPumpCommand cmd = {};
    cmd.pump_ = pump;
    cmd.function_ = 0x41; // Go
    cmd.on_data_func_ = [](CenturyVSPump *pump, const std::vector<uint8_t> data)
    {
        ESP_LOGD(TAG, "Confirmed pump running");
        pump->run_switch_->publish_state(true);
    };
    return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpCommand::create_stop_command(CenturyVSPump *pump)
{
    CenturyPumpCommand cmd = {};
    cmd.pump_ = pump;
    cmd.function_ = 0x42; // Stop
    cmd.on_data_func_ = [](CenturyVSPump *pump, const std::vector<uint8_t> data)
    {
        ESP_LOGD(TAG, "Confirmed pump stopped");
        pump->run_switch_->publish_state(false);
    };
    return cmd;
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpSensor::create_command()
{
    return CenturyPumpCommand::create_read_sensor_command(pump_, page_, address_, scale_, [=](CenturyVSPump *pump, uint16_t value)
                                                          { this->publish_state((float)value); });
}

/////////////////////////////////////////////////////////////////////////////////////////////
CenturyPumpCommand CenturyPumpRunSwitch::create_command()
{
    return CenturyPumpCommand::create_status_command(pump_);
}

/////////////////////////////////////////////////////////////////////////////////////////////
void CenturyPumpRunSwitch::write_state(bool state)
{
    if (state)
    {
        pump_->queue_command_(CenturyPumpCommand::create_run_command(pump_));
    }
    else
    {
        pump_->queue_command_(CenturyPumpCommand::create_stop_command(pump_));
    }

    pump_->queue_command_(pump_->rpm_sensor_->create_command());
}
