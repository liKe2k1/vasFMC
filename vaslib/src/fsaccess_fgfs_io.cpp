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


#include "fsaccess_fgfs_base.h"
#include "fsaccess_fgfs.h"
#include "fsaccess_fgfs_xmlprot.h"
#include "fsaccess_fgfs_flightstatusmodel.h"
#include "fsaccess_fgfs_io.h"

/////////////////////////////////////////////////////////////////////////////
FGFSIo::FGFSIo(VasFlightStatusModel* flightstatusmodel, VasTcasModel* tcasmodel) 
  : m_xmlprot(0),
    enabled(false),
    m_flightstatusmodel(flightstatusmodel),
    m_tcasmodel(tcasmodel)
{};
FGFSIo::~FGFSIo() {
};
FGFSIoUdp::FGFSIoUdp (VasFlightStatusModel* flightstatusmodel, QHostAddress hostaddress=QHostAddress::LocalHost, quint16 port=12000)
  : FGFSIo(flightstatusmodel)
{ 
 
  socket= new QUdpSocket(this);
  if (!socket->bind(hostaddress, port) )
    { DEBUGP(LOGDEBUG, "error was:" << socket->errorString());
      qFatal ("FGFSIo::FGFSIoUdp: udp socket could not be bound: %i"
	      ,socket->error());
	}
 
   socket->open(QIODevice::ReadOnly);
  initCommon();
  if ( !connect(socket, SIGNAL(readyRead()), this, SLOT(slotIoRead()))) {
    DEBUGP(LOGCRITICAL, "connecting error");
      }
}
void FGFSIoUdp::slotIoRead()
{
  DEBUGP(LOGBULK, "entered");
  // buffer.resize(FG_MAX_MSG_SIZE);
  while (socket->hasPendingDatagrams())
    {
      DEBUGP(LOGBULK, "reading bytes, expect=" 
	     << socket->pendingDatagramSize());
      buffer.append(socket->read(socket->pendingDatagramSize()));
      DEBUGP(LOGBULK, "buffer size" << buffer.size());
     
      if (dissectXfer(true)) {
	m_read_timout_timer.start(timeout);
	emit sigSetValid(true);
	
      } else {
	emit sigSetValid(false);
      }
    }
};

FGFSIoFile::FGFSIoFile (QString filename)
{
  file = new QFile(this);
  file->setFileName(filename);
  if (!file->open(QIODevice::ReadOnly)) {
    DEBUGP(LOGDEBUG, "FGFSIo::" << file->errorString());
    qFatal ("FGFSIo::FGFSIoFile: could not be opended: %i", file->error());
  }
  initCommon();
  if (file->size() >0) {
    file->seek(file->size());
  }
  connect(file, SIGNAL(readyRead()),
	  this, SLOT(slotIoRead())) ;
  
  notifier= new QFileSystemWatcher();
  notifier->addPath(file->fileName());
  DEBUGP(LOGDEBUG, "watch:" << notifier->files());
  connect(this, SIGNAL(sigNotify()),
  	  this, SLOT(slotIoRead()));
  connect(notifier, SIGNAL(fileChanged(const QString)),
    	  this, SLOT (slotIoRead(const QString)));

}
void FGFSIoFile::slotIoRead(const QString foo) {
   DEBUGP(LOGBULK, "entered" << foo);
  emit sigNotify();
}
void FGFSIoFile::slotIoRead()
{
  DEBUGP(LOGBULK, "entered" << file->size() << "at pos" << file->pos());
  if  (!file->atEnd())
    {
      DEBUGP(LOGDEBUG, "reading bytes, expect=" 
	     << file->bytesAvailable() << "absolute size" << file->size() << "at pos" << file->pos());
      buffer.append(file->read(file->bytesAvailable()));
      
      if (buffer.size() <= 0)
	{
	  DEBUGP(LOGWARNING, "no data read from socket");
	  return;
	}        
      DEBUGP(LOGBULK, "bytes read:" << buffer.size());
      
      if (dissectXfer(false)) {
	m_read_timout_timer.start(timeout);
	//emit sigStatusUpdate(&message);
	emit sigSetValid(true);
      } else {
	emit sigSetValid(false);
      }
    }
};

bool FGFSIo::initCommon(){
  buffer.reserve(FG_MAX_MSG_SIZE);
  return true;
}

bool FGFSIo::enable(){
  if ( enabled ) {
    DEBUGP( LOGWARNING, "This shouldn't happen, but the channel is already in use, ignoring" );
    return false;
  }
  if (true) {
    MYASSERT(connect(&m_read_timout_timer, SIGNAL(timeout()),
	         this, SLOT(slotIoTimeout())));
    m_read_timout_timer.start(timeout);
    enabled=true;
    DEBUGP(LOGDEBUG, "enabled? " << isEnabled());
    return true;
  }
  return false;
}


void FGFSIo::slotIoTimeout() {
  // uh how do we get it?
  // m_flightstatus.setValid(false);  
  emit sigSetValid(false);  
}
bool FGFSIo::dissectXfer(bool last_only) {
  int i, match, index_end, index_start;
  QByteArray data;
 
  DEBUGP(LOGBULK, "linesep:" << m_xmlprot->getLineSep().pattern().data() << "EOF");
  //DEBUGH( "buffered bytes", buffer.data()  );
  message.clear();
  index_end=-m_xmlprot->getLineSep().pattern().size();
  index_start=0;
  i=0;
  while (buffer.size()+m_xmlprot->getLineSep().pattern().size()>i) {
    // just a plausibility check, real decision made from line_sep_matcher stuff
    match=m_xmlprot->getLineSep().indexIn(buffer, i);
    if ( match==-1 ){
      if ( i==0 ) {
	DEBUGP(LOGWARNING, "no line separator yet");
	return false;
      }
      if (last_only) {
	data=buffer.mid(index_start, index_end-index_start+m_xmlprot->getLineSep().pattern().size());
	DEBUGP( LOGBULK, "cut chunk size:" << data.size()  );
	if (parseDataset(&data)) {};
      }
      // keep stuff in the buffer after last line_sep, obviously/hopefully
      // the next read will append missing data up to the next line_sep (or beyond...)
      buffer.remove(0, index_end+m_xmlprot->getLineSep().pattern().size());
      DEBUGP(LOGBULK, "trailing stuff in buffer len=" << buffer.size());
      return true;
    }
    index_start=index_end+m_xmlprot->getLineSep().pattern().size();
    index_end=match;
    DEBUGP(LOGBULK, "msg between" << index_start << "and (including linesep)" << index_end);
    if (!last_only) {
      data=buffer.mid(index_start, index_end-index_start+m_xmlprot->getLineSep().pattern().size());
      DEBUGP( LOGBULK, "cut chunk size:" << data.size()  );
      if (parseDataset(&data)) {};
    }
    i=index_end+m_xmlprot->getLineSep().pattern().size();
    DEBUGP(LOGBULK, "cont last msg search at: " << index_start);
  }
  qFatal ("bs at dissecting data (evil should not happen case)");
}

bool FGFSIo::parseDataset(QByteArray* data) {
  int i, index_start, index_end;
  QList<int> index_list;
   
  //DEBUGH( "parsing:", data->data()  );
  index_list.clear();
  i=0;
  while (i<data->size()) {
    index_list.append(m_xmlprot->getVarSep().indexIn(*data, i));
    if ( index_list.last()==-1 ) {
      if ( i==0 ) {
	DEBUGP(LOGWARNING, "deal with silly 1-byte data!! (so no sep)");
	return false;
      } else {
	index_list.removeLast();
	index_list.append(m_xmlprot->getLineSep().indexIn(*data, i)); //last one doesn't have var_sep
	if (index_list.last()==-1) {
	  DEBUGP(LOGWARNING, "confused by invalid data (no line sep at end)");
	  return false;
	}
	break;
      }
    } else {
    i=index_list.last()+1;
    //    DEBUGP(LOGDEBUG, "cont last chunk search at: " << i);
    }
  }

  if ( index_list.size() != m_xmlprot->getSize()) {
    DEBUGP(LOGWARNING, "bs happended: split:"
	   << index_list.size() << " expected:" << m_xmlprot->getSize()
	   );
    return false;
  }
  DEBUGP(LOGBULK, "successful split to chunks:"
	 << index_list.size() << " expected:" << m_xmlprot->getSize()
	 );
  index_list.prepend(-m_xmlprot->getVarSep().pattern().size());
  i=-1;
  while (i++ < m_xmlprot->getSize()-1) {
    index_start=index_list[i]+m_xmlprot->getVarSep().pattern().size();
    index_end=index_list[i+1]-index_list[i]-m_xmlprot->getVarSep().pattern().size();
    // bit misnamed due to reuse, denotes length not end index here
    DEBUGP(LOGBULK, "chunk_nr:" << m_xmlprot->getChunkInfo()[i].index.row() 
	   << "from:" << index_start << "len:" << index_end
	   );
    
    newFGmsgchunk pChunk(m_xmlprot->getChunkInfo()[i].FgType,
                         m_xmlprot->getChunkInfo()[i].count
			 );
    
    pChunk.m_description.index=m_xmlprot->getChunkInfo()[i].index;
    int row=pChunk.m_description.index.parent().row();
    if (row<0) row=pChunk.m_description.index.row(); 
#if 0
    DEBUGP(LOGWARNING, row
	   << pChunk.m_description.index.internalId()
	   << pChunk.m_description.index.parent().row() 
	   << "->" 
	   << pChunk.m_description.index.row()
	   );
#endif
    if (!m_flightstatusmodel->setData(pChunk.m_description.index, data->mid(index_start, index_end)))
      DEBUGP(LOGWARNING, "Error converting" 
	     << m_flightstatusmodel->data(pChunk.m_description.index, Qt::ToolTipRole).toString()) 
	     << data->mid(index_start, index_end);
  }
  return true;
}

bool FGFSIo::setIoProt(const VasFlightStatusModel* flightstatusmodel, const QString& filetrunc, const int config_timeout)
{
  // uh clean up the whole io mess, sane ctor's etc
  if (m_xmlprot==0) {
    delete m_xmlprot;
    m_xmlprot=new FgXmlFlightstatusProtocol(flightstatusmodel);
  }
  timeout=config_timeout;
  QString error;
  if (m_xmlprot->loadFromXMLFile(filetrunc, error )) {
#if 0
    for (int i=0; i<m_xmlprot->getSize(); i++) {
      DEBUGP(LOGWARNING, QString("row#") 
	     << m_xmlprot->getChunkInfo().at(i).index.row() 
	     << "->" << m_flightstatusmodel->data(m_xmlprot->getChunkInfo().at(i).index, Qt::ToolTipRole).toString()
	     << "(" << i << "of" << m_xmlprot->getSize() << ")");
    };
#endif    
    DEBUGP(LOGWARNING, "Protocol from:" << filetrunc << endl << m_xmlprot->getChunkInfo());
    return true;
  }
  Logger::log(error);
  return false;
}
bool FGFSIo::setIoProt(const VasTcasModel* tcasmodel, const QString& filetrunc, const int config_timeout) {
///FIXME see above
MYASSERT(false);
}
/////////////////////////////////////////////////////////////////////////////
FGFSIoTelnet::FGFSIoTelnet (QHostAddress hostaddress, quint16 port) 
  : m_timeout(1000),
    m_internal_connected(false),
    m_hostaddress(hostaddress),
    m_port(port),
    m_telnetchannel()
 {
  initCommon(); // init buffer
  MYASSERT(QObject::connect (&m_read_timout_timer, SIGNAL(timeout()),
	                     this, SLOT(slotIoTimeout())));
  MYASSERT(QObject::connect (&m_telnetchannel, SIGNAL(sigRcvd(const QByteArray&)),
	                     this, SIGNAL(sigRcvd(const QByteArray&))));
}
void FGFSIoTelnet::slotConnectToHost() {
  if (m_telnetchannel.open()) {
    switch (m_telnetchannel.netChannel::connect(m_hostaddress.toString().toLatin1(), m_port)) {
       case 0:
         emit connected();
         m_internal_connected=true;
         break;
       case -1:
         m_internal_connected=false;
         // error occured, see handleError()
         break;
    }
  }
}
void FGFSIoTelnet::slotDisconnect() {
  m_telnetchannel.close();
  m_internal_connected=false;
  emit disconnected();
}

void FGFSIoTelnet::slotChangeConnection(bool checked) {
  // convenient for easier messing with checkable PushButton
  if (checked) {
    slotConnectToHost();
  } else {
    slotDisconnect();
  }
};

void FGFSIoTelnet::slotIoRead(){
  //m_read_timout_timer.start(timeout);
  if (m_internal_connected) {
    m_telnetchannel.poll(timeout);
  }
  return;
};
void FGFSIoTelnet::slotIoTimeout() {
  // huh?
};
void FGFSIoTelnet::slotIoWrite(const QByteArray& obuffer){
  //DEBUGH("to send:", obuffer);
  if (!m_internal_connected) {
    //DEBUGP(LOGWARNING, "not connected(?)");
    return;
  }
  if (m_telnetchannel.push((obuffer.trimmed() + m_telnetchannel.getTerminator()).data())) { 
    //DEBUGP(LOGDEBUG, "pushed");
    slotIoRead();
    emit written(); 
  }
  return;
};


/////////////////////////////////////////////////////////////////////////////
void FGFSIo::stateChangedSlot(QAbstractSocket::SocketState state) {
  switch (state) {
  case QAbstractSocket::UnconnectedState:
    DEBUGP(LOGDEBUG, "Status: Unconnected");
    break;
  case QAbstractSocket::HostLookupState:
    DEBUGP(LOGDEBUG, "Status: Searching for host");
    break;
  case QAbstractSocket::ConnectingState:
    DEBUGP(LOGDEBUG, "Status: Connecting to host");
    break;
  case QAbstractSocket::ConnectedState:
    DEBUGP(LOGDEBUG, "Status: Connected");
    break;
  case QAbstractSocket::ClosingState:
    DEBUGP(LOGDEBUG, "Status: Closing");
    break;
  default:
    break;
  }
}
FGFSIoTelnetChannel::FGFSIoTelnetChannel()
 : m_netbuffer(1024)
   
{
  setTerminator( "\r\n" );
}
void FGFSIoTelnetChannel::collectIncomingData (const char* s, int n) {
  DEBUGP(LOGBULK, "collect bs" << n);
  emit sigRcvd(QByteArray(s));
};
void FGFSIoTelnetChannel::foundTerminator () {
  //DEBUGP(LOGDEBUG, "got answer");
};


////////////////////////////////////////////////////////////////////////////////
FGFSTelnetDialog::FGFSTelnetDialog(QObject* parent){

 QVBoxLayout *layout = new QVBoxLayout();
  layout->addWidget((textEdit = new QTextEdit));
  layout->addWidget((lineEdit = new QLineEdit));
  layout->addWidget((connectButton = new QPushButton(tr("Connect"))));
  layout->addWidget((statusLabel = new QLabel(tr("öff"))));
  connectButton->setCheckable(true);
  connectButton->setDefault (false);     // should be default http://doc.trolltech.com/4.2/qpushbutton.html
  connectButton->setAutoDefault (false); // grrr
  textEdit->setReadOnly (true);
  textEdit->setEnabled(true);
  lineEdit->setEnabled(false);

  connect (connectButton, SIGNAL(toggled(bool)),
	   this, SIGNAL (connect_checked(bool)));
  connect (lineEdit, SIGNAL(returnPressed()),
	   this, SLOT(sendInput()));
  setLayout(layout);
  setWindowTitle(tr("FGFS Telnet Example"));
  data.reserve(1024);
}
FGFSTelnetDialog::~FGFSTelnetDialog() {
  //delete this->layout();
}
void FGFSTelnetDialog::slotEnable() {
  statusLabel->setText(tr("öff"));
  //textEdit->clear();
  //textEdit->setEnabled(true);
  lineEdit->setEnabled(true);
  connectButton->setDown(true);
};
void FGFSTelnetDialog::slotDisable() {
statusLabel->setText(tr("ön"));
// textEdit->setEnabled(false);
 lineEdit->setEnabled(false);
 connectButton->setDown(false);
};
void FGFSTelnetDialog::slotDisplay(const QByteArray& data) {
  //DEBUGP(LOGDEBUG, "display:" << data.data());
  textEdit->append(data.data());
}
void FGFSTelnetDialog::sendInput() {
 
  DEBUGP(LOGDEBUG, "input:" << (lineEdit->text()).toAscii());
  data.append((lineEdit->text()).toAscii().trimmed());
  emit sigInput(data);
  lineEdit->clear();
  data.clear();
}
 
