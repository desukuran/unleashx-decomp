#include "XmlDocument.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../common/StrUtil.h"

int g_nXmlElementCount;  // 0x0026C614

static int FindChar(const std::string &s, char ch, std::string::size_type start = 0)
{
	std::string::size_type pos = s.find(ch, start);
	return pos == std::string::npos ? -1 : (int)pos;
}

static void Unescape(std::string &s)
{
	StrReplaceAll(s, "&amp;", "&");
	StrReplaceAll(s, "&lt;", "<");
	StrReplaceAll(s, "&gt;", ">");
}

static void Escape(std::string &s)
{
	StrReplaceAll(s, "&", "&amp;");
	StrReplaceAll(s, "<", "&lt;");
	StrReplaceAll(s, ">", "&gt;");
}

// Strips one pair of surrounding quotes and unescapes; '""' becomes empty.
// Shared by CXmlAttribute::SetValue and CXmlElement::SetValue.
static void StoreQuotedValue(std::string &value)
{
	if (value.size() < 3)
	{
		if (value.compare("\"\"") == 0)
			value.clear();
		return;
	}
	if (StrLeft(value, 1).compare("\"") == 0)
		value = StrMid(value, 1, (int)value.size() - 2);
	Unescape(value);
}

//
// CXmlAttribute
//

// 0x0001CADA
CXmlAttribute::CXmlAttribute(const char *name, const char *value)
{
	m_strName = name;
	if (value != NULL)
	{
		m_strValue = value;
		Unescape(m_strValue);
	}
}

// 0x0001CB45
CXmlAttribute::~CXmlAttribute()
{
	m_strName.clear();
	m_strValue.clear();
}

// 0x0001CB7C
void CXmlAttribute::SetValue(const char *value)
{
	m_strValue = value;
	StoreQuotedValue(m_strValue);
}

// 0x0001CC44
std::string CXmlAttribute::GetEscapedValue() const
{
	std::string value(m_strValue);
	if (value.size() > 0)
		Escape(value);
	return value;
}

//
// CXmlElement
//

// 0x0001CD9D
CXmlElement::CXmlElement()
	: m_children(10), m_pParent(NULL), m_findPos(NULL)
{
}

// 0x0001CE1E
CXmlElement::~CXmlElement()
{
	while (m_children.GetCount() != 0)
	{
		CXmlElement *child = (CXmlElement *)m_children.RemoveHead();
		if (child != NULL)
			delete child;
	}
	DeleteAttributes();
	m_comments.clear();
}

// 0x0001CEBC
void CXmlElement::DeleteAttributes()
{
	for (std::vector<CXmlAttribute *>::iterator it = m_attributes.begin(); it != m_attributes.end(); ++it)
	{
		if (*it != NULL)
			delete *it;
	}
	m_attributes.clear();
}

// 0x0001CCD0
void CXmlElement::RebuildAttributeText()
{
	m_strAttributes.clear();
	std::string value;
	for (std::vector<CXmlAttribute *>::iterator it = m_attributes.begin(); it != m_attributes.end(); ++it)
	{
		CXmlAttribute *attr = *it;
		if (attr == NULL || attr->m_strValue.size() == 0)
			continue;
		value = attr->m_strValue;
		StrReplaceAll(value, "\"", "\\\"");
		m_strAttributes += attr->m_strName;
		m_strAttributes += "=\"";
		m_strAttributes += value;
		m_strAttributes += "\" ";
	}
	m_strAttributes = StrTrimRight(m_strAttributes);
}

// 0x0001CEE9
std::string CXmlElement::GetEscapedValue() const
{
	std::string value(m_strValue);
	if (value.size() > 0)
		Escape(value);
	return value;
}

// 0x0001CF59
// Returns the new attribute count, or -1 if nothing changed (same value) or
// the attribute was removed because value is empty.
int CXmlElement::SetAttribute(const char *name, const char *value)
{
	if (value != NULL && strlen(value) != 0)
	{
		CXmlAttribute *attr = FindAttribute(name);
		if (attr == NULL)
		{
			attr = new CXmlAttribute(name, value);
			if (attr != NULL)
				m_attributes.push_back(attr);
		}
		else
		{
			if (StrCmpNoCase(attr->m_strValue, value) == 0)
				return -1;
			attr->SetValue(value);
		}
		RebuildAttributeText();
		return (int)m_attributes.size();
	}

	RemoveAttribute(name);
	return -1;
}

// 0x0001CFFF
CXmlElement *CXmlElement::FindFirstChild(const char *name)
{
	m_findPos = NULL;
	m_strFindName = name;

	POSITION pos = m_children.GetHeadPosition();
	CXmlElement *child;
	do
	{
		if (pos == NULL)
			return NULL;
		child = (CXmlElement *)m_children.GetNext(pos);
	} while (child == NULL || StrCmpNoCase(child->m_strName, name) != 0);

	m_findPos = pos;
	return child;
}

// 0x0001D052
CXmlElement *CXmlElement::FindNextChild()
{
	for (;;)
	{
		if (m_findPos == NULL)
			return NULL;
		CXmlElement *child = (CXmlElement *)m_children.GetNext(m_findPos);
		if (child != NULL && StrCmpNoCase(child->m_strName, m_strFindName.c_str()) == 0)
			return child;
	}
}

// 0x0001D09C
std::string CXmlElement::GetComment(int index) const
{
	if (index >= 0 && index < (int)m_comments.size())
		return m_comments[index];
	return std::string("");
}

// 0x0001D0D5
CXmlAttribute *CXmlElement::FindAttribute(const char *name) const
{
	for (std::vector<CXmlAttribute *>::const_iterator it = m_attributes.begin(); it != m_attributes.end(); ++it)
	{
		if (*it != NULL && StrCmpNoCase((*it)->m_strName, name) == 0)
			return *it;
	}
	return NULL;
}

// 0x0001D14D
std::string CXmlElement::GetAttribute(const char *name) const
{
	CXmlAttribute *attr = FindAttribute(name);
	if (attr == NULL)
		return std::string("");
	return attr->m_strValue;
}

// 0x0001D178
CXmlElement *CXmlElement::GetFirstChild()
{
	m_findPos = NULL;
	POSITION pos = m_children.GetHeadPosition();
	if (pos == NULL)
		return NULL;
	CXmlElement *child = (CXmlElement *)m_children.GetNext(pos);
	m_findPos = pos;
	return child;
}

// 0x000213F2
// Continues a GetFirstChild()/FindFirstChild() walk without name matching.
CXmlElement *CXmlElement::GetNextChild()
{
	if (m_findPos == NULL)
		return NULL;
	return (CXmlElement *)m_children.GetNext(m_findPos);
}

// 0x0001D193
BOOL CXmlElement::RemoveAttribute(const char *name)
{
	for (std::vector<CXmlAttribute *>::iterator it = m_attributes.begin(); it != m_attributes.end(); ++it)
	{
		CXmlAttribute *attr = *it;
		if (attr != NULL && StrCmpNoCase(attr->m_strName, name) == 0)
		{
			delete attr;
			m_attributes.erase(it);
			RebuildAttributeText();
			return TRUE;
		}
	}
	return FALSE;
}

// 0x0001D202
void CXmlElement::SetValue(const char *value)
{
	m_strValue = value;
	StrTrimRight(StrTrimLeft(m_strValue));
	StoreQuotedValue(m_strValue);
}

// 0x0001D2EA
void CXmlElement::AddComment(const char *text)
{
	if (text != NULL && strlen(text) != 0)
		m_comments.push_back(std::string(text));
}

//
// CXmlDocument
//

// 0x0001D362
CXmlDocument::CXmlDocument()
	: m_bModified(false), m_nIndent(-1), m_pCurrent(NULL)
{
}

// 0x0001D38D
CXmlDocument::~CXmlDocument()
{
	Clear();
}

// 0x0001E0AA
// Note: the root's attributes and comments are left as they are.
void CXmlDocument::Clear()
{
	while (m_root.m_children.GetCount() != 0)
	{
		CXmlElement *child = (CXmlElement *)m_root.m_children.RemoveHead();
		if (child != NULL)
			delete child;
	}
	m_pCurrent = &m_root;
	m_root.m_pParent = NULL;
	m_root.m_strName.clear();
	m_root.m_strValue.clear();
	m_root.m_strAttributes.clear();
	m_root.m_strFindName.clear();
	m_root.m_findPos = NULL;
}

// 0x0001DD78
BOOL CXmlDocument::Load(const char *fileName)
{
	std::string text;

	FILE *fp = fopen(fileName, "rb");
	if (fp == NULL)
		return FALSE;

	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *buf = new char[size];
	if (buf == NULL)
	{
		fclose(fp);
		return FALSE;
	}
	if (fread(buf, 1, size, fp) == 0)
	{
		delete[] buf;
		fclose(fp);
		return FALSE;
	}
	fclose(fp);

	text.assign(buf, size);
	StrRemoveChar(text, '\n');
	StrRemoveChar(text, '\r');
	StrRemoveChar(text, '\t');
	StrReplaceChar(text, (char)0x94, '"');
	delete[] buf;

	m_strFileName = fileName;
	return Parse(text.c_str());
}

// 0x0001DE81
BOOL CXmlDocument::LoadFromString(const char *source)
{
	std::string text(source);
	if (text.size() == 0)
		return FALSE;

	StrRemoveChar(text, '\n');
	StrRemoveChar(text, '\r');
	StrRemoveChar(text, '\t');
	StrReplaceChar(text, (char)0x94, '"');
	return Parse(text.c_str());
}

// 0x0001DEF4
BOOL CXmlDocument::Refresh()
{
	std::string text = ToString();
	return LoadFromString(text.c_str());
}

// 0x0001DF31
BOOL CXmlDocument::Save(BOOL markModified)
{
	if (m_strFileName.size() == 0)
		return FALSE;
	if (markModified)
		m_bModified = true;
	return SaveAs(m_strFileName.c_str());
}

// 0x0001DF5E
BOOL CXmlDocument::SaveAs(const char *fileName)
{
	std::string text = ToString();

	SetFileAttributesA(fileName, FILE_ATTRIBUTE_NORMAL);
	FILE *fp = fopen(fileName, "w+b");
	if (fp == NULL || text.size() == 0)
	{
		// The original leaks fp when the text is empty.
		OutputDebugStringA("Unable to save XML document.\n");
		return FALSE;
	}

	// text ends with the '\0' that ToString() appends, so the file does too.
	fwrite(text.c_str(), text.size(), 1, fp);
	fclose(fp);
	m_bModified = false;

	// The original assigns from _strdup() and never frees the copy.
	m_strFileName = fileName;
	return TRUE;
}

// 0x0001D3BD
std::string CXmlDocument::ToString()
{
	std::string text;
	text = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n";

	std::string body;
	m_nIndent = -1;
	SerializeElement(&m_root, body);
	text += body;
	text.append(1, '\0');
	return text;
}

// 0x0001E1C9
BOOL CXmlDocument::SerializeElement(CXmlElement *element, std::string &out)
{
	int i;

	m_nIndent++;
	out.clear();
	for (i = 0; i < m_nIndent; i++)
		out += "\t";

	out += "<";
	out += element->m_strName;
	if (element->m_attributes.size() != 0)
	{
		for (std::vector<CXmlAttribute *>::iterator it = element->m_attributes.begin();
		     it != element->m_attributes.end(); ++it)
		{
			CXmlAttribute *attr = *it;
			if (attr == NULL || attr->m_strValue.size() == 0)
				continue;
			out += " ";
			out += attr->m_strName;
			out += "=\"";
			out += attr->GetEscapedValue();
			out += "\"";
		}
	}
	out += ">";

	if (element->m_strValue.size() == 0)
	{
		if (element->GetFirstChild() != NULL)
			out += "\r\n";
	}
	else
	{
		out += element->GetEscapedValue();
	}

	POSITION pos = element->m_children.GetHeadPosition();
	while (pos != NULL)
	{
		CXmlElement *child = (CXmlElement *)element->m_children.GetNext(pos);
		std::string childText;
		if (child->m_strName.size() != 0)
			SerializeElement(child, childText);
		g_nXmlElementCount++;
		out += childText;
	}
	if (element == &m_root)
		g_nXmlElementCount = 1;

	if (element->m_strValue.size() == 0 && element->GetFirstChild() != NULL)
	{
		for (i = 0; i < m_nIndent; i++)
			out += "\t";
	}
	out += "</";
	out += element->m_strName;
	out += ">\r\n";

	for (i = 0; i < (int)element->m_comments.size(); i++)
	{
		std::string comment = element->GetComment(i);
		if (comment.size() == 0)
			continue;

		std::string indent;
		indent = "";
		for (int j = 0; j < m_nIndent; j++)
			indent += "\t";

		out += indent;
		out += "<!-- ";
		out += comment.c_str();
		if (StrRight(comment, 1).compare("\n") == 0)
			out += indent;
		out += " -->\r\n";
	}

	m_nIndent--;
	return TRUE;
}

// 0x0001E041
// Starts a FindNextChild() search on parent (the root if NULL) and makes the
// first match the current element.
CXmlElement *CXmlDocument::FindElement(CXmlElement *parent, const char *name)
{
	if (parent == NULL)
	{
		parent = &m_root;
		m_pCurrent = parent;
	}
	parent->m_findPos = NULL;
	parent->m_strFindName = name;

	POSITION pos = parent->m_children.GetHeadPosition();
	CXmlElement *child;
	do
	{
		if (pos == NULL)
			return NULL;
		child = (CXmlElement *)parent->m_children.GetNext(pos);
	} while (StrCmpNoCase(child->m_strName, name) != 0);

	parent->m_findPos = pos;
	m_pCurrent = child;
	return child;
}

// 0x00021DC2
CXmlElement *CXmlDocument::FindNextElement(CXmlElement *parent)
{
	if (parent == NULL)
		return NULL;
	CXmlElement *child;
	do
	{
		do
		{
			if (parent->m_findPos == NULL)
				return NULL;
			child = (CXmlElement *)parent->m_children.GetNext(parent->m_findPos);
		} while (child == NULL);
	} while (StrCmpNoCase(child->m_strName, parent->m_strFindName.c_str()) != 0);
	m_pCurrent = child;
	return child;
}

// 0x00021D93
BOOL CXmlElement::RemoveChild(CXmlElement *child)
{
	if (child == NULL)
		return FALSE;
	POSITION pos = m_children.GetHeadPosition();
	while (pos != NULL)
	{
		POSITION current = pos;
		if (m_children.GetNext(pos) == child)
		{
			m_children.RemoveAt(current);
			delete child;
			return TRUE;
		}
	}
	return FALSE;
}

// 0x000265F2
// Depth-first search below parent (the root if NULL): first the direct
// children, then each child's subtree.
CXmlElement *CXmlDocument::FindElementRecursive(const char *name, CXmlElement *parent)
{
	if (parent == NULL)
	{
		parent = &m_root;
		m_pCurrent = parent;
	}

	CXmlElement *child;
	for (child = parent->GetFirstChild(); child != NULL; child = parent->GetNextChild())
	{
		if (StrCmpNoCase(child->m_strName, name) == 0)
			return child;
	}

	for (child = parent->GetFirstChild(); child != NULL; child = parent->GetNextChild())
	{
		CXmlElement *found = FindElementRecursive(name, child);
		if (found != NULL)
			return found;
	}
	return NULL;
}

// 0x00023319
// Like CXmlElement::GetFirstChild, but also makes the child current.
CXmlElement *CXmlDocument::GetFirstChild(CXmlElement *parent)
{
	if (parent == NULL)
	{
		parent = &m_root;
		m_pCurrent = parent;
	}
	parent->m_findPos = NULL;
	POSITION pos = parent->m_children.GetHeadPosition();
	if (pos == NULL)
		return NULL;
	CXmlElement *child = (CXmlElement *)parent->m_children.GetNext(pos);
	parent->m_findPos = pos;
	m_pCurrent = child;
	return child;
}

// 0x00023347
CXmlElement *CXmlDocument::GetNextChild(CXmlElement *parent)
{
	if (parent == NULL)
	{
		parent = &m_root;
		m_pCurrent = parent;
	}
	if (parent->m_findPos == NULL)
		return NULL;
	CXmlElement *child = (CXmlElement *)parent->m_children.GetNext(parent->m_findPos);
	m_pCurrent = child;
	return child;
}

// 0x0001DD1A
// Removing the root clears the whole document.
BOOL CXmlDocument::RemoveElement(CXmlElement *element)
{
	if (element == NULL)
		return FALSE;

	if (element == &m_root)
	{
		Clear();
		return m_root.m_strName.size() == 0;
	}

	CXmlElement *parent = element->m_pParent;
	if (parent == NULL)
	{
		delete element;
		return FALSE;
	}

	POSITION pos = parent->m_children.GetHeadPosition();
	while (pos != NULL)
	{
		POSITION at = pos;
		if (parent->m_children.GetNext(pos) == element)
		{
			parent->m_children.RemoveAt(at);
			delete element;
			return TRUE;
		}
	}
	return FALSE;
}

// 0x0001E115
// Extracts the tag's contents (name and attribute text) without the angle
// brackets and classifies it.
int CXmlDocument::ParseTag(const std::string &tag, std::string &name)
{
	name.clear();
	if (tag.size() == 0)
		return TAG_INVALID;

	int type = TAG_OPEN;
	char prev = '0';
	for (int i = 0; i < (int)tag.size(); i++)
	{
		char c = tag[i];
		if (prev == '<')
		{
			if (c == '!' || c == '?')
				return TAG_INVALID;
			if (c == '/')
				type = TAG_CLOSE;
			else if (c != '<' && c != '>')
				name += c;
		}
		else if (prev == '/' && c == '>')
		{
			type = TAG_EMPTY;
			if (name.size() != 0)
				name.erase(name.size() - 1, 1);  // drop the '/'
		}
		else if (c != '<' && c != '>')
		{
			name += c;
		}
		prev = c;
	}
	return type;
}

// Collapses " =" and "= " to "=" so pairs split cleanly.
void NormalizeXmlAttributeText(std::string &text)
{
	while (text.find(" =") != std::string::npos)
		StrReplaceAll(text, " =", "=");
	while (text.find("= ") != std::string::npos)
		StrReplaceAll(text, "= ", "=");
}

// Splits 'a="b" c=d' into attributes on element. Each copy of this loop in
// the original treats quoted values slightly differently (XmlQuoteRule). In
// all of them an unquoted value ends at the next space, so an unquoted last
// attribute is dropped.
void ParseXmlAttributeList(CXmlElement *element, std::string rest, XmlQuoteRule rule)
{
	std::string key, value;
	int pos = FindChar(rest, '=');
	while (pos != -1)
	{
		key = StrLeft(rest, pos);
		rest = StrMid(rest, pos + 1, (int)rest.size() - (pos + 1));

		if (StrLeft(rest, 1).compare("\"") == 0)
		{
			rest = StrMid(rest, 1, (int)rest.size() - 1);
			pos = FindChar(rest, '"');
			if (rule == XML_QUOTES_DOUBLED)
			{
				while (pos > 0 && StrMid(rest, pos - 1, 1).compare("\"") == 0)
					pos = FindChar(rest, '"', pos + 1);
			}
			else if (rule == XML_QUOTES_DOUBLED_BOUNDED)
			{
				while (pos >= 1 && pos + 1 < (int)rest.size() && StrMid(rest, pos - 1, 1).compare("\"") == 0)
					pos = FindChar(rest, '"', pos + 1);
			}
		}
		else
		{
			pos = FindChar(rest, ' ');
		}
		if (pos == -1)
			break;

		value = StrLeft(rest, pos);
		std::string after = StrMid(rest, pos + 1, (int)rest.size() - (pos + 1));
		rest = StrTrimLeft(after);
		pos = FindChar(rest, '=');
		if (value.size() != 0)
			element->SetAttribute(key.c_str(), value.c_str());
	}
}

// Parses element->m_strAttributes after Parse() split it off the tag name.
// The original inlines this twice: the root element allows "" inside quoted
// values, other elements end a quoted value at the next quote.
void CXmlDocument::ParseAttributes(CXmlElement *element, bool rootRules)
{
	std::string &text = element->m_strAttributes;
	StrTrimRight(StrTrimLeft(text));
	if (text.size() == 0)
		return;

	NormalizeXmlAttributeText(text);
	ParseXmlAttributeList(element, text, rootRules ? XML_QUOTES_DOUBLED : XML_QUOTES_SIMPLE);
	StrTrimRight(element->m_strAttributes);
}

// 0x0001D446
BOOL CXmlDocument::Parse(const char *text)
{
	Clear();
	m_strXml = text;

	CXmlElement *commentTarget = &m_root;
	bool inTag = false;
	std::string tag, value, tagText;

	int length = (int)m_strXml.size();
	for (int i = 0; i < length; i++)
	{
		char c = m_strXml[i];

		if (c == '<')
		{
			if (i + 2 < length && m_strXml[i + 1] == '!' && m_strXml[i + 2] == '-')
			{
				// Comment: skip to the "->" that ends it.
				std::string::size_type end = m_strXml.find("->", i + 1);
				i = end != std::string::npos ? (int)end + 1 : length;
				continue;
			}
			tag.append(1, '<');
			m_pCurrent->SetValue(value.c_str());
			value.clear();
			inTag = true;
		}
		else if (c == '>')
		{
			tag.append(1, '>');
			int type = ParseTag(tag, tagText);
			tag.clear();
			if (type == TAG_INVALID)
				continue;  // note: inTag stays set, so text up to the next '<' joins the next tag

			if (type == TAG_COMMENT || type == TAG_COMMENT2)
			{
				if (commentTarget == NULL)
					commentTarget = m_pCurrent;
				commentTarget->AddComment(tagText.c_str());
			}
			else if (type == TAG_OPEN || type == TAG_EMPTY)
			{
				StrRemoveChar(tagText, '\n');
				StrRemoveChar(tagText, '\r');

				CXmlElement *element;
				bool isRoot = m_root.m_strName.size() == 0;
				if (isRoot)
				{
					element = &m_root;
				}
				else
				{
					element = new CXmlElement();
					if (element == NULL)
						continue;
					element->m_pParent = m_pCurrent;
				}

				int space = FindChar(tagText, ' ');
				if (space == -1)
				{
					element->m_strName = tagText;
				}
				else
				{
					element->m_strName = StrLeft(tagText, space);
					element->m_strAttributes =
						StrMid(tagText, space + 1, (int)tagText.size() - (space + 1));
					ParseAttributes(element, isRoot);
				}

				if (!isRoot)
				{
					m_pCurrent->m_children.AddTail(element);
					m_pCurrent = element;
					commentTarget = element;
				}
			}

			if (type == TAG_CLOSE || type == TAG_EMPTY)
			{
				if (m_pCurrent->m_pParent != NULL)
					m_pCurrent = m_pCurrent->m_pParent;
			}
			inTag = false;
		}
		else
		{
			(inTag ? tag : value).append(1, c);
		}
	}

	m_pCurrent = &m_root;
	m_bModified = false;
	return TRUE;
}
