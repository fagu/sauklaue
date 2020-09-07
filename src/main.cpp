#include "mainwindow.h"

#include "file.pb.h"

#include <QApplication>

int main(int argc, char *argv[])
{
	// Verify that the version of the library that we linked against is
	// compatible with the version of the headers we compiled against.
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	
	QApplication app(argc, argv);
	MainWindow w;
	w.show();

	int res = app.exec();
	
	// Optional:  Delete all global objects allocated by libprotobuf.
	google::protobuf::ShutdownProtobufLibrary();
	
	return res;
}
