/*
 * Copyright (C) 2017, 2018, 2019 Theia Space, Universidad Politécnica de Madrid
 *
 * This file is part of Theia Space's ESAT OBC library.
 *
 * Theia Space's ESAT OBC library is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * Theia Space's ESAT OBC library is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Theia Space's ESAT OBC library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "ESAT_OBC-hardware/ESAT_TelecommandStorage.h"
#include "ESAT_OBC-hardware/ESAT_OBCClock.h"
#include <ESAT_CCSDSPacketFromKISSFrameReader.h>
#include <ESAT_CCSDSPacketToKISSFrameWriter.h>

// The filename of the telecommand archive cannot exceed 8 characters
// due to filesystem limitations.
const char ESAT_TelecommandStorageClass::TELECOMMAND_FILE[] = "tlcmd_db";
const char ESAT_TelecommandStorageClass::UPDATED_FILE[] = "updt_db";

void ESAT_TelecommandStorageClass::beginReading()
{
  // We must open the telecommand archive in read mode.
  // Close it first if it is already open.
  if (file)
  {
    file.close();
  }

  file = SD.open(TELECOMMAND_FILE, FILE_READ);

  readingInProgress = true;
}

void ESAT_TelecommandStorageClass::endReading()
{
  // We must close the telecommand archive when we are finished with
  // reading it so that it can be used for a new reading session
  // or for writing new packets.
  file.close();
  readingInProgress = false;
}

void ESAT_TelecommandStorageClass::erase()
{
  // Erasing the stored telecommand is as simple as removing the
  // telecommand archive.
  // A failure to remove the telecommand archive is a hardware error.
  const boolean correctRemoval = SD.remove((char *)TELECOMMAND_FILE);
  if (!correctRemoval)
  {
    error = true;
  }
}

boolean ESAT_TelecommandStorageClass::read(ESAT_CCSDSPacket &packet)
{
  // If we didn't call beginReading(), we aren't ready to read
  // telecommand, but this in itself isn't a hardware error, so we don't
  // set the error flag.
  if (!readingInProgress)
  {
    return false;
  }
  // It is a hardware error if we couldn't open the telecommand archive.
  if (!file)
  {
    error = true;
    return false;
  }
  // If everything went well, we can try to read the next packet.
  // Instead of naked packets, we store them in KISS frames, so
  // we must extract packets from frames.
  const unsigned long bufferLength =
      ESAT_CCSDSPrimaryHeader::LENGTH + packet.capacity();
  byte buffer[bufferLength];

  ESAT_CCSDSPacketFromKISSFrameReader reader(file, buffer, bufferLength);

  while (file.available() > 0)
  {
    const boolean correctPacket = reader.read(packet);
    if (!correctPacket)
    {
      error = true;
      return false;
    }
    packet.rewind();

    const ESAT_CCSDSSecondaryHeader secondaryHeader =
        packet.readSecondaryHeader();
    const bool isProgrammed = packet.readBoolean();
    const ESAT_Timestamp timestamp = packet.readTimestamp();
    const ESAT_Timestamp currentTime = ESAT_OBCClock.read();

    packet.rewind();
    if (isProgrammed && (timestamp < currentTime))
    {
      //updateBuffer(secondaryHeader);
      return true;
    }
  }
  return false;
}




void ESAT_TelecommandStorageClass::updateBuffer(ESAT_CCSDSPacket packet)
{
  packet.rewind();
  const ESAT_CCSDSPrimaryHeader primaryHeader = packet.readPrimaryHeader();
  const ESAT_CCSDSSecondaryHeader secondaryHeader =
        packet.readSecondaryHeader();
  const bool isProgrammed = packet.readBoolean();
  const ESAT_Timestamp timestamp = packet.readTimestamp();
  packet.rewind();
  updatedFile = SD.open(UPDATED_FILE, FILE_APPEND);
  const word bufferLength = 256;
  byte buffer[bufferLength];
  ESAT_CCSDSPacket datum = packet;
  endReading();
  beginReading();
  ESAT_CCSDSPacketFromKISSFrameReader reader(file, buffer, bufferLength);
  while (file.available() > 0)
  {
    const boolean correctPacket = reader.read(datum);
    datum.rewind();
    const ESAT_CCSDSPrimaryHeader updatedPrimaryHeader = datum.readPrimaryHeader();
    const ESAT_CCSDSSecondaryHeader updatedSecondaryHeader = datum.readSecondaryHeader();
    const bool updatedIsProgrammed = datum.readBoolean();
    const ESAT_Timestamp updatedTimestamp = datum.readTimestamp();
    datum.rewind();
    if (timestamp != updatedTimestamp)
    {
      ESAT_CCSDSPacketToKISSFrameWriter writer(updatedFile);
      const boolean correctWrite = writer.unbufferedWrite(datum);
    }
  }
  endReading();
  erase();
  updatedFile.close();

  updatedFile = SD.open(UPDATED_FILE, FILE_READ);
  ESAT_CCSDSPacketFromKISSFrameReader updatedReader(updatedFile, buffer, bufferLength);

  while (updatedFile.available() > 0)
  {
    const boolean correctPacket = updatedReader.read(datum);
    datum.rewind();
    write(datum);
  }
  updatedFile.close();
  const boolean correctRemoval = SD.remove((char *)UPDATED_FILE);
}





boolean ESAT_TelecommandStorageClass::reading() const
{
  return readingInProgress;
}

unsigned long ESAT_TelecommandStorageClass::size()
{
  if (file)
  {
    return file.size();
  }
  else
  {
    // If the telecommand store file isn't already open, just open it to
    // get its size and close it back to leave it as we found it.
    file = SD.open(TELECOMMAND_FILE, FILE_READ);
    if (!file)
    {
      error = true;
      return 0;
    }
    else
    {
      const unsigned long fileSize = file.size();
      file.close();
      return fileSize;
    }
  }
}

void ESAT_TelecommandStorageClass::write(ESAT_CCSDSPacket &packet)
{
  // We cannot write telecommand packets to the telecommand archive while
  // we are reading it.
  if (readingInProgress)
  {
    return;
  }
  // The file shouldn't be open.
  if (file)
  {
    return;
  }
  // We open and close the telecommand archive every time we write a new
  // packet.  This is simpler than tracking the state of the file from
  // call to call.
  // A failure to open the file is a hardware error.
  file = SD.open(TELECOMMAND_FILE, FILE_APPEND);
  if (!file)
  {
    error = true;
    return;
  }
  // We don't store naked packets, but KISS frames containing the
  // packets.  This helps when there is a small data corruption: if we
  // stored naked packets and the packet data length field of one
  // packet didn't match the actually stored packet data length, all
  // subsequent packets would be affected and couldn't be read, so
  // they would be as good as lost; with KISS frames, we limit the
  // data loss to the affected packet.
  ESAT_CCSDSPacketToKISSFrameWriter writer(file);
  const boolean correctWrite = writer.unbufferedWrite(packet);
  if (!correctWrite)
  {
    error = true;
  }
  file.close();
}

ESAT_TelecommandStorageClass ESAT_TelecommandStorage;
