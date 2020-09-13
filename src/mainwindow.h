#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>

#include <QMainWindow>
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
	int first_displayed_page = 0; // The first displayed page (0 if the document is empty).
	int focused_view = -1; // Index of the focused PageWidget.
	int current_page() {return focused_view == -1 ? -1 : first_displayed_page + focused_view;}
	std::vector<PageWidget*> pagewidgets; // owned and deleted by the QMainWindow
	QString curFile;
	
	/* Opening documents */
private slots:
	void newFile();
	void open();
public:
	void loadFile(const QString &fileName);
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
	QAction *nextPageAction;
	QAction *firstPageAction;
	QAction *lastPageAction;
	QAction *gotoPageAction;
// 	QSpinBox *currentPageBox;
	std::array<QLabel*,2> currentPageLabel;
	std::array<QLabel*,2> pageCountLabel;
	void updatePageNavigation();
public:
	void gotoPage(int index);
private slots:
	void newPageBefore();
	void newPageAfter();
	void deletePage();
	void previousPage();
	void nextPage();
	void firstPage();
	void lastPage();
	void actionGotoPage();
	void gotoPageBox(int index); // 1-based index!
	void insertPDF();
	
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
