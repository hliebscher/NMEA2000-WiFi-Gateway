/* 
N183ToN2k.cpp

2015 Copyright (c) Kave Oy, www.kave.fi  All right reserved.

Author: Timo Lappalainen, Ton Swieb

  This library is free software; you can redistribute it and/or
  modify it as you like.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/
 
#include "N183ToN2k.h"

#define PI_2 6.283185307179586476925286766559

tN2kGNSSmethod GNSMethofNMEA0183ToN2k(int Method) {
  switch (Method) {
    case 0: return N2kGNSSm_noGNSS;
    case 1: return N2kGNSSm_GNSSfix;
    case 2: return N2kGNSSm_DGNSS;
    default: return N2kGNSSm_noGNSS;  
  }
}

double toMagnetic(double True, double Variation) {

  double magnetic=True-Variation;
  while (magnetic<0) magnetic+=PI_2;
  while (magnetic>=PI_2) magnetic-=PI_2;
  return magnetic;    
}

N183ToN2k::N183ToN2k(tNMEA2000* pNMEA2000, Stream* nmea0183, Logger* logger, byte maxWpPerRoute, byte maxWpNameLength) {

  this->logger = logger;
  this->pNMEA2000 = pNMEA2000;
  route = new Route(maxWpPerRoute, maxWpNameLength, logger);
  info("Initializing NMEA0183 communication. Make sure the NMEA device uses the same baudrate.");
  NMEA0183.SetMessageStream(nmea0183);
  NMEA0183.Open();
}

/**
 * Resend PGN129285 at least every 5 seconds.
 * B&G Triton requires a PGN129285 message around every 10 seconds otherwise display of the destinationID is cleared.
 * Depending on how fast an updated route is constructed we are sending the same or updated route.
 */
void N183ToN2k::handleLoop() {

  tNMEA0183Msg NMEA0183Msg;  
  while (NMEA0183.GetMessage(NMEA0183Msg)) {
    HandleNMEA0183Msg(NMEA0183Msg);
  }
  
  static unsigned long timeUpdated=millis();
  if (timeUpdated+5000 < millis()) {
    timeUpdated=millis();
    sendPGN129285();
  }
}

/**
 * 129283 - Cross Track Error
 * Category: Navigation
 * This PGN provides the magnitude of position error perpendicular to the desired course.
 */
void N183ToN2k::sendPGN129283(const tRMB &rmb) {

  tN2kMsg N2kMsg;
  SetN2kXTE(N2kMsg,1,N2kxtem_Autonomous, false, rmb.xte);
  pNMEA2000->SendMsg(N2kMsg);
}

/*
 * 129284 - Navigation Data
 * Category: Navigation
 * This PGN provides essential navigation data for following a route.Transmissions will originate from products that can create and manage routes using waypoints. This information is intended for navigational repeaters. 
 * 
 * Send the navigation information of the current leg of the route.
 * With long routes from NMEA0183 GPS systems it can take a while before a PGN129285 can be sent. So the display value of originID or destinationID might not always be available.
 * B&G Trition ignores OriginWaypointNumber and DestinationWaypointNumber values in this message.
 * It always takes the 2nd waypoint from PGN129285 as DestinationWaypoint.
 * Not sure if that is compliant with NMEA2000 or B&G Trition specific.
 */
void N183ToN2k::sendPGN129284(const tRMB &rmb) {
  
      tN2kMsg N2kMsg;

      /*
       * PGN129285 only gives route/wp data ahead in the Active Route. So originID will always be 0 and destinationID will always be 1.
       * Unclear why these ID's need to be set in PGN129284. On B&G Triton displays other values are ignored anyway.
       */
      int originID=0;
      int destinationID=originID+1;
      
      //B&G Triton ignores the etaTime and etaDays values and does the calculation by itself. So let's leave them at 0 for now.
      double etaTime, etaDays = 0.0;
      double Mbtw = toMagnetic(rmb.btw,Variation);
      bool ArrivalCircleEntered = rmb.arrivalAlarm == 'A';
      //PerpendicularCrossed not calculated yet.
      //Need to calculate it based on current lat/long, pND->bod.magBearing and pND->rmb.lat/long
      bool PerpendicularCrossed = false;
      SetN2kNavigationInfo(N2kMsg,1,rmb.dtw,N2khr_magnetic,PerpendicularCrossed,ArrivalCircleEntered,N2kdct_RhumbLine,etaTime,etaDays,
                          bod.magBearing,Mbtw,originID,destinationID,rmb.latitude,rmb.longitude,rmb.vmg);
      pNMEA2000->SendMsg(N2kMsg);
      trace("129284: originID=%s,%u, destinationID=%s,%u, latitude=%s, longitude=%s, ArrivalCircleEntered=%u, VMG=%u, DTW=%u, BTW (Current to Destination)=%u, BTW (Orign to Desitination)=%u",
      bod.originID,originID,bod.destID,destinationID,toLogString(rmb.latitude,6,2),toLogString(rmb.longitude,6,2),ArrivalCircleEntered,rmb.vmg,rmb.dtw,Mbtw,bod.magBearing);
}

/**
 * 129285 - Navigation - Route/WP information
 * Category: Navigation
 * This PGN shall return Route and WP data ahead in the Active Route. It can be requested or may be transmitted without a request, typically at each Waypoint advance. 
 * 
 * Send the active route with all waypoints from the origin of the current leg and onwards.
 * So the waypoint that corresponds with the originID from the BOD message should be the 1st. The destinationID from the BOD message should be the 2nd. Etc.
 */
void N183ToN2k::sendPGN129285() {

  if (!route->isValid()) {
    info("Skip sending PGN129285, route is not complete yet.");
    return;
  }

  tN2kMsg N2kMsg;
  SetN2kPGN129285(N2kMsg,0, 1, route->getRouteId(), false, false, "Unknown");

  /*
   * With Garmin GPS 120 GOTO route we only get 1 waypoint. The destination. Perhaps this is the default for all NMEA0183 devices.
   * For NMEA2000 the destination needs to be 2nd waypoint in the route. So lets at the current location as the 1st waypoint in the route.
   */   
  if (route->getSize() == 1) {
    AppendN2kPGN129285(N2kMsg, 0, "CURRENT", Latitude, Longitude);
    tRouteWaypoint e = route->getWaypoint(0);
    AppendN2kPGN129285(N2kMsg, 1, e.name, e.latitude, e.longitude);
    trace("129285: CURRENT,%s,%s",toLogString(Latitude,6,2),toLogString(Longitude,6,2));
    trace("129285: %s,%s,%s",e.name,toLogString(e.latitude,6,2),toLogString(e.longitude,6,2));
  } else {
    for (byte i=route->getIndexOriginCurrentLeg(); i < route->getSize(); i++) {
      byte j = i - route->getIndexOriginCurrentLeg();
      tRouteWaypoint e = route->getWaypoint(i);
      //Continue adding waypoints as long as they fit within a single message
      trace("129285: %s,%s,%s",e.name,toLogString(e.latitude,6,2),toLogString(e.longitude,6,2));
      if (!AppendN2kPGN129285(N2kMsg, j, e.name, e.latitude, e.longitude)) {
        //Max. nr. of waypoints per message is reached.Send a message with all waypoints upto this one and start constructing a new message.
        pNMEA2000->SendMsg(N2kMsg); 
        N2kMsg.Clear();
        SetN2kPGN129285(N2kMsg,j, 1, route->getRouteId(), false, false, "Unknown");
        //TODO: Check for the result of the Append, should not fail due to message size. Perhaps some other reason?
        AppendN2kPGN129285(N2kMsg, j, e.name, e.latitude, e.longitude);
      }
    }
  }
  pNMEA2000->SendMsg(N2kMsg);       
}

void N183ToN2k::sendPGN129029(const tGGA &gga) {
  
    tN2kMsg N2kMsg;
    SetN2kGNSS(N2kMsg,1,DaysSince1970,gga.GPSTime,gga.latitude,gga.longitude,gga.altitude,
              N2kGNSSt_GPS,GNSMethofNMEA0183ToN2k(gga.GPSQualityIndicator),gga.satelliteCount,gga.HDOP,0,
              gga.geoidalSeparation,1,N2kGNSSt_GPS,gga.DGPSReferenceStationID,gga.DGPSAge
              );
    pNMEA2000->SendMsg(N2kMsg); 
}

void N183ToN2k::sendPGN129025(const double &latitude, const double &longitude) {

    tN2kMsg N2kMsg;
    SetN2kLatLonRapid(N2kMsg, latitude, longitude);
    pNMEA2000->SendMsg(N2kMsg);
}

void N183ToN2k::sendPGN129026(const tN2kHeadingReference ref, const double &COG, const double &SOG) {

    tN2kMsg N2kMsg;
    SetN2kCOGSOGRapid(N2kMsg,1,ref,COG,SOG);
    pNMEA2000->SendMsg(N2kMsg);
}

void N183ToN2k::sendPGN127250(const double &trueHeading) {

    tN2kMsg N2kMsg;
    double MHeading = toMagnetic(trueHeading,Variation);
    SetN2kMagneticHeading(N2kMsg,1,MHeading,0,Variation);
    pNMEA2000->SendMsg(N2kMsg);
}

// NMEA0183 message Handler functions
void N183ToN2k::HandleRMC(const tNMEA0183Msg &NMEA0183Msg) {

  tRMC rmc;
  if (NMEA0183ParseRMC(NMEA0183Msg,rmc) && rmc.status == 'A') {
    tN2kMsg N2kMsg;
    double MCOG = toMagnetic(rmc.trueCOG,rmc.variation);
    sendPGN129026(N2khr_magnetic,MCOG,rmc.SOG);
    sendPGN129025(rmc.latitude,rmc.longitude);
    Latitude = rmc.latitude;
    Longitude = rmc.longitude;
    DaysSince1970 = rmc.daysSince1970;
    Variation = rmc.variation;
    trace("RMC: GPSTime=%s, Latitude=%s, Longitude=%s, COG=%s, SOG=%s, DaysSince1970=%u, Variation=%s",
    toLogString(rmc.GPSTime,6,2),toLogString(rmc.latitude,6,2),toLogString(rmc.longitude,6,2),toLogString(rmc.trueCOG,6,2),toLogString(rmc.SOG,6,2),rmc.daysSince1970,toLogString(rmc.variation,6,2));
  } else if (rmc.status == 'V') { warn("RMC is Void");
  } else { error("RMC: Failed to parse message"); }
}

/**
 * Receive NMEA0183 RMB message (Recommended Navigation Data for GPS).
 * Contains enough information to send a NMEA2000 PGN129283 (XTE) message and NMEA2000 PGN129284 message.
 */
void N183ToN2k::HandleRMB(const tNMEA0183Msg &NMEA0183Msg) {

  tRMB rmb;
  if (NMEA0183ParseRMB(NMEA0183Msg, rmb)  && rmb.status == 'A') {
    sendPGN129283(rmb);
    sendPGN129284(rmb);
    trace("RMB: XTE=%s, DTW=%s, BTW=%s, VMG=%s, OriginID=%s, DestinationID=%s, Latittude=%s, Longitude=%s",
    toLogString(rmb.xte,6,2),toLogString(rmb.dtw,6,2),toLogString(rmb.btw,6,2),toLogString(rmb.vmg,6,2),rmb.originID,rmb.destID,toLogString(rmb.latitude,6,2),toLogString(rmb.longitude,6,2));
  } else if (rmb.status == 'V') { warn("RMB: Is Void");
  } else { error("RMB: Failed to parse message"); }
}

void N183ToN2k::HandleGGA(const tNMEA0183Msg &NMEA0183Msg) {

  tGGA gga;
  if (NMEA0183ParseGGA(NMEA0183Msg,gga) && gga.GPSQualityIndicator > 0) {
    sendPGN129029(gga);
    Latitude = gga.latitude;
    Longitude = gga.longitude;
    trace("GGA: Time=%s, Latitude=%s, Longitude=%s, Altitude=%s, GPSQualityIndicator=%u, SatelliteCount=%u, HDOP=%s, GeoidalSeparation=%s, DGPSAge=%s, DGPSReferenceStationID=%u",
    toLogString(gga.GPSTime,6,2),toLogString(gga.latitude,6,2),toLogString(gga.longitude,6,2),toLogString(gga.altitude,6,2),gga.GPSQualityIndicator,gga.satelliteCount,toLogString(gga.HDOP,6,2),toLogString(gga.geoidalSeparation,6,2),toLogString(gga.DGPSAge,6,2),gga.DGPSReferenceStationID);
  } else if (gga.GPSQualityIndicator == 0) { warn("GGA: Invalid GPS fix.");
  } else { error("GGA: Failed to parse message"); }
}

void N183ToN2k::HandleGLL(const tNMEA0183Msg &NMEA0183Msg) {

  tGLL gll;
  if (NMEA0183ParseGLL(NMEA0183Msg,gll) && gll.status == 'A') {
    sendPGN129025(gll.latitude,gll.longitude);
    Latitude = gll.latitude;
    Longitude = gll.longitude;
    trace("GLL: Time=%s, Latitude=%s, Longitude=%s",toLogString(gll.GPSTime,6,2),toLogString(gll.latitude,6,2),toLogString(gll.longitude,6,2));
  } else if (gll.status == 'V') { warn("GLL: Is  Void");
  } else { error("GLL: Failed to parse message"); }
}

void N183ToN2k::HandleHDT(const tNMEA0183Msg &NMEA0183Msg) {

  double TrueHeading;
  if (NMEA0183ParseHDT(NMEA0183Msg,TrueHeading)) {
    sendPGN127250(TrueHeading);
    trace("HDT: True heading=%s",toLogString(TrueHeading,6,2));
  } else { error("HDT: Failed to parse message"); }
}

void N183ToN2k::HandleVTG(const tNMEA0183Msg &NMEA0183Msg) {
 double MagneticCOG, COG, SOG;
  
  if (NMEA0183ParseVTG(NMEA0183Msg,COG,MagneticCOG,SOG)) {
    Variation=COG-MagneticCOG; // Save variation for Magnetic heading
    sendPGN129026(N2khr_true,COG,SOG);
    trace("VTG: COG=%s",toLogString(COG,6,2));
  } else { error("VTG: Failed to parse message"); }
}

/**
 * Receive NMEA0183 BOD message (Bearing Origin to Destination) and store it for later use when the RMB message is received.
 * Does not contain enough information itself to send a single NMEA2000 message.
 */
void N183ToN2k::HandleBOD(const tNMEA0183Msg &NMEA0183Msg) {

  if (NMEA0183ParseBOD(NMEA0183Msg,bod)) {
    trace("BOD: True heading=%s, Magnetic heading=%s, Origin ID=%s, Dest ID=%s",toLogString(bod.trueBearing,6,2),toLogString(bod.magBearing,6,2),bod.originID,bod.destID);
  } else { error("BOD: Failed to parse message"); }
}

/**
 * Handle receiving NMEA0183 RTE message (route).
 * Based on the info which is processed after the last correlated RTE message is received a NMEA2000 messages is periodically sent using a timer.
 * A timer is used because (at least) B&G Triton requires a new PGN129285 around every 10 sec, but receiving RTE messages could take much longer 
 * on routes with more then 5 waypoints. So we need to sent the same PGN129285 multiple times in between.
 * For this we also need previously send WPL, messages. A single WPL message is sent per NMEA0183 message cycle (RMC, RMB, GGA, WPL) followed 
 * by the RTE message(s) in the next cycle. So it can take a while for long routes before alle waypoints are received and finally the RTE messages.
 */
void N183ToN2k::HandleRTE(const tNMEA0183Msg &NMEA0183Msg) {

  tRTE rte;
  if (NMEA0183ParseRTE(NMEA0183Msg,rte)) {

    if (rte.currSentence == 1) {
      route->initialize(rte.routeID);
    }

    //Combine the waypoints of correlated RTE messages in a central list.
    //Will be processed when the last RTE message is recevied.
    for (int i=0; i<rte.nrOfwp; i++) {
      route->addWaypoint(rte[i]);
    }
    if (LOG_TRACE) {
      route->logWaypointNames();
      log_P("RTE: Waypoints in message: ");
      for (byte i=0; i<rte.nrOfwp;i++) {
        log(rte[i]);log_P(",");
      }
      log_P("\n");
    }

    if (rte.currSentence == rte.nrOfsentences) {
      route->finalize(rte.type,bod.originID);
      //Sending PGN129285 is handled from a timer.
    }
    trace("RTE: Time=%u, Nr of sentences=%u, Current sentence=%u, Type=%s, Route ID=%u",millis(),rte.nrOfsentences,rte.currSentence,rte.type,rte.routeID);
  } else { error("RTE: Failed to parse message"); }
}

/**
 * Receive NMEA0183 WPL message (Waypoint List) and store it for later use after all RTE messages are received.
 * A single WPL message is sent per NMEA0183 message cycle (RMC, RMB, GGA, WPL) so it can take a while for long routes 
 * before alle waypoints are received.
 * Does not contain enough information itself to send a single NMEA2000 message.
 */
void N183ToN2k::HandleWPL(const tNMEA0183Msg &NMEA0183Msg) {
  
  tWPL wpl;
  if (NMEA0183ParseWPL(NMEA0183Msg,wpl)) {
    route->addCoordinates(wpl.name, wpl.latitude, wpl.longitude);
  } else { error("WPL: Failed to parse message"); }
}

/*
 * NMEA0183Msg.IsMessageCode wrapper function using PROGMEM Strings to limited SRAM usage.
 */
boolean isMessageCode_P(const tNMEA0183Msg &NMEA0183Msg, const char* code) {
  
  char buffer[4];
  strcpy_P(buffer, code); //Copy from PROGMEM to SRAM
  return NMEA0183Msg.IsMessageCode(buffer);
}

void N183ToN2k::HandleNMEA0183Msg(const tNMEA0183Msg &NMEA0183Msg) {

  debug("Handling NMEA0183 message %s",NMEA0183Msg.MessageCode());
  if (isMessageCode_P(NMEA0183Msg,PSTR("GGA"))) {
    HandleGGA(NMEA0183Msg);
  } else if (isMessageCode_P(NMEA0183Msg,PSTR("GGL"))) {
    HandleGLL(NMEA0183Msg);
  } else if (isMessageCode_P(NMEA0183Msg,PSTR("RMB"))) {
    HandleRMB(NMEA0183Msg);
  } else if (isMessageCode_P(NMEA0183Msg,PSTR("RMC"))) {
    HandleRMC(NMEA0183Msg);
  } else if (isMessageCode_P(NMEA0183Msg,PSTR("WPL"))) {
    HandleWPL(NMEA0183Msg);
  } else if (isMessageCode_P(NMEA0183Msg,PSTR("BOD"))) {
    HandleBOD(NMEA0183Msg);
  } else if (isMessageCode_P(NMEA0183Msg,PSTR("VTG"))) {
    HandleVTG(NMEA0183Msg);
  } else if (isMessageCode_P(NMEA0183Msg,PSTR("HDT"))) {
    HandleHDT(NMEA0183Msg);
  } else if (isMessageCode_P(NMEA0183Msg,PSTR("RTE"))) {
    HandleRTE(NMEA0183Msg);
  }
} 

