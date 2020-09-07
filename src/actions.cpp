#include "actions.h"

#include "mainwindow.h"

#include <QtWidgets>

NewPageCommand::NewPageCommand(Document* _doc, int _index, std::unique_ptr<Page> _page, QUndoCommand *parent) :
	QUndoCommand(parent),
	doc(_doc),
	index(_index),
	page(std::move(_page))
{
	setText(QObject::tr("New page"));
}

void NewPageCommand::redo()
{
	assert(page);
	doc->add_page(index, std::move(page));
}

void NewPageCommand::undo()
{
	assert(!page);
	page = doc->delete_page(index);
}


DeletePageCommand::DeletePageCommand(Document* _doc, int _index, QUndoCommand *parent) :
	QUndoCommand(parent),
	doc(_doc),
	index(_index)
{
	setText(QObject::tr("Delete page"));
}

void DeletePageCommand::redo()
{
	assert(!page);
	page = doc->delete_page(index);
}

void DeletePageCommand::undo()
{
	assert(page);
	doc->add_page(index, std::move(page));
}

template <class StrokeType>
AddStrokeCommand<StrokeType>::AddStrokeCommand(Document* _doc, int _page, int _layer, std::unique_ptr<StrokeType> _stroke, QUndoCommand *parent) :
	QUndoCommand(parent),
	doc(_doc),
	page(_page),
	layer(_layer),
	stroke(std::move(_stroke))
{
}

template <class StrokeType>
void AddStrokeCommand<StrokeType>::redo()
{
	assert(stroke);
	doc->page(page)->layer(layer)->add_stroke(std::move(stroke));
}

template <class StrokeType>
void AddStrokeCommand<StrokeType>::undo()
{
	assert(!stroke);
	stroke = std::get<std::unique_ptr<StrokeType> >(doc->page(page)->layer(layer)->delete_stroke());
}

AddPenStrokeCommand::AddPenStrokeCommand(Document* _doc, int _page, int _layer, std::unique_ptr<PenStroke> _stroke, QUndoCommand* parent) : AddStrokeCommand<PenStroke>(_doc, _page, _layer, std::move(_stroke), parent)
{
	setText(QObject::tr("Draw stroke"));
}

AddEraserStrokeCommand::AddEraserStrokeCommand(Document* _doc, int _page, int _layer, std::unique_ptr<EraserStroke> _stroke, QUndoCommand* parent) : AddStrokeCommand<EraserStroke>(_doc, _page, _layer, std::move(_stroke), parent)
{
	setText(QObject::tr("Erase"));
}
