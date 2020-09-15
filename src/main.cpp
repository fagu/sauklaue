#include "mainwindow.h"

#include "serializer.h"

#include <iostream>

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QSaveFile>

std::unique_ptr<Document> read_document(QString infile) {
	QFile file(infile);
	if (!file.open(QFile::ReadOnly)) {
		std::cerr << "Error: Cannot read file " << infile.toStdString() << ": " << file.errorString().toStdString() << std::endl;
		exit(1);
	}
	QDataStream in(&file);
	try {
		return Serializer::load(in);
	} catch(const SauklaueReadException & e) {
		std::cerr << "Error: Cannot read file " << infile.toStdString() << ": " << e.reason().toStdString() << std::endl;
		exit(1);
	}
}

int gui_command(int argc, char **argv) {
	QApplication app(argc, argv);
	app.setWindowIcon(QIcon::fromTheme("application-x-sauklaue"));
	QCommandLineParser parser;
	parser.setApplicationDescription("Sauklaue (GUI)");
	parser.addHelpOption();
	parser.addPositionalArgument("file", "Input (sau) file to load");
	parser.process(app);
	QStringList files = parser.positionalArguments();
	if (files.size() != 0 && files.size() != 1)
		parser.showHelp(1);
	app.setApplicationDisplayName(QObject::tr("Sauklaue"));
	MainWindow w;
	w.show();
	if (files.size() == 1)
		w.loadFile(files[0]);
	return app.exec();
}

int export_command(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	QCommandLineParser parser;
	parser.setApplicationDescription("Export to PDF");
	parser.addHelpOption();
	parser.addPositionalArgument("source", "Input (sau) file");
	parser.addPositionalArgument("destination", "Output (pdf) file", "[destination]");
	parser.process(app);
	QStringList files = parser.positionalArguments();
	if (files.size() != 1 && files.size() != 2)
		parser.showHelp(1);
	QString infile = files[0];
	QString outfile;
	if (files.size() == 2) {
		outfile = files[1];
	} else {
		QFileInfo info(infile);
		outfile = info.path() + "/" + info.baseName() + ".pdf";
	}
	if (QFileInfo(infile) == QFileInfo(outfile)) {
		qDebug() << "Source and destination are the same file. Aborting...";
		return 1;
	}
	qDebug() << "Exporting" << infile << "to" << outfile;
	std::unique_ptr<Document> doc = read_document(infile);
	PDFExporter::save(doc.get(), outfile.toStdString());
	return 0;
}

/*int concatenate_command(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	QCommandLineParser parser;
	parser.setApplicationDescription("Concatenate documents");
	parser.addHelpOption();
	parser.addPositionalArgument("sources", "Input (sau) files (at least two files)", "sources...");
	parser.addPositionalArgument("destination", "Output (sau) file", "destination");
	parser.process(app);
	QStringList files = parser.positionalArguments();
	if (files.size() <= 2)
		parser.showHelp(1);
	std::vector<std::unique_ptr<Document> > in_docs;
	for (int i = 0; i+1 < files.size(); i++) {
		QString fileName = files[i];
		QFile file(fileName);
		if (!file.open(QFile::ReadOnly)) {
			std::cerr << "Cannot read file " << fileName.toStdString() << ": " << file.errorString().toStdString() << std::endl;
			return 1;
		}

		QDataStream in(&file);
		in_docs.emplace_back(Serializer::load(in));
	}
	
	std::unique_ptr<Document> out_doc = Document::concatenate(in_docs);
	
	QString outfile = files.back();
	QFile file(outfile);
	if (!file.open(QFile::WriteOnly)) {
		std::cerr << "Cannot open file " << outfile.toStdString() << " for writing: " << file.errorString().toStdString() << std::endl;
		return 1;
	}
	QDataStream out(&file);
	Serializer::save(out_doc.get(), out);
	return 0;
}*/

int save_command(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	QCommandLineParser parser;
	parser.setApplicationDescription("Open and then save a file. This is useful if you want to update the file to a newer file format.");
	parser.addHelpOption();
	parser.addPositionalArgument("source", "Input (sau) file");
	parser.addPositionalArgument("destination", "Output (sau) file (by default, the input file is overwritten)", "[destination]");
	parser.process(app);
	QStringList files = parser.positionalArguments();
	if (files.size() != 1 && files.size() != 2)
		parser.showHelp(1);
	QString infile = files[0];
	QString outfile;
	if (files.size() == 2) {
		outfile = files[1];
	} else {
		outfile = infile;
	}
	qDebug() << "Saving" << infile << "to" << outfile;
	std::unique_ptr<Document> doc = read_document(infile);
	QSaveFile file(outfile);
	if (!file.open(QSaveFile::WriteOnly)) {
		std::cerr << "Cannot open file " << outfile.toStdString() << " for writing: " << file.errorString().toStdString() << std::endl;
		return 1;
	}
	QDataStream out(&file);
	Serializer::save(doc.get(), out);
	if (!file.commit()) {
		std::cerr << "Cannot write file " << outfile.toStdString() << ": " << file.errorString().toStdString() << std::endl;
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int res;
	if (argc >= 2) {
		// Remove argv[1].
		int argcs = argc-1;
		char **argvs = new char*[argcs];
		argvs[0] = argv[0];
		for (int i = 1; i < argcs; i++)
			argvs[i] = argv[i+1];
		if (!strcmp(argv[1], "gui")) {
			res = gui_command(argcs, argvs);
		} else if (!strcmp(argv[1], "export")) {
			res = export_command(argcs, argvs);
		} /*else if (!strcmp(argv[1], "concatenate")) {
			res = concatenate_command(argcs, argvs);
		} */else if (!strcmp(argv[1], "save")) {
			res = save_command(argcs, argvs);
		} else {
			std::cerr << "Available commands:\n"
				<< "    " << argv[0] << " gui\n"
				<< "    " << argv[0] << " export\n"
// 				<< "    " << argv[0] << " concatenate\n"
				<< "    " << argv[0] << " save\n";
			res = 1;
		}
	} else {
		res = gui_command(argc, argv);
	}
	
	return res;
}
