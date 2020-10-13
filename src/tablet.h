#ifndef TABLET_H
#define TABLET_H

#include "util.h"

#include <QObject>
#include <QTimer>
#include <QRectF>
#include <QSize>

#include <cairomm/matrix.h>

struct _XDisplay;

class TabletSetting;

class Tablet : public QObject
{
	Q_OBJECT
public:
	static Tablet *self();
private:
	Tablet();
	~Tablet();
public:
	std::vector<QString> device_list();
	// Maps tablets to the given region in screen coordinates (px).
	void set_active_region(QRectF rect, QSize screen_size);
private slots:
	void time_to_set_transformation_matrix();
private:
	Cairo::Matrix matrix(const TabletSetting& tablet) const;
	QRectF m_rect;
	QSize m_screen_size;
	QTimer* transformation_matrix_timer; // Quick on-demand one-shot timer
	QTimer* transformation_matrix_timer_backup; // Occasionally try to set the transformation matrix just in case a tablet/pen was connected in the meantime.
	_XDisplay* display;
};

#endif // TABLET_H
