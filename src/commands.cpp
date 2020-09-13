#include "commands.h"

#include "mainwindow.h"



AddPagesCommand::AddPagesCommand(Document* _doc, int _first_page, std::vector<std::unique_ptr<SPage> > _pages, QUndoCommand *parent) :
	QUndoCommand(parent),
	doc(_doc),
	first_page(_first_page),
	number_of_pages(_pages.size()),
	pages(std::move(_pages))
{
	setText(number_of_pages == 1 ? QObject::tr("Add page") : QObject::tr("Add pages"));
}

void AddPagesCommand::redo()
{
// 	assert(pages);
	doc->add_pages(first_page, std::move(pages));
}

void AddPagesCommand::undo()
{
// 	assert(!pages);
	pages = doc->delete_pages(first_page, number_of_pages);
}


DeletePagesCommand::DeletePagesCommand(Document* _doc, int _first_page, int _number_of_pages, QUndoCommand *parent) :
	QUndoCommand(parent),
	doc(_doc),
	first_page(_first_page),
	number_of_pages(_number_of_pages)
{
	setText(number_of_pages == 1 ? QObject::tr("Delete page") : QObject::tr("Delete pages"));
}

void DeletePagesCommand::redo()
{
// 	assert(!pages);
	pages = doc->delete_pages(first_page, number_of_pages);
}

void DeletePagesCommand::undo()
{
// 	assert(pages);
	doc->add_pages(first_page, std::move(pages));
}

AddStrokeCommand::AddStrokeCommand(NormalLayer* _layer, unique_ptr_Stroke _stroke, QUndoCommand *parent) :
	QUndoCommand(parent),
	layer(_layer),
	stroke(std::move(_stroke))
{
	setText(std::visit(overloaded {
		[](PenStroke*) {return QObject::tr("Draw stroke");},
		[](EraserStroke*) {return QObject::tr("Erase");}
	}, get(stroke)));
}

void AddStrokeCommand::redo()
{
	assert(convert_variant<bool>(get(stroke)));
	layer->add_stroke(std::move(stroke));
}

void AddStrokeCommand::undo()
{
	assert(!convert_variant<bool>(get(stroke)));
	stroke = layer->delete_stroke();
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
