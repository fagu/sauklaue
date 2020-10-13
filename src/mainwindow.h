#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>

#include "util.h"

#include <QMainWindow>
#include <KRecentFilesAction>
class QSpinBox;
class QLabel;
class QSessionManager;
class QUndoStack;

#include "pagewidget.h"
#include "tablet.h"

class PenColorAction : public QObject {
	Q_OBJECT
public:
	PenColorAction(QColor color, QString name, MainWindow *view);
	QAction* action() const {
		return m_action;
	}
private slots:
	void triggered(bool on);
private:
	QColor m_color;
	MainWindow *m_view;
	QAction* m_action;
};

class PenSizeAction : public QObject {
	Q_OBJECT
public:
	PenSizeAction(int pen_size, int icon_size, QString name, MainWindow *view);
	QAction* action() const {
		return m_action;
	}
private slots:
	void triggered(bool on);
private:
	int m_size;
	MainWindow *m_view;
	QAction* m_action;
};

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
	int focused_view = -1; // Index of the focused PageWidget. Should be -1 if and only if the document has no pages.
	std::array<int,2> page_numbers; // Should be -1 if and only if the document has no pages.
	std::array<PageWidget*,2> pagewidgets; // owned and deleted by the QMainWindow
	QString curFile;
	
	/* Opening documents */
private slots:
	void newFile();
	void open();
private:
	KRecentFilesAction *recentFilesAction;
public:
	void loadFile(const QString &fileName);
	void loadUrl(const QUrl &url);
private:
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
	QAction *previousPageAction;
	std::array<QAction*,2> previousPageInViewAction;
	QAction *nextPageAction;
	std::array<QAction*,2> nextPageInViewAction;
	QAction *firstPageAction;
	QAction *lastPageAction;
	QAction *gotoPageAction;
// 	QSpinBox *currentPageBox;
	std::array<QLabel*,2> currentPageLabel;
	std::array<QLabel*,2> pageCountLabel;
	void updatePageNavigation();
public:
	void gotoPage(int index);
	void showPages(std::array<int,2> new_page_numbers, int new_focused_view);
private slots:
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
	void gotoPageBox(int index); // 1-based index!
	void insertPDF();
	
	/* Views */
private:
	QAction *previousViewAction;
	QAction *nextViewAction;
private slots:
	void previousView();
	void nextView();
	
	/* Settings */
private slots:
	void showSettings();
	
private slots:
#ifndef QT_NO_SESSIONMANAGER
	void commitData(QSessionManager &);
#endif
	
	void pages_added(int first_page, int number_of_pages);
	void pages_deleted(int first_page, int number_of_pages);
	
public:
	void setPenColor(QColor pen_color);
	QColor penColor() {
		return m_pen_color;
	}
private:
	QColor m_pen_color;
public:
	void setPenSize(int pen_size);
	int penSize() {
		return m_pen_size;
	}
private:
	int m_pen_size;
private:
	bool m_blackboard = false;
private slots:
	void setBlackboardMode(bool on);
signals:
	void blackboardModeToggled(bool on);
public:
	bool blackboardMode() {
		return m_blackboard;
	}
private:
	bool m_views_linked = true; // Whether the page widgets should always display consecutive pages.
private slots:
	void setLinkedPages(bool on);
	
public:
	void focusView(int view_index);
	
private:
	void createActions();
	
	void readSettings();
	void writeSettings();
	
protected:
	void closeEvent(QCloseEvent *event) override;
	void moveEvent(QMoveEvent *event) override;

public:
	QUndoStack *undoStack;
};

#endif // MAINWINDOW_H
