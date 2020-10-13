// Large parts copied from https://doc.qt.io/qt-5/qtwidgets-mainwindows-application-example.html

#include "mainwindow.h"

#include "util.h"
#include "commands.h"
#include "serializer.h"
#include "renderer.h"
#include "tablet.h"
#include "settings.h"

#include <QHBoxLayout>
#include <QStatusBar>
#include <QGuiApplication>
#include <QFile>
#include <QMessageBox>
#include <QDir>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QDebug>
#include <QSettings>
#include <QSaveFile>
#include <QElapsedTimer>
#include <QScreen>
#include <QSessionManager>
#include <QSpinBox>
#include <QLabel>
#include <QPainter>
#include <QInputDialog>

PenColorAction::PenColorAction(QColor color, QString name, MainWindow* view) : QObject(view), m_color(color), m_view(view)
{
	QPixmap pixmap(64,64);
// 	pixmap.fill(color);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::RenderHint::Antialiasing);
	painter.setBrush(color);
	painter.drawEllipse(QRect(0,0,64,64));
	QIcon icon(pixmap);
	m_action = new QAction(icon, name, this);
	m_action->setCheckable(true);
	connect(m_action, &QAction::triggered, this, &PenColorAction::triggered);
}

void PenColorAction::triggered(bool on)
{
	if (on)
		m_view->setPenColor(m_color);
}

PenSizeAction::PenSizeAction(int pen_size, int icon_size, QString name, MainWindow* view) : QObject(view), m_size(pen_size), m_view(view)
{
	QPixmap pixmap(64,64);
// 	pixmap.fill(color);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::RenderHint::Antialiasing);
	painter.setBrush(QColorConstants::Black);
	painter.drawEllipse(QPoint(32,32), icon_size, icon_size);
	QIcon icon(pixmap);
	m_action = new QAction(icon, name, this);
	m_action->setCheckable(true);
	connect(m_action, &QAction::triggered, this, &PenSizeAction::triggered);
}

void PenSizeAction::triggered(bool on)
{
	if (on)
		m_view->setPenSize(m_size);
}

// Some helper functions for assigning pages to views.

// Tries to assign consecutive pages, where the given view should get the given page.
std::array<int,2> assign_pages_linked_fixing_one_view(int number_of_pages, int view, int page) {
	std::array<int,2> res;
	if (number_of_pages == 0) {
		for (int i = 0; i < 2; i++)
			res[i] = -1;
	} else {
		assert(0 <= view && view < 2);
		assert(0 <= page && page < number_of_pages);
		int first_page = page-view;
		// The first view should be nonempty.
		if (first_page < 0)
			first_page = 0;
		// The last view should be nonempty unless there are fewer pages than views.
		if (number_of_pages >= 2 && first_page + 2 > number_of_pages)
			first_page = number_of_pages - 2;
		for (int i = 0; i < 2; i++)
			res[i] = first_page + i < number_of_pages ? first_page + i : -1;
	}
	return res;
}



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_pen_color(QColorConstants::Black)
{
	undoStack = new QUndoStack(this);
	QWidget *mainArea = new QWidget();
	QHBoxLayout *layout = new QHBoxLayout();
	for (int i = 0; i < 2; i++) {
		page_numbers[i] = -1;
		pagewidgets[i] = new PageWidget(this, i);
	}
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
		timer->start(180000);
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
	connect(doc.get(), &Document::pages_added, this, &MainWindow::pages_added);
	connect(doc.get(), &Document::pages_deleted, this, &MainWindow::pages_deleted);
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
	try {
		setDocument(Serializer::load(in));
	} catch(const SauklaueReadException & e) {
#ifndef QT_NO_CURSOR
	QGuiApplication::restoreOverrideCursor();
#endif
		QMessageBox::warning(this, tr("Application"), tr("Cannot read file %1:\n%2").arg(QDir::toNativeSeparators(fileName), e.reason()));
		return;
	}
#ifndef QT_NO_CURSOR
	QGuiApplication::restoreOverrideCursor();
#endif

	setCurrentFile(fileName);
	updatePageNavigation();
	statusBar()->showMessage(tr("File loaded"), 2000);
}

void MainWindow::loadUrl(const QUrl &url) {
	if (!url.isValid() || !url.isLocalFile()) {
		QMessageBox::warning(this, tr("Application"), tr("Cannot open url %1").arg(url.toString()));
		return;
	}
	loadFile(url.toLocalFile());
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

void MainWindow::moveEvent(QMoveEvent* )
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
		QStringList mimeTypeFilters({"application/x-sauklaue", "application/octet-stream"});
		QFileDialog dialog(this, tr("Open File"));
		dialog.setMimeTypeFilters(mimeTypeFilters);
		dialog.setFileMode(QFileDialog::ExistingFile);
		if (dialog.exec()) {
			QStringList fileNames = dialog.selectedFiles();
			if (fileNames.size() == 1) {
				QString fileName = fileNames[0];
				if (!fileName.isEmpty()) {
					loadFile(fileName);
					undoStack->clear();
				}
			}
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
	QFileDialog dialog(this, tr("Save File"));
	dialog.setWindowModality(Qt::WindowModal);
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	dialog.setDefaultSuffix("sau");
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
	std::array<QToolBar*,2> toolbar;
	for (size_t i = 0; i < 2; i++) {
		toolbar[i] = new QToolBar();
		toolbar[i]->setMovable(false);
	}
	addToolBar(Qt::ToolBarArea::LeftToolBarArea, toolbar[0]);
	addToolBar(Qt::ToolBarArea::RightToolBarArea, toolbar[1]);
	{
		const QIcon icon = QIcon::fromTheme("document-new");
		QAction *action = new QAction(icon, tr("&New"), this);
		action->setShortcuts(QKeySequence::New);
		action->setStatusTip(tr("Create a new file"));
		connect(action, &QAction::triggered, this, &MainWindow::newFile);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		const QIcon icon = QIcon::fromTheme("document-open");
		QAction *action = new QAction(icon, tr("&Open..."), this);
		action->setShortcuts(QKeySequence::Open);
		action->setStatusTip(tr("Open an existing file"));
		connect(action, &QAction::triggered, this, &MainWindow::open);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		recentFilesAction = new KRecentFilesAction(QIcon::fromTheme("document-open-recent"), tr("Open Recent"), this);
		connect(recentFilesAction, &KRecentFilesAction::urlSelected, this, &MainWindow::loadUrl);
		fileMenu->addAction(recentFilesAction);
	}
	{
		const QIcon icon = QIcon::fromTheme("document-save");
		QAction *action = new QAction(icon, tr("&Save"), this);
		action->setShortcuts(QKeySequence::Save);
		action->setStatusTip(tr("Save the document to disk"));
		connect(action, &QAction::triggered, this, &MainWindow::save);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
	{
		const QIcon icon = QIcon::fromTheme("document-save-as");
		QAction *action = new QAction(icon, tr("Save &As..."), this);
		action->setShortcuts(QKeySequence::SaveAs);
		action->setStatusTip(tr("Save the document under a new name"));
		connect(action, &QAction::triggered, this, &MainWindow::saveAs);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
    fileMenu->addSeparator();
	{
		const QIcon icon = QIcon::fromTheme("document-export");
		QAction *action = new QAction(icon, tr("&Export PDF"), this);
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
		action->setStatusTip(tr("Export PDF file"));
		connect(action, &QAction::triggered, this, &MainWindow::exportPDF);
		fileMenu->addAction(action);
	// 	fileToolBar->addAction(action);
	}
    fileMenu->addSeparator();
	{
		const QIcon icon = QIcon::fromTheme("application-exit");
		QAction *action = fileMenu->addAction(icon, tr("E&xit"), this, &QWidget::close);
		action->setShortcuts(QKeySequence::Quit);
		action->setStatusTip(tr("Exit the application"));
	}
	QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
	{
		const QIcon icon = QIcon::fromTheme("edit-undo");
		QAction *action = undoStack->createUndoAction(this, tr("&Undo"));
		action->setIcon(icon);
		action->setShortcuts(QKeySequence::Undo);
		editMenu->addAction(action);
	}
	{
		const QIcon icon = QIcon::fromTheme("edit-redo");
		QAction *action = undoStack->createRedoAction(this, tr("&Redo"));
		action->setIcon(icon);
		action->setShortcuts(QKeySequence::Redo);
		editMenu->addAction(action);
	}
	editMenu->addSeparator();
	{
		const QIcon icon = QIcon::fromTheme("configure");
		QAction *action = new QAction(icon, tr("&Preferences"));
		connect(action, &QAction::triggered, this, &MainWindow::showSettings);
		editMenu->addAction(action);
	}
	QMenu *pagesMenu = menuBar()->addMenu(tr("&Pages"));
	{
		const QIcon icon = QIcon::fromTheme("list-add");
		QAction *action = new QAction(icon, tr("New Page &Before"));
		action->setStatusTip(tr("Add a new page before the current one"));
		connect(action, &QAction::triggered, this, &MainWindow::newPageBefore);
		pagesMenu->addAction(action);
	}
	{
		const QIcon icon = QIcon::fromTheme("list-add");
		QAction *action = new QAction(icon, tr("New Page &After"));
		action->setStatusTip(tr("Add a new page after the current one"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
		connect(action, &QAction::triggered, this, &MainWindow::newPageAfter);
		pagesMenu->addAction(action);
	}
	{
		const QIcon icon = QIcon::fromTheme("list-remove");
		QAction *action = new QAction(icon, tr("&Delete Page"));
		action->setStatusTip(tr("Delete the current page"));
		connect(action, &QAction::triggered, this, &MainWindow::deletePage);
		pagesMenu->addAction(action);
		deletePageAction = action;
	}
	pagesMenu->addSeparator();
	{
		const QIcon icon = QIcon::fromTheme("go-up");
		QAction *action = new QAction(icon, tr("&Previous Page"));
		action->setStatusTip(tr("Move to the previous page"));
		action->setShortcuts(QKeySequence::MoveToPreviousPage);
		connect(action, &QAction::triggered, this, &MainWindow::previousPage);
		pagesMenu->addAction(action);
		previousPageAction = action;
	}
	// If the pages are unlinked, we should enable the "previous page" action on all views independently. (Depending on whether that particular view is showing the first page or not.) This is why we create a separate global action (in the menu) that applies to the currently focused view, and one action for each view.
	for (int i = 0; i < 2; i++) {
		const QIcon icon = QIcon::fromTheme("go-up");
		QAction *action = new QAction(icon, tr("&Previous Page"));
		action->setStatusTip(tr("Move to the previous page"));
		connect(action, &QAction::triggered, this, [this,i]() {previousPageInView(i);});
		toolbar[i]->addAction(action);
		previousPageInViewAction[i] = action;
	}
	{
		for (size_t i = 0; i < 2; i++) {
			currentPageLabel[i] = new QLabel("");
			currentPageLabel[i]->setAlignment(Qt::AlignCenter);
			QLabel *ofLabel = new QLabel("of");
			ofLabel->setAlignment(Qt::AlignCenter);
			pageCountLabel[i] = new QLabel("");
			pageCountLabel[i]->setAlignment(Qt::AlignCenter);
			toolbar[i]->addWidget(currentPageLabel[i]);
			toolbar[i]->addWidget(ofLabel);
			toolbar[i]->addWidget(pageCountLabel[i]);
		}
	}
	{
		const QIcon icon = QIcon::fromTheme("go-down");
		QAction *action = new QAction(icon, tr("&Next Page"));
		action->setStatusTip(tr("Move to the next page"));
		action->setShortcuts(QKeySequence::MoveToNextPage);
		connect(action, &QAction::triggered, this, &MainWindow::nextPage);
		pagesMenu->addAction(action);
		nextPageAction = action;
	}
	// If the pages are unlinked, we should enable the "next page" action on all views independently. (Depending on whether that particular view is showing the last page or not.) This is why we create a separate global action (in the menu) that applies to the currently focused view, and one action for each view.
	for (int i = 0; i < 2; i++) {
		const QIcon icon = QIcon::fromTheme("go-down");
		QAction *action = new QAction(icon, tr("&Next Page"));
		action->setStatusTip(tr("Move to the next page"));
		connect(action, &QAction::triggered, this, [this,i]() {nextPageInView(i);});
		toolbar[i]->addAction(action);
		nextPageInViewAction[i] = action;
	}
	{
		const QIcon icon = QIcon::fromTheme("go-first");
		QAction *action = new QAction(icon, tr("&First Page"));
		action->setStatusTip(tr("Move to the first page"));
		action->setShortcuts(QKeySequence::MoveToStartOfLine);
		connect(action, &QAction::triggered, this, &MainWindow::firstPage);
		pagesMenu->addAction(action);
		firstPageAction = action;
	}
	{
		const QIcon icon = QIcon::fromTheme("go-last");
		QAction *action = new QAction(icon, tr("&Last Page"));
		action->setStatusTip(tr("Move to the first page"));
		action->setShortcuts(QKeySequence::MoveToEndOfLine);
		connect(action, &QAction::triggered, this, &MainWindow::lastPage);
		pagesMenu->addAction(action);
		lastPageAction = action;
	}
	{
		const QIcon icon = QIcon::fromTheme("go-jump");
		QAction *action = new QAction(icon, tr("&Go to page..."));
		action->setStatusTip(tr("Move to a specific page"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_G));
		connect(action, &QAction::triggered, this, &MainWindow::actionGotoPage);
		pagesMenu->addAction(action);
		gotoPageAction = action;
	}
	pagesMenu->addSeparator();
	{
		const QIcon icon = QIcon::fromTheme("document-import");
		QAction *action = new QAction(icon, tr("&Insert PDF file"));
		action->setStatusTip(tr("Insert a PDF file after the current page"));
		connect(action, &QAction::triggered, this, &MainWindow::insertPDF);
		pagesMenu->addAction(action);
	}
	QMenu *viewsMenu = menuBar()->addMenu(tr("&Views"));
	{
		const QIcon icon = QIcon::fromTheme("go-previous");
		QAction *action = new QAction(icon, tr("&Previous View"));
		action->setStatusTip(tr("Focus the previous view"));
		action->setShortcut(QKeySequence(Qt::Key_Left));
		connect(action, &QAction::triggered, this, &MainWindow::previousView);
		viewsMenu->addAction(action);
		previousViewAction = action;
	}
	{
		const QIcon icon = QIcon::fromTheme("go-next");
		QAction *action = new QAction(icon, tr("&Next View"));
		action->setStatusTip(tr("Focus the next view"));
		action->setShortcut(QKeySequence(Qt::Key_Right));
		connect(action, &QAction::triggered, this, &MainWindow::nextView);
		viewsMenu->addAction(action);
		nextViewAction = action;
	}
	for (size_t i = 0; i < 2; i++)
		toolbar[i]->addSeparator();
	{
		QActionGroup* group = new QActionGroup(this);
		std::vector<std::pair<QColor,QString> > v = {
			{QColorConstants::White, "White"},
			{QColorConstants::Yellow, "Yellow"},
			{QColorConstants::Svg::orange, "Orange"},
			{QColorConstants::Magenta, "Magenta"},
			{QColorConstants::Green, "Green"},
			{QColorConstants::Cyan, "Cyan"},
			{QColorConstants::Gray, "Gray"},
			{QColorConstants::DarkGreen, "Dark Green"},
			{QColorConstants::Red, "Red"},
			{QColorConstants::Blue, "Blue"},
			{QColorConstants::Black, "Black"}
		};
		for (const auto &p : v) {
			PenColorAction *action = new PenColorAction(p.first, p.second, this);
			for (size_t i = 0; i < 2; i++)
				toolbar[i]->addAction(action->action());
			group->addAction(action->action());
		}
		group->actions().back()->trigger();
	}
	for (size_t i = 0; i < 2; i++)
		toolbar[i]->addSeparator();
	{
		QActionGroup* group = new QActionGroup(this);
		std::vector<std::tuple<int,int,QString> > v = {
			{500,8,"Small"},
			{1000,16,"Medium"},
			{2000,32,"Large"}
		};
		for (const auto &p : v) {
			PenSizeAction *action = new PenSizeAction(std::get<0>(p), std::get<1>(p), std::get<2>(p), this);
			for (size_t i = 0; i < 2; i++)
				toolbar[i]->addAction(action->action());
			group->addAction(action->action());
		}
		group->actions()[1]->trigger();
	}
	for (size_t i = 0; i < 2; i++)
		toolbar[i]->addSeparator();
	{
		QPixmap pixmap(64,64);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::RenderHint::Antialiasing);
		painter.setBrush(QColorConstants::Black);
		painter.drawRect(QRect(0,10,64,44));
		painter.setPen(QPen(QColorConstants::Yellow, 5));
		for (int j = 0; j < 3; j++) {
			painter.drawLine(QPoint(15,32+(j-1)*10), QPoint(49,32+(j-1)*10));
			painter.drawLine(QPoint(15,32+(j-1)*10), QPoint(49,32+(j-1)*10));
		}
		QIcon icon(pixmap);
		QAction *action = new QAction(icon, tr("Blackboard mode"), this);
		action->setCheckable(true);
		action->setStatusTip(tr("Blackboard mode"));
		connect(action, &QAction::triggered, this, &MainWindow::setBlackboardMode);
		for (size_t i = 0; i < 2; i++)
			toolbar[i]->addAction(action);
	}
	{
		const QIcon icon = QIcon::fromTheme("handle-move");
		QAction *action = new QAction(icon, tr("Unlink views"), this);
		action->setCheckable(true);
		action->setStatusTip(tr("Allow independently changing pages on different views"));
		connect(action, &QAction::triggered, this, &MainWindow::setLinkedPages);
		for (size_t i = 0; i < 2; i++)
			toolbar[i]->addAction(action);
	}
}

void MainWindow::readSettings()
{
    QSettings settings;
    const QByteArray geometry = settings.value("geometry", QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = screen()->availableGeometry();
        resize(availableGeometry.width() / 3, availableGeometry.height() / 2);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
    } else {
        restoreGeometry(geometry);
    }
	recentFilesAction->loadEntries(Settings::self()->config()->group("Recent Files"));
}

void MainWindow::writeSettings()
{
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
	recentFilesAction->saveEntries(Settings::self()->config()->group("Recent Files"));
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
	
	if (!fileName.isEmpty())
		recentFilesAction->addUrl(QUrl::fromLocalFile(fileName));
	
	QString shownName = curFile;
	if (curFile.isEmpty())
		shownName = "untitled.txt";
	setWindowFilePath(shownName);
}

QString MainWindow::strippedName(const QString& fullFileName)
{
	return QFileInfo(fullFileName).fileName();
}

std::unique_ptr<SPage> new_default_page() {
	// A4 paper
	int width = 0.210 * METER_TO_UNIT;
	int height = 0.297 * METER_TO_UNIT;
	auto page = std::make_unique<SPage>(width, height);
	page->add_layer(0);
	return page;
}

void MainWindow::newPageBefore()
{
	undoStack->push(new AddPagesCommand(doc.get(), focused_view != -1 ? page_numbers[focused_view] : 0, move_into_vector(new_default_page())));
}

void MainWindow::newPageAfter()
{
	undoStack->push(new AddPagesCommand(doc.get(), focused_view != -1 ? page_numbers[focused_view]+1 : 0, move_into_vector(new_default_page())));
}

void MainWindow::deletePage()
{
	if (focused_view == -1)
		return;
	undoStack->push(new DeletePagesCommand(doc.get(), page_numbers[focused_view], 1));
}

void MainWindow::nextPage()
{
	assert(focused_view != -1);
	gotoPage(page_numbers[focused_view]+1);
}

void MainWindow::nextPageInView(int view)
{
	if (m_views_linked)
		nextPage();
	else {
		// Try to keep everything as is, but change the given view.
		std::array<int,2> res = page_numbers;
		res[view]++;
		showPages(res, focused_view);
	}
}

void MainWindow::previousPage()
{
	assert(focused_view != -1);
	gotoPage(page_numbers[focused_view]-1);
}

void MainWindow::previousPageInView(int view)
{
	if (m_views_linked)
		previousPage();
	else {
		// Try to keep everything as is, but change the given view.
		std::array<int,2> res = page_numbers;
		res[view]--;
		showPages(res, focused_view);
	}
}

void MainWindow::firstPage()
{
	gotoPage(0);
}

void MainWindow::lastPage()
{
	gotoPage(doc->pages().size()-1);
}

void MainWindow::actionGotoPage()
{
	assert(focused_view != -1);
	bool ok;
	int page = QInputDialog::getInt(this, tr("Go to Page"), tr("Page:"), page_numbers[focused_view]+1, 1, doc->pages().size(), 1, &ok);
	if (ok)
		gotoPage(page - 1);
}

void MainWindow::gotoPage(int index)
{
	if (doc->pages().empty()) {
		std::array<int,2> res;
		for (int i = 0; i < 2; i++)
			res[i] = -1;
		showPages(res, -1);
	} else {
		int new_focused_view = focused_view;
		if (focused_view == -1)
			new_focused_view = 0;
		if (index < 0)
			index = 0;
		if (index >= (int)doc->pages().size())
			index = (int)doc->pages().size()-1;
		if (m_views_linked) {
			std::array<int,2> res = assign_pages_linked_fixing_one_view(doc->pages().size(), new_focused_view, index);
			new_focused_view = index - res[0];
			if (new_focused_view < 0 || new_focused_view >= 2)
				new_focused_view = 0;
			showPages(res, new_focused_view);
		} else {
			// Try to keep everything as is, but change the focused view.
			std::array<int,2> res = page_numbers;
			for (int i = 0; i < 2; i++) {
				if (res[i] == -1)
					res[i] = 0;
				if (res[i] >= (int)doc->pages().size())
					res[i] = (int)doc->pages().size()-1;
			}
			res[new_focused_view] = index;
			showPages(res, new_focused_view);
		}
	}
}

void MainWindow::showPages(std::array<int, 2> new_page_numbers, int new_focused_view)
{
	if (new_focused_view != focused_view && focused_view != -1) {
		pagewidgets[focused_view]->unfocusPage();
	}
	for (int i = 0; i < (int)pagewidgets.size(); i++) {
		page_numbers[i] = new_page_numbers[i];
		if (page_numbers[i] != -1) {
			assert(0 <= page_numbers[i] && page_numbers[i] < (int)doc->pages().size());
			pagewidgets[i]->setPage(doc->pages()[page_numbers[i]], page_numbers[i]);
		} else {
			pagewidgets[i]->setPage(nullptr, -1);
		}
	}
	if (new_focused_view != focused_view) {
		focused_view = new_focused_view;
		if (focused_view != -1) {
			assert(page_numbers[focused_view] != -1);
			pagewidgets[focused_view]->focusPage();
		}
		if (focused_view == -1) {
			TabletHandler::self()->set_active_region(screen()->virtualGeometry(), screen()->virtualSize());
		}
	}
	updatePageNavigation();
}

void MainWindow::gotoPageBox(int index)
{
	gotoPage(index-1);
}

void MainWindow::insertPDF()
{
	QStringList mimeTypeFilters({"application/pdf", "application/octet-stream"});
	QFileDialog dialog(this, tr("Import PDF file"));
	dialog.setMimeTypeFilters(mimeTypeFilters);
	dialog.setFileMode(QFileDialog::ExistingFile);
	if (dialog.exec()) {
		QStringList fileNames = dialog.selectedFiles();
		if (fileNames.size() == 1) {
			QString fileName = fileNames[0];
			if (!fileName.isEmpty()) {
				QFileInfo info(fileName);
				QFile file(fileName);
				if (!file.open(QIODevice::ReadOnly))
					return;
				QByteArray contents = file.readAll();
				try {
					std::unique_ptr<EmbeddedPDF> pdf = std::make_unique<EmbeddedPDF>(info.fileName(), contents);
					EmbeddedPDF* p_pdf = pdf.get();
					QUndoCommand *cmd = new QUndoCommand(tr("Insert PDF"));
					new AddEmbeddedPDFCommand(doc.get(), std::move(pdf), cmd);
					if (p_pdf->pages().empty()) {
						qDebug() << "Zero pages => skipping";
						return;
					}
					qDebug() << "Number of pages:" << p_pdf->pages().size();
					std::vector<std::unique_ptr<SPage> > pages;
					for (int page_number = 0; page_number < (int)p_pdf->pages().size(); page_number++) {
						PopplerPage* p_page = p_pdf->pages()[page_number];
						double width, height;
						poppler_page_get_size(p_page, &width, &height);
						width *= POINT_TO_UNIT;
						height *= POINT_TO_UNIT;
						auto page = std::make_unique<SPage>(width, height);
						std::unique_ptr<PDFLayer> pdf_layer = std::make_unique<PDFLayer>(p_pdf, page_number);
						page->add_layer(0, std::move(pdf_layer));
						page->add_layer(1); // NormalLayer
						pages.push_back(std::move(page));
					}
					new AddPagesCommand(doc.get(), focused_view != -1 ? page_numbers[focused_view]+1 : 0, std::move(pages), cmd);
					undoStack->push(cmd);
				} catch(const PDFReadException &e) {
					QMessageBox::warning(this, tr("Application"), tr("Cannot read pdf file %1:\n%2").arg(QDir::toNativeSeparators(fileName), e.reason()));
					return;
				}
			}
		}
	}
}

void MainWindow::previousView()
{
	int view = focused_view;
	if (view == -1)
		return;
	do {
		view++;
		if (view == 2)
			view = 0;
	} while(page_numbers[view] == -1);
	focusView(view);
}

void MainWindow::nextView()
{
	int view = focused_view;
	if (view == -1)
		return;
	do {
		view--;
		if (view == -1)
			view = 2 - 1;
	} while(page_numbers[view] == -1);
	focusView(view);
}

void MainWindow::showSettings()
{
	SettingsDialog *dialog = new SettingsDialog(this);
	dialog->show();
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

void MainWindow::pages_added(int first_page, int number_of_pages)
{
	gotoPage(first_page + number_of_pages - 1);
}

void MainWindow::pages_deleted(int first_page, [[maybe_unused]] int number_of_pages)
{
	gotoPage(first_page);
}


void MainWindow::updatePageNavigation() {
	deletePageAction->setEnabled(focused_view != -1);
	bool can_next = false, can_prev = false;
	if (focused_view != -1) {
		if (m_views_linked) {
			can_next = page_numbers[0]+2 < (int)doc->pages().size();
			can_prev = page_numbers[0] > 0;
		} else {
			can_next = page_numbers[focused_view]+1 < (int)doc->pages().size();
			can_prev = page_numbers[focused_view] > 0;
		}
	}
	nextPageAction->setEnabled(can_next);
	previousPageAction->setEnabled(can_prev);
	for (int i = 0; i < 2; i++) {
		if (m_views_linked) {
			nextPageInViewAction[i]->setEnabled(can_next);
			previousPageInViewAction[i]->setEnabled(can_prev);
		} else {
			nextPageInViewAction[i]->setEnabled(page_numbers[i]+1 < (int)doc->pages().size());
			previousPageInViewAction[i]->setEnabled(page_numbers[i] > 0);
		}
	}
	firstPageAction->setEnabled(doc->pages().size() > 1);
	lastPageAction->setEnabled(doc->pages().size() > 1);
	gotoPageAction->setEnabled(doc->pages().size() > 1);
	int number_of_active_views = 0;
	for (int i = 0; i < 2; i++)
		if (page_numbers[i] != -1)
			number_of_active_views++;
	previousViewAction->setEnabled(number_of_active_views > 1);
	nextViewAction->setEnabled(number_of_active_views > 1);
	for (size_t i = 0; i < 2; i++) {
		if (page_numbers[i] != -1)
			currentPageLabel[i]->setText(QString::number(page_numbers[i]+1));
		else
			currentPageLabel[i]->setText("-");
		pageCountLabel[i]->setText(QString::number(doc->pages().size()));
	}
}

void MainWindow::setPenColor(QColor pen_color)
{
	m_pen_color = pen_color;
}

void MainWindow::setPenSize(int pen_size)
{
	m_pen_size = pen_size;
}

void MainWindow::setBlackboardMode(bool on)
{
	if (m_blackboard != on) {
		m_blackboard = on;
		emit blackboardModeToggled(on);
	}
}

void MainWindow::setLinkedPages(bool on)
{
	if (m_views_linked != !on) {
		m_views_linked = !on;
		if (m_views_linked) {
			// Try to keep the focused view the same and change other views.
			// This might be impossible if there are too few pages before or after the focused page.
			showPages(assign_pages_linked_fixing_one_view(doc->pages().size(), focused_view, page_numbers[focused_view]), focused_view);
		} else {
			// Try to keep everything as is, but display the last page in all empty views, assuming the document is nonempty.
			std::array<int,2> res = page_numbers;
			for (size_t i = 0; i < 2; i++) {
				if (res[i] == -1)
					res[i] = (int)doc->pages().size()-1;
			}
			showPages(res, focused_view);
		}
	}
}

void MainWindow::focusView(int view_index)
{
	if (focused_view != view_index) {
		if (focused_view != -1)
			pagewidgets[focused_view]->unfocusPage();
		focused_view = view_index;
		if (focused_view != -1) {
			assert(page_numbers[focused_view] != -1);
			pagewidgets[focused_view]->focusPage();
		}
		updatePageNavigation();
	}
}


void MainWindow::exportPDF()
{
	if (!maybeSave())
		return;
	if (curFile.isEmpty())
		return;
	QFileInfo info(curFile);
	QString pdf_file_name = info.path() + "/" + info.baseName() + ".pdf";
	qDebug() << "Exporting to" << pdf_file_name;
	QGuiApplication::setOverrideCursor(Qt::WaitCursor);
	PDFExporter::save(doc.get(), pdf_file_name.toStdString());
	QGuiApplication::restoreOverrideCursor();
}
