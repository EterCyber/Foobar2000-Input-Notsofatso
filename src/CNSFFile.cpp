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

#include <Windows.h>
#include <stdio.h>
#include "NSF.h"

int CNSFFile::LoadFile(service_ptr_t<file> p_filehint, abort_callback &p_abort, BYTE needdata, BYTE ignoreversion)
{
  Destroy();

  if (!p_filehint.is_valid())
    return -1;

  UINT type = 0;
  p_filehint->read(&type, 4, p_abort);
  int ret = -1;

  if (type == HEADERTYPE_NESM)  ret = LoadFile_NESM(p_filehint, p_abort, needdata, ignoreversion);
  if (type == HEADERTYPE_NSFE)  ret = LoadFile_NSFE(p_filehint, p_abort, needdata);

  p_filehint.release();

  // Snake's revenge puts '00' for the initial track, which (after subtracting 1) makes it 256 or -1 (bad!)
  // This prevents that crap
  if (nInitialTrack >= nTrackCount)
    nInitialTrack = 0;
  if (nInitialTrack < 0)
    nInitialTrack = 0;

  // If there's no tracks... this is a crap NSF
  if (nTrackCount < 1)
  {
    Destroy();
    return -1;
  }

  return ret;
}

void  CNSFFile::Destroy()
{
  SAFE_DELETE(pDataBuffer);
  SAFE_DELETE(pPlaylist);
  SAFE_DELETE(pTrackTime);
  SAFE_DELETE(pTrackFade);
  if (szTrackLabels)
  {
    for (int i = 0; i < nTrackCount; i++)
      SAFE_DELETE(szTrackLabels[i]);
    SAFE_DELETE(szTrackLabels);
  }
  SAFE_DELETE(szGameTitle);
  SAFE_DELETE(szArtist);
  SAFE_DELETE(szCopyright);
  SAFE_DELETE(szRipper);

  ZeroMemory(this, sizeof(CNSFFile));
}

int CNSFFile::LoadFile_NESM(service_ptr_t<file> p_filehint, abort_callback &p_abort, BYTE needdata, BYTE ignoreversion)
{
  int len;

  len = p_filehint->get_size(p_abort) - 0x80;

  if (len < 1) return -1;
  p_filehint->seek(0, p_abort);

  // read the info
  NESM_HEADER  hdr;
  p_filehint->read(&hdr, 0x80, p_abort);

  // confirm the header
  if (hdr.nHeader != HEADERTYPE_NESM)  return -1;
  if (hdr.nHeaderExtra != 0x1A)  return -1;
  if ((!ignoreversion) && (hdr.nVersion != 1))return -1; // stupid NSFs claim to be above version 1  >_>

  // NESM is generally easier to work with (but limited!)
  // just move the data over from NESM_HEADER over to our member data

  bIsExtended = 0;
  nIsPal = hdr.nNTSC_PAL & 0x03;
  nPAL_PlaySpeed = hdr.nSpeedPAL; // blarg
  nNTSC_PlaySpeed = hdr.nSpeedNTSC; // blarg
  nLoadAddress = hdr.nLoadAddress;
  nInitAddress = hdr.nInitAddress;
  nPlayAddress = hdr.nPlayAddress;
  nChipExtensions = hdr.nExtraChip;


  nTrackCount = hdr.nTrackCount;
  nInitialTrack = hdr.nInitialTrack - 1; // stupid 1-based number =P

  memcpy(nBankswitch, hdr.nBankSwitch, 8);

  SAFE_NEW(szGameTitle, char, 33, 1);
  SAFE_NEW(szArtist, char, 33, 1);
  SAFE_NEW(szCopyright, char, 33, 1);

  memcpy(szGameTitle, hdr.szGameTitle, 32);
  memcpy(szArtist, hdr.szArtist, 32);
  memcpy(szCopyright, hdr.szCopyright, 32);

  // read the NSF data
  if (needdata)
  {
    SAFE_NEW(pDataBuffer, BYTE, len, 1);
    p_filehint->read(pDataBuffer, len, p_abort);
    // fread(pDataBuffer, len, 1, file);
    nDataBufferSize = len;
  }

  // if we got this far... it was a successful read
  return 0;
}

int CNSFFile::LoadFile_NSFE(service_ptr_t<file> p_filehint, abort_callback &p_abort, BYTE needdata)
{
  // restart the file
  p_filehint->seek(0, p_abort);

  // the vars we'll be using
  UINT nChunkType;
  int nChunkSize;
  int nChunkUsed;
  int nDataPos = 0;
  BYTE bInfoFound = 0;
  BYTE bEndFound = 0;
  BYTE bBankFound = 0;

  NSFE_INFOCHUNK  info;
  ZeroMemory(&info, sizeof(NSFE_INFOCHUNK));
  info.nTrackCount = 1; // default values

  // confirm the header!
  p_filehint->read(&nChunkType, 4, p_abort);
  if (nChunkType != HEADERTYPE_NSFE)  return -1;

  // begin reading chunks
  while (!bEndFound)
  {
    if (p_filehint->is_eof(p_abort))  return -1;
    p_filehint->read(&nChunkSize, 4, p_abort);
    p_filehint->read(&nChunkType, 4, p_abort);

    switch (nChunkType)
    {
    case CHUNKTYPE_INFO:
      if (bInfoFound)  return -1; // only one info chunk permitted
      if (nChunkSize < 8)  return -1; // minimum size

      bInfoFound = 1;
      nChunkUsed = min((int)sizeof(NSFE_INFOCHUNK), nChunkSize);

      p_filehint->read(&info, nChunkUsed, p_abort);
      p_filehint->seek(nChunkSize - nChunkUsed, p_abort);

      bIsExtended = 1;
      nIsPal = info.nIsPal & 3;
      nLoadAddress = info.nLoadAddress;
      nInitAddress = info.nInitAddress;
      nPlayAddress = info.nPlayAddress;
      nChipExtensions = info.nExt;
      nTrackCount = info.nTrackCount;
      nInitialTrack = info.nStartingTrack;

      nPAL_PlaySpeed = (WORD)(1000000 / PAL_NMIRATE); // blarg
      nNTSC_PlaySpeed = (WORD)(1000000 / NTSC_NMIRATE); // blarg
      break;

    case CHUNKTYPE_DATA:
      if (!bInfoFound)  return -1;
      if (nDataPos)  return -1;
      if (nChunkSize < 1)  return -1;

      nDataBufferSize = nChunkSize;
      nDataPos = p_filehint->get_position(p_abort);// ftell(file);

      p_filehint->seek(nChunkSize, p_abort);
      break;

    case CHUNKTYPE_NEND:
      bEndFound = 1;
      break;

    case CHUNKTYPE_TIME:
      if (!bInfoFound)  return -1;
      if (pTrackTime)  return -1;

      SAFE_NEW(pTrackTime, int, nTrackCount, 1);
      nChunkUsed = min(nChunkSize / 4, nTrackCount);

      p_filehint->read(pTrackTime, nChunkUsed * 4, p_abort);
      p_filehint->seek(nChunkSize - (nChunkUsed * 4), p_abort);

      for (; nChunkUsed < nTrackCount; nChunkUsed++)
        pTrackTime[nChunkUsed] = -1; // negative signals to use default time

      break;

    case CHUNKTYPE_FADE:
      if (!bInfoFound)  return -1;
      if (pTrackFade)  return -1;

      SAFE_NEW(pTrackFade, int, nTrackCount, 1);
      nChunkUsed = min(nChunkSize / 4, nTrackCount);

      p_filehint->read(pTrackFade, nChunkUsed * 4, p_abort);
      p_filehint->seek(nChunkSize - (nChunkUsed * 4), p_abort);

      for (; nChunkUsed < nTrackCount; nChunkUsed++)
        pTrackFade[nChunkUsed] = -1; // negative signals to use default time

      break;

    case CHUNKTYPE_BANK:
      if (bBankFound)  return -1;

      bBankFound = 1;
      nChunkUsed = min(8, nChunkSize);

      p_filehint->read(nBankswitch, nChunkUsed, p_abort);
      p_filehint->seek(nChunkSize - nChunkUsed, p_abort);
      break;

    case CHUNKTYPE_PLST:
      if (pPlaylist)  return -1;

      nPlaylistSize = nChunkSize;
      if (nPlaylistSize < 1)  break; // no playlist?

      SAFE_NEW(pPlaylist, BYTE, nPlaylistSize, 1);
      p_filehint->read(pPlaylist, nChunkSize, p_abort);
      break;

    case CHUNKTYPE_AUTH: {
      if (szGameTitle)  return -1;

      char* buffer;
      char* ptr;
      SAFE_NEW(buffer, char, nChunkSize + 4, 1);

      p_filehint->read(buffer, nChunkSize, p_abort);
      ptr = buffer;

      char** ar[4] = { &szGameTitle, &szArtist, &szCopyright, &szRipper };
      int i;
      for (i = 0; i < 4; i++)
      {
        nChunkUsed = strlen(ptr) + 1;
        *ar[i] = new char[nChunkUsed];
        if (!*ar[i]) { SAFE_DELETE(buffer); return 0; }
        memcpy(*ar[i], ptr, nChunkUsed);
        ptr += nChunkUsed;
      }
      SAFE_DELETE(buffer);
    }break;

    case CHUNKTYPE_TLBL: {
      if (!bInfoFound)  return -1;
      if (szTrackLabels)  return -1;

      SAFE_NEW(szTrackLabels, char*, nTrackCount, 1);

      char* buffer;
      char* ptr;
      SAFE_NEW(buffer, char, nChunkSize + nTrackCount, 1);

      p_filehint->read(buffer, nChunkSize, p_abort);
      ptr = buffer;

      int i;
      for (i = 0; i < nTrackCount; i++)
      {
        nChunkUsed = strlen(ptr) + 1;
        szTrackLabels[i] = new char[nChunkUsed];
        if (!szTrackLabels[i]) { SAFE_DELETE(buffer); return 0; }
        memcpy(szTrackLabels[i], ptr, nChunkUsed);
        ptr += nChunkUsed;
      }
      SAFE_DELETE(buffer);
    }break;

    default:  // unknown chunk
      nChunkType &= 0x000000FF; // check the first byte
      if ((nChunkType >= 'A') && (nChunkType <= 'Z'))  // chunk is vital... don't continue
        return -1;
      // otherwise, just skip it
      p_filehint->seek(nChunkSize, p_abort);

      break;
    }  // end switch
  }  // end while

  // if we exited the while loop without a 'return', we must have hit an NEND chunk
  // if this is the case, the file was layed out as it was expected.
  // now.. make sure we found both an info chunk, AND a data chunk... since these are
  // minimum requirements for a valid NSFE file

  if (!bInfoFound)  return -1;
  if (!nDataPos)  return -1;

  // if both those chunks existed, this file is valid.  Load the data if it's needed

  if (needdata)
  {
    p_filehint->seek(nDataPos, p_abort);
    SAFE_NEW(pDataBuffer, BYTE, nDataBufferSize, 1);
    p_filehint->read(pDataBuffer, nDataBufferSize, p_abort);
  }
  else
    nDataBufferSize = 0;

  // return success!
  return 0;
}


// // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // 
// File saving

int CNSFFile::SaveFile(service_ptr_t<file> p_filehint, abort_callback &p_abort)
{
  int ret;
  if (!pDataBuffer)  // if we didn't grab the data, we can't save it
    return 1;
  if (!p_filehint.is_valid())
    return 1;
  if (bIsExtended)  ret = SaveFile_NSFE(p_filehint, p_abort);
  else  ret = SaveFile_NESM(p_filehint, p_abort);
  p_filehint.release();
  return ret;
}

int CNSFFile::SaveFile_NESM(service_ptr_t<file> p_filehint, abort_callback &p_abort)
{
  NESM_HEADER  hdr;
  ZeroMemory(&hdr, 0x80);

  hdr.nHeader = HEADERTYPE_NESM;
  hdr.nHeaderExtra = 0x1A;
  hdr.nVersion = 1;
  hdr.nTrackCount = nTrackCount;
  hdr.nInitialTrack = nInitialTrack + 1;
  hdr.nLoadAddress = nLoadAddress;
  hdr.nInitAddress = nInitAddress;
  hdr.nPlayAddress = nPlayAddress;

  if (szGameTitle)  memcpy(hdr.szGameTitle, szGameTitle, min(strlen(szGameTitle), 31));
  if (szArtist)  memcpy(hdr.szArtist, szArtist, min(strlen(szArtist), 31));
  if (szCopyright)  memcpy(hdr.szCopyright, szCopyright, min(strlen(szCopyright), 31));

  hdr.nSpeedNTSC = nNTSC_PlaySpeed;
  memcpy(hdr.nBankSwitch, nBankswitch, 8);
  hdr.nSpeedPAL = nPAL_PlaySpeed;
  hdr.nNTSC_PAL = nIsPal;
  hdr.nExtraChip = nChipExtensions;

  // the header is all set... slap it in
  p_filehint->write(&hdr, 0x80, p_abort);

  // slap in the NSF info
  p_filehint->write(pDataBuffer, nDataBufferSize, p_abort);

  // we're done.. all the other info that isn't recorded is dropped for regular NSFs
  return 0;
}

int CNSFFile::SaveFile_NSFE(service_ptr_t<file> p_filehint, abort_callback &p_abort)
{
  // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // 
  // I must admit... NESM files are a bit easier to work with than NSFEs =P
  int i;
  UINT nChunkType;
  int nChunkSize;
  NSFE_INFOCHUNK  info;

  // write the header
  nChunkType = HEADERTYPE_NSFE;
  p_filehint->write(&nChunkType, 4, p_abort);

  // write the info chunk
  nChunkType = CHUNKTYPE_INFO;
  nChunkSize = sizeof(NSFE_INFOCHUNK);
  info.nExt = nChipExtensions;
  info.nInitAddress = nInitAddress;
  info.nIsPal = nIsPal;
  info.nLoadAddress = nLoadAddress;
  info.nPlayAddress = nPlayAddress;
  info.nStartingTrack = nInitialTrack;
  info.nTrackCount = nTrackCount;

  p_filehint->write(&nChunkSize, 4, p_abort);
  p_filehint->write(&nChunkType, 4, p_abort);
  p_filehint->write(&info, nChunkSize, p_abort);

  // If we need bankswitching... throw it in
  for (nChunkSize = 0; nChunkSize < 8; nChunkSize++)
  {
    if (nBankswitch[nChunkSize])
    {
      nChunkType = CHUNKTYPE_BANK;
      nChunkSize = 8;
      p_filehint->write(&nChunkSize, 4, p_abort);
      p_filehint->write(&nChunkType, 4, p_abort);
      p_filehint->write(nBankswitch, nChunkSize, p_abort);
      break;
    }
  }

  // if there's a time chunk, slap it in
  if (pTrackTime)
  {
    nChunkType = CHUNKTYPE_TIME;
    nChunkSize = 4 * nTrackCount;
    p_filehint->write(&nChunkSize, 4, p_abort);
    p_filehint->write(&nChunkType, 4, p_abort);
    p_filehint->write(pTrackTime, nChunkSize, p_abort);
  }

  // slap in a fade chunk if needed
  if (pTrackFade)
  {
    nChunkType = CHUNKTYPE_FADE;
    nChunkSize = 4 * nTrackCount;
    p_filehint->write(&nChunkSize, 4, p_abort);
    p_filehint->write(&nChunkType, 4, p_abort);
    p_filehint->write(pTrackFade, nChunkSize, p_abort);
  }

  // auth!
  if (szGameTitle || szCopyright || szArtist || szRipper)
  {
    nChunkType = CHUNKTYPE_AUTH;
    nChunkSize = 4;
    if (szGameTitle)  nChunkSize += strlen(szGameTitle);
    if (szArtist)  nChunkSize += strlen(szArtist);
    if (szCopyright)  nChunkSize += strlen(szCopyright);
    if (szRipper)  nChunkSize += strlen(szRipper);
    p_filehint->write(&nChunkSize, 4, p_abort);
    p_filehint->write(&nChunkType, 4, p_abort);

    if (szGameTitle)
      p_filehint->write(szGameTitle, strlen(szGameTitle) + 1, p_abort);
    else
      p_filehint->write("", 1, p_abort);
    if (szArtist)
      p_filehint->write(szArtist, strlen(szArtist) + 1, p_abort);
    else
      p_filehint->write("", 1, p_abort);
    if (szCopyright)
      p_filehint->write(szCopyright, strlen(szCopyright) + 1, p_abort);
    else
      p_filehint->write("", 1, p_abort);
    if (szRipper)
      p_filehint->write(szRipper, strlen(szRipper) + 1, p_abort);
    else
      p_filehint->write("", 1, p_abort);
  }

  // plst
  if (pPlaylist)
  {
    nChunkType = CHUNKTYPE_PLST;
    nChunkSize = nPlaylistSize;
    p_filehint->write(&nChunkSize, 4, p_abort);
    p_filehint->write(&nChunkType, 4, p_abort);
    p_filehint->write(pPlaylist, nChunkSize, p_abort);
  }

  // tlbl
  if (szTrackLabels)
  {
    nChunkType = CHUNKTYPE_TLBL;
    nChunkSize = nTrackCount;

    for (i = 0; i < nTrackCount; i++)
      nChunkSize += strlen(szTrackLabels[i]);

    p_filehint->write(&nChunkSize, 4, p_abort);
    p_filehint->write(&nChunkType, 4, p_abort);

    // changed from i to int i
    for (i = 0; i < nTrackCount; i++)
    {
      if (szTrackLabels[i])
        p_filehint->write(szTrackLabels[i], strlen(szTrackLabels[i]) + 1, p_abort);
      else
        p_filehint->write("", 1, p_abort);
    }
  }

  // data
  nChunkType = CHUNKTYPE_DATA;
  nChunkSize = nDataBufferSize;
  p_filehint->write(&nChunkSize, 4, p_abort);
  p_filehint->write(&nChunkType, 4, p_abort);
  p_filehint->write(pDataBuffer, nChunkSize, p_abort);

  // END
  nChunkType = CHUNKTYPE_NEND;
  nChunkSize = 0;
  p_filehint->write(&nChunkSize, 4, p_abort);
  p_filehint->write(&nChunkType, 4, p_abort);

  // w00t
  return 0;
}
