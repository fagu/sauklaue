// Large parts copied from https://doc.qt.io/qt-5/qtwidgets-mainwindows-application-example.html

#include "mainwindow.h"

#include "util.h"
#include "actions.h"

#include <QtWidgets>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{
	undoStack = new QUndoStack(this);
	QWidget *mainArea = new QWidget();
	QHBoxLayout *layout = new QHBoxLayout();
	pagewidgets.emplace_back(new PageWidget(this));
	pagewidgets.emplace_back(new PageWidget(this));
	for (PageWidget *p : pagewidgets)
		layout->addWidget(p);
	mainArea->setLayout(layout);
	setCentralWidget(mainArea);
	
	createActions();
	
	readSettings();
	
	connect(undoStack, &QUndoStack::cleanChanged, this, &MainWindow::documentWasModified);
	
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
	gotoPage(doc->number_of_pages()-1);
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
	setDocument(Document::load(in));
#ifndef QT_NO_CURSOR
	QGuiApplication::restoreOverrideCursor();
#endif

	setCurrentFile(fileName);
	updatePageNavigation();
// 	statusBar()->showMessage(tr("File loaded"), 2000);
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

void MainWindow::documentWasModified()
{
	qDebug() << "mod" << undoStack->isClean();
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
		QAction *action = new QAction(tr("New &Page"));
		action->setStatusTip(tr("Add a new page after the current one"));
		action->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_D));
		connect(action, &QAction::triggered, this, &MainWindow::newPage);
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
	undoStack->setClean();
// 	statusBar()->showMessage(tr("File saved"), 2000);
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

void MainWindow::newPage()
{
	qDebug() << "new page" << current_page()+1;
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
	gotoPage(current_page()+1);
}

void MainWindow::previousPage()
{
	gotoPage(current_page()-1);
}

void MainWindow::gotoPage(int index)
{
	int new_first_displayed_page = first_displayed_page;
	int new_focused_view = focused_view;
	if (doc->number_of_pages() == 0) {
		new_first_displayed_page = 0;
		new_focused_view = -1;
	} else {
		if (index < 0) // Page out of bounds => set to something reasonable
			index = 0;
		else if (index >= doc->number_of_pages()) // Page out of bounds => set to something reasonable
			index = doc->number_of_pages() - 1;
		if (index < first_displayed_page) {
			new_first_displayed_page = index;
		} else if (index >= first_displayed_page + (int)pagewidgets.size()) {
			new_first_displayed_page = index - (int)pagewidgets.size() + 1;
		}
		if (new_first_displayed_page + (int)pagewidgets.size() > doc->number_of_pages())
			new_first_displayed_page = doc->number_of_pages() - (int)pagewidgets.size();
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
		if (index < doc->number_of_pages())
			pagewidgets[i]->setPage(doc->page(index), index);
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
	nextPageAction->setEnabled(focused_view != -1 && current_page()+1 < doc->number_of_pages());
	previousPageAction->setEnabled(focused_view != -1 && current_page() > 0);
}

void draw_path(Cairo::RefPtr<Cairo::Context> cr, const std::vector<Point> &points) {
	assert(!points.empty());
	cr->move_to(points[0].x, points[0].y);
	if (points.size() == 1) {
		cr->line_to(points[0].x, points[0].y);
	} else {
		for (size_t i = 1; i < points.size(); i++)
			cr->line_to(points[i].x, points[i].y);
	}
	cr->stroke();
}

void MainWindow::exportPDF()
{
	if (!save())
		return;
	assert(!curFile.isEmpty());
	QString pdf_file_name = curFile + ".pdf";
	Cairo::RefPtr<Cairo::PdfSurface> surface = Cairo::PdfSurface::create(pdf_file_name.toStdString(), 0, 0);
	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(surface);
	cr->set_line_cap(Cairo::LINE_CAP_ROUND);
	cr->set_line_join(Cairo::LINE_JOIN_ROUND);
	cr->scale(0.1, 0.1);
	for (int ipage = 0; ipage < doc->number_of_pages(); ipage++) {
		Page *page = doc->page(ipage);
		surface->set_size(0.1*page->width(), 0.1*page->height());
		cr->rectangle(0,0,page->width(),page->height());
		cr->clip();
		for (size_t i = 0; i < page->layers().size(); i++)
			cr->push_group_with_content(Cairo::CONTENT_COLOR_ALPHA);
		cr->set_source_rgb(0.95,0.95,0.95);
		cr->paint();
		for (const auto& layer : page->layers()) {
			// Retrieve and draw the previous layers
			Cairo::RefPtr<Cairo::Pattern> background = cr->pop_group();
			cr->set_source(background);
			cr->paint();
			// TODO Find a better way to support the eraser.
			// The operator CAIRO_OPERATOR_SOURCE is apparently not supported by PDF files. Therefore Cairo falls back to saving a raster image in the PDF file, which uses a lot of space!
// 			cairo_set_operator(cr->cobj(), CAIRO_OPERATOR_SOURCE);
			for (const auto& stroke : layer->strokes()) {
				std::visit(overloaded {
					[&](PenStroke* st) {
						cr->set_line_width(st->width());
						Color co = st->color();
						cr->set_source_rgba(co.r(), co.g(), co.b(), co.a());
						draw_path(cr, st->points());
					},
					[&](EraserStroke* st) {
						cr->set_line_width(st->width());
						// TODO This seems to slow down the PDF viewer.
						cr->set_source(background); // Erase = draw the background again on top of this layer
						draw_path(cr, st->points());
					}
				}, get(stroke));
			}
		}
		surface->show_page();
	}
}
