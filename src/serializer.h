#ifndef SERIALIZER_H
#define SERIALIZER_H

#include "document.h"

class SauklaueReadException : public std::exception {
public:
	SauklaueReadException(QString reason) :
	    m_reason(reason) {
	}
	QString reason() const {
		return m_reason;
	}

private:
	QString m_reason;
};

class Serializer {
public:
	static void save(Document* doc, QDataStream& stream);
	static std::unique_ptr<Document> load(QDataStream& stream);  // May throw SauklaueReadException
};

#endif  // SERIALIZER_H
