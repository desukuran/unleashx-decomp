// UnleashX's own XML parser. Used for Config.xml, Skin.xml, the language files
// and most other settings. It's a small character-level tokenizer rather than
// a real XML parser:
//  - Load() strips every \n, \r and \t from the file before parsing, and
//    turns CP1252 right double quotes (0x94) into '"'.
//  - Element and attribute names match case-insensitively.
//  - Values are stored unescaped (&amp; &lt; &gt;) and escaped again on save.
//  - Comments ("<!--") are skipped on load; elements can carry comments that
//    are written after their closing tag.
// Method names are reconstructed from behavior; the class names are the
// originals (from RTTI).
#pragma once

#include <string>
#include <vector>

#include "../common/ObList.h"
#include "../common/Platform.h"
#include "../common/StdObject.h"

// vtable 0x0024B6DC, size 0x3C
class CXmlAttribute
{
public:
	CXmlAttribute(const char *name, const char *value);   // 0x0001CADA
	virtual ~CXmlAttribute();                             // 0x0001CB45

	void SetValue(const char *value);                     // 0x0001CB7C
	std::string GetEscapedValue() const;                  // 0x0001CC44

	std::string m_strName;    // +0x04
	std::string m_strValue;   // +0x20 (unescaped)
};

// vtable 0x0024B6C4, size 0xD4
class CXmlElement : public CStdObject
{
public:
	CXmlElement();                                                // 0x0001CD9D
	virtual ~CXmlElement();                                       // 0x0001CE1E

	// Attributes
	int SetAttribute(const char *name, const char *value);        // 0x0001CF59
	CXmlAttribute *FindAttribute(const char *name) const;         // 0x0001D0D5
	std::string GetAttribute(const char *name) const;             // 0x0001D14D
	BOOL RemoveAttribute(const char *name);                       // 0x0001D193
	void DeleteAttributes();                                      // 0x0001CEBC
	void RebuildAttributeText();                                  // 0x0001CCD0

	// Text content
	void SetValue(const char *value);                             // 0x0001D202
	std::string GetEscapedValue() const;                          // 0x0001CEE9

	// Children. Iteration state lives in the element (m_findPos).
	CXmlElement *GetFirstChild();                                 // 0x0001D178
	CXmlElement *FindFirstChild(const char *name);                // 0x0001CFFF
	CXmlElement *FindNextChild();                                 // 0x0001D052
	CXmlElement *GetNextChild();                                  // 0x000213F2
	// Removes and deletes child. FALSE if it isn't a child of this element.
	BOOL RemoveChild(CXmlElement *child);                         // 0x00021D93
	std::string GetValue() const { return m_strValue; }           // 0x000213DE

	// Comments written after the closing tag on save
	void AddComment(const char *text);                            // 0x0001D2EA
	std::string GetComment(int index) const;                      // 0x0001D09C

	std::string m_strName;                     // +0x04
	std::string m_strAttributes;               // +0x20 raw 'a="b" c="d"' text
	std::string m_strValue;                    // +0x3C (unescaped)
	std::string m_strUnknown58;                // +0x58 not used by the XML code
	CObList m_children;                        // +0x74 CXmlElement*
	std::vector<CXmlAttribute *> m_attributes; // +0x90
	std::vector<std::string> m_comments;       // +0xA0
	CXmlElement *m_pParent;                    // +0xB0
	POSITION m_findPos;                        // +0xB4
	std::string m_strFindName;                 // +0xB8
};

// vtable 0x0024B6D4, size 0x11C
class CXmlDocument
{
public:
	enum TagType
	{
		TAG_INVALID = -1,   // empty, "<!...>" or "<?...?>"
		TAG_OPEN = 0,       // <name ...>
		TAG_CLOSE = 1,      // </name>
		TAG_EMPTY = 2,      // <name ... />
		TAG_COMMENT = 3,    // handled by Parse() but never produced by ParseTag()
		TAG_COMMENT2 = 4,
	};

	CXmlDocument();                                               // 0x0001D362
	virtual ~CXmlDocument();                                      // 0x0001D38D

	BOOL Load(const char *fileName);                              // 0x0001DD78
	BOOL LoadFromString(const char *text);                        // 0x0001DE81
	BOOL Parse(const char *text);                                 // 0x0001D446
	BOOL Refresh();                                               // 0x0001DEF4
	BOOL Save(BOOL markModified);                                 // 0x0001DF31
	BOOL SaveAs(const char *fileName);                            // 0x0001DF5E
	std::string ToString();                                       // 0x0001D3BD
	void Clear();                                                 // 0x0001E0AA

	CXmlElement *FindElement(CXmlElement *parent, const char *name);  // 0x0001E041
	// The next child of parent with the name of the last FindElement(parent, ...).
	CXmlElement *FindNextElement(CXmlElement *parent);            // 0x00021DC2
	CXmlElement *FindElementRecursive(const char *name, CXmlElement *parent);   // 0x000265F2
	CXmlElement *GetFirstChild(CXmlElement *parent);              // 0x00023319
	CXmlElement *GetNextChild(CXmlElement *parent);               // 0x00023347
	BOOL RemoveElement(CXmlElement *element);                     // 0x0001DD1A

	static int ParseTag(const std::string &tag, std::string &name);   // 0x0001E115

protected:
	BOOL SerializeElement(CXmlElement *element, std::string &out);    // 0x0001E1C9
	void ParseAttributes(CXmlElement *element, bool rootRules);

public:
	std::string m_strFileName;   // +0x04
	bool m_bModified;            // +0x20
	int m_nIndent;               // +0x24 nesting depth while serializing
	std::string m_strXml;        // +0x28 text passed to Parse()
	CXmlElement m_root;          // +0x44
	CXmlElement *m_pCurrent;     // +0x118
};

enum XmlQuoteRule
{
	XML_QUOTES_SIMPLE,          // a quoted value ends at the next '"'
	XML_QUOTES_DOUBLED,         // ...unless that quote follows another '"'
	XML_QUOTES_DOUBLED_BOUNDED, // same, but only while not at the last character
};

void NormalizeXmlAttributeText(std::string &text);
void ParseXmlAttributeList(CXmlElement *element, std::string text, XmlQuoteRule rule);

// Incremented for every element written by CXmlDocument::SerializeElement and
// reset to 1 after the root; nothing reads it. 0x0026C614
extern int g_nXmlElementCount;
