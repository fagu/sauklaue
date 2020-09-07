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

template <class StrokeType>
class AddStrokeCommand : public QUndoCommand {
public:
	AddStrokeCommand(sauklaue* _view, int _page, int _layer, std::unique_ptr<StrokeType> _stroke, QUndoCommand *parent=nullptr);
	void redo() override;
	void undo() override;
	
private:
	sauklaue *view;
	int page;
	int layer;
	std::unique_ptr<StrokeType> stroke;
};

class AddPenStrokeCommand : public AddStrokeCommand<PenStroke> {
public:
	AddPenStrokeCommand(sauklaue* _view, int _page, int _layer, std::unique_ptr<PenStroke> _stroke, QUndoCommand *parent=nullptr);
};

class AddEraserStrokeCommand : public AddStrokeCommand<EraserStroke> {
public:
	AddEraserStrokeCommand(sauklaue* _view, int _page, int _layer, std::unique_ptr<EraserStroke> _stroke, QUndoCommand *parent=nullptr);
};

#endif // ACTIONS_H
