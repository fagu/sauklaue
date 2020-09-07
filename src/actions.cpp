#include "actions.h"

#include "mainwindow.h"

#include <QtWidgets>

NewPageCommand::NewPageCommand(MainWindow* _view, int _index, std::unique_ptr<Page> _page, QUndoCommand *parent) :
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


DeletePageCommand::DeletePageCommand(MainWindow* _view, int _index, QUndoCommand *parent) :
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

template <class StrokeType>
AddStrokeCommand<StrokeType>::AddStrokeCommand(MainWindow* _view, int _page, int _layer, std::unique_ptr<StrokeType> _stroke, QUndoCommand *parent) :
	QUndoCommand(parent),
	view(_view),
	page(_page),
	layer(_layer),
	stroke(std::move(_stroke))
{
}

template <class StrokeType>
void AddStrokeCommand<StrokeType>::redo()
{
	assert(stroke);
	view->doc->page(page)->layer(layer)->add_stroke(std::move(stroke));
}

template <class StrokeType>
void AddStrokeCommand<StrokeType>::undo()
{
	assert(!stroke);
	stroke = std::get<std::unique_ptr<StrokeType> >(view->doc->page(page)->layer(layer)->delete_stroke());
}

AddPenStrokeCommand::AddPenStrokeCommand(MainWindow* _view, int _page, int _layer, std::unique_ptr<PenStroke> _stroke, QUndoCommand* parent) : AddStrokeCommand<PenStroke>(_view, _page, _layer, std::move(_stroke), parent)
{
	setText(QObject::tr("Draw stroke"));
}

AddEraserStrokeCommand::AddEraserStrokeCommand(MainWindow* _view, int _page, int _layer, std::unique_ptr<EraserStroke> _stroke, QUndoCommand* parent) : AddStrokeCommand<EraserStroke>(_view, _page, _layer, std::move(_stroke), parent)
{
	setText(QObject::tr("Erase"));
}
