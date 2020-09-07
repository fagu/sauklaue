#ifndef SAUKLAUE_H
#define SAUKLAUE_H

#include <memory>

#include <QMainWindow>
class QSessionManager;
class QUndoStack;

#include "pagewidget.h"

class sauklaue : public QMainWindow
{
	Q_OBJECT

public:
	explicit sauklaue(QWidget *parent = nullptr);
	
	void loadFile(const QString &fileName);
	
protected:
	void closeEvent(QCloseEvent *event) override;
	void moveEvent(QMoveEvent *event) override;

private slots:
	void newFile();
	void open();
	bool save();
	bool saveAs();
	void newPage();
	void deletePage();
	void nextPage();
	void previousPage();
	void documentWasModified();
	void exportPDF();
#ifndef QT_NO_SESSIONMANAGER
	void commitData(QSessionManager &);
#endif

private:
	void createActions();
	void readSettings();
	void writeSettings();
	bool maybeSave();
	bool saveFile(const QString& fileName);
	void setCurrentFile(const QString& fileName);
	QString strippedName(const QString& fullFileName);
	
public:
	std::unique_ptr<Document> doc;
	void gotoPage(int index);
	void updatePageNavigation();
private:
	PageWidget *pagewidget; // owned and deleted by the QMainWindow
	QString curFile;
	
	QAction *deletePageAction;
	QAction *nextPageAction;
	QAction *previousPageAction;
	
public:
	int current_page;
	QUndoStack *undoStack;
};

#endif // SAUKLAUE_H
