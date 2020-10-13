#ifndef TABLET_H
#define TABLET_H

#include "util.h"

#include <QObject>
#include <QTimer>
#include <QRectF>
#include <QSize>

#include <cairomm/matrix.h>

struct _XDisplay;

class TabletSettings;

class TabletHandler : public QObject
{
	Q_OBJECT
public:
	static TabletHandler *self();
private:
	TabletHandler();
	~TabletHandler();
	
public:
	// An up-to-date list of connected pointer devices.
	std::vector<QString> device_list();
	
	// Maps tablets to the given region in screen coordinates (px).
	void set_active_region(QRectF rect, QSize screen_size);
	
private slots:
	void update_matrices_now();
	
private:
	Cairo::Matrix matrix(const TabletSettings& tablet) const;
	
	// The screen area the tablet should be mapped to.
	QRectF m_rect;
	// The total (virtual) screen size.
	QSize m_screen_size;
	
	// Update the transformation matrix soon when m_rect or m_screen_size changed.
	QTimer* on_demand_timer;
	// Occasionally try to set the transformation matrix just in case a tablet/pen was connected in the meantime.
	QTimer* periodic_timer;
	
	_XDisplay* display;
};

#endif // TABLET_H
