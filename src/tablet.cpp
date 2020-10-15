#include "tablet.h"

#include "settings.h"

#include <cmath>

#include <QDebug>
#include <QProcess>
#include <X11/extensions/XInput2.h>
// #include <QtX11Extras/QX11Info>

TabletHandler* tablet_singleton = nullptr;

TabletHandler* TabletHandler::self() {
	if (!tablet_singleton)
		tablet_singleton = new TabletHandler;
	return tablet_singleton;
}

TabletHandler::TabletHandler() {
	m_rect = QRectF(0, 0, 1, 1);  // TODO
	m_screen_size = QSize(1, 1);  // TODO
	on_demand_timer = new QTimer(this);
	on_demand_timer->setSingleShot(true);
	periodic_timer = new QTimer(this);
	connect(on_demand_timer, &QTimer::timeout, this, &TabletHandler::update_matrices_now);
	connect(periodic_timer, &QTimer::timeout, this, &TabletHandler::update_matrices_now);
	// 	display = QX11Info::display();
	display = XOpenDisplay(nullptr);
	if (!display) {
		qDebug() << "No X11 display!";
		return;
	}
	int xi_opcode, event, error;
	if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
		qDebug() << "X Input extension not available.";
		display = nullptr;
		return;
	}
	// TODO Instead of trying to set the transformation matrix once every second, watch out for "device connected" signals.
	periodic_timer->start(1000);
}

TabletHandler::~TabletHandler() {
	// Immediately reset the transformation matrix to the identity matrix.
	m_rect = QRectF(0, 0, 1, 1);  // TODO
	m_screen_size = QSize(1, 1);  // TODO
	update_matrices_now();
	XCloseDisplay(display);
}

std::vector<QString> TabletHandler::device_list() {
	std::vector<QString> tablet_names;
	// Print a list of all devices.
	// See list_xi2() in https://cgit.freedesktop.org/xorg/app/xinput/tree/src/list.c
	int ndevices;
	XIDeviceInfo* info;
	info = XIQueryDevice(display, XIAllDevices, &ndevices);
	for (int i = 0; i < ndevices; i++) {
		XIDeviceInfo* device = &info[i];
		qDebug() << "Device" << device->deviceid << device->name << device->attachment << device->use;
		if (device->use == XIMasterPointer || device->use == XISlavePointer)
			tablet_names.emplace_back(device->name);
	}
	XIFreeDeviceInfo(info);
	return tablet_names;
}

void TabletHandler::set_active_region(QRectF rect, QSize screen_size) {
	if (m_rect == rect && m_screen_size == screen_size)
		return;
	m_rect = rect;
	m_screen_size = screen_size;
	on_demand_timer->start(10);
}

Cairo::Matrix tablet_to_reality(const TabletSettings& tablet) {
	return Cairo::scaling_matrix(tablet.width, tablet.height) * Cairo::rotation_matrix(tablet.orientation * M_PI / 180);
}

Cairo::Matrix TabletHandler::matrix(const TabletSettings& tablet) const {
	Cairo::Matrix t2r = tablet_to_reality(tablet);
	Cairo::Matrix r2t = t2r;
	r2t.invert();
	QRectF minrect2 = bounding_rect(r2t, m_rect);
	double scale = std::max(minrect2.width(), minrect2.height());
	double dx = (minrect2.left() + minrect2.right() - scale) / 2, dy = (minrect2.top() + minrect2.bottom() - scale) / 2;
	return Cairo::scaling_matrix(scale, scale) * Cairo::translation_matrix(dx, dy) * t2r * Cairo::scaling_matrix(1. / m_screen_size.width(), 1. / m_screen_size.height());
}

void TabletHandler::update_matrices_now() {
	qDebug() << "Setting transformation matrix.";
	if (!display) {
		qDebug() << "No X11 display!";
		return;
	}
	// See do_set_prop_xi2() in https://cgit.freedesktop.org/xorg/app/xinput/tree/src/property.c
	static_assert(sizeof(float) == 4);
	int ndevices;
	XIDeviceInfo* info;
	info = XIQueryDevice(display, XIAllDevices, &ndevices);
	bool found = false;
	for (int i = 0; i < ndevices; i++) {
		XIDeviceInfo* device = &info[i];
		std::optional<TabletSettings> tablet = Settings::self()->tablet(device->name);
		if (tablet && tablet->enabled) {
			Cairo::Matrix cmat = matrix(*tablet);
			double dx = 0, dy = 0;
			cmat.transform_point(dx, dy);
			double e1x = 1, e1y = 0;
			cmat.transform_distance(e1x, e1y);
			double e2x = 0, e2y = 1;
			cmat.transform_distance(e2x, e2y);
			// 	QProcess::execute("xinput", {"set-prop", "XPPEN Tablet Pen (0)", "--type=float", "Coordinate Transformation Matrix", QString::number(e1x), QString::number(e2x), QString::number(dx), QString::number(e1y), QString::number(e2y), QString::number(dy), "0", "0", "1"});
			float mat[9] = {(float)e1x, (float)e2x, (float)dx, (float)e1y, (float)e2y, (float)dy, 0, 0, 1};
			XIChangeProperty(display, device->deviceid, XInternAtom(display, "Coordinate Transformation Matrix", False), XInternAtom(display, "FLOAT", False), 32, PropModeReplace, (unsigned char*)mat, 9);
			found = true;
		} else {
			float mat[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
			XIChangeProperty(display, device->deviceid, XInternAtom(display, "Coordinate Transformation Matrix", False), XInternAtom(display, "FLOAT", False), 32, PropModeReplace, (unsigned char*)mat, 9);
		}
	}
	if (!found)
		qDebug() << "Couldn't find device.";
	XIFreeDeviceInfo(info);
	// I don't understand why, but somehow the changed coordinate transformation matrix is not always applied immediately.
	// However, it seems to be applied the next time we get a list of all devices, or when we close the display.
	info = XIQueryDevice(display, XIAllDevices, &ndevices);
	XIFreeDeviceInfo(info);
}
