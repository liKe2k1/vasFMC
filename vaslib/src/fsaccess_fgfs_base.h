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
#ifndef FGACCESS_BASE_H
#define FGACCESS_BASE_H

#include <QtDebug>
#include <QStringList>
#include <QModelIndex>
#include <QHostAddress>


static int const READ_TIMEOUT_PERIOD_MS=3000;
static int const FG_MAX_MSG_SIZE=16384;

Q_ENUMS(fg_type) //huh?
enum fg_type { FG_BOOL=0, FG_INT, FG_DOUBLE, FG_STRING };
class FgChunkDesc
{
  //just in case we decide to use private stuff later...
  friend QDebug& operator<<(QDebug& out, const FgChunkDesc& descr);
 public:
 FgChunkDesc(fg_type a=FG_INT, int b=0)
   : count(b),
     FgType(a)
     {};
  void clear() { count=0;
    FgType=FG_INT;
    index=QModelIndex();
  };
    
  QModelIndex index;      //   now use the model/view approach
  int count;              // eventual multiple data of same "sort"
  fg_type FgType;         // the flightgear protocol data type
 
  const QString toVerboseString() const {
    return "index: (row: " + QString().setNum(index.row()) 
           + " col: " + QString().setNum(index.column()) + " ) " 
	   + "count: " + QString().setNum(count)
	   + " -> "+ index.data(Qt::ToolTipRole).toString()
	    + "\n";
  };
};
QDebug& operator<<(QDebug& out, const FgChunkDesc& descr);
typedef QList<FgChunkDesc> FgProt;

//those are for passing around the raw received data
typedef QPair<fg_type, QByteArray> FGmsgchunk;
typedef QMap<int, FGmsgchunk> FGmsg;
//           ^- the simple order to base a 'switch..case' on.
//           while a hash would be more direct (and easier to
//           adapt) it would lead to even more convoluted code.
//           imho of course.
typedef QMapIterator<int, FGmsgchunk> FGmsgIterator;

//rewrite more systematically
class newFGmsgchunk {
public:
  //! simple convenience for the typical case;
  newFGmsgchunk(const fg_type a=FG_BOOL, const int b=0)
  : m_description(a, b)
  {};
  virtual ~newFGmsgchunk() {};
  FgChunkDesc m_description;
  QByteArray m_rawdata;
};
typedef QList<newFGmsgchunk> newFGmsg;
typedef QListIterator<newFGmsgchunk> newFGmsgIterator;

//needed here?
class FgVas {
public:
   static const QStringList tcasnames;
};

// some cleanup (hopefully)
#define LOGFATAL       0
#define LOGCRITICAL    1
#define LOGWARNING     2
#define LOGDEBUG       3
#define LOGBULK        4

static int fgloglevel=LOGDEBUG;

#ifdef Q_CC_GNU
#define VERBOSE << QString("( %1 %2%3 )").arg(__FILE__).arg("+").arg(__LINE__)
#endif
#define DEBUGP(x, args...)      \
  if (fgloglevel>=x) \
  qDebug() VERBOSE << Q_FUNC_INFO << ":\n" << #x << args

#endif /* FGACCESS_BASE_H */
