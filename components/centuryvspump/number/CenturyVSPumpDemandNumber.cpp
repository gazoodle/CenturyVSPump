#include "CenturyVSPumpDemandNumber.h"

namespace esphome
{
    namespace century_vs_pump
    {
        static const char *const TAG = "century_vs_pump.number";

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyVSPumpDemandNumber::create_command()
        {
            return CenturyPumpCommand::create_read_sensor_command(pump_, 0, 3, 4, [this](CenturyVSPump *pump, uint16_t value)
                                                                  { this->publish_state((float)value); });
        }

        /////////////////////////////////////////////////////////////////////////////////////////////
        void CenturyVSPumpDemandNumber::control(float value)
        {
            ESP_LOGD(TAG, "Set demand to %f", value);
            pump_->queue_command_(CenturyPumpCommand::create_set_demand_command(pump_, (uint16_t)value, [this, value](CenturyVSPump *pump)
                                                                                { this->publish_state(value); }));
            this->publish_state(value);
            pump_->update();
        }
    }
}
