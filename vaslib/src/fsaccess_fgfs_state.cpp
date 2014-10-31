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
#include "fsaccess_fgfs_base.h"
#include "fsaccess_fgfs_io.h"
#include "fsaccess_fgfs_flightstatusmodel.h"
#include "flightstatus.h"
#include "navdata.h"
#include "fsaccess_fgfs_state.h"

FGFSstate::FGFSstate (FlightStatus* flightstatus) 
  : m_flightstatusmodel(new VasFlightStatusModel(flightstatus)),
    m_flightstatus(flightstatus)
{}; 
FGFSstate::~FGFSstate() {
  delete m_flightstatusmodel;
  //delete m_flightstatus;
  //delete m_navdata;
}
const bool FGFSstate::isValid() const { return m_flightstatus->isValid(); };

bool FGFSstate::initread(QHostAddress hostaddress, quint16 port, QString file)
{
  bulkread=new FGFSIoUdp (m_flightstatusmodel, hostaddress, port); // use udp socket
  bulkread->setIoProt(m_flightstatusmodel, file, 1000);
  bulkread->enable();
  if (!bulkread->isEnabled()) 
    qFatal ("FSAccessFgFs could not enable Io");

  connect(bulkread, SIGNAL(sigSetValid(bool)),
	  this, SIGNAL(sigSetValid(bool)));
  return true;
}
bool FGFSstate::initwrite(QHostAddress hostaddress, quint16 port)
{
  DEBUGP(LOGWARNING, "Will rw at:" << hostaddress.toString() << "on port:" << port);
  telnet= new FGFSIoTelnet (hostaddress, port);
  connect (telnet, SIGNAL(connected()),
	   this, SIGNAL(connected()));
  connect (telnet, SIGNAL(disconnected()),
	   this, SIGNAL(disconnected()));
  connect (this, SIGNAL(ChangeConnection(bool)),
	   telnet, SLOT(slotChangeConnection(bool)));
  MYASSERT(connect (telnet, SIGNAL(sigRcvd(const QByteArray&)),
	   this, SIGNAL(recvd(const QByteArray&))));
  MYASSERT(connect (this, SIGNAL(IoWrite(const QByteArray&)),
	   telnet, SLOT(slotIoWrite(const QByteArray&))));

  //new ModelTest(m_flightstatusmodel, this);
#if 0
  m_view = new QTreeView;
  m_view->setModel(m_flightstatusmodel);
  m_view->show();
#endif
 
  return true;
}

void FGFSstate::slotSetValid(bool valid) 
{
  if (valid) {
    m_flightstatus->recalcAndSetValid(); 
  } else {
    m_flightstatus->invalidate();
  };
  DEBUGP(LOGBULK, "valid recvd" <<  m_flightstatus->isValid());
}
void FGFSstate::slotStatusUpdate(int foo) 
{
  DEBUGP(LOGDEBUG, "recvd status hint" << foo );
}
void FGFSstate::slotChangeConnection(bool onoff)
{
 emit ChangeConnection(onoff);
}
void FGFSstate::slotIoWrite(const QByteArray& msg)
{
  emit IoWrite(msg);
}

/////////////////////////////////////////////////////////////////////////////




