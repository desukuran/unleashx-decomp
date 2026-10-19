// Host test: parse an UnleashX XML file with the reconstructed CXmlDocument,
// dump the element tree and the re-serialized text.
//   xml_roundtrip <file.xml>
#include <cstdio>

#include "../src/xml/XmlDocument.h"

static void Dump(CXmlElement *e, int depth)
{
	printf("%*s<%s>", depth * 2, "", e->m_strName.c_str());
	for (size_t i = 0; i < e->m_attributes.size(); i++)
		printf(" @%s=[%s]", e->m_attributes[i]->m_strName.c_str(), e->m_attributes[i]->m_strValue.c_str());
	if (!e->m_strValue.empty())
		printf(" value=[%s]", e->m_strValue.c_str());
	printf("\n");
	POSITION pos = e->m_children.GetHeadPosition();
	while (pos != NULL)
		Dump((CXmlElement *)e->m_children.GetNext(pos), depth + 1);
}

int main(int argc, char **argv)
{
	CXmlDocument doc;
	if (argc < 2 || !doc.Load(argv[1]))
	{
		fprintf(stderr, "load failed\n");
		return 1;
	}
	Dump(&doc.m_root, 0);
	std::string text = doc.ToString();
	printf("---- serialized (%zu bytes)\n%s\n", text.size(), text.c_str());
	return 0;
}
