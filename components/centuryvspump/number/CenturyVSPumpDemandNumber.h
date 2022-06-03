#pragma once

#include "esphome/components/centuryvspump/CenturyVSPump.h"
#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

namespace esphome
{
    using namespace number;

    namespace century_vs_pump
    {
        class CenturyVSPumpDemandNumber : public CenturyPumpItemBase, public Component, public Number
        {
        public:
            CenturyVSPumpDemandNumber() : CenturyPumpItemBase() {}

            // void write_state(bool state) override;
            CenturyPumpCommand create_command() override;
            void control(float value) override;

        private:
        };

    }
}