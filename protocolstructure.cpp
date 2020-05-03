#include "protocolstructure.h"
#include "protocolstructuremodule.h"
#include "protocolparser.h"
#include "protocolfield.h"
#include <QDomNodeList>
#include <QStringList>
#include <iostream>

/*!
 * Construct a protocol structure
 * \param parse points to the global protocol parser that owns everything
 * \param Parent is the hierarchical name of the object that owns this object.
 * \param support are the protocol support details
 */
ProtocolStructure::ProtocolStructure(ProtocolParser* parse, QString Parent, ProtocolSupport supported) :
    Encodable(parse, Parent, supported),
    numbitfieldgroupbytes(0),
    bitfields(false),
    usestempencodebitfields(false),
    usestempencodelongbitfields(false),
    usestempdecodebitfields(false),
    usestempdecodelongbitfields(false),
    needsEncodeIterator(false),
    needsDecodeIterator(false),
    needsInitIterator(false),
    needsVerifyIterator(false),
    needs2ndEncodeIterator(false),
    needs2ndDecodeIterator(false),
    needs2ndInitIterator(false),
    needs2ndVerifyIterator(false),
    defaults(false),
    hidden(false),
    hasinit(false),
    hasverify(false),
    encode(true),
    decode(true),
    compare(false),
    print(false),
    mapEncode(false),
    redefines(nullptr)
{
    // List of attributes understood by ProtocolStructure
    attriblist << "name" << "title" << "array" << "variableArray" << "array2d" << "variable2dArray" << "dependsOn" << "comment" << "hidden" << "limitOnEncode";

}


/*!
 * Reset all data to defaults
 */
void ProtocolStructure::clear(void)
{
    Encodable::clear();

    // Delete any encodable objects in our list
    for(int i = 0; i < encodables.length(); i++)
    {
        if(encodables[i])
        {
            delete encodables[i];
            encodables[i] = NULL;
        }
    }

    // Empty the list itself
    encodables.clear();

    // Objects in this list are owned by others, we just clear it, don't delete the objects
    enumList.clear();

    // The rest of the metadata
    numbitfieldgroupbytes = 0;
    bitfields = false;
    usestempencodebitfields = false;
    usestempencodelongbitfields = false;
    usestempdecodebitfields = false;
    usestempdecodelongbitfields = false;
    needsEncodeIterator = false;
    needsDecodeIterator = false;
    needsInitIterator = false;
    needsVerifyIterator = false;
    needs2ndEncodeIterator = false;
    needs2ndDecodeIterator = false;
    needs2ndInitIterator = false;
    needs2ndVerifyIterator = false;
    defaults = false;
    hidden = false;
    hasinit = false;
    hasverify = false;
    encode = decode = true;
    print = compare = mapEncode = false;
    structName.clear();
    redefines = nullptr;

}// ProtocolStructure::clear


/*!
 * Parse the DOM data for this structure
 */
void ProtocolStructure::parse(void)
{
    QDomNamedNodeMap map = e.attributes();

    // All the attribute we care about
    name = ProtocolParser::getAttribute("name", map);
    title = ProtocolParser::getAttribute("title", map);
    array = ProtocolParser::getAttribute("array", map);
    variableArray = ProtocolParser::getAttribute("variableArray", map);
    dependsOn = ProtocolParser::getAttribute("dependsOn", map);
    dependsOnValue = ProtocolParser::getAttribute("dependsOnValue", map);
    dependsOnCompare = ProtocolParser::getAttribute("dependsOnCompare", map);
    comment = ProtocolParser::reflowComment(ProtocolParser::getAttribute("comment", map));
    hidden = ProtocolParser::isFieldSet("hidden", map);

    if(name.isEmpty())
        name = "_unknown";

    if(title.isEmpty())
        title = name;

    // This will propagate to any of the children we create
    if(ProtocolParser::isFieldSet("limitOnEncode", map))
        support.limitonencode = true;
    else if(ProtocolParser::isFieldClear("limitOnEncode", map))
        support.limitonencode = false;

    testAndWarnAttributes(map, attriblist);

    // for now the typename is derived from the name
    structName = typeName = support.prefix + name + "_t";

    // We can't have a variable array length without an array
    if(array.isEmpty() && !variableArray.isEmpty())
    {
        emitWarning("must specify array length to specify variable array length");
        variableArray.clear();
    }

    if(!dependsOn.isEmpty() && !variableArray.isEmpty())
    {
        emitWarning("variable length arrays cannot also use dependsOn");
        dependsOn.clear();
    }

    if(!dependsOnValue.isEmpty() && dependsOn.isEmpty())
    {
        emitWarning("dependsOnValue does not make sense unless dependsOn is defined");
        dependsOnValue.clear();
    }

    if(!dependsOnCompare.isEmpty() && dependsOnValue.isEmpty())
    {
        emitWarning("dependsOnCompare does not make sense unless dependsOnValue is defined");
        dependsOnCompare.clear();
    }
    else if(dependsOnCompare.isEmpty() && !dependsOnValue.isEmpty())
    {
        // This is not a warning, it is expected
        dependsOnCompare = "==";
    }

    // Check to make sure we did not step on any keywords
    checkAgainstKeywords();

    // Get any enumerations
    parseEnumerations(e);

    // At this point a structure cannot be default, null, or reserved.
    parseChildren(e);

    // Sum the length of all the children
    EncodedLength length;
    for(int i = 0; i < encodables.length(); i++)
    {
        length.addToLength(encodables.at(i)->encodedLength);
    }

    // Account for array, variable array, and depends on
    encodedLength.clear();
    encodedLength.addToLength(length, array, !variableArray.isEmpty(), !dependsOn.isEmpty());

}// ProtocolStructure::parse


/*!
 * Return the string used to declare this encodable as part of a structure.
 * This includes the spacing, typename, name, semicolon, comment, and linefeed
 * \return the declaration string for this encodable
 */
QString ProtocolStructure::getDeclaration(void) const
{
    QString output = TAB_IN + "" + typeName + " " + name;

    if(array.isEmpty())
        output += ";";
    else if(array2d.isEmpty())
        output += "[" + array + "];";
    else
        output += "[" + array + "][" + array2d + "]";

    if(!comment.isEmpty())
        output += " //!< " + comment;

    output += "\n";

    return output;

}// ProtocolStructure::getDeclaration


/*!
 * Return the string that is used to encode *this* structure
 * \param isBigEndian should be true for big endian encoding, ignored
 * \param bitcount points to the running count of bits in a bitfields and should persist between calls, ignored
 * \param isStructureMember is true if this encodable is accessed by structure pointer
 * \return the string to add to the source to encode this structure
 */
QString ProtocolStructure::getEncodeString(bool isBigEndian, int* bitcount, bool isStructureMember) const
{
    Q_UNUSED(isBigEndian);
    Q_UNUSED(bitcount);

    QString output;
    QString access = getEncodeFieldAccess(isStructureMember);
    QString spacing = TAB_IN;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    if(!dependsOn.isEmpty())
    {
        output += spacing + "if(" + getEncodeFieldAccess(isStructureMember, dependsOn);

        if(!dependsOnValue.isEmpty())
            output += " " + dependsOnCompare + " " + dependsOnValue;

        output += ")\n" + spacing + "{\n";
        spacing += TAB_IN;
    }

    // Array handling
    output += getEncodeArrayIterationCode(spacing, isStructureMember);

    // Spacing for arrays
    if(isArray())
    {
        spacing += TAB_IN;
        if(is2dArray())
            spacing += TAB_IN;
    }

    // The actual encode function
    if(support.language == ProtocolSupport::c_language)
        output += spacing + "encode" + typeName + "(_pg_data, &_pg_byteindex, " + access + ");\n";
    else
        output += spacing + access + ".encode(_pg_data, &_pg_byteindex);\n";

    // Close the depends on block
    if(!dependsOn.isEmpty())
        output += TAB_IN + "}\n";

    return output;

}// ProtocolStructure::getEncodeString


/*!
 * Return the string that is used to decode this structure
 * \param isBigEndian should be true for big endian encoding.
 * \param bitcount points to the running count of bits in a bitfields and should persist between calls
 * \param isStructureMember is true if this encodable is accessed by structure pointer
 * \return the string to add to the source to decode this structure
 */
QString ProtocolStructure::getDecodeString(bool isBigEndian, int* bitcount, bool isStructureMember, bool defaultEnabled) const
{
    Q_UNUSED(isBigEndian);
    Q_UNUSED(bitcount);
    Q_UNUSED(defaultEnabled);

    QString output;
    QString access = getEncodeFieldAccess(isStructureMember);
    QString spacing = TAB_IN;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    if(!dependsOn.isEmpty())
    {
        output += spacing + "if(" + getDecodeFieldAccess(isStructureMember, dependsOn);

        if(!dependsOnValue.isEmpty())
            output += " " + dependsOnCompare + " " + dependsOnValue;

        output += ")\n" + spacing + "{\n";
        spacing += TAB_IN;
    }

    // Array handling
    output += getDecodeArrayIterationCode(spacing, isStructureMember);

    // Spacing for arrays
    if(isArray())
    {
        spacing += TAB_IN;
        if(is2dArray())
            spacing += TAB_IN;
    }

    if(support.language == ProtocolSupport::c_language)
    {
        output += spacing + "if(decode" + typeName + "(_pg_data, &_pg_byteindex, " + access + ") == 0)\n";
        output += spacing + TAB_IN + "return 0;\n";
    }
    else
    {
        output += spacing + "if(" + access + ".decode(_pg_data, &_pg_byteindex) == false)\n";
        output += spacing + TAB_IN + "return false;\n";
    }

    if(!dependsOn.isEmpty())
        output += TAB_IN + "}\n";

    return output;

}// ProtocolStructure::getDecodeString


/*!
 * Get the code which verifies this structure
 * \return the code to put in the source file
 */
QString ProtocolStructure::getVerifyString(void) const
{
    QString output;
    QString access = name;
    QString spacing = TAB_IN;

    if(!hasverify)
        return output;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    // Do not call getDecodeArrayIterationCode() because we explicity don't handle variable length arrays here
    if(isArray())
    {
        output += spacing + "for(_pg_i = 0; _pg_i < " + array + "; _pg_i++)\n";
        spacing += TAB_IN;

        if(is2dArray())
        {
            output += spacing + "for(_pg_j = 0; _pg_j < " + array2d + "; _pg_j++)\n";
            spacing += TAB_IN;
        }
    }

    if(support.language == ProtocolSupport::c_language)
    {
        output += spacing + "if(verify" + typeName + "(" + getDecodeFieldAccess(true) + ") == 0)\n";
        output += spacing + TAB_IN + "_pg_good = 0;\n";
    }
    else
    {
        output += spacing + "if(" + getDecodeFieldAccess(true) + ".verify() == false)\n";
        output += spacing + TAB_IN + "_pg_good = false;\n";
    }

    return output;

}// ProtocolStructure::getVerifyString


/*!
 * Get the code which sets this structure member to initial values
 * \return the code to put in the source file
 */
QString ProtocolStructure::getSetInitialValueString(bool isStructureMember) const
{
    QString output;
    QString spacing = TAB_IN;

    // We only need this function if we are C language, C++ classes initialize themselves
    if((!hasinit) || (support.language != ProtocolSupport::c_language))
        return output;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    // Do not call getDecodeArrayIterationCode() because we explicity don't handle variable length arrays here
    if(isArray())
    {
        output += spacing + "for(_pg_i = 0; _pg_i < " + array + "; _pg_i++)\n";
        spacing += TAB_IN;

        if(is2dArray())
        {
            output += spacing + "for(_pg_j = 0; _pg_j < " + array2d + "; _pg_j++)\n";
            spacing += TAB_IN;
        }
    }

    output += spacing + "init" + typeName + "(" + getDecodeFieldAccess(isStructureMember) + ");\n";

    return output;

}// ProtocolStructure::getSetInitialValueString


//! Return the strings that #define initial and variable values
QString ProtocolStructure::getInitialAndVerifyDefines(bool includeComment) const
{
    QString output;

    for(int i = 0; i < encodables.count(); i++)
    {
        // Children's outputs do not have comments, just the top level stuff
        output += encodables.at(i)->getInitialAndVerifyDefines(false);
    }

    // I don't want to output this comment if there are no values being commented,
    // that's why I insert the comment after the #defines are output
    if(!output.isEmpty() && includeComment)
    {
        output.insert(0, "// Initial and verify values for " + name + "\n");
    }

    return output;

}// ProtocolStructure::getInitialAndVerifyDefines


/*!
 * Get the string used for comparing this field.
 * \return the function string, which may be empty
 */
QString ProtocolStructure::getComparisonString(void) const
{
    QString output;
    QString access1, access2;

    // We must have parameters that we decode to do a comparison
    if(!compare || (getNumberOfDecodeParameters() == 0))
        return output;

    QString spacing = TAB_IN;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    /// TODO: obey variable array length limits?

    if(support.language == ProtocolSupport::c_language)
    {
        // The dereference of the array gets us back to the object, but we need the pointer
        access1 = "&_pg_user1->" + name;
        access2 = "&_pg_user2->" + name;
    }
    else
    {
        access2 = "&_pg_user->" + name;
    }

    if(isArray())
    {
        output += spacing + "for(_pg_i = 0; _pg_i < " + array + "; _pg_i++)\n";
        spacing += TAB_IN;

        access1 += "[_pg_i]";
        access2 += "[_pg_i]";

        if(is2dArray())
        {
            access1 += "[_pg_j]";
            access2 += "[_pg_j]";
            output += spacing + "for(_pg_j = 0; _pg_j < " + array2d + "; _pg_j++)\n";
            spacing += TAB_IN;

        }// if 2D array of structures

    }// if array of structures

    if(support.language == ProtocolSupport::c_language)
        output += spacing + "_pg_report += compare" + typeName + "(_pg_prename + \":" + name + "\"";
    else
        output += spacing + "_pg_report += " + typeName + "::compare(_pg_prename + \":" + name + "\"";

    if(isArray())
        output += " + \"[\" + QString::number(_pg_i) + \"]\"";

    if(is2dArray())
        output += " + \"[\" + QString::number(_pg_j) + \"]\"";

    if(support.language == ProtocolSupport::c_language)
        output += ", " + access1 + ", " + access2 + ");\n";
    else
        output += ", " + access2 + ");\n";

    return output;

}// ProtocolStructure::getComparisonString


/*!
 * Get the string used for printing this field as text.
 * \return the print string, which may be empty
 */
QString ProtocolStructure::getTextPrintString(void) const
{
    QString output;
    QString access;
    QString spacing = TAB_IN;

    // We must parameters that we decode to do a print out
    if(!print || (getNumberOfDecodeParameters() == 0))
        return output;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    output += getEncodeArrayIterationCode(spacing, true);
    if(isArray())
    {
        spacing += TAB_IN;
        if(is2dArray())
            spacing += TAB_IN;
    }

    if(support.language == ProtocolSupport::c_language)
        output += spacing + "_pg_report += textPrint" + typeName + "(_pg_prename + \":" + name + "\"";
    else
        output += spacing + "_pg_report += " + getEncodeFieldAccess(true) + ".textPrint(_pg_prename + \":" + name + "\"";

    if(isArray())
        output += " + \"[\" + QString::number(_pg_i) + \"]\"";

    if(is2dArray())
        output += " + \"[\" + QString::number(_pg_j) + \"]\"";

    if(support.language == ProtocolSupport::c_language)
        output += ", " + getEncodeFieldAccess(true);

    output += ");\n";

    return output;

}// ProtocolStructure::getTextPrintString


/*!
 * Get the string used for reading this field from text.
 * \return the read string, which may be empty
 */
QString ProtocolStructure::getTextReadString(void) const
{
    QString output;
    QString access;
    QString spacing = TAB_IN;

    // We must parameters that we decode to do a print out
    if(!print || (getNumberOfDecodeParameters() == 0))
        return output;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    output += getEncodeArrayIterationCode(spacing, true);
    if(isArray())
    {
        spacing += TAB_IN;
        if(is2dArray())
            spacing += TAB_IN;
    }

    if(support.language == ProtocolSupport::c_language)
        output += spacing + "_pg_fieldcount += textRead" + typeName + "(_pg_prename + \":" + name + "\"";
    else
        output += spacing + "_pg_fieldcount += " + getEncodeFieldAccess(true) + ".textRead(_pg_prename + \":" + name + "\"";

    if(isArray())
        output += " + \"[\" + QString::number(_pg_i) + \"]\"";

    if(is2dArray())
        output += " + \"[\" + QString::number(_pg_j) + \"]\"";

    output += ", _pg_source";

    if(support.language == ProtocolSupport::c_language)
        output += ", " + getEncodeFieldAccess(true);

    output += ");\n";

    return output;

}// ProtocolStructure::getTextReadString


/*!
 * Return the string used for encoding this field to a map
 * \return the encode string, which may be empty
 */
QString ProtocolStructure::getMapEncodeString(void) const
{
    QString output;
    QString spacing = TAB_IN;

    // We must parameters that we decode to do a print out
    if(!mapEncode || (getNumberOfDecodeParameters() == 0))
        return output;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    QString key = "\":" + name + "\"";

    output += getEncodeArrayIterationCode(spacing, true);
    if(isArray())
    {
        spacing += TAB_IN;
        key += " + \"[\" + QString::number(_pg_i) + \"]\"";

        if(is2dArray())
        {
            spacing += TAB_IN;
            key += " + \"[\" + QString::number(_pg_j) + \"]\"";
        }
    }

    if(support.language == ProtocolSupport::c_language)
        output += spacing + "mapEncode" + typeName + "(_pg_prename + " + key + ", _pg_map, " + getEncodeFieldAccess(true);
    else
        output += spacing + getEncodeFieldAccess(true) + ".mapEncode(_pg_prename + " + key + ", _pg_map";

    output += ");\n";

    return output;

}// ProtocolStructure::getMapEncodeString


/*!
 * Get the string used for decoding this field from a map.
 * \return the map decode string, which may be empty
 */
QString ProtocolStructure::getMapDecodeString(void) const
{
    QString output;
    QString spacing = TAB_IN;

    // We must parameters that we decode to do a print out
    if(!mapEncode || (getNumberOfDecodeParameters() == 0))
        return output;

    if(!comment.isEmpty())
        output += spacing + "// " + comment + "\n";

    QString key = "\":" + name + "\"";

    output += getDecodeArrayIterationCode(spacing, true);
    if(isArray())
    {
        spacing += TAB_IN;
        key += " + \"[\" + QString::number(_pg_i) + \"]\"";

        if(is2dArray())
        {
            spacing += TAB_IN;
            key += " + \"[\" + QString::number(_pg_j) + \"]\"";
        }
    }

    if(support.language == ProtocolSupport::c_language)
        output += spacing + "mapDecode" + typeName + "(_pg_prename + " + key + ", _pg_map, " + getDecodeFieldAccess(true);
    else
        output += spacing + getDecodeFieldAccess(true) + ".mapDecode(_pg_prename + " + key + ", _pg_map";

    output += ");\n";

    return output;

}// ProtocolStructure::getMapDecodeString


/*!
 * Parse and output all enumerations which are direct children of a DomNode
 * \param node is parent node
 */
void ProtocolStructure::parseEnumerations(const QDomNode& node)
{
    // Build the top level enumerations
    QList<QDomNode>list = ProtocolParser::childElementsByTagName(node, "Enum");

    for(int i = 0; i < list.size(); i++)
    {
        enumList.append(parser->parseEnumeration(getHierarchicalName(), list.at(i).toElement()));

    }// for all my enum tags

}// ProtocolStructure::parseEnumerations


/*!
 * Parse the DOM data for the children of this structure
 * \param field is the DOM data for this structure
 */
void ProtocolStructure::parseChildren(const QDomElement& field)
{
    Encodable* prevEncodable = NULL;

    // All the direct children, which may themselves be structures or primitive fields
    QDomNodeList children = field.childNodes();

    // Make encodables out of them, and add to our list
    for (int i = 0; i < children.size(); i++)
    {
        Encodable* encodable = generateEncodable(parser, getHierarchicalName(), support, children.at(i).toElement());
        if(encodable != NULL)
        {            
            // If the encodable is null, then none of the metadata
            // matters, its not going to end up in the output
            if(!encodable->isNotEncoded())
            {
                ProtocolField* field = dynamic_cast<ProtocolField*>(encodable);

                if(field != NULL)
                {
                    // Let the new encodable know about the preceding one
                    field->setPreviousEncodable(prevEncodable);

                    if(field->overridesPreviousEncodable())
                    {
                        int prev;
                        for(prev = 0; prev < encodables.size(); prev++)
                        {
                            if(field->getOverriddenTypeData(dynamic_cast<ProtocolField*>(encodables.at(prev))))
                                break;
                        }

                        if(prev >= encodables.size())
                        {
                            field->emitWarning("override failed, could not find previous field");
                            delete encodable;
                            continue;
                        }

                    }// if encodable does override operation

                    // Track our metadata
                    if(field->usesBitfields())
                    {
                        field->getBitfieldGroupNumBytes(&numbitfieldgroupbytes);
                        bitfields = true;

                        if(field->usesEncodeTempBitfield())
                            usestempencodebitfields = true;

                        if(field->usesEncodeTempLongBitfield())
                            usestempencodelongbitfields = true;

                        if(field->usesDecodeTempBitfield())
                            usestempdecodebitfields = true;

                        if(field->usesDecodeTempLongBitfield())
                            usestempdecodelongbitfields = true;
                    }

                    if(field->usesEncodeIterator())
                        needsEncodeIterator = true;

                    if(field->usesDecodeIterator())
                        needsDecodeIterator = true;

                    if(field->usesInitIterator())
                        needsInitIterator = true;

                    if(field->usesVerifyIterator())
                        needsVerifyIterator = true;

                    if(field->uses2ndEncodeIterator())
                        needs2ndEncodeIterator = true;

                    if(field->uses2ndDecodeIterator())
                        needs2ndDecodeIterator = true;

                    if(field->uses2ndInitIterator())
                        needs2ndInitIterator = true;

                    if(field->uses2ndVerifyIterator())
                        needs2ndVerifyIterator = true;

                    if(field->usesDefaults())
                        defaults = true;
                    else if(defaults && field->invalidatesPreviousDefault())
                    {
                        // Check defaults. If a previous field was defaulted but this
                        // field is not, then we have to terminate the previous default,
                        // only the last fields can have defaults
                        for(int j = 0; j < encodables.size(); j++)
                        {
                            if(encodables[j]->usesDefaults())
                            {
                                encodables[j]->clearDefaults();
                                encodables[j]->emitWarning("default value ignored, field is followed by non-default");
                            }
                        }

                        defaults = false;
                    }

                }// if this encodable is a field
                else
                {
                    // Structures can be arrays as well.
                    if(encodable->isArray())
                    {
                        needsDecodeIterator = needsEncodeIterator = true;
                        needsInitIterator = encodable->hasInit();
                        needsVerifyIterator = encodable->hasVerify();
                    }

                    if(encodable->is2dArray())
                    {
                        needs2ndDecodeIterator = needs2ndEncodeIterator = true;
                        needs2ndInitIterator = encodable->hasInit();
                        needs2ndVerifyIterator = encodable->hasVerify();
                    }

                }// else if this encodable is not a field


                // Handle the variable array case. We have to make sure that the referenced variable exists
                if(!encodable->variableArray.isEmpty())
                {
                    int prev;
                    for(prev = 0; prev < encodables.size(); prev++)
                    {
                       Encodable* previous = encodables.at(prev);
                       if(previous == NULL)
                           continue;

                       // It has to be a named variable that is both in memory and encoded
                       if(previous->isNotEncoded() || previous->isNotInMemory())
                           continue;

                       // It has to be a primitive, and it cannot be an array itself
                       if(!previous->isPrimitive() && !previous->isArray())
                           continue;

                       // Now check to see if this previously defined encodable is our variable
                       if(previous->name == encodable->variableArray)
                           break;
                    }

                    if(prev >= encodables.size())
                    {
                       encodable->emitWarning("variable length array ignored, failed to find length variable");
                       encodable->variableArray.clear();
                    }

                }// if this is a variable length array

                // Handle the variable 2d array case. We have to make sure that the referenced variable exists
                if(!encodable->variable2dArray.isEmpty())
                {
                    int prev;
                    for(prev = 0; prev < encodables.size(); prev++)
                    {
                       Encodable* previous = encodables.at(prev);
                       if(previous == NULL)
                           continue;

                       // It has to be a named variable that is both in memory and encoded
                       if(previous->isNotEncoded() || previous->isNotInMemory())
                           continue;

                       // It has to be a primitive, and it cannot be an array itself
                       if(!previous->isPrimitive() && !previous->isArray())
                           continue;

                       // Now check to see if this previously defined encodable is our variable
                       if(previous->name == encodable->variable2dArray)
                           break;
                    }

                    if(prev >= encodables.size())
                    {
                       encodable->emitWarning("variable 2d length array ignored, failed to find 2d length variable");
                       encodable->variable2dArray.clear();
                    }

                }// if this is a 2d variable length array

                // Handle the dependsOn case. We have to make sure that the referenced variable exists
                if(!encodable->dependsOn.isEmpty())
                {
                    if(encodable->isBitfield())
                    {
                        encodable->emitWarning("bitfields cannot use dependsOn");
                        encodable->dependsOn.clear();
                    }
                    else
                    {
                        int prev;
                        for(prev = 0; prev < encodables.size(); prev++)
                        {
                           Encodable* previous = encodables.at(prev);
                           if(previous == NULL)
                               continue;

                           // It has to be a named variable that is both in memory and encoded
                           if(previous->isNotEncoded() || previous->isNotInMemory())
                               continue;

                           // It has to be a primitive, and it cannot be an array itself
                           if(!previous->isPrimitive() && !previous->isArray())
                               continue;

                           // Now check to see if this previously defined encodable is our variable
                           if(previous->name == encodable->dependsOn)
                               break;
                        }

                        if(prev >= encodables.size())
                        {
                           encodable->emitWarning("dependsOn ignored, failed to find dependsOn variable");
                           encodable->dependsOn.clear();
                           encodable->dependsOnValue.clear();
                           encodable->dependsOnCompare.clear();
                        }
                    }

                }// if this field depends on another

                // If our child has init or verify capabilities
                // we have to inherit those as well
                if(encodable->hasInit())
                    hasinit = true;

                if(encodable->hasVerify())
                    hasverify = true;

                // We can only determine bitfield group numBytes after
                // we have given the encodable a look at its preceding members
                if(encodable->isPrimitive() && encodable->usesBitfields())
                    encodable->getBitfieldGroupNumBytes(&numbitfieldgroupbytes);

                // Remember who our previous encodable was
                prevEncodable = encodable;

            }// if is encoded

            // Remember this encodable
            encodables.push_back(encodable);

        }// if the child was an encodable

    }// for all children

}// ProtocolStructure::parseChildren


//! Set the compare flag for this structure and all children structure
void ProtocolStructure::setCompare(bool enable)
{
    for(int i = 0; i < encodables.length(); i++)
    {
        // Is this encodable a structure?
        ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

        if(structure == nullptr)
            continue;

        structure->setCompare(enable);

    }// for all children

    compare = enable;
}


//! Set the print flag for this structure and all children structure
void ProtocolStructure::setPrint(bool enable)
{
    for(int i = 0; i < encodables.length(); i++)
    {
        // Is this encodable a structure?
        ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

        if(structure == nullptr)
            continue;

        structure->setPrint(enable);

    }// for all children

    print = enable;
}


//! Set the mapEncode flag for this structure and all children structure
void ProtocolStructure::setMapEncode(bool enable)
{
    for(int i = 0; i < encodables.length(); i++)
    {
        // Is this encodable a structure?
        ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

        if(structure == nullptr)
            continue;

        structure->setMapEncode(enable);

    }// for all children

    mapEncode = enable;
}


//! Get the maximum number of temporary bytes needed for a bitfield group of our children
void ProtocolStructure::getBitfieldGroupNumBytes(int* num) const
{
    if(numbitfieldgroupbytes > (*num))
        (*num) = numbitfieldgroupbytes;
}


/*!
 * Get the number of encoded fields. This is not the same as the length of the
 * encodables list, because some or all of them could be isNotEncoded()
 * \return the number of encodables in the packet.
 */
int ProtocolStructure::getNumberOfEncodes(void) const
{
    int numEncodes = 0;
    for(int i = 0; i < encodables.length(); i++)
    {
        // If its not encoded at all, then it does not count
        if(encodables.at(i)->isNotEncoded())
            continue;
        else
            numEncodes++;
    }

    return numEncodes;
}


/*!
 * Get the number of encoded fields. This is not the same as the length of the
 * encodables list, because some or all of them could be isNotEncoded() or isConstant()
 * \return the number of encodables in the packet set by the user.
 */
int ProtocolStructure::getNumberOfEncodeParameters(void) const
{
    int numEncodes = 0;
    for(int i = 0; i < encodables.length(); i++)
    {
        // If its not encoded at all, or its encoded as a constant, then it does not count
        if(encodables.at(i)->isNotEncoded() || encodables.at(i)->isConstant())
            continue;
        else
            numEncodes++;
    }

    return numEncodes;
}


/*!
 * Get the number of decoded fields whose value is written into memory. This is
 * not the same as the length of the encodables list, because some or all of
 * them could be isNotEncoded(), isNotInMemory()
 * \return the number of values decoded from the packet.
 */
int ProtocolStructure::getNumberOfDecodeParameters(void) const
{
    // Its is possible that all of our decodes are in fact not set by the user
    int numDecodes = 0;
    for(int i = 0; i < encodables.length(); i++)
    {
        // If its not encoded at all, or its not in memory (hence not decoded) it does not count
        if(encodables.at(i)->isNotEncoded() || encodables.at(i)->isNotInMemory())
            continue;
        else
            numDecodes++;
    }

    return numDecodes;
}


/*!
 * Get the number of fields in memory. This can be different than the number of decodes or encodes.
 * \return the number of user set encodables that appear in the packet.
 */
int ProtocolStructure::getNumberInMemory(void) const
{
    int num = 0;
    for(int i = 0; i < encodables.length(); i++)
    {
        if(encodables.at(i)->isNotInMemory())
            continue;
        else
            num++;
    }

    return num;
}


/*!
 * Append the include directives needed for this encodable. Mostly this is empty,
 * but for external structures or enumerations we need to bring in the include file
 * \param list is appended with any directives this encodable requires.
 */
void ProtocolStructure::getIncludeDirectives(QStringList& list) const
{
    // Includes that our encodable members may need
    for(int i = 0; i < encodables.length(); i++)
        encodables.at(i)->getIncludeDirectives(list);

    // Array sizes could be enumerations that need an include directive
    if(!array.isEmpty())
    {
        QString include = parser->lookUpIncludeName(array);
        if(!include.isEmpty())
            list.append(include);
    }

    // Array sizes could be enumerations that need an include directive
    if(!array2d.isEmpty())
    {
        QString include = parser->lookUpIncludeName(array2d);
        if(!include.isEmpty())
            list.append(include);
    }

    list.removeDuplicates();

}// ProtocolStructure::getIncludeDirectives


/*!
 * Append the include directives in source code for this encodable. Mostly this is empty,
 * but code encodables may have source code includes.
 * \param list is appended with any directives this encodable requires.
 */
void ProtocolStructure::getSourceIncludeDirectives(QStringList& list) const
{
    // Includes that our encodable members may need
    for(int i = 0; i < encodables.length(); i++)
        encodables.at(i)->getSourceIncludeDirectives(list);

    list.removeDuplicates();
}


/*!
 * Return the include directives needed for this encodable's init and verify functions
 * \param list is appended with any directives this encodable requires.
 */
void ProtocolStructure::getInitAndVerifyIncludeDirectives(QStringList& list) const
{
    // Includes that our encodable members may need
    for(int i = 0; i < encodables.length(); i++)
        encodables.at(i)->getInitAndVerifyIncludeDirectives(list);

    list.removeDuplicates();
}


/*!
 * Return the include directives needed for this encodable's map functions
 * \param list is appended with any directives this encodable requires.
 */
void ProtocolStructure::getMapIncludeDirectives(QStringList& list) const
{
    // Includes that our encodable members may need
    for(int i = 0; i < encodables.length(); i++)
        encodables.at(i)->getMapIncludeDirectives(list);

    list.removeDuplicates();
}


/*!
 * Return the include directives needed for this encodable's compare functions
 * \param list is appended with any directives this encodable requires.
 */
void ProtocolStructure::getCompareIncludeDirectives(QStringList& list) const
{
    // Includes that our encodable members may need
    for(int i = 0; i < encodables.length(); i++)
        encodables.at(i)->getCompareIncludeDirectives(list);

    list.removeDuplicates();
}


/*!
 * Return the include directives needed for this encodable's print functions
 * \param list is appended with any directives this encodable requires.
 */
void ProtocolStructure::getPrintIncludeDirectives(QStringList& list) const
{
    // Includes that our encodable members may need
    for(int i = 0; i < encodables.length(); i++)
        encodables.at(i)->getPrintIncludeDirectives(list);

    list.removeDuplicates();
}


/*!
 * Get the declaration that goes in the header which declares this structure
 * and all its children.
 * \param alwaysCreate should be true to force creation of the structure, even if there is only one member
 * \return the string that represents the structure declaration
 */
QString ProtocolStructure::getStructureDeclaration(bool alwaysCreate) const
{
    QString output;
    QString structure;

    // Declare our childrens structures first, but only if we are not redefining
    // someone else, in which case they have already declared the children
    if(redefines == nullptr)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            // Is this encodable a structure?
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(structure == nullptr)
                continue;

            output += structure->getStructureDeclaration(true);
            ProtocolFile::makeLineSeparator(output);

        }// for all children

    }// if not redefining

    /// TODO: Move this to be within the class for C++
    // Output enumerations specific to this structure
    for(int i = 0; i < enumList.size(); i++)
    {
        output += enumList.at(i)->getOutput();
        ProtocolFile::makeLineSeparator(output);
    }

    if(support.language == ProtocolSupport::c_language)
        output += getStructureDeclaration_C(alwaysCreate);
    else
        output += getClassDeclaration_CPP();

    return output;

}// ProtocolStructure::getStructureDeclaration


/*!
 * Get the structure declaration, for this structure only (not its children) for the C language
 * \return The string that gives the structure declaration
 */
QString ProtocolStructure::getStructureDeclaration_C(bool alwaysCreate) const
{
    QString output;

    // We don't generate the structure if there is only one element, whats
    // the point? Unless the the caller tells us to always create it. We
    // also do not write the structure if we are redefining, in that case
    // the structure already exists.
    if((redefines == nullptr) && (getNumberInMemory() > 0) && ((getNumberInMemory() > 1) || alwaysCreate))
    {
        QString structure;

        // The top level comment for the structure definition
        if(!comment.isEmpty())
        {
            output += "/*!\n";
            output += ProtocolParser::outputLongComment(" *", comment) + "\n";
            output += " */\n";
        }

        // The opening to the structure
        output += "typedef struct\n";
        output += "{\n";
        for(int i = 0; i < encodables.length(); i++)
            structure += encodables[i]->getDeclaration();

        // Make structures pretty with alignment goodness
        output += alignStructureData(structure);

        // Close out the structure
        output += "}" + typeName + ";\n";

    }// if we have some data to declare

    return output;

}// ProtocolStructure::getStructureDeclaration_C


/*!
 * Get the class declaration, for this structure only (not its children) for the C++ language
 * \return The string that gives the class declaration
 */
QString ProtocolStructure::getClassDeclaration_CPP(void) const
{
    QString output;
    QString structure;

    // The top level comment for the class definition
    if(!comment.isEmpty())
    {
        output += "/*!\n";
        output += ProtocolParser::outputLongComment(" *", comment) + "\n";
        output += " */\n";
    }

    if(redefines == nullptr)
    {
        // The opening to the class.
        output += "class " + typeName + "\n";
        output += "{\n";

        // All members of the structure are public.
        output += "public:\n";

        // Function prototypes, in C++ these are part of the class definition
        if(getNumberInMemory() > 0)
        {
            ProtocolFile::makeLineSeparator(output);
            output += getSetToInitialValueFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
        }

        if(encode)
        {
            ProtocolFile::makeLineSeparator(output);
            output += getEncodeFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
        }

        if(decode)
        {
            ProtocolFile::makeLineSeparator(output);
            output += getDecodeFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
        }

        if(hasVerify())
        {
            ProtocolFile::makeLineSeparator(output);
            output += getVerifyFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
        }

        if(print)
        {
            ProtocolFile::makeLineSeparator(output);
            output += getTextPrintFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
            output += getTextReadFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
        }

        if(mapEncode)
        {
            ProtocolFile::makeLineSeparator(output);
            output += getMapEncodeFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
            output += getMapDecodeFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
        }

        if(compare)
        {
            ProtocolFile::makeLineSeparator(output);
            output += getComparisonFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
        }

        // Now declare the members of this class
        for(int i = 0; i < encodables.length(); i++)
            structure += encodables[i]->getDeclaration();

        // Make classes pretty with alignment goodness
        output += alignStructureData(structure);

        ProtocolFile::makeLineSeparator(output);

    }// if not redefining
    else
    {
        // In the context of C++ redefining means inheriting from a base class,
        // and adding a new encode or decode function. All the other members and
        // methods come from the base class
        // The opening to the class.
        output += "class " + typeName + "public: " + redefines->typeName + "\n";
        output += "{\n";

        // All members of the structure are public.
        output += "public:\n";

        if(encode)
        {
            ProtocolFile::makeLineSeparator(output);
            output += getEncodeFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
        }

        if(decode)
        {
            ProtocolFile::makeLineSeparator(output);
            output += getDecodeFunctionPrototype(TAB_IN, false);
            ProtocolFile::makeLineSeparator(output);
        }

    }// else if redefining another class

    // Close out the class
    output += "}; // " + typeName + "\n";

    return output;

}// ProtocolStructure::getClassDeclaration_CPP


/*!
 * Make a structure output be prettily aligned
 * \param structure is the input structure string
 * \return a new string that is the aligned structure string
 */
QString ProtocolStructure::alignStructureData(const QString& structure) const
{
    int i, index;

    // The strings as a list separated by line feeds
    QStringList list = structure.split("\n", QString::SkipEmptyParts);

    // The space separates the typeName from the name,
    // but skip the indent spaces
    int max = 0;
    for(i = 0; i < list.size(); i++)
    {
        int index = list[i].indexOf(" ", 4);
        if(index > max)
            max = index;
    }

    for(int i = 0; i < list.size(); i++)
    {
        // Insert spaces until we have reached the max
        index = list[i].indexOf(" ", 4);
        for(; index < max; index++)
            list[i].insert(index, " ");
    }

    // The first semicolon we find separates the name from the comment
    max = 0;
    for(i = 0; i < list.size(); i++)
    {
        // we want the character after the semicolon
        index = list[i].indexOf(";") + 1;
        if(index > max)
            max = index;
    }

    for(i = 0; i < list.size(); i++)
    {
        // Insert spaces until we have reached the max
        index = list[i].indexOf(";") + 1;
        for(; index < max; index++)
            list[i].insert(index, " ");
    }

    // Re-assemble the output, put the line feeds back on
    QString output;
    for(i = 0; i < list.size(); i++)
        output += list[i] + "\n";

    return output;

}// ProtocolStructure::alignStructureData


/*!
 * Get the signature of the function that encodes this structure.
 * \param insource should be true to indicate this signature is in source code
 * \return the signature of the encode function.
 */
QString ProtocolStructure::getEncodeFunctionSignature(bool insource) const
{
    QString output;

    if(support.language == ProtocolSupport::c_language)
    {
        QString pg;

        if(insource)
            pg = "_pg_";

        if(getNumberOfEncodeParameters() > 0)
        {
            output = "void encode" + typeName + "(uint8_t* " + pg + "data, int* " + pg + "bytecount, const " + structName + "* " + pg+ "user)";
        }
        else
        {
            output = "void encode" + typeName + "(uint8_t* " + pg + "data, int* " + pg + "bytecount)";
        }
    }
    else
    {
        // For C++ these functions are within the class namespace and they
        // reference their own members. This function is const, unless it
        // doesn't have any encode parameters, in which case it is static.
        if(getNumberOfEncodeParameters() > 0)
        {
            if(insource)
                output = "void " + typeName + "::encode(uint8_t* _pg_data, int* _pg_bytecount) const";
            else
                output = "void encode(uint8_t* data, int* bytecount) const";
        }
        else
        {
            if(insource)
                output = "void " + typeName + "::encode(uint8_t* _pg_data, int* _pg_bytecount)";
            else
                output = "static void encode(uint8_t* data, int* bytecount)";
        }
    }

    return output;

}// ProtocolStructure::getEncodeFunctionSignature


/*!
 * Return the string that gives the prototype of the functions used to encode
 * the structure, and all child structures. The encoding is to a simple byte array.
 * \param spacing gives the spacing to offset each line.
 * \param includeChildren should be true to output the children's prototypes.
 * \return The string including the comments and prototypes with linefeeds and semicolons.
 */
QString ProtocolStructure::getEncodeFunctionPrototype(const QString& spacing, bool includeChildren) const
{
    QString output;

    // Only the C language needs this. C++ declares the prototype within the class
    if(includeChildren && (support.language == ProtocolSupport::c_language))
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            // Is this encodable a structure?
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(structure == nullptr)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getEncodeFunctionPrototype(spacing, includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);

    }

    // My encoding and decoding prototypes in the header file
    output += spacing + "//! Encode a " + typeName + " into a byte array\n";
    output += spacing + getEncodeFunctionSignature(false) + ";\n";

    return output;

}// ProtocolStructure::getEncodeFunctionPrototype


/*!
 * Return the string that gives the function used to encode this structure and
 * all its children to a simple byte array.
 * \param isBigEndian should be true for big endian encoding.
 * \param includeChildren should be true to output the children's functions.
 * \return The string including the comments and code with linefeeds and semicolons.
 */
QString ProtocolStructure::getEncodeFunctionBody(bool isBigEndian, bool includeChildren) const
{
    QString output;

    if(includeChildren)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            // Is this encodable a structure?
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(structure == nullptr)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getEncodeFunctionBody(isBigEndian, includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    // My encoding function
    output += "/*!\n";
    output += " * \\brief Encode a " + typeName + " into a byte array\n";
    output += " *\n";
    output += ProtocolParser::outputLongComment(" *", comment) + "\n";
    output += " * \\param _pg_data points to the byte array to add encoded data to\n";
    output += " * \\param _pg_bytecount points to the starting location in the byte array, and will be incremented by the number of encoded bytes.\n";
    if((support.language == ProtocolSupport::c_language) && (getNumberOfEncodeParameters() > 0))
        output += " * \\param _pg_user is the data to encode in the byte array\n";
    output += " */\n";

    output += getEncodeFunctionSignature(true) + "\n";
    output += "{\n";

    output += TAB_IN + "int _pg_byteindex = *_pg_bytecount;\n";

    if(usestempencodebitfields)
        output += TAB_IN + "unsigned int _pg_tempbitfield = 0;\n";

    if(usestempencodelongbitfields)
        output += TAB_IN + "uint64_t _pg_templongbitfield = 0;\n";

    if(numbitfieldgroupbytes > 0)
    {
        output += TAB_IN + "int _pg_bitfieldindex = 0;\n";
        output += TAB_IN + "uint8_t _pg_bitfieldbytes[" + QString::number(numbitfieldgroupbytes) + "];\n";
    }

    if(needsEncodeIterator)
        output += TAB_IN + "unsigned _pg_i = 0;\n";

    if(needs2ndEncodeIterator)
        output += TAB_IN + "unsigned _pg_j = 0;\n";

    int bitcount = 0;
    for(int i = 0; i < encodables.length(); i++)
    {
        ProtocolFile::makeLineSeparator(output);
        output += encodables[i]->getEncodeString(isBigEndian, &bitcount, true);
    }

    ProtocolFile::makeLineSeparator(output);
    output += TAB_IN + "*_pg_bytecount = _pg_byteindex;\n";
    output += "\n";
    output += "}// encode" + typeName + "\n";

    return output;

}// ProtocolStructure::getEncodeFunctionBody


/*!
 * Get the signature of the function that decodes this structure.
 * \param insource should be true to indicate this signature is in source code
 * \return the signature of the decode function.
 */
QString ProtocolStructure::getDecodeFunctionSignature(bool insource) const
{
    QString output;

    if(support.language == ProtocolSupport::c_language)
    {
        QString pg;

        if(insource)
            pg = "_pg_";

        if(getNumberOfDecodeParameters() > 0)
        {
            output = "int decode" + typeName + "(const uint8_t* " + pg + "data, int* " + pg + "bytecount, " + structName + "* " + pg + "user)";
        }
        else
        {
            output = "int decode" + typeName + "(const uint8_t* " + pg + "data, int* " + pg + "bytecount)";
        }
    }
    else
    {
        // For C++ these functions are within the class namespace and they
        // reference their own members.
        if(insource)
            output = "bool " + typeName + "::decode(const uint8_t* _pg_data, int* _pg_bytecount)";
        else
            output = "bool decode(const uint8_t* data, int* bytecount)";
    }

    return output;

}// ProtocolStructure::getDecodeFunctionSignature


/*!
 * Return the string that gives the prototype of the functions used to decode
 * the structure. The encoding is to a simple byte array.
 * \param spacing gives the spacing to offset each line.
 * \param includeChildren should be true to output the children's prototypes.
 * \return The string including the comments and prototypes with linefeeds and semicolons.
 */
QString ProtocolStructure::getDecodeFunctionPrototype(const QString& spacing, bool includeChildren) const
{
    QString output;

    // Only the C language needs this. C++ declares the prototype within the class
    if(includeChildren && (support.language == ProtocolSupport::c_language))
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            // Is this encodable a structure?
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(structure == nullptr)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getDecodeFunctionPrototype(spacing, includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    output += spacing + "//! Decode a " + typeName + " from a byte array\n";
    output += spacing + getDecodeFunctionSignature(false) + ";\n";

    return output;

}// ProtocolStructure::getDecodeFunctionPrototype


/*!
 * Return the string that gives the function used to decode this structure.
 * The decoding is from a simple byte array.
 * \param isBigEndian should be true for big endian decoding.
 * \param includeChildren should be true to output the children's functions.
 * \return The string including the comments and code with linefeeds and semicolons.
 */
QString ProtocolStructure::getDecodeFunctionBody(bool isBigEndian, bool includeChildren) const
{
    QString output;

    if(includeChildren)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            // Is this encodable a structure?
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(structure == nullptr)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getDecodeFunctionBody(isBigEndian);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    output += "/*!\n";
    output += " * \\brief Decode a " + typeName + " from a byte array\n";
    output += " *\n";
    output += ProtocolParser::outputLongComment(" *", comment) + "\n";
    output += " * \\param _pg_data points to the byte array to decoded data from\n";
    output += " * \\param _pg_bytecount points to the starting location in the byte array, and will be incremented by the number of bytes decoded\n";
    if((support.language == ProtocolSupport::c_language) && (getNumberOfDecodeParameters() > 0))
        output += " * \\param _pg_user is the data to decode from the byte array\n";

    output += " * \\return " + getReturnCode(true) + " if the data are decoded, else " + getReturnCode(false) + ".";
    output += " */\n";
    output += getDecodeFunctionSignature(true) + "\n";
    output += "{\n";

    output += TAB_IN + "int _pg_byteindex = *_pg_bytecount;\n";

    if(usestempdecodebitfields)
        output += TAB_IN + "unsigned int _pg_tempbitfield = 0;\n";

    if(usestempdecodelongbitfields)
        output += TAB_IN + "uint64_t _pg_templongbitfield = 0;\n";

    if(numbitfieldgroupbytes > 0)
    {
        output += TAB_IN + "int _pg_bitfieldindex = 0;\n";
        output += TAB_IN + "uint8_t _pg_bitfieldbytes[" + QString::number(numbitfieldgroupbytes) + "];\n";
    }

    if(needsDecodeIterator)
        output += TAB_IN + "unsigned _pg_i = 0;\n";

    if(needs2ndDecodeIterator)
        output += TAB_IN + "unsigned _pg_j = 0;\n";

    int bitcount = 0;
    for(int i = 0; i < encodables.length(); i++)
    {
        ProtocolFile::makeLineSeparator(output);
        output += encodables[i]->getDecodeString(isBigEndian, &bitcount, true);
    }

    ProtocolFile::makeLineSeparator(output);
    output += TAB_IN + "*_pg_bytecount = _pg_byteindex;\n\n";
    output += TAB_IN + "return " + getReturnCode(true) + ";\n";
    output += "\n";
    output += "}// decode" + typeName + "\n";

    return output;

}// ProtocolStructure::getDecodeFunctionBody


/*!
 * Get the signature of the function that sets initial values of this structure.
 * This is just the constructor for C++
 * \param insource should be true to indicate this signature is in source code.
 * \return the signature of the set to initial value function.
 */
QString ProtocolStructure::getSetToInitialValueFunctionSignature(bool insource) const
{
    QString output;

    if(support.language == ProtocolSupport::c_language)
    {
        if(getNumberInMemory() > 0)
        {
            if(insource)
                output = "void init" + typeName + "(" + structName + "* _pg_user)";
            else
                output = "void init" + typeName + "(" + structName + "* user)";
        }
        else
        {
            output = "void init" + typeName + "(void)";
        }
    }
    else
    {
        // For C++ this function is the constructor
        if(insource)
            output = typeName + "::" + typeName + "(void)";
        else
            output = typeName + "(void)";
    }

    return output;

}// ProtocolStructure::getSetToInitialValueFunctionSignature


/*!
 * Return the string that gives the prototypes of the functions used to set
 * this structure to initial values.
 * \param spacing gives the spacing to offset each line.
 * \param includeChildren should be true to output the children's functions.
 * \return The string including the comments and code with linefeeds and semicolons
 */
QString ProtocolStructure::getSetToInitialValueFunctionPrototype(const QString& spacing, bool includeChildren) const
{
    QString output;

    // C++ always has init (constructor) functions, but not C
    if(!hasInit() && (support.language == ProtocolSupport::c_language))
        return output;

    // Go get any children structures set to initial functions
    if(includeChildren && (support.language == ProtocolSupport::c_language))
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getSetToInitialValueFunctionPrototype(spacing, includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    // My set to initial values function
    if(support.language == ProtocolSupport::c_language)
        output += spacing + "//! Set a " + typeName + " to initial values\n";
    else
        output += spacing + "//! Construct a " + typeName + "\n";

    output += spacing + getSetToInitialValueFunctionSignature(false) + ";\n";

    return output;

}// ProtocolStructure::getSetToInitialValueFunctionPrototype


/*!
 * Return the string that gives the function used to this structure to initial
 * values. This is NOT the call that sets this structure to initial values, this
 * is the actual code that is in that call.
 * \param includeChildren should be true to output the children's functions.
 * \return The string including the comments and code with linefeeds and semicolons
 */
QString ProtocolStructure::getSetToInitialValueFunctionBody(bool includeChildren) const
{
    QString output;

    // C++ always has init (constructor) functions, but not C
    if(!hasInit() && (support.language == ProtocolSupport::c_language))
        return output;

    // Go get any children structures set to initial functions
    if(includeChildren)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getSetToInitialValueFunctionBody(includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    if(support.language == ProtocolSupport::c_language)
    {
        // My set to initial values function
        output += "/*!\n";
        output += " * \\brief Set a " + typeName + " to initial values.\n";
        output += " *\n";
        output += " * Set a " + typeName + " to initial values. Not all fields are set,\n";
        output += " * only those which the protocol specifies.\n";
        output += " * \\param _pg_user is the structure whose data are set to initial values\n";
        output += " */\n";
        output += getSetToInitialValueFunctionSignature(true) + "\n";
        output += "{\n";

        if(needsInitIterator)
            output += TAB_IN + "int _pg_i = 0;\n";

        if(needs2ndInitIterator)
            output += TAB_IN + "int _pg_j = 0;\n";

        for(int i = 0; i < encodables.length(); i++)
        {
            /// TODO: change this to zeroize all fields
            ProtocolFile::makeLineSeparator(output);
            output += encodables[i]->getSetInitialValueString(true);
        }

        ProtocolFile::makeLineSeparator(output);
        output += "}// init" + typeName + "\n";

    }// If the C language output
    else
    {
        // Set to initial values is just the constructor. We initialize every
        // member that is not itself another class (they take care of themselves).
        QString initializerlist;
        bool hasarray1d = false;
        bool hasarray2d = false;

        for(int i = 0; i < encodables.length(); i++)
        {
            // Structures (classes really) take care of themselves
            if(!encodables.at(i)->isPrimitive())
                continue;

            // Arrays are done later
            if(encodables.at(i)->isArray())
            {
                hasarray1d = true;
                if(encodables.at(i)->is2dArray())
                    hasarray2d = true;

                continue;
            }

            initializerlist += encodables.at(i)->getSetInitialValueString(true);
        }

        // Get rid of the comma on the last member of the initializer list
        if(initializerlist.endsWith(",\n"))
            initializerlist = initializerlist.left(initializerlist.lastIndexOf(",\n")) + "\n";

        if(initializerlist.isEmpty())
            initializerlist = "\n";
        else
            initializerlist = " :\n" + initializerlist;

        output += "/*!\n";
        output += " * Construct a " + typeName + ".\n";
        output += " */\n";
        output += getSetToInitialValueFunctionSignature(true) + initializerlist;
        output += "{\n";

        // We have our own version of `needsInitIterator` because arrays of classes do not need to be handed here.
        if(hasarray1d)
            output += TAB_IN + "int _pg_i = 0;\n";

        if(hasarray2d)
            output += TAB_IN + "int _pg_j = 0;\n";

        for(int i = 0; i < encodables.length(); i++)
        {
            // Structures (classes really) take care of themselves
            if(!encodables.at(i)->isPrimitive())
                continue;

            // Arrays are done here
            if(!encodables.at(i)->isArray())
                continue;

            /// TODO: in C++ we should be able to do array initialization like this: myarray = {0};
            ProtocolFile::makeLineSeparator(output);
            output += encodables[i]->getSetInitialValueString(true);
        }

        ProtocolFile::makeLineSeparator(output);
        output += "}// " + typeName + "::" + typeName + "\n";

    }// else if C++ language

    return output;

}// ProtocolStructure::getSetToInitialValueFunctionBody


/*!
 * Get the signature of the verify function.
 * \param insource should be true to indicate this signature is in source code.
 * \return the signature of the verify function.
 */
QString ProtocolStructure::getVerifyFunctionSignature(bool insource) const
{
    if(support.language == ProtocolSupport::c_language)
    {
        if(insource)
            return "int verify" + typeName + "(" + structName + "* _pg_user)";
        else
            return "int verify" + typeName + "(" + structName + "* user)";
    }
    else
    {
        if(insource)
            return "bool " + typeName + "::verify(void)";
        else
            return "bool verify(void)";
    }

}// ProtocolStructure::getVerifyFunctionSignature


/*!
 * Return the string that gives the prototypes of the functions used to verify
 * the data in this.
 * \param spacing gives the spacing to offset each line.
 * \param includeChildren should be true to output the children's functions.
 * \return The string including the comments and code with linefeeds and semicolons
 */
QString ProtocolStructure::getVerifyFunctionPrototype(const QString& spacing, bool includeChildren) const
{
    QString output;

    // Only if we have verify functions
    if(!hasVerify())
        return output;

    // Go get any children structures verify functions
    if(includeChildren && (support.language == ProtocolSupport::c_language))
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getVerifyFunctionPrototype(spacing, includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    // My verify values function
    output += spacing + "//! Verify a " + typeName + " has acceptable values\n";
    output += spacing + getVerifyFunctionSignature(false) + ";\n";

    return output;

}// ProtocolStructure::getVerifyFunctionPrototype


/*!
 * Return the string that gives the function used to verify the data in this
 * structure. This is NOT the call that verifies this structure, this
 * is the actual code that is in that call.
 * \param includeChildren should be true to output the children's functions.
 * \return The string including the comments and code with linefeeds and semicolons
 */
QString ProtocolStructure::getVerifyFunctionBody(bool includeChildren) const
{
    QString output;

    // Only if we have verify functions
    if(!hasVerify())
        return output;

    // Go get any children structures verify functions
    if(includeChildren)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getVerifyFunctionBody(includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    // My set to initial values function
    output += "/*!\n";
    output += " * \\brief Verify a " + typeName + " has acceptable values.\n";
    output += " *\n";
    output += " * Verify a " + typeName + " has acceptable values. Not all fields are\n";
    output += " * verified, only those which the protocol specifies. Fields which are outside\n";
    output += " * the allowable range are changed to the maximum or minimum allowable value. \n";

    if(support.language == ProtocolSupport::c_language)
    {
        output += " * \\param _pg_user is the structure whose data are verified\n";
        output += " * \\return 1 if all verifiable data where valid, else 0 if data had to be corrected\n";
        output += " */\n";
        output += getVerifyFunctionSignature(true) + "\n";
        output += "{\n";
        output += TAB_IN + "int _pg_good = 1;\n";
    }
    else
    {
        output += " * \\return true if all verifiable data where valid, else false if data had to be corrected\n";
        output += " */\n";
        output += getVerifyFunctionSignature(true) + "\n";
        output += "{\n";
        output += TAB_IN + "bool _pg_good = true;\n";
    }

    if(needsVerifyIterator)
        output += TAB_IN + "unsigned _pg_i = 0;\n";

    if(needs2ndVerifyIterator)
        output += TAB_IN + "unsigned _pg_j = 0;\n";

    for(int i = 0; i < encodables.length(); i++)
    {
        ProtocolFile::makeLineSeparator(output);
        output += encodables[i]->getVerifyString();
    }

    ProtocolFile::makeLineSeparator(output);
    output += TAB_IN + "return _pg_good;\n";
    output += "\n";
    if(support.language == ProtocolSupport::c_language)
        output += "}// verify" + typeName + "\n";
    else
        output += "}// " + typeName + "::verify\n";

    return output;

}// ProtocolStructure::getVerifyFunctionBody


/*!
 * Get the signature of the comparison function.
 * \param insource should be true to indicate this signature is in source code.
 * \return the signature of the comparison function.
 */
QString ProtocolStructure::getComparisonFunctionSignature(bool insource) const
{
    if(support.language == ProtocolSupport::c_language)
    {
        if(insource)
            return "QString compare" + typeName + "(const QString& _pg_prename, const " + structName + "* _pg_user1, const " + structName + "* _pg_user2)";
        else
            return "QString compare" + typeName + "(const QString& prename, const " + structName + "* user1, const " + structName + "* user2)";
    }
    else
    {
        if(insource)
            return "QString " + typeName + "::compare(const QString& _pg_prename, const " + structName + "* _pg_user) const";
        else
            return "QString compare(const QString& prename, const " + structName + "* user) const";
    }

}// ProtocolStructure::getComparisonFunctionSignature


/*!
 * Return the string that gives the prototype of the function used to compare this structure
 * \param spacing gives the spacing to offset each line.
 * \param includeChildren should be true to include the function prototypes of the children structures of this structure
 * \return the function prototype string, which may be empty
 */
QString ProtocolStructure::getComparisonFunctionPrototype(const QString& spacing, bool includeChildren) const
{
    QString output;

    // We must have parameters that we decode to do a compare
    if(!compare || (getNumberOfDecodeParameters() == 0))
        return output;

    // Go get any children structures compare functions
    if(includeChildren && (support.language == ProtocolSupport::c_language))
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getComparisonFunctionPrototype(spacing, includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }


    // My comparison function
    output += spacing + "//! Compare two " + typeName + " and generate a report\n";
    output += spacing + getComparisonFunctionSignature(false) + ";\n";

    return output;

}// ProtocolStructure::getComparisonFunctionPrototype


/*!
 * Return the string that gives the function used to compare this structure
 * \param includeChildren should be true to include the function prototypes of
 *        the children structures of this structure
 * \return the function string, which may be empty
 */
QString ProtocolStructure::getComparisonFunctionBody(bool includeChildren) const
{
    QString output;

    // We must have parameters that we decode to do a compare
    if(!compare || (getNumberOfDecodeParameters() == 0))
        return output;

    // Go get any childrens structure compare functions
    if(includeChildren)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getComparisonFunctionBody(includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    // My compare function
    output += "/*!\n";

    if(support.language == ProtocolSupport::c_language)
    {
        output += " * Compare two " + typeName + " and generate a report of any differences.\n";
        output += " * \\param _pg_prename is prepended to the name of the data field in the comparison report\n";
        output += " * \\param _pg_user1 is the first data to compare\n";
        output += " * \\param _pg_user1 is the second data to compare\n";
        output += " * \\return a string describing any differences between _pg_user1 and _pg_user2. The string will be empty if there are no differences\n";
    }
    else
    {
        output += " * Compare this " + typeName + " with another " + typeName + " and generate a report of any differences.\n";
        output += " * \\param _pg_prename is prepended to the name of the data field in the comparison report\n";
        output += " * \\param _pg_user is the data to compare\n";
        output += " * \\return a string describing any differences between this " + typeName + " and `_pg_user`. The string will be empty if there are no differences\n";
    }
    output += " */\n";
    output += getComparisonFunctionSignature(true) + "\n";
    output += "{\n";
    output += TAB_IN + "QString _pg_report;\n";

    if(needsDecodeIterator)
        output += TAB_IN + "unsigned _pg_i = 0;\n";

    if(needs2ndDecodeIterator)
        output += TAB_IN + "unsigned _pg_j = 0;\n";

    for(int i = 0; i < encodables.length(); i++)
    {
        ProtocolFile::makeLineSeparator(output);
        output += encodables[i]->getComparisonString();
    }

    ProtocolFile::makeLineSeparator(output);
    output += TAB_IN + "return _pg_report;\n";
    output += "\n";
    if(support.language == ProtocolSupport::c_language)
        output += "}// compare" + typeName + "\n";
    else
        output += "}// " + typeName + "::compare\n";

    return output;

}// ProtocolStructure::getComparisonFunctionBody


/*!
 * Get the signature of the textPrint function.
 * \param insource should be true to indicate this signature is in source code.
 * \return the signature of the comparison function.
 */
QString ProtocolStructure::getTextPrintFunctionSignature(bool insource) const
{
    if(support.language == ProtocolSupport::c_language)
    {
        if(insource)
            return "QString textPrint" + typeName + "(const QString& _pg_prename, const " + structName + "* _pg_user)";
        else
            return "QString textPrint" + typeName + "(const QString& prename, const " + structName + "* user)";
    }
    else
    {
        if(insource)
            return "QString " + typeName + "::textPrint(const QString& _pg_prename) const";
        else
            return "QString textPrint(const QString& prename) const";
    }

}// ProtocolStructure::getTextPrintFunctionSignature


/*!
 * Return the string that gives the prototype of the function used to text print this structure
 * \param spacing gives the spacing to offset each line.
 * \param includeChildren should be true to include the function prototypes of the children of this structure.
 * \return the function prototype string, which may be empty
 */
QString ProtocolStructure::getTextPrintFunctionPrototype(const QString& spacing, bool includeChildren) const
{
    QString output;

    // We must have parameters that we decode to do a print out
    if(!print || (getNumberOfDecodeParameters() == 0))
        return output;

    // Go get any children structures textPrint functions
    if(includeChildren && (support.language == ProtocolSupport::c_language))
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getTextPrintFunctionPrototype(spacing, includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    // My textPrint function
    output += spacing + "//! Generate a string that describes the contents of a " + typeName + "\n";
    output += spacing + getTextPrintFunctionSignature(false) + ";\n";

    return output;

}// ProtocolStructure::getTextPrintFunctionPrototype


/*!
 * Return the string that gives the function used to compare this structure
 * \param includeChildren should be true to include the function prototypes of
 *        the children structures of this structure
 * \return the function string, which may be empty
 */
QString ProtocolStructure::getTextPrintFunctionBody(bool includeChildren) const
{
    QString output;

    // We must have parameters that we decode to do a print out
    if(!print || (getNumberOfDecodeParameters() == 0))
        return output;

    // Go get any childrens structure textPrint functions
    if(includeChildren)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getTextPrintFunctionBody(includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    // My textPrint function
    output += "/*!\n";
    output += " * Generate a string that describes the contents of a " + typeName + "\n";
    output += " * \\param _pg_prename is prepended to the name of the data field in the report\n";
    if(support.language == ProtocolSupport::c_language)
        output += " * \\param _pg_user is the structure to report\n";
    output += " * \\return a string containing a report of the contents of user\n";
    output += " */\n";
    output += getTextPrintFunctionSignature(true) + "\n";
    output += "{\n";
    output += TAB_IN + "QString _pg_report;\n";

    if(needsDecodeIterator)
        output += TAB_IN + "unsigned _pg_i = 0;\n";

    if(needs2ndDecodeIterator)
        output += TAB_IN + "unsigned _pg_j = 0;\n";

    for(int i = 0; i < encodables.length(); i++)
    {
        ProtocolFile::makeLineSeparator(output);
        output += encodables[i]->getTextPrintString();
    }

    ProtocolFile::makeLineSeparator(output);
    output += TAB_IN + "return _pg_report;\n";
    output += "\n";
    if(support.language == ProtocolSupport::c_language)
        output += "}// textPrint" + typeName + "\n";
    else
        output += "}// " + typeName + "::textPrint\n";

    return output;

}// ProtocolStructure::getTextPrintFunctionString


/*!
 * Get the signature of the textRead function.
 * \param insource should be true to indicate this signature is in source code.
 * \return the signature of the comparison function.
 */
QString ProtocolStructure::getTextReadFunctionSignature(bool insource) const
{
    if(support.language == ProtocolSupport::c_language)
    {
        if(insource)
            return "int textRead" + typeName + "(const QString& _pg_prename, const QString& _pg_source, " + structName + "* _pg_user)";
        else
            return "int textRead" + typeName + "(const QString& prename, const QString& source, " + structName + "* user)";
    }
    else
    {
        if(insource)
            return "int " + typeName + "::textRead(const QString& _pg_prename, const QString& _pg_source)";
        else
            return "int textRead(const QString& prename, const QString& source)";
    }

}// ProtocolStructure::getTextReadFunctionSignature


/*!
 * Return the string that gives the prototype of the function used to read this
 * structure from text
 * \param spacing gives the spacing to offset each line.
 * \param includeChildren should be true to include the function prototypes of
 *        the children structures of this structure
 * \return the function prototype string, which may be empty
 */
QString ProtocolStructure::getTextReadFunctionPrototype(const QString& spacing, bool includeChildren) const
{
    QString output;

    // We must have parameters that we decode to do a read
    if(!print || (getNumberOfDecodeParameters() == 0))
        return output;

    // Go get any children structures textRead functions
    if(includeChildren && (support.language == ProtocolSupport::c_language))
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getTextReadFunctionPrototype(spacing, includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    // My textRead function
    output += spacing + "//! Read the contents of a " + typeName + " from text\n";
    output += spacing + getTextReadFunctionSignature(false) + ";\n";

    return output;

}// ProtocolStructure::getTextReadFunctionPrototype


/*!
 * Get the string that gives the function used to read this structure from text
 * \param includeChildren should be true to include the function prototypes of
 *        the children structures of this structure
 * \return the function string, which may be empty
 */
QString ProtocolStructure::getTextReadFunctionBody(bool includeChildren) const
{
    QString output;

    // We must have parameters that we decode to do a read
    if(!print || (getNumberOfDecodeParameters() == 0))
        return output;

    // Go get any childrens structure textRead functions
    if(includeChildren)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getTextReadFunctionBody(includeChildren);
        }
        ProtocolFile::makeLineSeparator(output);
    }

    // My textRead function
    output += "/*!\n";
    output += " * Read the contents of a " + typeName + " structure from text\n";
    output += " * \\param _pg_prename is prepended to the name of the data field to form the text key\n";
    output += " * \\param _pg_source is text to search to find the data field keys\n";
    if(support.language == ProtocolSupport::c_language)
        output += " * \\param _pg_user receives any data read from the text source\n";
    output += " * \\return The number of fields that were read from the text source\n";
    output += " */\n";
    output += getTextReadFunctionSignature(true) + "\n";
    output += "{\n";
    output += TAB_IN + "QString _pg_text;\n";
    output += TAB_IN + "int _pg_fieldcount = 0;\n";

    if(needsDecodeIterator)
        output += TAB_IN + "unsigned _pg_i = 0;\n";

    if(needs2ndDecodeIterator)
        output += TAB_IN + "unsigned _pg_j = 0;\n";

    for(int i = 0; i < encodables.length(); i++)
    {
        ProtocolFile::makeLineSeparator(output);
        output += encodables[i]->getTextReadString();
    }

    ProtocolFile::makeLineSeparator(output);
    output += TAB_IN + "return _pg_fieldcount;\n";
    output += "\n";
    if(support.language == ProtocolSupport::c_language)
        output += "}// textRead" + typeName + "\n";
    else
        output += "}// " + typeName + "::textRead\n";

    return output;

}// ProtocolStructure::getTextReadFunctionString


/*!
 * Get the signature of the mapEncode function.
 * \param insource should be true to indicate this signature is in source code.
 * \return the signature of the comparison function.
 */
QString ProtocolStructure::getMapEncodeFunctionSignature(bool insource) const
{
    if(support.language == ProtocolSupport::c_language)
    {
        if(insource)
            return "void mapEncode" + typeName + "(const QString& _pg_prename, QVariantMap& _pg_map, const " + structName + "* _pg_user)";
        else
            return "void mapEncode" + typeName + "(const QString& prename, QVariantMap& map, const " + structName + "* user)";
    }
    else
    {
        if(insource)
            return "void " + typeName + "::mapEncode(const QString& _pg_prename, QVariantMap& _pg_map) const";
        else
            return "void mapEncode(const QString& prename, QVariantMap& map) const";
    }

}// ProtocolStructure::getMapEncodeFunctionSignature


/*!
 * Return the string that gives the prototype of the function used to encode
 * this structure to a map
 * \param spacing gives the spacing to offset each line.
 * \param includeChildren should be true to include the function prototypes
 *        of the child structures
 * \return the function prototype string, which may be empty
 */
QString ProtocolStructure::getMapEncodeFunctionPrototype(const QString& spacing, bool includeChildren) const
{
    QString output;

    // Only the C language has to create the prototype functions, in C++ this is part of the class declaration
    if(!mapEncode || (getNumberOfDecodeParameters() == 0))
        return output;

    if(includeChildren && (support.language == ProtocolSupport::c_language))
    {
        for(int i = 0; i < encodables.length();  i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);

            output += structure->getMapEncodeFunctionPrototype(spacing, includeChildren);
        }

        ProtocolFile::makeLineSeparator(output);
    }

    // My mapEncode function
    output += spacing + "//! Encode the contents of a " + typeName + " to a string Key:Value map\n";
    output += spacing + getMapEncodeFunctionSignature(false) + ";\n";

    return output;

}// ProtocolStructure::getMapEncodeFunctionPrototype


/*!
 * Return the string that gives the function used to encode this structure to a map
 * \param includeChildren should be true to include the functions of the child structures
 * \return the function string, which may be empty
 */
QString ProtocolStructure::getMapEncodeFunctionBody(bool includeChildren) const
{
    QString output;

    if(!mapEncode || (getNumberOfDecodeParameters() == 0))
        return output;

    if(includeChildren)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if (!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getMapEncodeFunctionBody(includeChildren);
        }

        ProtocolFile::makeLineSeparator(output);
    }

    // My mapEncode function
    output += "/*!\n";
    output += " * Encode the contents of a " + typeName + " to a Key:Value string map\n";
    output += " * \\param _pg_prename is prepended to the key fields in the map\n";
    output += " * \\param _pg_map is a reference to the map\n";
    if(support.language == ProtocolSupport::c_language)
        output += " * \\param _pg_user is the structure to encode\n";
    output += " */\n";
    output += getMapEncodeFunctionSignature(true) + "\n";
    output += "{\n";
    output += TAB_IN + "QString key;\n";

    if(needsDecodeIterator)
        output += TAB_IN + "unsigned _pg_i = 0;\n";

    if(needs2ndDecodeIterator)
        output += TAB_IN + "unsigned _pg_j = 0;\n";

    for(int i=0; i<encodables.length(); i++)
    {
        ProtocolFile::makeLineSeparator(output);
        output += encodables[i]->getMapEncodeString();
    }

    ProtocolFile::makeLineSeparator(output);

    output += "\n";
    if(support.language == ProtocolSupport::c_language)
        output += "}// mapEncode" + typeName + "\n";
    else
        output += "}// " + typeName + "::mapEncode\n";

    return output;

}// ProtocolStructure::getMapEncodeFunctionString


/*!
 * Get the signature of the mapDecode function.
 * \param insource should be true to indicate this signature is in source code.
 * \return the signature of the comparison function.
 */
QString ProtocolStructure::getMapDecodeFunctionSignature(bool insource) const
{
    if(support.language == ProtocolSupport::c_language)
    {
        if(insource)
            return "void mapDecode" + typeName + "(const QString& _pg_prename, const QVariantMap& _pg_map, " + structName + "* _pg_user)";
        else
            return "void mapDecode" + typeName + "(const QString& prename, const QVariantMap& map, " + structName + "* user)";
    }
    else
    {
        if(insource)
            return "void " + typeName + "::mapDecode(const QString& _pg_prename, const QVariantMap& _pg_map)";
        else
            return "void mapDecode(const QString& prename, const QVariantMap& map)";
    }

}// ProtocolStructure::getMapDecodeFunctionSignature


/*!
 * Get the string that gives the prototype of the function used to decode this
 * structure from a map
 * \param spacing gives the spacing to offset each line.
 * \param includeChildren should be true to include the function prototypes of
 *        the children structures of this structure
 * \return the function string, which may be empty
 */
QString ProtocolStructure::getMapDecodeFunctionPrototype(const QString& spacing, bool includeChildren) const
{
    QString output;

    if(!mapEncode || (getNumberOfDecodeParameters() == 0))
        return output;

    if(includeChildren && (support.language == ProtocolSupport::c_language))
    {
        for(int i = 0; i < encodables.length();  i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if(!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);

            output += structure->getMapDecodeFunctionPrototype(spacing, includeChildren);
        }

        ProtocolFile::makeLineSeparator(output);
    }

    // My mapEncode function
    output += spacing + "//! Decode the contents of a " + typeName + " from a string Key:Value map\n";
    output += spacing + getMapDecodeFunctionSignature(false) + ";\n";

    return output;

}// ProtocolStructure::getMapDecodeeFunctionPrototype


/*!
 * Get the string that gives the function used to decode this structure from a map
 * \param includeChildren should be true to include the function prototypes of
 *        the children structures of this structure
 * \return the function string, which may be empty
 */
QString ProtocolStructure::getMapDecodeFunctionBody(bool includeChildren) const
{
    QString output;

    if(!mapEncode || (getNumberOfDecodeParameters() == 0))
        return output;

    if(includeChildren)
    {
        for(int i = 0; i < encodables.length(); i++)
        {
            ProtocolStructure* structure = dynamic_cast<ProtocolStructure*>(encodables.at(i));

            if (!structure)
                continue;

            ProtocolFile::makeLineSeparator(output);
            output += structure->getMapDecodeFunctionBody(includeChildren);
        }

        ProtocolFile::makeLineSeparator(output);
    }

    // My mapDecode function
    output += "/*!\n";
    output += " * Decode the contents of a " + typeName + " from a Key:Value string map\n";
    output += " * \\param _pg_prename is prepended to the key fields in the map\n";
    output += " * \\param _pg_map is a reference to the map\n";
    if(support.language == ProtocolSupport::c_language)
        output += " * \\param _pg_user is the structure to decode\n";
    output += " */\n";
    output += getMapDecodeFunctionSignature(true) + "\n";
    output += "{\n";
    output += TAB_IN + "QString key;\n";
    output += TAB_IN + "bool ok = false;\n";

    if(needsDecodeIterator)
        output += TAB_IN + "unsigned _pg_i = 0;\n";

    if(needs2ndDecodeIterator)
        output += TAB_IN + "unsigned _pg_j = 0;\n";

    for(int i=0; i<encodables.length(); i++)
    {
        ProtocolFile::makeLineSeparator(output);
        output += encodables[i]->getMapDecodeString();
    }

    ProtocolFile::makeLineSeparator(output);

    output += "\n";
    if(support.language == ProtocolSupport::c_language)
        output += "}// mapDecode" + typeName + "\n";
    else
        output += "}// " + typeName + "::mapDecode\n";

    return output;

}// ProtocolStructure::getMapDecodeFunctionString


/*!
 * Get details needed to produce documentation for this encodable.
 * \param parentName is the name of the parent which will be pre-pended to the name of this encodable
 * \param startByte is the starting byte location of this encodable, which will be updated for the following encodable.
 * \param bytes is appended for the byte range of this encodable.
 * \param names is appended for the name of this encodable.
 * \param encodings is appended for the encoding of this encodable.
 * \param repeats is appended for the array information of this encodable.
 * \param comments is appended for the description of this encodable.
 */
void ProtocolStructure::getDocumentationDetails(QList<int>& outline, QString& startByte, QStringList& bytes, QStringList& names, QStringList& encodings, QStringList& repeats, QStringList& comments) const
{
    QString description;

    QString maxEncodedLength = encodedLength.maxEncodedLength;

    // See if we can replace any enumeration names with values
    maxEncodedLength = parser->replaceEnumerationNameWithValue(maxEncodedLength);

    // The byte after this one
    QString nextStartByte = EncodedLength::collapseLengthString(startByte + "+" + maxEncodedLength);

    // The length data
    if(maxEncodedLength.isEmpty() || (maxEncodedLength.compare("1") == 0))
        bytes.append(startByte);
    else
    {
        QString endByte = EncodedLength::subtractOneFromLengthString(nextStartByte);

        // The range of the data
        bytes.append(startByte + "..." + endByte);
    }

    // The name information
    outline.last() += 1;
    QString outlineString;
    outlineString.setNum(outline.at(0));
    for(int i = 1; i < outline.size(); i++)
        outlineString += "." + QString().setNum(outline.at(i));
    outlineString += ")" + title;
    names.append(outlineString);

    // Encoding is blank for structures
    encodings.append(QString());

    // The repeat/array column
    if(array.isEmpty())
        repeats.append(QString());
    else
        repeats.append(getRepeatsDocumentationDetails());

    // The commenting
    description += comment;

    if(!dependsOn.isEmpty())
    {
        if(!description.endsWith('.'))
            description += ".";

        if(dependsOnValue.isEmpty())
            description += " Only included if " + dependsOn + " is non-zero.";
        else
        {
            if(dependsOnCompare.isEmpty())
                description += " Only included if " + dependsOn + " equal to " + dependsOnValue + ".";
            else
                description += " Only included if " + dependsOn + " " + dependsOnCompare + " " + dependsOnValue + ".";
        }
    }

    if(description.isEmpty())
        comments.append(QString());
    else
        comments.append(description);

    // Now go get the sub-encodables
    getSubDocumentationDetails(outline, startByte, bytes, names, encodings, repeats, comments);

    // These two may be the same, but they won't be if this structure is repeated.
    startByte = nextStartByte;

}// ProtocolStructure::getDocumentationDetails


/*!
 * Get details needed to produce documentation for this encodable.
 * \param parentName is the name of the parent which will be pre-pended to the name of this encodable
 * \param startByte is the starting byte location of this encodable, which will be updated for the following encodable.
 * \param bytes is appended for the byte range of this encodable.
 * \param names is appended for the name of this encodable.
 * \param encodings is appended for the encoding of this encodable.
 * \param repeats is appended for the array information of this encodable.
 * \param comments is appended for the description of this encodable.
 */
void ProtocolStructure::getSubDocumentationDetails(QList<int>& outline, QString& startByte, QStringList& bytes, QStringList& names, QStringList& encodings, QStringList& repeats, QStringList& comments) const
{
    outline.append(0);

    // Go get the sub-encodables
    for(int i = 0; i < encodables.length(); i++)
        encodables.at(i)->getDocumentationDetails(outline, startByte, bytes, names, encodings, repeats, comments);

    outline.removeLast();

}// ProtocolStructure::getSubDocumentationDetails

