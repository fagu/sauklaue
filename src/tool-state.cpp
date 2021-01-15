#include "tool-state.h"

#include <QUndoStack>

ToolState::ToolState(QObject* parent) :
    QObject(parent), m_undoStack(new QUndoStack(this)) {
}

void ToolState::setPenColor(QColor pen_color) {
	m_pen_color = pen_color;
}

void ToolState::setPenSize(int pen_size) {
	m_pen_size = pen_size;
}
