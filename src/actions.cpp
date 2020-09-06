#include "actions.h"

#include "sauklaue.h"

#include <QtWidgets>

NewPageCommand::NewPageCommand(sauklaue* _view, int _index, std::unique_ptr<Page> _page, QUndoCommand *parent) :
	QUndoCommand(parent),
	view(_view),
	index(_index),
	page(std::move(_page))
{
	setText(QObject::tr("New page"));
}

void NewPageCommand::redo()
{
	assert(page);
	view->doc->add_page(index, std::move(page));
	view->gotoPage(index);
}

void NewPageCommand::undo()
{
	assert(!page);
	page = view->doc->delete_page(index);
	view->gotoPage(index);
}


DeletePageCommand::DeletePageCommand(sauklaue* _view, int _index, QUndoCommand *parent) :
	QUndoCommand(parent),
	view(_view),
	index(_index)
{
	setText(QObject::tr("Delete page"));
}

void DeletePageCommand::redo()
{
	assert(!page);
	page = view->doc->delete_page(index);
	view->gotoPage(index);
}

void DeletePageCommand::undo()
{
	assert(page);
	view->doc->add_page(index, std::move(page));
	view->gotoPage(index);
}


AddStrokeCommand::AddStrokeCommand(sauklaue* _view, int _page, int _layer, std::unique_ptr<Stroke> _stroke, QUndoCommand *parent) :
	QUndoCommand(parent),
	view(_view),
	page(_page),
	layer(_layer),
	stroke(std::move(_stroke))
{
	setText(QObject::tr("Draw stroke"));
}

void AddStrokeCommand::redo()
{
	assert(stroke);
	view->doc->page(page)->layer(layer)->add_stroke(std::move(stroke));
}

void AddStrokeCommand::undo()
{
	assert(!stroke);
	stroke = view->doc->page(page)->layer(layer)->delete_stroke();
}


