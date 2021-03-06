#ifndef COMMANDS_H
#define COMMANDS_H

#include "all-types.h"

#include <QUndoCommand>

class AddPagesCommand : public QUndoCommand {
public:
	AddPagesCommand(Document* _doc, int _first_page, std::vector<std::unique_ptr<SPage> > _pages, QUndoCommand* parent = nullptr);
	void redo() override;
	void undo() override;

private:
	Document* doc;
	int first_page;
	int number_of_pages;
	std::vector<std::unique_ptr<SPage> > pages;
};

class DeletePagesCommand : public QUndoCommand {
public:
	DeletePagesCommand(Document* _doc, int _first_page, int _number_of_pages, QUndoCommand* parent = nullptr);
	void redo() override;
	void undo() override;

private:
	Document* doc;
	int first_page;
	int number_of_pages;
	std::vector<std::unique_ptr<SPage> > pages;
};

class AddStrokeCommand : public QUndoCommand {
public:
	AddStrokeCommand(NormalLayer* _layer, unique_ptr_Stroke _stroke, QUndoCommand* parent = nullptr);
	void redo() override;
	void undo() override;

private:
	NormalLayer* layer;
	unique_ptr_Stroke stroke;
};

class AddEmbeddedPDFCommand : public QUndoCommand {
public:
	AddEmbeddedPDFCommand(Document* doc, std::unique_ptr<EmbeddedPDF> pdf, QUndoCommand* parent = nullptr);
	void redo() override;
	void undo() override;

private:
	Document* m_doc;
	std::unique_ptr<EmbeddedPDF> m_pdf;
	std::list<std::unique_ptr<EmbeddedPDF> >::iterator m_it;
};

class GotoPDFPageCommand : public QUndoCommand {
public:
	GotoPDFPageCommand(PDFLayer* layer, int page, QUndoCommand* parent = nullptr);
	void redo() override;
	void undo() override;

private:
	PDFLayer* m_layer;
	int m_page;
};

#endif  // COMMANDS_H
