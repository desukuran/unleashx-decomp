// CObList: a doubly linked list of void* that matches MFC's CObList/CPtrList
// (afxcoll), probably copied from it since MFC isn't available on the Xbox.
// Nodes come from CPlex blocks of m_nBlockSize and are recycled through a
// free list. Only the members the binary uses are reconstructed.
#pragma once

#include "StdObject.h"

typedef struct __POSITION { } *POSITION;

struct CPlex
{
	CPlex *pNext;
	// followed by the element data

	void *data() { return this + 1; }

	static CPlex *Create(CPlex *&head, unsigned int nMax, unsigned int cbElement);
	static void FreeDataChain(CPlex *p);
};

class CObList : public CStdObject
{
protected:
	struct CNode
	{
		CNode *pNext;
		CNode *pPrev;
		void *data;
	};

public:
	explicit CObList(int nBlockSize = 10);
	virtual ~CObList();  // 0x0001CAA6

	int GetCount() const { return m_nCount; }
	bool IsEmpty() const { return m_nCount == 0; }

	POSITION GetHeadPosition() const { return (POSITION)m_pNodeHead; }
	void *&GetNext(POSITION &pos)
	{
		CNode *node = (CNode *)pos;
		pos = (POSITION)node->pNext;
		return node->data;
	}

	void *GetHead() const { return m_pNodeHead->data; }

	void AddTail(void *newElement);
	void AddHead(void *newElement);                                // 0x0002A7A9
	// Inserts before position (at the head if NULL).
	void InsertBefore(POSITION position, void *newElement);        // 0x0002A7D3
	void *RemoveHead();
	void RemoveAt(POSITION position);
	void RemoveAll();

protected:
	CNode *NewNode(CNode *pPrev, CNode *pNext);
	void FreeNode(CNode *node);

	// Layout (0x1C bytes): vtable at +0x00
	CNode *m_pNodeHead;   // +0x04
	CNode *m_pNodeTail;   // +0x08
	int m_nCount;         // +0x0C
	CNode *m_pNodeFree;   // +0x10
	CPlex *m_pBlocks;     // +0x14
	int m_nBlockSize;     // +0x18
};
