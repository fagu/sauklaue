#include "document.h"

#include "file.pb.h"

#include <QtWidgets>

// sauklaue-conc FILE1 ... FILEn OUTFILE
int main(int argc, char *argv[])
{
	// Verify that the version of the library that we linked against is
	// compatible with the version of the headers we compiled against.
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	
	/*{
		auto layer = std::make_unique<NormalLayer>();
		for (int i = 0; i < 40000; i++) {
			auto stroke = std::make_unique<PenStroke>(10, Color::BLACK);
			for (int j = 0; j < 100; j++) {
				stroke->push_back(Point(0,0));
			}
			layer->add_stroke(std::move(stroke));
		}
		while(1);
	}*/
	
	assert(argc >= 4);
	
	std::vector<std::unique_ptr<Document> > in_docs;
	for (int i = 1; i+1 < argc; i++) {
		QString fileName = argv[i];
		QFile file(fileName);
		if (!file.open(QFile::ReadOnly)) {
			qCritical() << "Cannot read file" << fileName << file.errorString();
			return 1;
		}

		QDataStream in(&file);
		in_docs.emplace_back(Document::load(in));
		in_docs.back().reset();
	}
	while(1);
	
	std::unique_ptr<Document> out_doc = Document::concatenate(in_docs);
	
	QString fileName = argv[argc-1];
	QSaveFile file(fileName);
	if (file.open(QFile::WriteOnly)) {
		QDataStream out(&file);
		out_doc->save(out);
		if (!file.commit()) {
			qCritical() << "Cannot write file" << fileName << file.errorString();
		}
	} else {
		qCritical() << "Cannot open file for writing" << fileName << file.errorString();
	}
	
	// Optional:  Delete all global objects allocated by libprotobuf.
	google::protobuf::ShutdownProtobufLibrary();
	
	return 0;
}

