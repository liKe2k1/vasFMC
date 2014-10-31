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

#ifndef FGACCESS_PROT_H
#define FGACCESS_PROT_H

#include <QByteArrayMatcher>
#include "xml.model.h"
#include "fsaccess_fgfs_base.h"

class ProtXmlDef {
public:
  static const QString NodePropertyList;
  static const QString NodeProtocolType;
  static const QString NodeProtocolIn;
  static const QString NodeProtocolOut;
  static const QString NodeLineSep;
  static const QString NodeVarSep;
  static const QString NodeBinary;
  static const QString NodeBinaryFooter;
  static const QString NodeChunk;
  static const QString NodeType;
  static const QString NodeProp;
  //not in fg generic
  static const QString NodeName;
  static const QString NodeCount;
  //static const QString NodeRadio;
  //unused by us
  static const QString NodeFormat;
  static const QString NodeOffset;
  static const QString NodeFactor;
  
  static const QString SepNewlineRep;
  static const QString SepTabRep;
  static const QString SepSpaceRep;
  static const QString SepFormfeedRep;
  static const QString SepCarriagereturnRep;
  static const QString SepVerticaltabRep;

  static const QString ProtFileSubDir;
};

class QDir;
class FgXmlProtocol : public XMLModel {
    Q_OBJECT
public:
    //!not sure if we should play along wrt base m_xml_dom
    virtual ~FgXmlProtocol();    
    //base considered sufficient
    //virtual void close();

    //! Loads the given file.
    //Mapping the protocol definition onto the target data structure.
    bool loadFromXMLFile(const QString& filenamestub, QString& err_msg);
    
    //! mangle with the protocol definition
    virtual bool parseDOM(QString& err_msg);
    virtual const int getSize() const=0;

    const QByteArrayMatcher& getVarSep() const { return m_var_separator; };
    const QByteArrayMatcher& getLineSep() const { return m_line_separator; };
protected:
    //!Construct and ensure ro for now, default to unix path\n
    //!to the $FGROOT
    FgXmlProtocol();
    QDir* m_fgroot_path;
    QByteArrayMatcher m_var_separator, m_line_separator;

    //! inform model that parsing of a new chunk has begun
    virtual bool nextChunk()=0;
    virtual bool insertChunkName(const QString& name)=0;
    virtual bool insertChunkType(const fg_type FgType=FG_INT)=0;
    virtual bool insertChunkCount(const int count=0)=0;

    //! checks and handles known representations
    static QByteArrayMatcher setSep (const QString&);

private:
    FgXmlProtocol(const FgXmlProtocol&);
    const FgXmlProtocol& operator = (const FgXmlProtocol&);

    //! return false on isNull() results
    static bool getFirstChildElement(QDomElement& element, const QDomElement& dom_elem_prot, const QString& name, QString& err_msg, const QString& err_msg_template="");

    
};
class VasFlightStatusModel;
class FgXmlFlightstatusProtocol : public FgXmlProtocol 
{
 public:
  FgXmlFlightstatusProtocol(const VasFlightStatusModel* current);
  const FgProt& getChunkInfo() const { return m_protinfo; };
  const int getSize() const { return m_protinfo.size(); };
 private:
  const VasFlightStatusModel* m_currentmodel;
  FgProt m_protinfo;
  QModelIndex m_tmpIndex;
  FgChunkDesc m_tmpDesc;
 
  bool insertChunkName(const QString& name);
  bool insertChunkType(fg_type FgType);
  bool insertChunkCount(int count);
  bool nextChunk();
  void clearTmp();
  //! dumbed down for our needs of matching from QAbstractItemModel::match (...)
  //probably a bit clumsy, but as it should only be run the few times a protocol file
  //is parsed...
  static QModelIndex getMatchingModelIndex(const QString& value, const VasFlightStatusModel* currentmodel);
};
class FGTcasModel;
class FgXmlTcasProtocol : public FgXmlProtocol
{
 public:
  FgXmlTcasProtocol(const FGTcasModel* current);
  const int getSize() const { return -1; };
 private:
  const FGTcasModel* m_currentmodel;
  
  bool insertChunkName(const QString& name);
  bool insertChunkType(fg_type FgType);
  bool insertChunkCount(int count);
  bool nextChunk();
};
#endif /* FGACCESS_PROT_H */
