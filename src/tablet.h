#ifndef TABLET_H
#define TABLET_H

#include "all-types.h"

#include <QObject>
#include <QRectF>
#include <QSize>

#include <cairomm/matrix.h>

class QTimer;
struct _XDisplay;

class TabletHandler : public QObject {
	Q_OBJECT
public:
	static TabletHandler* self();

private:
	TabletHandler();
	~TabletHandler();

public:
	// An up-to-date list of connected pointer devices.
	std::vector<QString> device_list();

	// Maps tablets to the given region in screen coordinates (px).
	void set_active_region(QRectF rect, QRectF rect_both, QSize screen_size);

private:
	void update_matrices_now();

	Cairo::Matrix matrix(const TabletSettings& tablet) const;

	// The screen area the tablet should be mapped to.
	QRectF m_rect;
	// A rectangle encompassing both pages.
	QRectF m_rect_both;
	// The total (virtual) screen size.
	QSize m_screen_size;

	// Update the transformation matrix soon when m_rect or m_screen_size changed.
	QTimer* on_demand_timer;
	// Occasionally try to set the transformation matrix just in case a tablet/pen was connected in the meantime.
	QTimer* periodic_timer;

	_XDisplay* display;
};

#endif  // TABLET_H
