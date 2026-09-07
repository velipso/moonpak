// SPDX-License-Identifier: 0BSD
#include "SndData.hpp"

namespace SndData {
  const uint16_t sampleCountPerFrame[32] = {
    #include "SndDataSampleCount.inc"
  };

  const uint16_t frequencyPerPitch[1728] = {
    #include "SndDataFrequency.inc"
  };

  const uint16_t waveTables[27520] = {
    #include "SndDataWaveTables.inc"
  };
}
