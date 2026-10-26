#include "CenturyVSPumpSensor.h"

namespace esphome
{
    namespace century_vs_pump
    {
        static const char *const TAG = "century_vs_pump.sensor";

        /////////////////////////////////////////////////////////////////////////////////////////////
        CenturyPumpCommand CenturyVSPumpSensor::create_command()
        {
            return CenturyPumpCommand::create_read_sensor_command(pump_, page_, address_, scale_, [this](CenturyVSPump *pump, uint16_t value)
                                                                  { this->publish_state((float)value); });
        }
    }
}
