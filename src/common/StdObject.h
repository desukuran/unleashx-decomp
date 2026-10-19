// CStdObject: root of UnleashX's small MFC-like object hierarchy (CObList,
// CXmlElement, ...). Its vtable (0x0024B5E8) holds only the deleting destructor.
#pragma once

class CStdObject
{
public:
	virtual ~CStdObject() {}  // 0x0001C988 (scalar deleting destructor)
};
