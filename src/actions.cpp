#include "actions.h"

#include "mainwindow.h"



NewPageCommand::NewPageCommand(Document* _doc, int _index, std::unique_ptr<SPage> _page, QUndoCommand *parent) :
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
AddStrokeCommand<StrokeType>::AddStrokeCommand(NormalLayer* _layer, std::unique_ptr<StrokeType> _stroke, QUndoCommand *parent) :
	QUndoCommand(parent),
	layer(_layer),
	stroke(std::move(_stroke))
{
}

template <class StrokeType>
void AddStrokeCommand<StrokeType>::redo()
{
	assert(stroke);
	layer->add_stroke(std::move(stroke));
}

template <class StrokeType>
void AddStrokeCommand<StrokeType>::undo()
{
	assert(!stroke);
	stroke = std::get<std::unique_ptr<StrokeType> >(layer->delete_stroke());
}

AddPenStrokeCommand::AddPenStrokeCommand(NormalLayer* _layer, std::unique_ptr<PenStroke> _stroke, QUndoCommand* parent) : AddStrokeCommand<PenStroke>(_layer, std::move(_stroke), parent)
{
	setText(QObject::tr("Draw stroke"));
}

AddEraserStrokeCommand::AddEraserStrokeCommand(NormalLayer* _layer, std::unique_ptr<EraserStroke> _stroke, QUndoCommand* parent) : AddStrokeCommand<EraserStroke>(_layer, std::move(_stroke), parent)
{
	setText(QObject::tr("Erase"));
}

AddEmbeddedPDFCommand::AddEmbeddedPDFCommand(Document* doc, std::unique_ptr<EmbeddedPDF> pdf, QUndoCommand* parent) :
	QUndoCommand(parent),
	m_doc(doc),
	m_pdf(std::move(pdf))
{
	setText(QObject::tr("Embed PDF"));
}

void AddEmbeddedPDFCommand::redo()
{
	assert(m_pdf);
	m_it = m_doc->add_embedded_pdf(std::move(m_pdf));
}

void AddEmbeddedPDFCommand::undo()
{
	assert(!m_pdf);
	m_pdf = m_doc->delete_embedded_pdf(m_it);
}
