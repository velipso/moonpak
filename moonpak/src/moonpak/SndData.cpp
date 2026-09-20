// SPDX-License-Identifier: 0BSD
#include "moonpak/SndData.hpp"

namespace moonpak {

namespace SndData {
  extern const uint16_t noteToTimer[108] {
    #include "moonpak/SndDataNoteToTimer.inc"
  };

  const uint16_t timerToDphase[4097] {
    #include "moonpak/SndDataTimerToDphase.inc"
  };

  const uint16_t waveTables[27520] = {
    #include "moonpak/SndDataWaveTables.inc"
  };
}

} // moonpak
