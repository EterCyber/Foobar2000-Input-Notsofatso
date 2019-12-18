/*
  Notsofatso, a nintendo sound format player derived from in_notsofatso
  Copyright (C) 2004 Disch
  Copyright (C) 2015 Wilbert Lee

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "CNSFCore.h"

void CFME07Wave::DoTicks(int ticks, BYTE mix)
{
  register int mn;

  if (!bChannelEnabled)  return;
  if (!nFreqTimer.W)  return;

  while (ticks)
  {
    mn = min(nFreqCount, ticks);
    ticks -= mn;

    nFreqCount -= mn;

    if (mix && (nDutyCount < 16))
    {
      nMixL += nOutputTable_L[nVolume] * mn;
      nMixR += nOutputTable_R[nVolume] * (bDoInvert ? -mn : mn);
    }

    if (nFreqCount > 0) continue;
    nFreqCount = nFreqTimer.W;

    if (++nDutyCount >= 32)
    {
      bDoInvert = bInvert && (nFreqTimer.W < nInvertFreqCutoff);
      nDutyCount = 0;
    }
  }
}

void CFME07Wave::Mix_Mono(int& mix, int downsample)
{
  mix += (nMixL / downsample);
  nMixL = 0;
}

void CFME07Wave::Mix_Stereo(int& mixL, int& mixR, int downsample)
{
  mixL += (nMixL / downsample);
  mixR += (nMixR / downsample);

  nMixL = nMixR = 0;
}
