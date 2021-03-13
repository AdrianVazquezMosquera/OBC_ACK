/*
 * Copyright (C) 2019 Theia Space, Universidad Politécnica de Madrid
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

#include "ESAT_OBC-telemetry/ESAT_OBCACKTelemetry.h"
#include "ESAT_OBC-telecommands/ESAT_OBCDisableTelemetryTelecommand.h"
#include "ESAT_OBC-telecommands/ESAT_OBCDownloadStoredTelemetryTelecommand.h"
#include "ESAT_OBC-telecommands/ESAT_OBCEnableTelemetryTelecommand.h"
#include "ESAT_OBC-telecommands/ESAT_OBCEraseStoredTelemetryTelecommand.h"
#include "ESAT_OBC-telecommands/ESAT_OBCSetTimeTelecommand.h"
#include "ESAT_OBC-telecommands/ESAT_OBCStoreTelemetryTelecommand.h"
#include <RAMStatistics.h>

boolean ESAT_OBCACKTelemetryClass::makeAvailable(bool isAvailable)
{
  // The OBC housekeeping telemetry packet is always available.
  return activated = isAvailable;
}

boolean ESAT_OBCACKTelemetryClass::available()
{
  if (activated)
  {
  return true;
  }
  else
  {
    return false;
  }
}

ESAT_CCSDSSecondaryHeader ESAT_OBCACKTelemetryClass::saveSecondaryHeader(ESAT_CCSDSPacket &packet)
{
  return datum = packet.readSecondaryHeader();
}

boolean ESAT_OBCACKTelemetryClass::handlerIsCompatibleWithPacket(byte packetIdentifier,
                                                                 ESAT_CCSDSSecondaryHeader datum)
{
  if (packetIdentifier == datum.packetIdentifier)
  {
    return true;
  }
  else
  {
    return false;
  }
}

boolean ESAT_OBCACKTelemetryClass::fillUserData(ESAT_CCSDSPacket &packet)
{
  byte handlers[] = {ESAT_OBCDisableTelemetryTelecommand.packetIdentifier(),
                     ESAT_OBCDownloadStoredTelemetryTelecommand.packetIdentifier(),
                     ESAT_OBCEnableTelemetryTelecommand.packetIdentifier(),
                     ESAT_OBCEraseStoredTelemetryTelecommand.packetIdentifier(),
                     ESAT_OBCSetTimeTelecommand.packetIdentifier(),
                     ESAT_OBCStoreTelemetryTelecommand.packetIdentifier()};
  for (int i = 0; i < sizeof(handlers); i++)
  {
    if (handlerIsCompatibleWithPacket(handlers[i], datum))
    {
      packet.writeByte(handlers[i]);
    }
  }
  activated = false;
  return true;
}

ESAT_OBCACKTelemetryClass ESAT_OBCACKTelemetry;
