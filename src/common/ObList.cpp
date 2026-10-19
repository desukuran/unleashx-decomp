#include "ObList.h"

#include <cstddef>

// 0x0001C956
CPlex *CPlex::Create(CPlex *&head, unsigned int nMax, unsigned int cbElement)
{
	CPlex *p = (CPlex *)::operator new(sizeof(CPlex) + nMax * cbElement);
	if (p != NULL)
	{
		p->pNext = head;
		head = p;
	}
	return p;
}

// 0x0001C972
void CPlex::FreeDataChain(CPlex *p)
{
	while (p != NULL)
	{
		CPlex *next = p->pNext;
		::operator delete(p);
		p = next;
	}
}

// Inlined into CXmlElement::CXmlElement (0x0001CD9D)
CObList::CObList(int nBlockSize)
	: m_pNodeHead(NULL), m_pNodeTail(NULL), m_nCount(0), m_pNodeFree(NULL), m_pBlocks(NULL),
	  m_nBlockSize(nBlockSize)
{
}

// 0x0001CAA6
CObList::~CObList()
{
	RemoveAll();
}

// 0x0001C9F1
void CObList::RemoveAll()
{
	m_nCount = 0;
	m_pNodeFree = NULL;
	m_pNodeTail = NULL;
	m_pNodeHead = NULL;
	CPlex::FreeDataChain(m_pBlocks);
	m_pBlocks = NULL;
}

// 0x0001CA3C
CObList::CNode *CObList::NewNode(CNode *pPrev, CNode *pNext)
{
	if (m_pNodeFree == NULL)
	{
		// Add another block and thread its nodes onto the free list, last first.
		CPlex *block = CPlex::Create(m_pBlocks, m_nBlockSize, sizeof(CNode));
		CNode *node = (CNode *)block->data() + (m_nBlockSize - 1);
		for (int i = m_nBlockSize - 1; i >= 0; i--, node--)
		{
			node->pNext = m_pNodeFree;
			m_pNodeFree = node;
		}
	}

	CNode *node = m_pNodeFree;
	m_pNodeFree = m_pNodeFree->pNext;
	node->pPrev = pPrev;
	node->pNext = pNext;
	m_nCount++;
	node->data = NULL;
	return node;
}

// 0x0001CA8F
void CObList::FreeNode(CNode *node)
{
	node->pNext = m_pNodeFree;
	m_pNodeFree = node;
	m_nCount--;
	if (m_nCount == 0)
		RemoveAll();
}

// 0x0001C9C8
void CObList::AddTail(void *newElement)
{
	CNode *node = NewNode(m_pNodeTail, NULL);
	node->data = newElement;
	if (m_pNodeTail != NULL)
		m_pNodeTail->pNext = node;
	else
		m_pNodeHead = node;
	m_pNodeTail = node;
}

// 0x0001C9A5
void *CObList::RemoveHead()
{
	CNode *oldNode = m_pNodeHead;
	void *ret = oldNode->data;

	m_pNodeHead = oldNode->pNext;
	if (m_pNodeHead != NULL)
		m_pNodeHead->pPrev = NULL;
	else
		m_pNodeTail = NULL;
	FreeNode(oldNode);
	return ret;
}

// 0x0001CA0D
void CObList::RemoveAt(POSITION position)
{
	CNode *oldNode = (CNode *)position;

	if (oldNode == m_pNodeHead)
		m_pNodeHead = oldNode->pNext;
	else
		oldNode->pPrev->pNext = oldNode->pNext;

	if (oldNode == m_pNodeTail)
		m_pNodeTail = oldNode->pPrev;
	else
		oldNode->pNext->pPrev = oldNode->pPrev;

	FreeNode(oldNode);
}
