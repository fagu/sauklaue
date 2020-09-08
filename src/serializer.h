#ifndef SERIALIZER_H
#define SERIALIZER_H

#include "document.h"

class Serializer
{
public:
	static void save(Document* doc, QDataStream &stream);
	static std::unique_ptr<Document> load(QDataStream &stream);
};

#endif // SERIALIZER_H
