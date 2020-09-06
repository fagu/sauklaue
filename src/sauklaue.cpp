// Large parts copied from https://doc.qt.io/qt-5/qtwidgets-mainwindows-application-example.html

#include "sauklaue.h"

#include "actions.h"

#include <QtWidgets>

sauklaue::sauklaue(QWidget *parent) :
    QMainWindow(parent)
{
	undoStack = new QUndoStack(this);
	pagewidget = new PageWidget(this);
	
	setCentralWidget(pagewidget);
	
	createActions();
	
	readSettings();
	
// 	connect(textedit->document(), &QTextDocument::contentsChanged, this, &sauklaue::documentWasModified);
	
#ifndef QT_NO_SESSIONMANAGER
	QGuiApplication::setFallbackSessionManagementEnabled(false);
	connect(qApp, &QGuiApplication::commitDataRequest,
			this, &sauklaue::commitData);
#endif
	
	doc = std::make_unique<Document>();
	current_page = -1;
	pagewidget->setPage(nullptr);
	setCurrentFile(QString());
	updatePageNavigation();
	setUnifiedTitleAndToolBarOnMac(true);
}

void sauklaue::loadFile(const QString& fileName)
{
	QFile file(fileName);
	if (!file.open(QFile::ReadOnly | QFile::Text)) {
		QMessageBox::warning(this, tr("Application"),
		                     tr("Cannot read file %1:\n%2.")
		                     .arg(QDir::toNativeSeparators(fileName), file.errorString()));
		return;
	}

	QDataStream in(&file);
#ifndef QT_NO_CURSOR
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
#endif
	doc = Document::load(in);
	current_page = doc->number_of_pages() - 1;
	pagewidget->setPage(current_page == -1 ? nullptr : doc->page(current_page));
#ifndef QT_NO_CURSOR
	QGuiApplication::restoreOverrideCursor();
#endif

	setCurrentFile(fileName);
	updatePageNavigation();
// 	statusBar()->showMessage(tr("File loaded"), 2000);
}

void sauklaue::closeEvent(QCloseEvent* event)
{
	if (maybeSave()) {
		writeSettings();
		event->accept();
	} else {
		event->ignore();
	}
}

void sauklaue::moveEvent(QMoveEvent* event)
{
	pagewidget->update_tablet_map();
}

void sauklaue::newFile()
{
	if (maybeSave()) {
		doc = std::make_unique<Document>();
		current_page = -1;
		pagewidget->setPage(nullptr);
		setCurrentFile(QString());
		updatePageNavigation();
	}
}

void sauklaue::open()
{
	if (maybeSave()) {
		QString fileName = QFileDialog::getOpenFileName(this);
		if (!fileName.isEmpty())
			loadFile(fileName);
	}
}

bool sauklaue::save()
{
	if (curFile.isEmpty()) {
		return saveAs();
	} else {
		return saveFile(curFile);
	}
}

bool sauklaue::saveAs()
{
	QFileDialog dialog(this);
	dialog.setWindowModality(Qt::WindowModal);
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	if (dialog.exec() != QDialog::Accepted)
		return false;
	return saveFile(dialog.selectedFiles().first());
}

void sauklaue::documentWasModified()
{
// 	setWindowModified(textedit->document()->isModified());
}

void sauklaue::createActions()
{
	QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
// 	QToolBar *fileToolBar = addToolBar(tr("File"));
	{
		const QIcon newIcon = QIcon::fromTheme("document-new", QIcon(":/images/new.png"));
		QAction *action = new QAction(newIcon, tr("&New"), this);
		action->setShortcuts(QKeySequence::New);
		action->setStatusTip(tr("Create a new file"));
		connect(action, &QAction::triggered, this, &sauklaue::newFile);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		const QIcon openIcon = QIcon::fromTheme("document-open", QIcon(":/images/open.png"));
		QAction *action = new QAction(openIcon, tr("&Open..."), this);
		action->setShortcuts(QKeySequence::Open);
		action->setStatusTip(tr("Open an existing file"));
		connect(action, &QAction::triggered, this, &sauklaue::open);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		const QIcon saveIcon = QIcon::fromTheme("document-save", QIcon(":/images/save.png"));
		QAction *action = new QAction(saveIcon, tr("&Save"), this);
		action->setShortcuts(QKeySequence::Save);
		action->setStatusTip(tr("Save the document to disk"));
		connect(action, &QAction::triggered, this, &sauklaue::save);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		const QIcon saveAsIcon = QIcon::fromTheme("document-save-as", QIcon(":/images/saveas.png"));
		QAction *action = new QAction(saveAsIcon, tr("Save &As..."), this);
		action->setShortcuts(QKeySequence::SaveAs);
		action->setStatusTip(tr("Save the document under a new name"));
		connect(action, &QAction::triggered, this, &sauklaue::saveAs);
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
		QAction *action = new QAction(tr("New &Page"));
		action->setStatusTip(tr("Add a new page after the current one"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
		connect(action, &QAction::triggered, this, &sauklaue::newPage);
		pagesMenu->addAction(action);
	}
	{
		QAction *action = new QAction(tr("&Delete Page"));
		action->setStatusTip(tr("Delete the current page"));
		connect(action, &QAction::triggered, this, &sauklaue::deletePage);
		pagesMenu->addAction(action);
		deletePageAction = action;
	}
	{
		QAction *action = new QAction(tr("&Next Page"));
		action->setStatusTip(tr("Move to the next page"));
		action->setShortcuts(QKeySequence::MoveToNextPage);
		connect(action, &QAction::triggered, this, &sauklaue::nextPage);
		pagesMenu->addAction(action);
		nextPageAction = action;
	}
	{
		QAction *action = new QAction(tr("&Previous Page"));
		action->setStatusTip(tr("Move to the previous page"));
		action->setShortcuts(QKeySequence::MoveToPreviousPage);
		connect(action, &QAction::triggered, this, &sauklaue::previousPage);
		pagesMenu->addAction(action);
		previousPageAction = action;
	}
}

void sauklaue::readSettings()
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

void sauklaue::writeSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("geometry", saveGeometry());
}

bool sauklaue::maybeSave()
{
// 	if (!textEdit->document()->isModified())
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

bool sauklaue::saveFile(const QString& fileName)
{
	QString errorMessage;
	
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	QSaveFile file(fileName);
	if (file.open(QFile::WriteOnly | QFile::Text)) {
		QDataStream out(&file);
		doc->save(out);
		if (!file.commit()) {
			errorMessage = tr("Cannot write file %1:\n%2.")
				.arg(QDir::toNativeSeparators(fileName), file.errorString());
		}
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
// 	statusBar()->showMessage(tr("File saved"), 2000);
	return true;
}

void sauklaue::setCurrentFile(const QString& fileName)
{
	curFile = fileName;
// 	textEdit->document()->setModified(false);
	setWindowModified(false);
	
	QString shownName = curFile;
	if (curFile.isEmpty())
		shownName = "untitled.txt";
	setWindowFilePath(shownName);
}

QString sauklaue::strippedName(const QString& fullFileName)
{
	return QFileInfo(fullFileName).fileName();
}

void sauklaue::newPage()
{
	// A4 paper
	double m2unit = 720/0.0254; // 1 unit = 1pt/10 = 1in/720 = 25.4mm/720 = 0.0254m/720
	int width = pow(2, -0.25 - 2) * m2unit;
	int height = pow(2, 0.25 - 2) * m2unit;
	auto page = std::make_unique<Page>(width, height);
	page->add_layer(0);
	undoStack->push(new NewPageCommand(this, current_page+1, std::move(page)));
	gotoPage(current_page+1);
}

void sauklaue::deletePage()
{
	if (current_page == -1)
		return;
	undoStack->push(new DeletePageCommand(this, current_page));
	updatePageNavigation();
}

void sauklaue::nextPage()
{
	gotoPage(current_page+1);
}

void sauklaue::previousPage()
{
	gotoPage(current_page-1);
}

void sauklaue::gotoPage(int index)
{
	if (doc->number_of_pages() == 0)
		index = -1;
	else if (index < 0) // Page out of bounds => set to something reasonable
		index = 0;
	else if (index >= doc->number_of_pages()) // Page out of bounds => set to something reasonable
		index = doc->number_of_pages() - 1;
	current_page = index;
	pagewidget->setPage(index == -1 ? nullptr : doc->page(index));
	updatePageNavigation();
}


#ifndef QT_NO_SESSIONMANAGER
void sauklaue::commitData(QSessionManager &manager)
{
	if (manager.allowsInteraction()) {
		if (!maybeSave())
			manager.cancel();
	} else {
		// Non-interactive: save without asking
// 		if (textEdit->document()->isModified())
			save();
	}
}
#endif

void sauklaue::updatePageNavigation() {
	deletePageAction->setEnabled(current_page != -1);
	nextPageAction->setEnabled(current_page+1 < doc->number_of_pages());
	previousPageAction->setEnabled(current_page > 0);
}
