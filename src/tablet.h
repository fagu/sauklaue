#ifndef TABLET_H
#define TABLET_H

#include "all-types.h"

#include <QObject>
#include <QRectF>
#include <QSize>
#include <QMatrix>

#include <set>

class QTimer;
struct _XDisplay;

class TabletHandler : public QObject {
	Q_OBJECT
public:
	static TabletHandler* self();

	TabletHandler();
	~TabletHandler();

public:
	// An up-to-date list of connected pointer devices.
	std::vector<QString> device_list();

	// Maps tablets to the given region in screen coordinates (px).
	void set_active_region(QRectF rect_one, QRectF rect_both, QSize screen_size);

private:
	void update_matrices_now();

	QMatrix matrix(const TabletSettings& tablet) const;

	// A rectangle encompassing the focused page.
	QRectF m_rect_one;
	// A rectangle encompassing both pages.
	QRectF m_rect_both;
	// The total (virtual) screen size.
	QSize m_screen_size;

	// Whether we should currently manage the transformation matrix. (We should stop when the application loses focus. For example, if there are multiple instances of this program running, they should cooperate.)
	bool m_active = true;

	// Set of tablet ids whose transformation matrix we have changed.
	// TODO Save and restore the original transformation matrix.
	std::set<int> m_changed;

	// Update the transformation matrix soon when m_rect_one or m_screen_size changed.
	QTimer* on_demand_timer;
	// Occasionally try to set the transformation matrix just in case a tablet/pen was connected in the meantime.
	QTimer* periodic_timer;

	_XDisplay* display;
};

#endif  // TABLET_H
