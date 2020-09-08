#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>

#include <QMainWindow>
class QSessionManager;
class QUndoStack;

#include "pagewidget.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	
	void loadFile(const QString &fileName);
	
protected:
	void closeEvent(QCloseEvent *event) override;
	void moveEvent(QMoveEvent *event) override;

private slots:
	void newFile();
	void open();
	bool save();
	bool saveAs();
	void newPageBefore();
	void newPageAfter();
	void deletePage();
	void nextPage();
	void previousPage();
	void documentWasModified();
	void exportPDF();
#ifndef QT_NO_SESSIONMANAGER
	void commitData(QSessionManager &);
#endif
	
	void page_added(int index);
	void page_deleted(int index);

private:
	void createActions();
	void readSettings();
	void writeSettings();
	bool maybeSave();
	bool saveFile(const QString& fileName);
	void setCurrentFile(const QString& fileName);
	QString strippedName(const QString& fullFileName);
	
	void setDocument(std::unique_ptr<Document> _doc);
	
	int current_page() {return focused_view == -1 ? -1 : first_displayed_page + focused_view;}
	
public:
	std::unique_ptr<Document> doc;
	void gotoPage(int index);
	void updatePageNavigation();
private:
	std::vector<PageWidget*> pagewidgets; // owned and deleted by the QMainWindow
	QString curFile;
	
	QAction *deletePageAction;
	QAction *nextPageAction;
	QAction *previousPageAction;
	
public:
	int first_displayed_page; // The first displayed page (0 if the document is empty).
	int focused_view; // Index of the focused PageWidget.
	QUndoStack *undoStack;
};

#endif // MAINWINDOW_H
