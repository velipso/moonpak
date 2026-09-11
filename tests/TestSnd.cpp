// SPDX-License-Identifier: 0BSD
#include "TestSnd.hpp"

#include "gba/Snd.cpp"
#include "gba/Snd.iwram.cpp"
#include "gba/SndData.cpp"
#include "data/songs/outro.cpp" // TODO: remove
#include "data/songs/basic.cpp" // TODO: remove
#include "data/dpcm/DpcmTable.cpp" // TODO: remove

TestSnd::TestSnd() {
  Test::name = "Snd";
}

int TestSnd::run(bool verbose) {
  return Snd::test(verbose);
}
