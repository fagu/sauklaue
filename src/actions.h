#ifndef ACTIONS_H
#define ACTIONS_H

#include "document.h"

#include <QUndoCommand>

class sauklaue;

class NewPageCommand : public QUndoCommand {
public:
	NewPageCommand(sauklaue* _view, int _index, std::unique_ptr<Page> _page, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	sauklaue *view;
	int index;
	std::unique_ptr<Page> page;
};

class DeletePageCommand : public QUndoCommand {
public:
	DeletePageCommand(sauklaue* _view, int _index, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	sauklaue *view;
	int index;
	std::unique_ptr<Page> page;
};

class AddStrokeCommand : public QUndoCommand {
public:
	AddStrokeCommand(sauklaue* _view, int _page, int _layer, std::unique_ptr<Stroke> _stroke, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	sauklaue *view;
	int page;
	int layer;
	std::unique_ptr<Stroke> stroke;
};

#endif // ACTIONS_H
