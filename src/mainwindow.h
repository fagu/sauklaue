#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>

#include <QMainWindow>
class QSessionManager;
class QUndoStack;

#include "pagewidget.h"
#include "tablet.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	
private slots:
	void documentWasModified();
	
public:
	std::unique_ptr<Document> doc;
private:
	int first_displayed_page = 0; // The first displayed page (0 if the document is empty).
	int focused_view = -1; // Index of the focused PageWidget.
	int current_page() {return focused_view == -1 ? -1 : first_displayed_page + focused_view;}
	std::vector<PageWidget*> pagewidgets; // owned and deleted by the QMainWindow
	QString curFile;
	
	/* Opening documents */
private slots:
	void newFile();
	void open();
private:
	void loadFile(const QString &fileName);
	void setCurrentFile(const QString& fileName);
	QString strippedName(const QString& fullFileName);
	void setDocument(std::unique_ptr<Document> _doc);
	
	/* Saving documents */
private slots:
	bool save();
	bool saveAs();
	void autoSave();
private:
	bool maybeSave();
	bool saveFile(const QString& fileName);
private slots:
	void exportPDF();
	
	/* Pages */
private:
	QAction *deletePageAction;
	QAction *nextPageAction;
	QAction *previousPageAction;
	void updatePageNavigation();
public:
	void gotoPage(int index);
private slots:
	void newPageBefore();
	void newPageAfter();
	void deletePage();
	void nextPage();
	void previousPage();
	
#ifndef QT_NO_SESSIONMANAGER
	void commitData(QSessionManager &);
#endif
	
	void page_added(int index);
	void page_deleted(int index);

private:
	void createActions();
	
	void readSettings();
	void writeSettings();
	
protected:
	void closeEvent(QCloseEvent *event) override;
	void moveEvent(QMoveEvent *event) override;

public:
	QUndoStack *undoStack;
	std::unique_ptr<Tablet> tablet;
};

#endif // MAINWINDOW_H
