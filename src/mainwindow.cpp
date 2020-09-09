// Large parts copied from https://doc.qt.io/qt-5/qtwidgets-mainwindows-application-example.html

#include "mainwindow.h"

#include "util.h"
#include "actions.h"
#include "serializer.h"
#include "renderer.h"
#include "tablet.h"

#include <QtWidgets>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{
	undoStack = new QUndoStack(this);
	tablet = std::make_unique<Tablet>();
	QWidget *mainArea = new QWidget();
	QHBoxLayout *layout = new QHBoxLayout();
	pagewidgets.emplace_back(new PageWidget(this));
	pagewidgets.emplace_back(new PageWidget(this));
	for (PageWidget *p : pagewidgets)
		layout->addWidget(p);
	mainArea->setLayout(layout);
	setCentralWidget(mainArea);
	
	createActions();
	statusBar()->show();
	
	readSettings();
	
	connect(undoStack, &QUndoStack::cleanChanged, this, &MainWindow::documentWasModified);
	
	{
		QTimer *timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, &MainWindow::autoSave);
		timer->start(60000);
	}
	
#ifndef QT_NO_SESSIONMANAGER
	QGuiApplication::setFallbackSessionManagementEnabled(false);
	connect(qApp, &QGuiApplication::commitDataRequest,
			this, &MainWindow::commitData);
#endif
	setDocument(std::make_unique<Document>());
	setCurrentFile(QString());
	updatePageNavigation();
	setUnifiedTitleAndToolBarOnMac(true);
}

void MainWindow::setDocument(std::unique_ptr<Document> _doc)
{
	if (doc)
		disconnect(doc.get(), 0, this, 0);
	doc = std::move(_doc);
	assert(doc);
	connect(doc.get(), &Document::page_added, this, &MainWindow::page_added);
	connect(doc.get(), &Document::page_deleted, this, &MainWindow::page_deleted);
	gotoPage(doc->pages().size()-1);
}


void MainWindow::loadFile(const QString& fileName)
{
	QFile file(fileName);
	if (!file.open(QFile::ReadOnly)) {
		QMessageBox::warning(this, tr("Application"),
		                     tr("Cannot read file %1:\n%2.")
		                     .arg(QDir::toNativeSeparators(fileName), file.errorString()));
		return;
	}

	QDataStream in(&file);
#ifndef QT_NO_CURSOR
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
#endif
	setDocument(Serializer::load(in));
#ifndef QT_NO_CURSOR
	QGuiApplication::restoreOverrideCursor();
#endif

	setCurrentFile(fileName);
	updatePageNavigation();
	statusBar()->showMessage(tr("File loaded"), 2000);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	if (maybeSave()) {
		writeSettings();
		event->accept();
	} else {
		event->ignore();
	}
}

void MainWindow::moveEvent(QMoveEvent* event)
{
	for (PageWidget* p : pagewidgets)
		p->update_tablet_map();
}

void MainWindow::newFile()
{
	if (maybeSave()) {
		setDocument(std::make_unique<Document>());
		setCurrentFile(QString());
		updatePageNavigation();
		undoStack->clear();
	}
}

void MainWindow::open()
{
	if (maybeSave()) {
		QString fileName = QFileDialog::getOpenFileName(this);
		if (!fileName.isEmpty()) {
			loadFile(fileName);
			undoStack->clear();
		}
	}
}

bool MainWindow::save()
{
	if (curFile.isEmpty()) {
		return saveAs();
	} else {
		return saveFile(curFile);
	}
}

bool MainWindow::saveAs()
{
	QFileDialog dialog(this);
	dialog.setWindowModality(Qt::WindowModal);
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	if (dialog.exec() != QDialog::Accepted)
		return false;
	return saveFile(dialog.selectedFiles().first());
}

// TODO Do autosaving in a different thread to avoid interruptions?
// Question: Is copying a Document fast enough to make a separate copy for the autosave thread?
// Otherwise, we could perhaps always keep two copies of the Document, one for the view, one for the autosave thread. While the autosave thread is saving, its Document doesn't update but instead keeps track of the edits it's currently missing. The edits are applied as soon as the autosave is complete.
void MainWindow::autoSave()
{
	if (doc && !curFile.isEmpty()) {
		qDebug() << "Autosaving...";
		save();
	}
}

void MainWindow::documentWasModified()
{
	setWindowModified(!undoStack->isClean());
}

void MainWindow::createActions()
{
	QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
// 	QToolBar *fileToolBar = addToolBar(tr("File"));
	{
		const QIcon newIcon = QIcon::fromTheme("document-new", QIcon(":/images/new.png"));
		QAction *action = new QAction(newIcon, tr("&New"), this);
		action->setShortcuts(QKeySequence::New);
		action->setStatusTip(tr("Create a new file"));
		connect(action, &QAction::triggered, this, &MainWindow::newFile);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		const QIcon openIcon = QIcon::fromTheme("document-open", QIcon(":/images/open.png"));
		QAction *action = new QAction(openIcon, tr("&Open..."), this);
		action->setShortcuts(QKeySequence::Open);
		action->setStatusTip(tr("Open an existing file"));
		connect(action, &QAction::triggered, this, &MainWindow::open);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		const QIcon saveIcon = QIcon::fromTheme("document-save", QIcon(":/images/save.png"));
		QAction *action = new QAction(saveIcon, tr("&Save"), this);
		action->setShortcuts(QKeySequence::Save);
		action->setStatusTip(tr("Save the document to disk"));
		connect(action, &QAction::triggered, this, &MainWindow::save);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		const QIcon saveAsIcon = QIcon::fromTheme("document-save-as", QIcon(":/images/saveas.png"));
		QAction *action = new QAction(saveAsIcon, tr("Save &As..."), this);
		action->setShortcuts(QKeySequence::SaveAs);
		action->setStatusTip(tr("Save the document under a new name"));
		connect(action, &QAction::triggered, this, &MainWindow::saveAs);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		const QIcon saveAsIcon = QIcon::fromTheme("document-export", QIcon(":/images/export.png"));
		QAction *action = new QAction(saveAsIcon, tr("&Export PDF"), this);
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
		action->setStatusTip(tr("Export PDF file"));
		connect(action, &QAction::triggered, this, &MainWindow::exportPDF);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
    fileMenu->addSeparator();
	{
		const QIcon exitIcon = QIcon::fromTheme("application-exit");
		QAction *action = fileMenu->addAction(exitIcon, tr("E&xit"), this, &QWidget::close);
		action->setShortcuts(QKeySequence::Quit);
		action->setStatusTip(tr("Exit the application"));
	}
	QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
	{
		QAction *action = undoStack->createUndoAction(this, tr("&Undo"));
		action->setShortcuts(QKeySequence::Undo);
		editMenu->addAction(action);
	}
	{
		QAction *action = undoStack->createRedoAction(this, tr("&Redo"));
		action->setShortcuts(QKeySequence::Redo);
		editMenu->addAction(action);
	}
	QMenu *pagesMenu = menuBar()->addMenu(tr("&Pages"));
	{
		QAction *action = new QAction(tr("New Page &Before"));
		action->setStatusTip(tr("Add a new page before the current one"));
		connect(action, &QAction::triggered, this, &MainWindow::newPageBefore);
		pagesMenu->addAction(action);
	}
	{
		QAction *action = new QAction(tr("New Page &After"));
		action->setStatusTip(tr("Add a new page after the current one"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
		connect(action, &QAction::triggered, this, &MainWindow::newPageAfter);
		pagesMenu->addAction(action);
	}
	{
		QAction *action = new QAction(tr("&Delete Page"));
		action->setStatusTip(tr("Delete the current page"));
		connect(action, &QAction::triggered, this, &MainWindow::deletePage);
		pagesMenu->addAction(action);
		deletePageAction = action;
	}
	{
		QAction *action = new QAction(tr("&Next Page"));
		action->setStatusTip(tr("Move to the next page"));
		action->setShortcuts(QKeySequence::MoveToNextPage);
		connect(action, &QAction::triggered, this, &MainWindow::nextPage);
		pagesMenu->addAction(action);
		nextPageAction = action;
	}
	{
		QAction *action = new QAction(tr("&Previous Page"));
		action->setStatusTip(tr("Move to the previous page"));
		action->setShortcuts(QKeySequence::MoveToPreviousPage);
		connect(action, &QAction::triggered, this, &MainWindow::previousPage);
		pagesMenu->addAction(action);
		previousPageAction = action;
	}
}

void MainWindow::readSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = screen()->availableGeometry();
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
    } else {
        restoreGeometry(geometry);
    }
}

void MainWindow::writeSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("geometry", saveGeometry());
}

bool MainWindow::maybeSave()
{
	if (undoStack->isClean())
		return true;
	const QMessageBox::StandardButton ret
		= QMessageBox::warning(this, tr("Application"),
		                       tr("The document has been modified.\n"
		                          "Do you want to save your changes?"),
		                       QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
	switch (ret) {
	case QMessageBox::Save:
		return save();
	case QMessageBox::Cancel:
		return false;
	default:
		break;
	}
	return true;
}

bool MainWindow::saveFile(const QString& fileName)
{
	QString errorMessage;
	
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	QSaveFile file(fileName);
	if (file.open(QFile::WriteOnly)) {
		QDataStream out(&file);
		QElapsedTimer save_timer; save_timer.start();
		Serializer::save(doc.get(), out);
		qDebug() << "Serializer::save(doc, out)" << save_timer.elapsed();
		QElapsedTimer commit_timer; commit_timer.start();
		if (!file.commit()) {
			errorMessage = tr("Cannot write file %1:\n%2.")
				.arg(QDir::toNativeSeparators(fileName), file.errorString());
		}
		qDebug() << "file.commit()" << commit_timer.elapsed();
	} else {
		errorMessage = tr("Cannot open file %1 for writing:\n%2.")
			.arg(QDir::toNativeSeparators(fileName), file.errorString());
	}
	QGuiApplication::restoreOverrideCursor();
	
	if (!errorMessage.isEmpty()) {
		QMessageBox::warning(this, tr("Application"), errorMessage);
		return false;
	}
	
	setCurrentFile(fileName);
	undoStack->setClean();
	statusBar()->showMessage(tr("File saved"), 2000);
	return true;
}

void MainWindow::setCurrentFile(const QString& fileName)
{
	curFile = fileName;
	setWindowModified(false);
	
	QString shownName = curFile;
	if (curFile.isEmpty())
		shownName = "untitled.txt";
	setWindowFilePath(shownName);
}

QString MainWindow::strippedName(const QString& fullFileName)
{
	return QFileInfo(fullFileName).fileName();
}

void MainWindow::newPageBefore()
{
	// A4 paper
	double m2unit = 72000/0.0254; // 1 unit = 1pt/1000 = 1in/72000 = 25.4mm/72000 = 0.0254m/72000
	int width = pow(2, -0.25 - 2) * m2unit;
	int height = pow(2, 0.25 - 2) * m2unit;
	auto page = std::make_unique<Page>(width, height);
	page->add_layer(0);
	undoStack->push(new NewPageCommand(doc.get(), current_page(), std::move(page)));
}

void MainWindow::newPageAfter()
{
	// A4 paper
	double m2unit = 72000/0.0254; // 1 unit = 1pt/1000 = 1in/72000 = 25.4mm/72000 = 0.0254m/72000
	int width = pow(2, -0.25 - 2) * m2unit;
	int height = pow(2, 0.25 - 2) * m2unit;
	auto page = std::make_unique<Page>(width, height);
	page->add_layer(0);
	undoStack->push(new NewPageCommand(doc.get(), current_page()+1, std::move(page)));
}

void MainWindow::deletePage()
{
	if (current_page() == -1)
		return;
	undoStack->push(new DeletePageCommand(doc.get(), current_page()));
}

void MainWindow::nextPage()
{
	qDebug() << "nextPage";
	gotoPage(current_page()+1);
}

void MainWindow::previousPage()
{
	qDebug() << "previousPage";
	gotoPage(current_page()-1);
}

void MainWindow::gotoPage(int index)
{
	int new_first_displayed_page = first_displayed_page;
	int new_focused_view = focused_view;
	if (doc->pages().size() == 0) {
		new_first_displayed_page = 0;
		new_focused_view = -1;
	} else {
		if (index < 0) // Page out of bounds => set to something reasonable
			index = 0;
		else if (index >= doc->pages().size()) // Page out of bounds => set to something reasonable
			index = doc->pages().size() - 1;
		if (index < first_displayed_page) {
			new_first_displayed_page = index;
		} else if (index >= first_displayed_page + (int)pagewidgets.size()) {
			new_first_displayed_page = index - (int)pagewidgets.size() + 1;
		}
		if (new_first_displayed_page + (int)pagewidgets.size() > doc->pages().size())
			new_first_displayed_page = doc->pages().size() - (int)pagewidgets.size();
		if (new_first_displayed_page < 0)
			new_first_displayed_page = 0;
		new_focused_view = index - new_first_displayed_page;
	}
	assert(new_first_displayed_page >= 0);
	assert(new_focused_view == -1 || (0 <= new_focused_view && new_focused_view < (int)pagewidgets.size()));
	if (new_focused_view != focused_view && focused_view != -1) {
		pagewidgets[focused_view]->unfocusPage();
	}
	first_displayed_page = new_first_displayed_page;
	for (int i = 0; i < (int)pagewidgets.size(); i++) {
		int index = first_displayed_page + i;
		if (index < doc->pages().size())
			pagewidgets[i]->setPage(doc->pages()[index], index);
		else
			pagewidgets[i]->setPage(nullptr, -1);
	}
	if (new_focused_view != focused_view) {
		focused_view = new_focused_view;
		if (focused_view != -1)
			pagewidgets[focused_view]->focusPage();
	}
	updatePageNavigation();
}


#ifndef QT_NO_SESSIONMANAGER
void MainWindow::commitData(QSessionManager &manager)
{
	if (manager.allowsInteraction()) {
		if (!maybeSave())
			manager.cancel();
	} else {
		// Non-interactive: save without asking
		if (!undoStack->isClean())
			save();
	}
}
#endif

void MainWindow::page_added(int index)
{
	gotoPage(index);
}

void MainWindow::page_deleted(int index)
{
	gotoPage(index);
}


void MainWindow::updatePageNavigation() {
	deletePageAction->setEnabled(focused_view != -1);
	nextPageAction->setEnabled(focused_view != -1 && current_page()+1 < doc->pages().size());
	previousPageAction->setEnabled(focused_view != -1 && current_page() > 0);
	statusBar()->showMessage(QString("Page ")+QString::number(current_page()+1)+QString(" of ")+QString::number(doc->pages().size()));
}

void MainWindow::exportPDF()
{
	if (!maybeSave())
		return;
	assert(!curFile.isEmpty());
	QString pdf_file_name = curFile + ".pdf";
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	PDFExporter::save(doc.get(), pdf_file_name.toStdString());
	QGuiApplication::restoreOverrideCursor();
}
