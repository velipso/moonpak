// SPDX-License-Identifier: 0BSD
#include "SndData.hpp"

namespace SndData {
  extern const uint16_t noteToTimer[108] {
    #include "SndDataNoteToTimer.inc"
  };

  const uint16_t timerToDphase[4097] {
    #include "SndDataTimerToDphase.inc"
  };

  const uint16_t waveTables[27520] = {
    #include "SndDataWaveTables.inc"
  };
}
