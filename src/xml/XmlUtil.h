// Helpers for building and copying XML trees in code (default menu, merging
// documents). Names reconstructed.
#pragma once

#include "XmlDocument.h"

// Creates <name attributes>value</name> under parent. attributes is raw
// text like 'Action="Settings" Sort="Off"'; value and attributes may be NULL.
CXmlElement *AddChildElement(CXmlElement *parent, const char *name, const char *value,
                             const char *attributes);                         // 0x000234C7

// Deep copy of an element: name, value, attributes, comments and children.
CXmlElement *CloneElement(CXmlElement *src);                                 // 0x0002A801

// Links child under parent.
BOOL AddChild(CXmlElement *parent, CXmlElement *child);                      // 0x0002A8F9
// Like AddChildElement, but right after `after` (at the end if NULL).
CXmlElement *InsertChildElement(CXmlElement *parent, CXmlElement *after, const char *name, const char *value,
                                const char *attributes);                     // 0x0002A913
// Same, and marks the document modified.
BOOL AppendElement(CXmlElement *parent, CXmlElement *child, CXmlDocument *doc);  // 0x0002AEA9

// Replaces all attributes with those parsed from text.
void SetAttributeText(CXmlElement *element, const char *text);               // 0x0002ABE1
