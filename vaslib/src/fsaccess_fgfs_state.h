///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2007, 2008 K. Hoercher
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
#ifndef FGACCESS_STATE_H
#define FGACCESS_STATE_H

class FlightStatus;
class FGFSIo;
class VasFlightStatusModel;
class QTreeView;

#include "fsaccess_fgfs_base.h"

class FGFSstate : public QObject
{ Q_OBJECT

public:
  
  FGFSstate (FlightStatus* flightstatus); 
  virtual ~FGFSstate();

  const bool isValid() const;
 
  bool setParameter (const FGmsg* message);
    
  bool initread(QHostAddress hostaddress, quint16 port, QString file);
  bool initwrite(QHostAddress hostaddress, quint16 port);

public slots:
  void slotStatusUpdate(int);
  void slotSetValid(bool); 

  void slotChangeConnection(bool);
  void slotIoWrite(const QByteArray&);

signals:
  // udp
  void sigStatusUpdate(const FGmsg&); // was used for tcas, rewrite pending
  void sigSetValid(bool);

  // telnet
  void connected();
  void disconnected();
  void ChangeConnection(bool);
  void recvd(const QByteArray&);
  void IoWrite(const QByteArray&);

protected:
  VasFlightStatusModel* m_flightstatusmodel;
  FlightStatus* m_flightstatus;

private:
  FGFSstate () {};
  FGFSIo* bulkread;
  FGFSIo* telnet;
  //QTableView *m_view;
  //QListView *m_view;
  QTreeView* m_view;
};


/* #include "fsaccess.h" */
/* Q_DECLARE_METATYPE(SmoothedValueWithDelay<double>*) */
/* Q_DECLARE_METATYPE(double*) */

#endif /* FGACCESS_STATE_H */
