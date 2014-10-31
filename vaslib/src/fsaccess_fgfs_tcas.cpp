///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2008 K. Hoercher 
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
///////////////////////////////////////////////////////////////////////////////

#include "waypoint.h"
#include "flightstatus.h"
#include "fsaccess_fgfs_base.h"
#include "fsaccess_fgfs_io.h"
#include "fsaccess_fgfs_tcas.h"


const QStringList FgVas::tcasnames=(QStringList()
  << "id" // index 0
  << "lat"
  << "lon"
  << "alt"
  << "hdg"
  << "gs"
  << "vs"
  << "idATC"
  << "bState"
  << "com1"
  << "fgvalid"
  << "fginrange"
);

FGTcasItem::FGTcasItem()
  : m_valid(false),
    m_id(-1),
    m_position(),
    m_altitude_ft(-1),
    m_true_heading(-1),
    m_groundspeed_kts(-1),
    m_callsign(QString("BANDIT"))
{};
FGTcasItem::~FGTcasItem() {};

FGTcas::FGTcas()
  : m_tcas_entry_list()
{
      //FSTcasEntryValueList m_tcas_entry_list;
      //just test with empty value
      if (m_tcas_entry_list.isEmpty()) {
        m_tcas_entry_list.append(TcasEntryContainer::TcasEntryContainer().getFSTcasEntry());
      }
      //DEBUGP(LOGWARNING, "dummy tcas:" );

      entry_timeout=30000; // msec
      timer.setSingleShot(true);
};
bool FGTcas::initread(const QHostAddress& hostaddress, quint16 port, const QString& protocol) {
  tcasread=new FGFSIoUdp (0,hostaddress, port);
  tcasread->setIoProt(new VasTcasModel(), protocol, 10000);
  tcasread->enable();
 
  if (!tcasread->isEnabled()) 
    qFatal ("FGTcas could not enable udp Io");

   connect (tcasread, SIGNAL(sigStatusUpdate(FGmsg*)),
 	   this, SLOT(slotStatusUpdate(FGmsg*)));
   // do expicitly not connect slotSetValid, which has only business for
   // main FGFSstate
}
bool FGTcas::initread(const QString& readfile, const QString& protocol) {

  tcasread=new FGFSIoFile(readfile);
  tcasread->setIoProt(new VasTcasModel(), protocol, 10000);
  tcasread->enable();
  if (!tcasread->isEnabled()) 
    qFatal ("FGTcas could not enable file Io");
  connect (tcasread, SIGNAL(sigStatusUpdate(FGmsg*)),
 	   this, SLOT(slotStatusUpdate(FGmsg*)));
  connect (&timer, SIGNAL(timeout()),
	   this, SLOT(slotDie()));
}

FGTcas::~FGTcas(){
  delete tcasread;
}

void FGTcas::slotDummy(int foo) {
  DEBUGP(LOGWARNING, "recvd fd activated=" << foo);
};
void FGTcas::slotDie() {
  int id=tcas_entry_die_list.first();
  DEBUGP(LOGWARNING, "id going to die:" <<id);
  if (id==oldest_entry) { // check again, still oldest, perhaps superfluous
    for (int i = 0; i < tcas_entry_die_list.size(); ++i) {
      if (m_tcas_entry_list.at(i).m_id==id) {
	m_tcas_entry_list.removeAt(i);
	tcas_entry_die_list.removeFirst(); // if nothing went wrong...
	break;
      }
    };
  };
};
void FGTcas::slotStatusUpdate(const FGmsg& message) {
  DEBUGP(LOGDEBUG, "recvd message size=" << message.size());
  TcasEntryContainer tmp;
  FGmsgIterator iMessage(message);

  while (iMessage.hasNext()) 
    { iMessage.next();
      DEBUGP(LOGBULK, "name:" << FgVas::tcasnames.at(iMessage.key()).toLatin1() << ":");
      switch (iMessage.key()) {
      case 0: //id
	DEBUGP(LOGWARNING, "Error converting id" << iMessage.value());
	break;
      case 1:  //lat
	DEBUGP(LOGWARNING, "Error converting lat" << iMessage.value());
	break;
      case 2:  //lon
	DEBUGP(LOGWARNING, "Error converting lon" << iMessage.value());
	break;
      case 3:  //alt
	DEBUGP(LOGWARNING, "Error converting alt" << iMessage.value());
	break;
      case 4:  //hdg
	DEBUGP(LOGWARNING, "Error converting hdg" << iMessage.value());
	break;
      case 5:  //gs
	DEBUGP(LOGWARNING,   "Error converting gs" );
	break;
      case 6:  //vs
	DEBUGP(LOGWARNING,   "Error converting vs") ;
	break;
      case 7:  //idATC
	DEBUGP(LOGWARNING,   "Error converting idATC") ;
	break;
      case 8:  //bState
	DEBUGP(LOGWARNING,   "Error converting bState");
	break;
      case 9:  //com1 
	DEBUGP(LOGWARNING,   "Error converting com1") ;
        break;
      case 10:  //fgvalid 
	DEBUGP(LOGWARNING,   "Error converting fgvalid") ;
	break;
      case 11:  //fginrange
	DEBUGP(LOGWARNING,   "Error converting fginrange") ;
	break;
      default: // including -1 (unnkown) -> ignore
	DEBUGP(LOGWARNING, "unknown data: vasradar_type:" << iMessage.key() 
	       <<  "value:" << iMessage.value().second );
 	break;
     }
    }
 
 bool ok=tmp.check();
 DEBUGP(LOGBULK, "valid:" << ok );
 //DEBUGP(LOGDEBUG, newTcasEntry2.toString());
 // m_tcas_entry_list.append(newTcasEntry);
 for (int i = 0; m_tcas_entry_list.size() >= i; ++i) {
   DEBUGP(LOGBULK, "scan # in list:" << i );
   if ((m_tcas_entry_list.size()==i) or // uh, is there some eval order guaranteed?
       (m_tcas_entry_list.at(i).m_id>tmp.getFSTcasEntry().m_id)) {
     if (tmp.isValid()) {
       m_tcas_entry_list.insert(i, tmp.getFSTcasEntry());
       DEBUGP(LOGDEBUG, "new id:" << tmp.getFSTcasEntry().m_id);
       slotDieAdd(tmp.getFSTcasEntry().m_id);
     }
     break;
   }
   if (m_tcas_entry_list.at(i).m_id==tmp.getFSTcasEntry().m_id) {
     DEBUGP(LOGDEBUG, "matced tcas id");
     if (tmp.isValid()) {
       DEBUGP(LOGDEBUG, "update id:" <<  tmp.getFSTcasEntry().m_id);
       m_tcas_entry_list.replace(i, tmp.getFSTcasEntry());
       slotDieAdd(tmp.getFSTcasEntry().m_id);
     } else {
       DEBUGP(LOGDEBUG, "delete id:" <<  tmp.getFSTcasEntry().m_id);
       m_tcas_entry_list.removeAt(i);
       slotDieRemove(tmp.getFSTcasEntry().m_id);
     }
     break;
   }
 }
 if (!tcas_entry_die_list.empty()) {
   DEBUGP(LOGBULK, "ordered list:" << tcas_entry_die_list);
   if (oldest_entry!=tcas_entry_die_list.first()) {
     oldest_entry=tcas_entry_die_list.first();
     //emit sigResetTimer();
     slotResetTimer();
   }
 }
 DEBUGP(LOGDEBUG, "# in list:" << m_tcas_entry_list.size() );
};
void FGTcas::slotSetValid(bool valid) {
  
};
void FGTcas::slotResetTimer() {
  DEBUGP(LOGDEBUG, "timer started (for id):" << oldest_entry);
  timer.start(entry_timeout);
};
void FGTcas::slotDieAdd(int id) {
  for (int i = 0; i < tcas_entry_die_list.size(); ++i) {
    if (tcas_entry_die_list.at(i)==id) {
      tcas_entry_die_list.removeAt(i);
      break;
    }
  };
  tcas_entry_die_list.append(id);
};
void FGTcas::slotDieRemove(int id) {
  for (int i = 0; i < tcas_entry_die_list.size(); ++i) {
    if (tcas_entry_die_list.at(i)==id) {
      tcas_entry_die_list.removeAt(i);
      break;
    }
  };
};
TcasEntryContainer::TcasEntryContainer(bool cvalid, bool cairborne) 
  : blob(new TcasEntry()),
    valid(cvalid),
    airborne(cairborne)
{};

TcasEntry TcasEntryContainer::getFSTcasEntry()
{
  TcasEntry tmp;
  //tmp.m_tcas_data=blob;
  tmp.m_valid=valid;
  //tmp.airborne=airborne;
  return tmp;
};

const bool TcasEntryContainer::check() {
  if (blob->m_id<0) 
    // variant 1 of fg signalling invalidity
    { 
    setValid(false);
    return isValid();
    }
 
  if (blob->m_altitude_ft<0.0) // uh, watch out the stream
    {
      setAirborne(false);
    }
  // gnarf
  setValid(true);
   return isValid();
}
bool TcasEntryContainer::setId()
{
  blob->m_id=q32value;
  return true;
};
bool TcasEntryContainer::setLat() 
{
  blob->m_position.setLat(fvalue);
  return true;
};
bool TcasEntryContainer::setLon() 
{
  blob->m_position.setLon(fvalue);
  return true;
};
bool TcasEntryContainer::setAlt() 
{
  blob->m_altitude_ft=fvalue;
  return true;
};
bool TcasEntryContainer::setHdg()
{
  blob->m_true_heading=q16value;
  return true;
};
bool TcasEntryContainer::setGs()
{
  blob->m_groundspeed_kts=q16value;
  return true;
};
bool TcasEntryContainer::setVs()
{
  blob->m_vs_fpm=q16value;
  return true;
};
bool TcasEntryContainer::setIdATC()
{
  if (qsvalue.right(14).toLatin1()=="") {// ufo *g* that is, ai aircraft might have empty callsign
    qsvalue="654321 NOT IDENT"; // cut correctly?
    //         ^- expect "4321 NOT IDENT"
  }
//  strcpy(blob.id, qsvalue.right(14).toLatin1());
  return true;
};
bool TcasEntryContainer::setBState() 
{
  //blob.bState=qu8value; // not yet
  return true;
}; 
bool TcasEntryContainer::setCom1() 
{
//  blob.com1=q16value;
  return true;
};
