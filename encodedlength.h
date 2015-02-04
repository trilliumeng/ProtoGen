#ifndef ENCODEDLENGTH_H
#define ENCODEDLENGTH_H

#include <QString>

class EncodedLength
{
public:
    EncodedLength(void);

    //! Add successive length strings
    void addToLength(const QString & length, bool isString = false, bool isVariable = false, bool  isDependent = false, bool isDefault = false);

    //! Add a grouping of length strings
    void addToLength(const EncodedLength& rightLength, const QString& array = QString(), bool isVariable = false, bool isDependent = false);

    //! Add a grouping of length strings
    static void add(EncodedLength* leftLength, const EncodedLength& rightLength, const QString& array = QString(), bool isVariable = false, bool isDependent = false);

    //! Clear the encoded length
    void clear(void);

    //! Determine if there is any data here
    bool isEmpty(void);

    //! The minimum encoded length
    QString minEncodedLength;

    //! The maximum encoded length
    QString maxEncodedLength;

    //! The maximum encoded length of everything except default fields
    QString nonDefaultEncodedLength;

private:

    //! Create a length string like "4 + 3 + N3D*2" by adding successive length strings
    static void addToLengthString(QString & totalLength, const QString & length);

};

#endif // ENCODEDLENGTH_H
