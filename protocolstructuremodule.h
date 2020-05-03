#ifndef PROTOCOLSTRUCTUREMODULE_H
#define PROTOCOLSTRUCTUREMODULE_H

#include "protocolstructure.h"
#include "protocolfile.h"
#include "enumcreator.h"
#include "protocolbitfield.h"
#include <QString>
#include <QDomElement>

class ProtocolStructureModule : public ProtocolStructure
{
public:

    //! Construct the structure parsing object, with details about the overall protocol
    ProtocolStructureModule(ProtocolParser* parse, ProtocolSupport supported, const QString& protocolApi, const QString& protocolVersion);

    //! Parse a packet from the DOM
    void parse(void) Q_DECL_OVERRIDE;

    //! Reset our data contents
    void clear(void) Q_DECL_OVERRIDE;

    //! Destroy the protocol packet
    ~ProtocolStructureModule(void);

    //! Return the include directives needed for this encodable
    void getIncludeDirectives(QStringList& list) const Q_DECL_OVERRIDE;

    //! Return the include directives that go into source code for this encodable
    void getSourceIncludeDirectives(QStringList& list) const Q_DECL_OVERRIDE;

    //! Return the include directives needed for this encodable's init and verify functions
    void getInitAndVerifyIncludeDirectives(QStringList& list) const Q_DECL_OVERRIDE;

    //! Return the include directives needed for this encodable's map functions
    void getMapIncludeDirectives(QStringList& list) const Q_DECL_OVERRIDE;

    //! Return the include directives needed for this encodable's compare functions
    void getCompareIncludeDirectives(QStringList& list) const Q_DECL_OVERRIDE;

    //! Return the include directives needed for this encodable's print functions
    void getPrintIncludeDirectives(QStringList& list) const Q_DECL_OVERRIDE;

    //! Get the name of the header file that encompasses this structure definition
    QString getDefinitionFileName(void) const {return structfile->fileName();}

    //! Get the name of the header file that encompasses this structure interface functions
    QString getHeaderFileName(void) const {return header.fileName();}

    //! Get the name of the source file for this structure
    QString getSourceFileName(void) const {return source.fileName();}

    //! Get the path of the header file that encompasses this structure definition
    QString getDefinitionFilePath(void) const {return structfile->filePath();}

    //! Get the path of the header file that encompasses this structure interface functions
    QString getHeaderFilePath(void) const {return header.filePath();}

    //! Get the path of the source file for this structure
    QString getSourceFilePath(void) const {return source.filePath();}

    //! Get the name of the header file that encompasses this structure verify functions
    QString getVerifyHeaderFileName(void) const {return verifyheaderfile->fileName();}

    //! Get the name of the source file that encompasses this structure verify functions
    QString getVerifySourceFileName(void) const {return verifysourcefile->fileName();}

    //! Get the path of the header file that encompasses this structure verify functions
    QString getVerifyHeaderFilePath(void) const {return verifyheaderfile->filePath();}

    //! Get the path of the source file that encompasses this structure verify functions
    QString getVerifySourceFilePath(void) const {return verifysourcefile->filePath();}

    //! Get the name of the header file that encompasses this structure comparison functions
    QString getCompareHeaderFileName(void) const {return compareHeader.fileName();}

    //! Get the name of the source file that encompasses this structure comparison functions
    QString getCompareSourceFileName(void) const {return compareSource.fileName();}

    //! Get the path of the header file that encompasses this structure comparison functions
    QString getCompareHeaderFilePath(void) const {return compareHeader.filePath();}

    //! Get the path of the source file that encompasses this structure comparison functions
    QString getCompareSourceFilePath(void) const {return compareSource.filePath();}

    //! Get the name of the header file that encompasses this structure comparison functions
    QString getPrintHeaderFileName(void) const {return printHeader.fileName();}

    //! Get the name of the source file that encompasses this structure comparison functions
    QString getPrintSourceFileName(void) const {return printSource.fileName();}

    //! Get the path of the header file that encompasses this structure comparison functions
    QString getPrintHeaderFilePath(void) const {return printHeader.filePath();}

    //! Get the path of the source file that encompasses this structure comparison functions
    QString getPrintSourceFilePath(void) const {return printSource.filePath();}

    //! Get the name of the header file that encompasses this structure map functions
    QString getMapHeaderFileName(void) const {return mapHeader.fileName();}

    //! Get the path of the header file that encompasses this structure map functions
    QString getMapHeaderFilePath(void) const {return mapHeader.filePath();}

    //! Get the name of the source file that encompasses this structure map functions
    QString getMapSourceFileName(void) const {return mapSource.fileName();}

    //! Get the path of the source file that encompasses this structure map functions
    QString getMapSourceFilePath(void) const {return mapSource.filePath();}

protected:

    //! Setup the files, which accounts for all the ways the files can be organized for this structure.
    void setupFiles(QString moduleName,
                    QString defheadermodulename,
                    QString verifymodulename,
                    QString comparemodulename,
                    QString printmodulename,
                    QString mapmodulename,
                    bool forceStructureDeclaration = true, bool outputUtilities = true);

    //! Issue warnings for the structure module.
    void issueWarnings(const QDomNamedNodeMap& map);

    //! Write data to the source and header files to encode and decode this structure and all its children
    void createStructureFunctions(void);

    //! Create the functions that encode/decode sub stuctures.
    void createSubStructureFunctions(void);

    //! Write data to the source and header files to encode and decode this structure but not its children
    void createTopLevelStructureFunctions(bool suppressEncodeAndDecode = true);

    //! Get the text used to extract text for text read functions
    static QString getExtractTextFunction(void);

    ProtocolSourceFile source;          //!< The source file (*.c)
    ProtocolHeaderFile header;          //!< The header file (*.h)
    ProtocolHeaderFile defheader;       //!< The header file name for the structure definition
    ProtocolSourceFile verifySource;    //!< The source file for verify code (*.c)
    ProtocolHeaderFile verifyHeader;    //!< The header file for verify code (*.h)
    ProtocolSourceFile compareSource;   //!< The source file for comparison code (*.cpp)
    ProtocolHeaderFile compareHeader;   //!< The header file for comparison code (*.h)
    ProtocolSourceFile printSource;     //!< The source file for print code (*.cpp)
    ProtocolHeaderFile printHeader;     //!< The header file for print code (*.h)
    ProtocolSourceFile mapSource;       //!< The source file for map code (*.cpp)
    ProtocolHeaderFile mapHeader;       //!< The header file for map code (*.h)
    ProtocolHeaderFile* structfile;     //!< Reference to the file that holds the structure definition
    ProtocolHeaderFile* verifyheaderfile;   //!< Reference to the file that holds the verify prototypes
    ProtocolSourceFile* verifysourcefile;   //!< Reference to the file that holds the verify source code
    QString api;                    //!< The protocol API enumeration
    QString version;                //!< The version string
};

#endif // PROTOCOLSTRUCTUREMODULE_H
