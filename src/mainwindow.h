#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "all-types.h"

#include <QMainWindow>
class KRecentFilesAction;
class QSpinBox;
class QLabel;
class QSessionManager;
class QUndoStack;
class ToolState;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);

private:
	void createActions();
	void updatePageNavigation();
	void readGeometrySettings();
	void writeGeometrySettings();

	void documentWasModified();

	/* Opening documents */
public:
	void loadFile(const QString& fileName);

private:
	void loadUrl(const QUrl& url);
	void setCurrentFile(const QString& fileName);
	void setDocument(std::unique_ptr<Document> _doc);
	void newFile();
	void open();

	KRecentFilesAction* recentFilesAction;

	/* Saving documents */
private:
	bool maybeSave();
	bool saveFile(const QString& fileName);
	bool save();
	bool saveAs();
	void autoSave();
	void exportPDF();

	/* Exit */
protected:
	void closeEvent(QCloseEvent* event) override;

	/* Pages */
private:
	void gotoPage(int index);
	void showPages(std::array<int, 2> new_page_numbers, int new_focused_view);
	void newPageBefore();
	void newPageAfter();
	void deletePage();
	void previousPage();
	void previousPageInView(int view);
	void nextPage();
	void nextPageInView(int view);
	void firstPage();
	void lastPage();
	void actionGotoPage();
	void insertPDF();
	void setLinkedPages(bool on);

	QAction* deletePageAction;
	QAction* previousPageAction;
	std::array<QAction*, 2> previousPageInViewAction;
	QAction* nextPageAction;
	std::array<QAction*, 2> nextPageInViewAction;
	QAction* firstPageAction;
	QAction* lastPageAction;
	QAction* gotoPageAction;
	std::array<QLabel*, 2> currentPageLabel;
	std::array<QLabel*, 2> pageCountLabel;

	/* Views */
private:
	void focusView(int view_index);
	void previousView();
	void nextView();

	QAction* previousViewAction;
	QAction* nextViewAction;

	/* Settings */
private:
	void showSettings();

	void commitData(QSessionManager&);

	void pages_added(int first_page, int number_of_pages);
	void pages_deleted(int first_page, int number_of_pages);
	
	/* Tablet */
	void updateTabletMap();

protected:
	void moveEvent(QMoveEvent* event) override;

private:
	std::array<PageWidget*, 2> pagewidgets;  // owned and deleted by the QMainWindow

	ToolState* m_tool_state;
	bool m_views_linked = true;  // Whether the page widgets should always display consecutive pages.

	std::unique_ptr<Document> doc;
	QString curFile;

	int focused_view = -1;  // Index of the focused PageWidget. Should be -1 if and only if the document has no pages.
	std::array<int, 2> page_numbers;  // Should be -1 if and only if the document has no pages.
};

#endif  // MAINWINDOW_H
