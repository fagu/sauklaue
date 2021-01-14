// Large parts copied from https://doc.qt.io/qt-5/qtwidgets-mainwindows-application-example.html

#include "mainwindow.h"

#include "commands.h"
#include "serializer.h"
#include "tablet.h"
#include "settings.h"
#include "settings-dialog.h"
#include "pagewidget.h"
#include "document.h"
#include "renderer.h"
#include "tool-state.h"

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
#include <QSaveFile>
#include <QElapsedTimer>
#include <QScreen>
#include <QSessionManager>
#include <QSpinBox>
#include <QLabel>
#include <QPainter>
#include <QInputDialog>
#include <QTimer>
#include <QIconEngine>

#include <KRecentFilesAction>

// See KCursorSaver in KGuiAddons for a more complete implementation. It is unfortunately only available starting with KDE Frameworks 5.73.
class SimpleCursorSaver {
public:
	SimpleCursorSaver(const QCursor& cursor) {
		QGuiApplication::setOverrideCursor(cursor);
	}
	~SimpleCursorSaver() {
		QGuiApplication::restoreOverrideCursor();
	}
};

// Some helper functions for assigning pages to views.

// Tries to assign consecutive pages, where the given view should get the given page.
std::array<int, 2> assign_pages_linked_fixing_one_view(int number_of_pages, int view, int page) {
	std::array<int, 2> res;
	if (number_of_pages == 0) {
		for (int i = 0; i < 2; i++)
			res[i] = -1;
	} else {
		assert(0 <= view && view < 2);
		assert(0 <= page && page < number_of_pages);
		int first_page = page - view;
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

class ColorIconEngine : public QIconEngine {
public:
	ColorIconEngine(QColor color) :
	    m_color(color) {
	}
	~ColorIconEngine() {
	}
	void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode [[maybe_unused]], QIcon::State state [[maybe_unused]]) override {
		painter->setRenderHint(QPainter::RenderHint::Antialiasing);
		painter->setBrush(m_color);
		painter->setPen(Qt::gray);
		int margins = 0;
		if (rect.width() > 16 && rect.height() > 16)
			margins = 2;
		else if (rect.width() > 8 && rect.height() > 8)
			margins = 1;
		painter->drawEllipse(rect.marginsRemoved(QMargins(margins, margins, margins, margins)));
	}
	QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
		QPixmap pixmap(size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		paint(&painter, QRect(QPoint(0, 0), size), mode, state);
		return pixmap;
	}
	QIconEngine* clone(void) const override {
		return new ColorIconEngine(m_color);
	}

private:
	QColor m_color;
};

class SizeIconEngine : public QIconEngine {
public:
	SizeIconEngine(double size) :
	    m_size(size) {
	}
	~SizeIconEngine() {
	}
	void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode [[maybe_unused]], QIcon::State state [[maybe_unused]]) override {
		painter->setRenderHint(QPainter::RenderHint::Antialiasing);
		painter->setBrush(Qt::black);
		int margins = 0;
		if (rect.width() > 16 && rect.height() > 16)
			margins = 2;
		else if (rect.width() > 8 && rect.height() > 8)
			margins = 1;
		QRectF r = rect.marginsRemoved(QMargins(margins, margins, margins, margins));
		double width = std::max<double>(r.width() * m_size, 1);
		double height = std::max<double>(r.height() * m_size, 1);
		r = QRectF(r.center() - QPoint(width / 2, height / 2), QSize(width, height));
		painter->drawEllipse(r);
	}
	QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
		QPixmap pixmap(size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		paint(&painter, QRect(QPoint(0, 0), size), mode, state);
		return pixmap;
	}
	QIconEngine* clone(void) const override {
		return new SizeIconEngine(m_size);
	}

private:
	double m_size;
};

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    m_tool_state(new ToolState(this)) {
	QWidget* mainArea = new QWidget();
	setCentralWidget(mainArea);
	QHBoxLayout* layout = new QHBoxLayout();
	mainArea->setLayout(layout);
	for (int i = 0; i < 2; i++) {
		page_numbers[i] = -1;
		pagewidgets[i] = new PageWidget(m_tool_state);
		connect(pagewidgets[i], &PageWidget::focus, this, [this, i]() { focusView(i); });
		connect(pagewidgets[i], &PageWidget::update_minimum_rect_in_pixels, this, &MainWindow::updateTabletMap);
		layout->addWidget(pagewidgets[i]);
	}

	createActions();
	statusBar()->show();
	setUnifiedTitleAndToolBarOnMac(true);
	readGeometrySettings();

	connect(m_tool_state->undoStack(), &QUndoStack::cleanChanged, this, &MainWindow::documentWasModified);

	setDocument(std::make_unique<Document>());
	setCurrentFile(QString());

	autoSaveTimer = new QTimer(this);
	autoSaveTimer->setSingleShot(true);
	connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSave);
	autoSaveTimer->start(std::max(1, Settings::self()->autoSaveInterval()) * 1000);

	connect(Settings::self(), &Settings::autoSaveIntervalChanged, this, [this]() {
		autoSaveTimer->start(std::min(autoSaveTimer->remainingTime(), std::max(1, Settings::self()->autoSaveInterval()) * 1000));
	});

	QGuiApplication::setFallbackSessionManagementEnabled(false);
	connect(qApp, &QGuiApplication::commitDataRequest, this, &MainWindow::commitData);
}

void MainWindow::createActions() {
	QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
	std::array<QToolBar*, 2> toolbars;
	for (size_t i = 0; i < 2; i++) {
		toolbars[i] = new QToolBar();
		toolbars[i]->setMovable(false);
		toolbars[i]->setFloatable(false);
		// By default, right-clicking on a toolbar opes a menu in which we can hide a toolbar.
		// But without a corresponding button in the window menu, there's currently no obvious way of un-hiding.
		// We therefore disable and don't show the hiding option.
		toolbars[i]->toggleViewAction()->setEnabled(false);
		toolbars[i]->toggleViewAction()->setVisible(false);
	}
	toolbars[0]->setWindowTitle(tr("Left"));
	toolbars[1]->setWindowTitle(tr("Right"));
	addToolBar(Qt::ToolBarArea::LeftToolBarArea, toolbars[0]);
	addToolBar(Qt::ToolBarArea::RightToolBarArea, toolbars[1]);
	{
		QAction* action = new QAction(QIcon::fromTheme("document-new"), tr("&New"), this);
		action->setShortcuts(QKeySequence::New);
		action->setStatusTip(tr("Create a new file"));
		connect(action, &QAction::triggered, this, &MainWindow::newFile);
		fileMenu->addAction(action);
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("document-open"), tr("&Open..."), this);
		action->setShortcuts(QKeySequence::Open);
		action->setStatusTip(tr("Open an existing file"));
		connect(action, &QAction::triggered, this, &MainWindow::open);
		fileMenu->addAction(action);
	}
	{
		recentFilesAction = new KRecentFilesAction(QIcon::fromTheme("document-open-recent"), tr("Open Recent"), this);
		connect(recentFilesAction, &KRecentFilesAction::urlSelected, this, &MainWindow::loadUrl);
		fileMenu->addAction(recentFilesAction);
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("document-save"), tr("&Save"), this);
		action->setShortcuts(QKeySequence::Save);
		action->setStatusTip(tr("Save the document to disk"));
		connect(action, &QAction::triggered, this, &MainWindow::save);
		fileMenu->addAction(action);
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("document-save-as"), tr("Save &As..."), this);
		action->setShortcuts(QKeySequence::SaveAs);
		action->setStatusTip(tr("Save the document under a new name"));
		connect(action, &QAction::triggered, this, &MainWindow::saveAs);
		fileMenu->addAction(action);
	}
	fileMenu->addSeparator();
	{
		QAction* action = new QAction(QIcon::fromTheme("document-export"), tr("&Export PDF"), this);
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
		action->setStatusTip(tr("Export PDF file"));
		connect(action, &QAction::triggered, this, &MainWindow::exportPDF);
		fileMenu->addAction(action);
	}
	fileMenu->addSeparator();
	{
		QAction* action = new QAction(QIcon::fromTheme("application-exit"), tr("E&xit"), this);
		action->setShortcuts(QKeySequence::Quit);
		action->setStatusTip(tr("Exit the application"));
		connect(action, &QAction::triggered, this, &QWidget::close);
		fileMenu->addAction(action);
	}
	QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
	{
		QAction* action = m_tool_state->undoStack()->createUndoAction(this, tr("&Undo"));
		action->setIcon(QIcon::fromTheme("edit-undo"));
		action->setShortcuts(QKeySequence::Undo);
		editMenu->addAction(action);
	}
	{
		QAction* action = m_tool_state->undoStack()->createRedoAction(this, tr("&Redo"));
		action->setIcon(QIcon::fromTheme("edit-redo"));
		action->setShortcuts(QKeySequence::Redo);
		editMenu->addAction(action);
	}
	editMenu->addSeparator();
	{
		QAction* action = new QAction(QIcon::fromTheme("configure"), tr("&Preferences"));
		connect(action, &QAction::triggered, this, &MainWindow::showSettings);
		editMenu->addAction(action);
	}
	QMenu* pagesMenu = menuBar()->addMenu(tr("&Pages"));
	{
		QAction* action = new QAction(QIcon::fromTheme("list-add"), tr("New Page &Before"));
		action->setStatusTip(tr("Add a new page before the current one"));
		connect(action, &QAction::triggered, this, &MainWindow::newPageBefore);
		pagesMenu->addAction(action);
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("list-add"), tr("New Page &After"));
		action->setStatusTip(tr("Add a new page after the current one"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
		connect(action, &QAction::triggered, this, &MainWindow::newPageAfter);
		pagesMenu->addAction(action);
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("list-remove"), tr("&Delete Page"));
		action->setStatusTip(tr("Delete the current page"));
		connect(action, &QAction::triggered, this, &MainWindow::deletePage);
		pagesMenu->addAction(action);
		deletePageAction = action;
	}
	pagesMenu->addSeparator();
	{
		QAction* action = new QAction(QIcon::fromTheme("go-up"), tr("&Previous Page"));
		action->setStatusTip(tr("Move to the previous page"));
		action->setShortcuts(QKeySequence::MoveToPreviousPage);
		connect(action, &QAction::triggered, this, &MainWindow::previousPage);
		pagesMenu->addAction(action);
		previousPageAction = action;
	}
	// If the pages are unlinked, we should enable the "previous page" action on all views independently. (Depending on whether that particular view is showing the first page or not.) This is why we create a separate global action (in the menu) that applies to the currently focused view, and one action for each view.
	for (int i = 0; i < 2; i++) {
		QAction* action = new QAction(QIcon::fromTheme("go-up"), tr("&Previous Page"));
		action->setStatusTip(tr("Move to the previous page"));
		connect(action, &QAction::triggered, this, [this, i]() { previousPageInView(i); });
		toolbars[i]->addAction(action);
		previousPageInViewAction[i] = action;
	}
	{
		for (size_t i = 0; i < 2; i++) {
			currentPageLabel[i] = new QLabel("");
			currentPageLabel[i]->setAlignment(Qt::AlignCenter);
			QLabel* ofLabel = new QLabel("of");
			ofLabel->setAlignment(Qt::AlignCenter);
			pageCountLabel[i] = new QLabel("");
			pageCountLabel[i]->setAlignment(Qt::AlignCenter);
			toolbars[i]->addWidget(currentPageLabel[i]);
			toolbars[i]->addWidget(ofLabel);
			toolbars[i]->addWidget(pageCountLabel[i]);
		}
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("go-down"), tr("&Next Page"));
		action->setStatusTip(tr("Move to the next page"));
		action->setShortcuts(QKeySequence::MoveToNextPage);
		connect(action, &QAction::triggered, this, &MainWindow::nextPage);
		pagesMenu->addAction(action);
		nextPageAction = action;
	}
	// If the pages are unlinked, we should enable the "next page" action on all views independently. (Depending on whether that particular view is showing the last page or not.) This is why we create a separate global action (in the menu) that applies to the currently focused view, and one action for each view.
	for (int i = 0; i < 2; i++) {
		QAction* action = new QAction(QIcon::fromTheme("go-down"), tr("&Next Page"));
		action->setStatusTip(tr("Move to the next page"));
		connect(action, &QAction::triggered, this, [this, i]() { nextPageInView(i); });
		toolbars[i]->addAction(action);
		nextPageInViewAction[i] = action;
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("go-first"), tr("&First Page"));
		action->setStatusTip(tr("Move to the first page"));
		action->setShortcuts(QKeySequence::MoveToStartOfLine);
		connect(action, &QAction::triggered, this, &MainWindow::firstPage);
		pagesMenu->addAction(action);
		firstPageAction = action;
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("go-last"), tr("&Last Page"));
		action->setStatusTip(tr("Move to the first page"));
		action->setShortcuts(QKeySequence::MoveToEndOfLine);
		connect(action, &QAction::triggered, this, &MainWindow::lastPage);
		pagesMenu->addAction(action);
		lastPageAction = action;
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("go-jump"), tr("&Go to page..."));
		action->setStatusTip(tr("Move to a specific page"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_G));
		connect(action, &QAction::triggered, this, &MainWindow::actionGotoPage);
		pagesMenu->addAction(action);
		gotoPageAction = action;
	}
	pagesMenu->addSeparator();
	{
		QAction* action = new QAction(QIcon::fromTheme("document-import"), tr("&Insert Sauklaue file"));
		action->setStatusTip(tr("Insert a Sauklaue file after the current page"));
		connect(action, &QAction::triggered, this, &MainWindow::insertSauklaue);
		pagesMenu->addAction(action);
	}
	pagesMenu->addSeparator();
	{
		QAction* action = new QAction(QIcon::fromTheme("document-import"), tr("&Insert PDF file"));
		action->setStatusTip(tr("Insert a PDF file after the current page"));
		connect(action, &QAction::triggered, this, [this]() { insertPDF(normal); });
		pagesMenu->addAction(action);
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("document-import"), tr("&Insert PDF file (by label)"));
		action->setStatusTip(tr("Insert a PDF file after the current page, collecting pages with the same label"));
		connect(action, &QAction::triggered, this, [this]() { insertPDF(by_label); });
		pagesMenu->addAction(action);
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("document-import"), tr("&Insert PDF file (all-in-one)"));
		action->setStatusTip(tr("Insert a PDF file after the current page, collecting all pages"));
		connect(action, &QAction::triggered, this, [this]() { insertPDF(all_in_one); });
		pagesMenu->addAction(action);
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("go-up"), tr("&Previous PDF page"));
		action->setStatusTip(tr("Switch PDF background to previous page"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Up));
		connect(action, &QAction::triggered, this, &MainWindow::previousPDFPage);
		pagesMenu->addAction(action);
	}
	{
		QAction* action = new QAction(QIcon::fromTheme("go-down"), tr("&Next PDF page"));
		action->setStatusTip(tr("Switch PDF background to next page"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Down));
		connect(action, &QAction::triggered, this, &MainWindow::nextPDFPage);
		pagesMenu->addAction(action);
	}
	QMenu* viewsMenu = menuBar()->addMenu(tr("&Views"));
	{
		QAction* action = new QAction(QIcon::fromTheme("go-next"), tr("&Other View"));
		action->setStatusTip(tr("Focus the other view"));
		action->setShortcut(QKeySequence(Qt::Key_Right));
		connect(action, &QAction::triggered, this, &MainWindow::otherView);
		viewsMenu->addAction(action);
		otherViewAction = action;
	}
	for (QToolBar* tb : toolbars)
		tb->addSeparator();
	{
		QActionGroup* group = new QActionGroup(this);
		std::vector<std::pair<QColor, QString> > v = {
		        {Qt::white, "White"},
		        {Qt::yellow, "Yellow"},
		        {QColor("#ffa500"), "Orange"},
		        {Qt::magenta, "Magenta"},
		        {Qt::green, "Green"},
		        {Qt::cyan, "Cyan"},
		        {Qt::gray, "Gray"},
		        {Qt::darkGreen, "Dark Green"},
		        {Qt::red, "Red"},
		        {Qt::blue, "Blue"},
		        {Qt::black, "Black"}};
		for (const auto& p : v) {
			QColor color = p.first;
			QString name = p.second;
			QAction* action = new QAction(QIcon(new ColorIconEngine(color)), name, this);
			action->setCheckable(true);
			connect(action, &QAction::triggered, this, [this, color](bool on) {if (on) m_tool_state->setPenColor(color); });
			for (QToolBar* tb : toolbars)
				tb->addAction(action);
			group->addAction(action);
		}
		group->actions().back()->trigger();
	}
	for (QToolBar* tb : toolbars)
		tb->addSeparator();
	{
		QActionGroup* group = new QActionGroup(this);
		std::vector<std::tuple<int, int, QString> > v = {
		        {500, 8, "Small"},
		        {1000, 16, "Medium"},
		        {1500, 24, "Large"},
		        {2000, 32, "Huge"}};
		for (const auto& p : v) {
			int pen_size = std::get<0>(p);
			int icon_size = std::get<1>(p);
			QString name = std::get<2>(p);
			QAction* action = new QAction(QIcon(new SizeIconEngine(icon_size / 32.)), name, this);
			action->setCheckable(true);
			connect(action, &QAction::triggered, this, [this, pen_size](bool on) {if (on) m_tool_state->setPenSize(pen_size); });
			for (QToolBar* tb : toolbars)
				tb->addAction(action);
			group->addAction(action);
		}
		group->actions()[1]->trigger();
	}
	for (QToolBar* tb : toolbars)
		tb->addSeparator();
	/*{
		QPixmap pixmap(64, 64);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::RenderHint::Antialiasing);
		painter.setBrush(Qt::black);
		painter.drawRect(QRect(0, 10, 64, 44));
		painter.setPen(QPen(Qt::yellow, 5));
		for (int j = 0; j < 3; j++) {
			painter.drawLine(QPoint(15, 32 + (j - 1) * 10), QPoint(49, 32 + (j - 1) * 10));
			painter.drawLine(QPoint(15, 32 + (j - 1) * 10), QPoint(49, 32 + (j - 1) * 10));
		}
		QAction* action = new QAction(pixmap, tr("Blackboard mode"), this);
		action->setCheckable(true);
		action->setStatusTip(tr("Blackboard mode"));
		connect(action, &QAction::triggered, m_tool_state, &ToolState::setBlackboardMode);
		for (QToolBar* tb : toolbars)
			tb->addAction(action);
	}*/
	{
		QAction* action = new QAction(QIcon::fromTheme("handle-move"), tr("Unlink views"), this);
		action->setCheckable(true);
		action->setStatusTip(tr("Allow independently changing pages on different views"));
		connect(action, &QAction::triggered, this, &MainWindow::setLinkedPages);
		for (QToolBar* tb : toolbars)
			tb->addAction(action);
	}
}

void MainWindow::updatePageNavigation() {
	deletePageAction->setEnabled(focused_view != -1);
	bool can_next = false, can_prev = false;
	if (focused_view != -1) {
		if (m_views_linked) {
			can_next = page_numbers[0] + 2 < (int)doc->pages().size();
			can_prev = page_numbers[0] > 0;
		} else {
			can_next = page_numbers[focused_view] + 1 < (int)doc->pages().size();
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
			nextPageInViewAction[i]->setEnabled(page_numbers[i] + 1 < (int)doc->pages().size());
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
	otherViewAction->setEnabled(number_of_active_views > 1);
	for (size_t i = 0; i < 2; i++) {
		if (page_numbers[i] != -1)
			currentPageLabel[i]->setText(QString::number(page_numbers[i] + 1));
		else
			currentPageLabel[i]->setText("-");
		pageCountLabel[i]->setText(QString::number(doc->pages().size()));
	}
}

void MainWindow::readGeometrySettings() {
	KConfig* config = Settings::self()->config();
	QByteArray geometry = config->group("Window").readEntry("geometry", QByteArray());
	if (geometry.isEmpty()) {
		QRect availableGeometry = screen()->availableGeometry();
		resize(availableGeometry.width() / 2, availableGeometry.height() / 2);
		showMaximized();
	} else {
		restoreGeometry(geometry);
	}
	recentFilesAction->loadEntries(config->group("Recent Files"));
}

void MainWindow::writeGeometrySettings() {
	KConfig* config = Settings::self()->config();
	config->group("Window").writeEntry("geometry", saveGeometry());
	recentFilesAction->saveEntries(config->group("Recent Files"));
}

void MainWindow::documentWasModified() {
	setWindowModified(!m_tool_state->undoStack()->isClean());
}

void MainWindow::loadFile(const QString& fileName) {
	QFile file(fileName);
	if (!file.open(QFile::ReadOnly)) {
		QMessageBox::warning(this, tr("Application"), tr("Cannot read file %1:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
		return;
	}

	QDataStream in(&file);
	try {
		SimpleCursorSaver cursor(Qt::WaitCursor);
		setDocument(Serializer::load(in));
	} catch (const SauklaueReadException& e) {
		QMessageBox::warning(this, tr("Application"), tr("Cannot read file %1:\n%2").arg(QDir::toNativeSeparators(fileName), e.reason()));
		return;
	}

	setCurrentFile(fileName);
	statusBar()->showMessage(tr("File loaded"), 2000);
}

void MainWindow::loadUrl(const QUrl& url) {
	if (!url.isValid() || !url.isLocalFile()) {
		QMessageBox::warning(this, tr("Application"), tr("Cannot open url %1").arg(url.toString()));
		return;
	}
	loadFile(url.toLocalFile());
}

void MainWindow::setCurrentFile(const QString& fileName) {
	curFile = fileName;
	setWindowModified(false);

	if (!curFile.isEmpty())
		recentFilesAction->addUrl(QUrl::fromLocalFile(curFile));

	setWindowFilePath(curFile.isEmpty() ? "untitled.sau" : curFile);
}

void MainWindow::setDocument(std::unique_ptr<Document> _doc) {
	if (doc)
		disconnect(doc.get(), 0, this, 0);
	doc = std::move(_doc);
	assert(doc);
	m_tool_state->undoStack()->clear();
	connect(doc.get(), &Document::pages_added, this, &MainWindow::pages_added);
	connect(doc.get(), &Document::pages_deleted, this, &MainWindow::pages_deleted);
	gotoPage((int)doc->pages().size() - 1);
}

void MainWindow::newFile() {
	if (!maybeSave())
		return;
	setDocument(std::make_unique<Document>());
	setCurrentFile(QString());
}

void MainWindow::open() {
	if (!maybeSave())
		return;
	QStringList mimeTypeFilters({"application/x-sauklaue", "application/octet-stream"});
	QFileDialog dialog(this, tr("Open File"));
	dialog.setMimeTypeFilters(mimeTypeFilters);
	dialog.setFileMode(QFileDialog::ExistingFile);
	if (!dialog.exec())
		return;
	loadFile(dialog.selectedFiles().first());
}

bool MainWindow::maybeSave() {
	if (m_tool_state->undoStack()->isClean())
		return true;
	const QMessageBox::StandardButton ret = QMessageBox::warning(this, tr("Application"), tr("The document has been modified.\n"
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

bool MainWindow::saveFile(const QString& fileName) {
	QSaveFile file(fileName);
	if (file.open(QFile::WriteOnly)) {
		SimpleCursorSaver cursor(Qt::WaitCursor);
		QDataStream out(&file);
		QElapsedTimer save_timer;
		save_timer.start();
		Serializer::save(doc.get(), out);
		qDebug() << "Serializer::save(doc, out)" << save_timer.elapsed();
		QElapsedTimer commit_timer;
		commit_timer.start();
		if (!file.commit()) {
			QMessageBox::warning(
			        this, tr("Application"), tr("Cannot write file %1:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
			return false;
		}
		qDebug() << "file.commit()" << commit_timer.elapsed();
	} else {
		QMessageBox::warning(
		        this, tr("Application"), tr("Cannot open file %1 for writing:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
		return false;
	}

	setCurrentFile(fileName);
	m_tool_state->undoStack()->setClean();
	statusBar()->showMessage(tr("File saved"), 2000);
	return true;
}

bool MainWindow::save() {
	if (curFile.isEmpty()) {
		return saveAs();
	} else {
		return saveFile(curFile);
	}
}

bool MainWindow::saveAs() {
	QStringList mimeTypeFilters({"application/x-sauklaue", "application/octet-stream"});
	QFileDialog dialog(this, tr("Save File"));
	dialog.setMimeTypeFilters(mimeTypeFilters);
	dialog.setAcceptMode(QFileDialog::AcceptSave);
	if (!dialog.exec())
		return false;
	return saveFile(dialog.selectedFiles().first());
}

// TODO Do autosaving in a different thread to avoid interruptions?
// Question: Is copying a Document fast enough to make a separate copy for the autosave thread?
// Otherwise, we could perhaps always keep two copies of the Document, one for the view, one for the autosave thread. While the autosave thread is saving, its Document doesn't update but instead keeps track of the edits it's currently missing. The edits are applied as soon as the autosave is complete.
void MainWindow::autoSave() {
	if (!curFile.isEmpty()) {
		qDebug() << "Autosaving...";
		save();
	}
	autoSaveTimer->start(std::max(1, Settings::self()->autoSaveInterval()) * 1000);
}

void MainWindow::exportPDF() {
	if (!maybeSave())
		return;
	if (curFile.isEmpty())
		return;
	QFileInfo info(curFile);
	QString pdf_file_name = info.path() + "/" + info.baseName() + ".pdf";
	qDebug() << "Exporting to" << pdf_file_name;
	SimpleCursorSaver cursor(Qt::WaitCursor);
	PDFExporter::save(doc.get(), pdf_file_name.toStdString());
}

void MainWindow::closeEvent(QCloseEvent* event) {
	if (maybeSave()) {
		writeGeometrySettings();
		event->accept();
	} else {
		event->ignore();
	}
}

void MainWindow::gotoPage(int index) {
	if (doc->pages().empty()) {
		std::array<int, 2> res;
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
			index = (int)doc->pages().size() - 1;
		if (m_views_linked) {
			std::array<int, 2> res = assign_pages_linked_fixing_one_view(doc->pages().size(), new_focused_view, index);
			new_focused_view = index - res[0];
			if (new_focused_view < 0 || new_focused_view >= 2)
				new_focused_view = 0;
			showPages(res, new_focused_view);
		} else {
			// Try to keep everything as is, but change the focused view.
			std::array<int, 2> res = page_numbers;
			for (int i = 0; i < 2; i++) {
				if (res[i] == -1)
					res[i] = 0;
				if (res[i] >= (int)doc->pages().size())
					res[i] = (int)doc->pages().size() - 1;
			}
			res[new_focused_view] = index;
			showPages(res, new_focused_view);
		}
	}
}

void MainWindow::showPages(std::array<int, 2> new_page_numbers, int new_focused_view) {
	if (new_focused_view != focused_view && focused_view != -1) {
		pagewidgets[focused_view]->unfocusPage();
	}
	for (int i = 0; i < (int)pagewidgets.size(); i++) {
		page_numbers[i] = new_page_numbers[i];
		if (page_numbers[i] != -1) {
			assert(0 <= page_numbers[i] && page_numbers[i] < (int)doc->pages().size());
			pagewidgets[i]->setPage(doc->pages()[page_numbers[i]]);
		} else {
			pagewidgets[i]->setPage(nullptr);
		}
	}
	if (new_focused_view != focused_view) {
		focused_view = new_focused_view;
		if (focused_view != -1) {
			assert(page_numbers[focused_view] != -1);
			pagewidgets[focused_view]->focusPage();
		}
		updateTabletMap();
	}
	updatePageNavigation();
}

std::unique_ptr<SPage> new_default_page() {
	// A4 paper
	int width = 0.210 * METER_TO_UNIT;
	int height = 0.297 * METER_TO_UNIT;
	auto page = std::make_unique<SPage>(width, height);
	page->add_layer(0);
	return page;
}

void MainWindow::newPageBefore() {
	m_tool_state->undoStack()->push(new AddPagesCommand(doc.get(), focused_view != -1 ? page_numbers[focused_view] : 0, move_into_vector(new_default_page())));
}

void MainWindow::newPageAfter() {
	m_tool_state->undoStack()->push(new AddPagesCommand(doc.get(), focused_view != -1 ? page_numbers[focused_view] + 1 : 0, move_into_vector(new_default_page())));
}

void MainWindow::deletePage() {
	assert(focused_view != -1);
	m_tool_state->undoStack()->push(new DeletePagesCommand(doc.get(), page_numbers[focused_view], 1));
}

void MainWindow::previousPage() {
	assert(focused_view != -1);
	gotoPage(page_numbers[focused_view] - 1);
}

void MainWindow::previousPageInView(int view) {
	if (m_views_linked)
		previousPage();
	else {
		// Try to keep everything as is, but change the given view.
		std::array<int, 2> res = page_numbers;
		res[view]--;
		showPages(res, focused_view);
	}
}

void MainWindow::nextPage() {
	assert(focused_view != -1);
	gotoPage(page_numbers[focused_view] + 1);
}

void MainWindow::nextPageInView(int view) {
	if (m_views_linked)
		nextPage();
	else {
		// Try to keep everything as is, but change the given view.
		std::array<int, 2> res = page_numbers;
		res[view]++;
		showPages(res, focused_view);
	}
}

void MainWindow::firstPage() {
	gotoPage(0);
}

void MainWindow::lastPage() {
	gotoPage(doc->pages().size() - 1);
}

void MainWindow::actionGotoPage() {
	assert(focused_view != -1);
	bool ok;
	int page = QInputDialog::getInt(this, tr("Go to Page"), tr("Page:"), page_numbers[focused_view] + 1, 1, doc->pages().size(), 1, &ok);
	if (ok)
		gotoPage(page - 1);
}

void MainWindow::insertSauklaue() {
	QStringList mimeTypeFilters({"application/x-sauklaue", "application/octet-stream"});
	QFileDialog dialog(this, tr("Import PDF file"));
	dialog.setMimeTypeFilters(mimeTypeFilters);
	dialog.setFileMode(QFileDialog::ExistingFile);
	if (!dialog.exec())
		return;
	QString fileName = dialog.selectedFiles().first();
	SimpleCursorSaver cursor(Qt::WaitCursor);
	QFileInfo info(fileName);
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return;
	QDataStream in(&file);
	std::unique_ptr<Document> importedDoc;
	try {
		SimpleCursorSaver cursor(Qt::WaitCursor);
		importedDoc = Serializer::load(in);
	} catch (const SauklaueReadException& e) {
		QMessageBox::warning(this, tr("Application"), tr("Cannot read file %1:\n%2").arg(QDir::toNativeSeparators(fileName), e.reason()));
		return;
	}
	QUndoCommand* cmd = new QUndoCommand(tr("Insert Sauklaue file"));
	auto [pages, embedded_pdfs] = extract_all(std::move(importedDoc));
	qDebug() << "Number of embedded PDFs:" << embedded_pdfs.size();
	qDebug() << "Number of pages:" << pages.size();
	for (auto& embedded_pdf : embedded_pdfs)
		new AddEmbeddedPDFCommand(doc.get(), std::move(embedded_pdf), cmd);
	if (!pages.empty())
		new AddPagesCommand(doc.get(), focused_view != -1 ? page_numbers[focused_view] + 1 : 0, std::move(pages), cmd);
	// Run the command macro.
	m_tool_state->undoStack()->push(cmd);
	statusBar()->showMessage(tr("Inserted %1 pages").arg(pages.size()), 2000);
}

void MainWindow::insertPDF(insertPDFMode mode) {
	QStringList mimeTypeFilters({"application/pdf", "application/octet-stream"});
	QFileDialog dialog(this, tr("Import PDF file"));
	dialog.setMimeTypeFilters(mimeTypeFilters);
	dialog.setFileMode(QFileDialog::ExistingFile);
	if (!dialog.exec())
		return;
	QString fileName = dialog.selectedFiles().first();
	SimpleCursorSaver cursor(Qt::WaitCursor);
	QFileInfo info(fileName);
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return;
	QByteArray contents = file.readAll();
	std::unique_ptr<EmbeddedPDF> pdf;
	try {
		pdf = std::make_unique<EmbeddedPDF>(info.fileName(), contents);
	} catch (const PDFReadException& e) {
		QMessageBox::warning(this, tr("Application"), tr("Cannot read pdf file %1:\n%2").arg(QDir::toNativeSeparators(fileName), e.reason()));
		return;
	}
	if (pdf->pages().empty()) {
		qDebug() << "Zero pages => skipping";
		statusBar()->showMessage(tr("PDF file is empty"), 2000);
		return;
	}
	EmbeddedPDF* p_pdf = pdf.get();
	// Command macro consisting of two steps: 1) Embed the pdf file. 2) Add the pdf's pages.
	QUndoCommand* cmd = new QUndoCommand(tr("Insert PDF"));
	// 1) Embed the pdf file.
	new AddEmbeddedPDFCommand(doc.get(), std::move(pdf), cmd);
	int number_of_pages = p_pdf->pages().size();
	qDebug() << "Number of pages:" << number_of_pages;
	std::vector<std::pair<int, int> > ranges;
	if (mode == normal) {
		for (int page_number = 0; page_number < number_of_pages; page_number++)
			ranges.emplace_back(page_number, page_number);
	} else if (mode == by_label) {
		ranges = p_pdf->page_label_ranges();
	} else if (mode == all_in_one) {
		ranges.emplace_back(0, number_of_pages - 1);
	} else {
		assert(false);
	}
	std::vector<std::unique_ptr<SPage> > pages;
	for (const auto& range : ranges) {
		auto p_pdf_layer = std::make_unique<PDFLayer>(p_pdf, range.first, range.first, range.second);
		// We use the size of the first page in the range. Note that later pages are therefore clipped if they are larger.
		std::pair<int, int> size = p_pdf_layer->size();
		auto page = std::make_unique<SPage>(size.first, size.second);
		page->add_layer(0, std::move(p_pdf_layer));
		page->add_layer(1);  // NormalLayer
		pages.push_back(std::move(page));
	}
	// 2) Add the pdf's pages.
	new AddPagesCommand(doc.get(), focused_view != -1 ? page_numbers[focused_view] + 1 : 0, std::move(pages), cmd);
	// Run the command macro.
	m_tool_state->undoStack()->push(cmd);
	statusBar()->showMessage(tr("Inserted %1 pages").arg(number_of_pages), 2000);
}

PDFLayer* MainWindow::currentPDFLayer() const {
	if (focused_view == -1)
		return nullptr;
	SPage* page = doc->pages()[page_numbers[focused_view]];
	for (ptr_Layer layer : page->layers()) {
		PDFLayer* pdf = std::visit(overloaded{[](PDFLayer* layer) -> PDFLayer* {
			                                      return layer;
		                                      },
		                                      [](auto) -> PDFLayer* {
			                                      return nullptr;
		                                      }},
		                           layer);
		if (pdf)
			return pdf;
	}
	return nullptr;
}

void MainWindow::previousPDFPage() {
	PDFLayer* layer = currentPDFLayer();
	if (layer && layer->page_number() - 1 >= layer->min_page_number()) {
		m_tool_state->undoStack()->push(new GotoPDFPageCommand(layer, layer->page_number() - 1));
	}
}

void MainWindow::nextPDFPage() {
	PDFLayer* layer = currentPDFLayer();
	if (layer && layer->page_number() + 1 <= layer->max_page_number()) {
		m_tool_state->undoStack()->push(new GotoPDFPageCommand(layer, layer->page_number() + 1));
	}
}

void MainWindow::setLinkedPages(bool on) {
	if (m_views_linked != !on) {
		m_views_linked = !on;
		if (m_views_linked) {
			// Try to keep the focused view the same and change other views.
			// This might be impossible if there are too few pages before or after the focused page.
			showPages(assign_pages_linked_fixing_one_view(doc->pages().size(), focused_view, page_numbers[focused_view]), focused_view);
		} else {
			// Try to keep everything as is, but display the last page in all empty views, assuming the document is nonempty.
			std::array<int, 2> res = page_numbers;
			for (size_t i = 0; i < 2; i++) {
				if (res[i] == -1)
					res[i] = (int)doc->pages().size() - 1;
			}
			showPages(res, focused_view);
		}
	}
}

void MainWindow::focusView(int view_index) {
	if (focused_view != view_index) {
		if (focused_view != -1)
			pagewidgets[focused_view]->unfocusPage();
		focused_view = view_index;
		if (focused_view != -1) {
			assert(page_numbers[focused_view] != -1);
			pagewidgets[focused_view]->focusPage();
		}
		updatePageNavigation();
		updateTabletMap();
	}
}

void MainWindow::otherView() {
	assert(focused_view != -1);
	int view = focused_view;
	do {
		view--;
		if (view == -1)
			view = 2 - 1;
	} while (page_numbers[view] == -1);
	focusView(view);
}

void MainWindow::showSettings() {
	if (KConfigDialog::showDialog("settings"))
		return;
	SettingsDialog* dialog = new SettingsDialog(this);
	dialog->show();
}

void MainWindow::commitData(QSessionManager& manager) {
	if (manager.allowsInteraction()) {
		if (!maybeSave())
			manager.cancel();
	} else {
		// Non-interactive: save without asking
		if (!m_tool_state->undoStack()->isClean())
			save();
	}
}

void MainWindow::pages_added(int first_page, int number_of_pages) {
	gotoPage(first_page + number_of_pages - 1);
}

void MainWindow::pages_deleted(int first_page, [[maybe_unused]] int number_of_pages) {
	gotoPage(first_page);
}

void MainWindow::updateTabletMap() {
	QRectF rect_both;
	for (PageWidget* p : pagewidgets)
		rect_both = rect_both.united(p->minimum_rect_in_pixels());
	QRectF rect = focused_view == -1 ? rect_both : pagewidgets[focused_view]->minimum_rect_in_pixels();
	TabletHandler::self()->set_active_region(rect, rect_both, screen()->virtualSize());
}

void MainWindow::moveEvent(QMoveEvent*) {
	updateTabletMap();
}
