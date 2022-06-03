#pragma once

#include "esphome/components/centuryvspump/CenturyVSPump.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome
{
    using namespace switch_;

    namespace century_vs_pump
    {
        class CenturyVSPumpRunSwitch : public CenturyPumpItemBase, public Component, public Switch
        {
        public:
            CenturyVSPumpRunSwitch() : CenturyPumpItemBase() {}

            void write_state(bool state) override;
            CenturyPumpCommand create_command() override;

        private:
        };

    }
}