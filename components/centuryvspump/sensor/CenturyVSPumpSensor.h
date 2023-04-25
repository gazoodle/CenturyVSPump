#pragma once

#include "esphome/components/centuryvspump/CenturyVSPump.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome
{
    using namespace sensor;

    namespace century_vs_pump
    {
        class CenturyVSPumpSensor : public CenturyPumpItemBase, public Component, public Sensor
        {
        public:
            CenturyVSPumpSensor(uint8_t page, uint8_t address, uint16_t scale) : CenturyPumpItemBase(), page_(page), address_(address), scale_(scale)
            {
            }
            CenturyVSPumpSensor(CenturyVSPump *pump, uint8_t page, uint8_t address, uint16_t scale, const std::string &name, const std::string &unit_of_measurement)
                : CenturyPumpItemBase(pump), page_(page), address_(address), scale_(scale)
            {
                set_name(name.c_str());
                set_unit_of_measurement(unit_of_measurement);
            }

            CenturyPumpCommand create_command() override;

        private:
            uint8_t page_;
            uint8_t address_;
            uint16_t scale_;
        };

    }
}