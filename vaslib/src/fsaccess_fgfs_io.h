///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2007 K. Hoercher
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

#ifndef FGACCESS_IO_H
#define FGACCESS_IO_H
#include <plib/netChat.h>

#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>

#include <QHostAddress>
#include <QUdpSocket>

#include <QFile>

#include <QtGui>

#include "fsaccess_fgfs_xmlprot.h"
#include "fsaccess_fgfs_base.h"
class VasFlightStatusModel;
class VasTcasModel;

class FGFSIo : public QObject
{
Q_OBJECT
public:
  FGFSIo (VasFlightStatusModel* flightstatusmodel=0, VasTcasModel* tcasmodel=0 );
 virtual ~FGFSIo();
  bool enable();
  bool disable();
  bool isEnabled() { return enabled; };
  bool setIoProt(const VasFlightStatusModel*, const QString&, const int config_timeout=1000);
  bool setIoProt(const VasTcasModel*, const QString&, int config_timeout=1000);
protected:
  QByteArray buffer;
  FGmsg message;
 
  QTimer m_read_timout_timer;
  QIODevice* m_read_iodevice;
  bool initCommon();
  bool dissectXfer(bool);
  bool parseDataset(QByteArray*);
  FgXmlFlightstatusProtocol* m_xmlprot;
  int timeout;

  VasFlightStatusModel* m_flightstatusmodel;
  VasTcasModel* m_tcasmodel;

public slots:
  void stateChangedSlot(QAbstractSocket::SocketState);

protected slots:
  virtual void slotIoRead()=0;
  void slotIoTimeout();

signals:
  void sigStatusUpdate(const FGmsg&);

  void sigStatusUpdate(int);  
  void sigSetValid(bool);
private:
  bool enabled; 
};

// specialized sublasses
class FGFSIoUdp : public FGFSIo
{ 
  Q_OBJECT
public:
  FGFSIoUdp (VasFlightStatusModel* flightstatusmodel, QHostAddress hostaddress, quint16 port);
  virtual ~FGFSIoUdp(){};
protected slots:
 void slotIoRead();
private:
  QUdpSocket* socket;
};

class FGFSIoFile : public FGFSIo
{
  Q_OBJECT 
public:
  FGFSIoFile (QString file);
  virtual ~FGFSIoFile(){};
protected slots:
  void slotIoRead();
  void slotIoRead(const QString);
 signals:
  void sigNotify();
private: 
 QFile* file;
 QFileSystemWatcher* notifier;
};

class FGFSIoTelnetChannel : public QObject, public netChat
{ Q_OBJECT
    netBuffer m_netbuffer;
public:
    FGFSIoTelnetChannel();
    
    void collectIncomingData( const char* s, int n );
    void foundTerminator(); // found more or less uneeded
    // Override netChannel
signals: 
  void sigRcvd(const QByteArray&);
  void stateChange(QAbstractSocket::SocketState);
};


class FGFSIoTelnet : public FGFSIo
{ 
  Q_OBJECT
public:
  FGFSIoTelnet (QHostAddress hostaddress=QHostAddress::LocalHost, quint16 port=12001);
  ~FGFSIoTelnet(){};
 
public slots:
  void slotConnectToHost();
  void slotDisconnect();
  void slotChangeConnection(bool);
  void slotIoWrite(const QByteArray&);
  void slotIoRead();

protected:

protected slots:
  void slotIoTimeout();

signals:
  void connected();
  void disconnected();
  void written();
  void sigRcvd(const QByteArray&);

private:
  int m_timeout;
  bool m_internal_connected;
  QHostAddress m_hostaddress;
  quint16 m_port;
  FGFSIoTelnetChannel m_telnetchannel;
};

class FGFSTelnetDialog : public QWidget
{ Q_OBJECT
public:
  FGFSTelnetDialog(QObject* parent=0);
  virtual ~FGFSTelnetDialog() ;

public slots:
  void slotEnable();
  void slotDisable();
  void slotDisplay(const QByteArray&);
protected slots:
  void sendInput();
signals:
  void connect_checked(bool);
  void sigInput(const QByteArray&);

private:
  QPushButton *connectButton;
  QTextEdit *textEdit;
  QLineEdit *lineEdit;
  QLabel *statusLabel;
  QByteArray data;
};

#endif /* FGACCESS_IO_H */
