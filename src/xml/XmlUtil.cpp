#include "XmlUtil.h"

#include "../common/StrUtil.h"

// 0x000234C7
CXmlElement *AddChildElement(CXmlElement *parent, const char *name, const char *value,
                             const char *attributes)
{
	CXmlElement *element = new CXmlElement();
	if (element == NULL)
		return NULL;

	element->m_strName = name;
	StrTrimRight(StrTrimLeft(element->m_strName));
	if (value != NULL)
		element->SetValue(value);

	if (attributes != NULL)
	{
		element->m_strAttributes = attributes;
		if (element->m_strAttributes.size() > 0)
		{
			NormalizeXmlAttributeText(element->m_strAttributes);
			ParseXmlAttributeList(element, element->m_strAttributes, XML_QUOTES_SIMPLE);
		}
	}

	element->m_pParent = parent;
	parent->m_children.AddTail(element);
	return element;
}

// 0x0002A801
CXmlElement *CloneElement(CXmlElement *src)
{
	CXmlElement *copy = new CXmlElement();
	if (copy == NULL)
		return NULL;

	// Refresh the source's attribute text from its parsed attributes first.
	src->RebuildAttributeText();
	copy->m_strName = src->m_strName;
	copy->m_strValue = src->m_strValue;
	SetAttributeText(copy, src->m_strAttributes.c_str());

	for (int i = 0; i < (int)src->m_comments.size(); i++)
	{
		std::string comment = src->GetComment(i);
		if (comment.size() != 0)
			copy->AddComment(comment.c_str());
	}

	for (CXmlElement *child = src->GetFirstChild(); child != NULL; child = src->GetNextChild())
		AddChild(copy, CloneElement(child));

	return copy;
}

// 0x0002A8F9
BOOL AddChild(CXmlElement *parent, CXmlElement *child)
{
	if (child == NULL)
		return FALSE;
	child->m_pParent = parent;
	parent->m_children.AddTail(child);
	return TRUE;
}

// 0x0002AEA9
BOOL AppendElement(CXmlElement *parent, CXmlElement *child, CXmlDocument *doc)
{
	if (parent == NULL || child == NULL)
		return FALSE;
	child->m_pParent = parent;
	parent->m_children.AddTail(child);
	doc->m_bModified = true;
	return TRUE;
}

// 0x0002ABE1
// Note: the attribute text is normalized, but the original text (not the
// normalized copy) is what gets parsed.
void SetAttributeText(CXmlElement *element, const char *text)
{
	element->m_strAttributes = text;
	element->DeleteAttributes();
	if (text == NULL || element->m_strAttributes.size() == 0)
		return;

	NormalizeXmlAttributeText(element->m_strAttributes);
	ParseXmlAttributeList(element, std::string(text), XML_QUOTES_DOUBLED_BOUNDED);
}
