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
#include <QDir>
#include <QFile>
#include "fsaccess_fgfs_base.h"
#include "fsaccess_fgfs_flightstatusmodel.h"
#include "fsaccess_fgfs_xmlprot.h"

//constants for xml protocolfile parsing
const QString ProtXmlDef::NodePropertyList="PropertyList";
const QString ProtXmlDef::NodeProtocolType="generic";
const QString ProtXmlDef::NodeProtocolIn="input";
const QString ProtXmlDef::NodeProtocolOut="output";
const QString ProtXmlDef::NodeLineSep="line_separator";
const QString ProtXmlDef::NodeVarSep="var_separator";
const QString ProtXmlDef::NodeBinary="binary_mode";
const QString ProtXmlDef::NodeBinaryFooter="binary_footer";
const QString ProtXmlDef::NodeChunk="chunk";
const QString ProtXmlDef::NodeType="type";
const QString ProtXmlDef::NodeProp="node";
//not in fg generic
const QString ProtXmlDef::NodeName="name";
const QString ProtXmlDef::NodeCount="count";
//const QString ProtXmlDef::NodeRadio="radio"; //not yet

//unused by us
const QString ProtXmlDef::NodeFormat="format";
const QString ProtXmlDef::NodeOffset="offset";
const QString ProtXmlDef::NodeFactor="factor";
const QString ProtXmlDef::ProtFileSubDir="Protocol";

// the textual representations known in the fg code
const QString ProtXmlDef::SepNewlineRep="newline";
const QString ProtXmlDef::SepTabRep="tab";
const QString ProtXmlDef::SepSpaceRep="space";
const QString ProtXmlDef::SepFormfeedRep="formfeed";
const QString ProtXmlDef::SepCarriagereturnRep="carriagereturn";
const QString ProtXmlDef::SepVerticaltabRep="verticaltab";


FgXmlProtocol::FgXmlProtocol() :
  XMLModel(false),
  m_fgroot_path(new QDir("/usr/share/games/FlightGear")) //!FIXME
{
  DEBUGP(LOGWARNING, m_fgroot_path->path());
};


FgXmlProtocol::~FgXmlProtocol() { 
  close();
  delete m_fgroot_path;
};

bool FgXmlProtocol::loadFromXMLFile(const QString& filenamestub, QString& err_msg) {
  DEBUGP(LOGWARNING, m_fgroot_path->path());
  QString filepath = QDir::cleanPath(
                       m_fgroot_path->path()
                     + QDir::separator()
                     + ProtXmlDef::ProtFileSubDir
	             + QDir::separator()
	             + filenamestub
	             + ".xml" //another static?
		     );
  QFile protfile(filepath);
  if (!protfile.exists()) {
    err_msg=protfile.fileName()+"doesn't exist";
    return false;
  }
  return XMLModel::loadFromXMLFile(protfile.fileName(), err_msg);
} 

bool FgXmlProtocol::parseDOM(QString& err_msg) {
  QDomElement dom_elem_base = m_xml_dom.documentElement();
  if (dom_elem_base.nodeName()!=ProtXmlDef::NodePropertyList) {
    err_msg="Protocol file not valid " + ProtXmlDef::NodePropertyList;
    return false;
  }
  QDomElement dom_elem_prot = dom_elem_base.firstChildElement(ProtXmlDef::NodeProtocolType).firstChildElement(ProtXmlDef::NodeProtocolOut);
  if (dom_elem_prot.isNull()) {
    err_msg="invalid FG generic protocol file";
    return false;
  }
  QDomElement current_elem;

  //FIXME: all of simgear/props/props.cxx SGPropertyNode::getBoolValue () could be construed as 
  //setting binary mode. Considered silly either ways, pulling in simgear dependency
  //for this case, or to reimplement and track the checks for all possibilities just to deny
  //them.
  if (getFirstChildElement(current_elem, dom_elem_prot, ProtXmlDef::NodeBinary, err_msg, "")) {
      err_msg="Not implemented yet: PITA of deciding if " + ProtXmlDef::NodeBinary 
      + " is used. It isn't implemented here anyways. Please remove <" 
      + ProtXmlDef::NodeBinary + "> tag.";
      return false;
      }
  err_msg.clear(); //it's not an error here
  
  if (!getFirstChildElement(current_elem, dom_elem_prot, ProtXmlDef::NodeLineSep,
      err_msg, "No needed <" + ProtXmlDef::NodeLineSep +"> set")) return false;
  m_line_separator=setSep(current_elem.text());
  if (m_line_separator.pattern().isEmpty()) {
    err_msg="Empty <" + ProtXmlDef::NodeLineSep +"> value not allowed";
    return false;
  }
  if (!getFirstChildElement(current_elem, dom_elem_prot, ProtXmlDef::NodeVarSep, 
      err_msg, "No needed <" + ProtXmlDef::NodeVarSep +"> set")) return false;
  m_var_separator=setSep(current_elem.text());
  if (m_var_separator.pattern().isEmpty()) {
    err_msg="Empty <" + ProtXmlDef::NodeVarSep +"> value not allowed";
    return false;
  }

  if (!getFirstChildElement(current_elem, dom_elem_prot, ProtXmlDef::NodeChunk,
      err_msg, "No protocol chunk defined")) return false;
  QDomElement info_elem;
  while (!current_elem.isNull()) {
    if (getFirstChildElement(info_elem, current_elem, ProtXmlDef::NodeName, err_msg)) {
      if (!insertChunkName(info_elem.text())) {
	current_elem=current_elem.nextSiblingElement(ProtXmlDef::NodeChunk);
	break; //that chunk is not known, discard
      }
    }
    
    if (getFirstChildElement(info_elem, current_elem, ProtXmlDef::NodeType, err_msg)) {
      if (info_elem.text()=="bool") 
	insertChunkType(FG_BOOL);
      else if (info_elem.text()=="float")
	insertChunkType(FG_DOUBLE);
      else if (info_elem.text()=="string")
	insertChunkType(FG_STRING);
      else if (info_elem.text()=="double") 
	insertChunkType(FG_INT); 
      //really! actually it just falls through to default, which happens to be 
      //FG_INT (set it explicitly here to avoid confusion, been there, done that...)
      else
	insertChunkType(FG_INT);
    }
    if (getFirstChildElement(info_elem, current_elem, ProtXmlDef::NodeCount, err_msg)) {
      insertChunkCount(info_elem.text().toInt());
    }

    
    //tmpDesc.index=QModelIndex(tmpIndex);
    //m_protinfo.append(tmpDesc);

    if (!nextChunk()) break; //target data does not like another chunk
    current_elem=current_elem.nextSiblingElement(ProtXmlDef::NodeChunk);
  };
  return true;
};
bool FgXmlProtocol::getFirstChildElement(QDomElement& element, 
                                         const QDomElement& dom_elem_prot, const QString& name,
					 QString& err_msg, const QString& err_msg_template) {
  element=dom_elem_prot.firstChildElement(name);
  if (element.isNull()) {
    err_msg.clear();
    if (!err_msg_template.isEmpty()) err_msg.append(err_msg_template);
    return false;
  }
  return true;
}

QByteArrayMatcher FgXmlProtocol::setSep (const QString& what) {
  QByteArrayMatcher matcher;
  if (what==ProtXmlDef::SepNewlineRep) 
    matcher.setPattern(QString('\n').toAscii());
  else if (what==ProtXmlDef::SepTabRep) 
    matcher.setPattern(QString('\t').toAscii());
  else if (what==ProtXmlDef::SepSpaceRep)
    matcher.setPattern(QString(' ').toAscii());
  else if (what==ProtXmlDef::SepFormfeedRep)
    matcher.setPattern(QString('\f').toAscii());
  else if (what==ProtXmlDef::SepCarriagereturnRep)
    matcher.setPattern(QString('\r').toAscii());
  else if (what==ProtXmlDef::SepVerticaltabRep)
    matcher.setPattern(QString('\v').toAscii());
  else matcher.setPattern(what.toAscii());
    
  return matcher;
}

FgXmlFlightstatusProtocol::FgXmlFlightstatusProtocol(VasFlightStatusModel const* current) : 
  FgXmlProtocol(),
  m_currentmodel(current)
  //m_tmpIndex(QModelIndex())
{};

bool FgXmlFlightstatusProtocol::insertChunkName(const QString& name) {
  m_tmpIndex=getMatchingModelIndex(name, m_currentmodel);
  if (m_tmpIndex!=QModelIndex()) return true;
  clearTmp();
  return false;
};
bool FgXmlFlightstatusProtocol::insertChunkType(fg_type FgType) {
  m_tmpDesc.FgType=FgType;
  return true;
};
bool FgXmlFlightstatusProtocol::insertChunkCount(int count) {
  m_tmpDesc.count=count;
  return true;
};
bool FgXmlFlightstatusProtocol::nextChunk() {
  //DEBUGP(LOGWARNING, m_tmpIndex.data(Qt::ToolTipRole) << "->" << m_currentmodel->rowCount(m_tmpIndex));
  //DEBUGP(LOGWARNING, m_tmpIndex.parent().row() << "->" << m_tmpIndex.row());
  if (m_currentmodel->rowCount(m_tmpIndex)>0) {
    if (m_tmpDesc.count<m_currentmodel->rowCount(m_tmpIndex)) {
      m_tmpIndex=m_currentmodel->index(m_tmpDesc.count,0,m_tmpIndex);
    }
  }
  m_tmpDesc.index=m_tmpIndex;
  m_protinfo.append(m_tmpDesc);
  clearTmp();
  return true;
};
void FgXmlFlightstatusProtocol::clearTmp() {
  m_tmpDesc.clear();
  m_tmpIndex=QModelIndex();
};
FgXmlTcasProtocol::FgXmlTcasProtocol(FGTcasModel const* current) :
  FgXmlProtocol(),
  m_currentmodel(current)
{};

bool FgXmlTcasProtocol::insertChunkName(const QString& name) {
  return false;
};
bool FgXmlTcasProtocol::insertChunkType(fg_type FgType) {
  return false;
};
bool FgXmlTcasProtocol::insertChunkCount(int count) {
  return false;
};
bool FgXmlTcasProtocol::nextChunk() {
  return false;
};
QModelIndex FgXmlFlightstatusProtocol::getMatchingModelIndex(const QString& value, const VasFlightStatusModel* currentmodel) {
  QModelIndexList tmp_list;
  tmp_list=currentmodel->match
    (currentmodel->index(0,0,QModelIndex()), // start from the root item
     Qt::ToolTipRole,         // by convention the strings are there
     value,                   // the (QString) we look for
     -1,                      // search all (discard later, whats the point of 
     // QModelIndexList anyhow? )
     Qt::MatchFlags(Qt::MatchFixedString | 
		    Qt::MatchCaseSensitive | 
		    Qt::MatchWrap |
		    Qt::MatchRecursive)
     );
  if (!tmp_list.isEmpty()) {
    /*  if (tmp_list.first().parent()!=QModelIndex()) {
     	}*/
    return tmp_list.first(); //for now ignore multiple matches (should not happen anyways *g*)
  } else {
    DEBUGP(LOGWARNING, value << " not found");
    return QModelIndex();
  }
  
};

QDebug& operator<<(QDebug& out, const FgChunkDesc& descr)
{ 
   out<<descr.toVerboseString(); 
   return out; 
};
