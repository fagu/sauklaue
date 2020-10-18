#include "tablet.h"

#include "cairo-helpers.h"
#include "settings.h"

#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <X11/extensions/XInput2.h>
// #include <QtX11Extras/QX11Info>

std::unique_ptr<TabletHandler> tablet_singleton;

TabletHandler* TabletHandler::self() {
	if (!tablet_singleton)
		tablet_singleton = std::make_unique<TabletHandler>();
	return tablet_singleton.get();
}

TabletHandler::TabletHandler() {
	m_rect_one = m_rect_both = QRectF(0, 0, 1, 1);  // TODO
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
	m_active = false;
	update_matrices_now();
	XCloseDisplay(display);
}

std::vector<QString> TabletHandler::device_list() {
	if (!display)
		return {};
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

void TabletHandler::set_active_region(QRectF rect_one, QRectF rect_both, QSize screen_size) {
	if (m_rect_one == rect_one && m_rect_both == rect_both && m_screen_size == screen_size)
		return;
	m_rect_one = rect_one;
	m_rect_both = rect_both;
	m_screen_size = screen_size;
	if (!on_demand_timer->isActive())
		on_demand_timer->start(10);
}

QMatrix TabletHandler::matrix(const TabletSettings& tablet) const {
	// Without loss of generality, the unit in reality is 1 pixel.
	// Tablet -> Reality
	QMatrix t2r = QMatrix().scale(tablet.width, tablet.height) * QMatrix().rotate(tablet.orientation);
	// Reality -> Tablet
	QMatrix r2t = t2r.inverted();
	// Bounding rectangle for the rectangle to be mapped to in tablet space
	QRectF rect = r2t.mapRect(tablet.bothSides ? m_rect_both : m_rect_one);
	// Rescale the tablet so it becomes at least as large as the bounding rectangle.
	double scale = std::max(rect.width(), rect.height());
	// Translate the tablet so it contains the bounding rectangle.
	double dx = (rect.left() + rect.right() - scale) / 2, dy = (rect.top() + rect.bottom() - scale) / 2;
	return QMatrix().scale(scale, scale) * QMatrix().translate(dx, dy) * t2r * QMatrix().scale(1. / m_screen_size.width(), 1. / m_screen_size.height());
}

// TODO We waste quite a bit of time waiting for the X server to respond. It would be better to do this in a separate thread. We could then perhaps even increase the update frequency.
void TabletHandler::update_matrices_now() {
	// 	qDebug() << "Setting transformation matrix.";
	if (!display)
		return;
	// See do_set_prop_xi2() in https://cgit.freedesktop.org/xorg/app/xinput/tree/src/property.c
	static_assert(sizeof(float) == 4);
	int ndevices;
	XIDeviceInfo* info;
	info = XIQueryDevice(display, XIAllDevices, &ndevices);
	bool active = m_active && (qApp && qApp->activeWindow() != nullptr);
	//	bool found = false;
	for (int i = 0; i < ndevices; i++) {
		XIDeviceInfo* device = &info[i];
		if (!(device->use == XIMasterPointer || device->use == XISlavePointer))
			continue;
		std::optional<TabletSettings> tablet = Settings::self()->tablet(device->name);
		if (active && tablet && tablet->enabled) {
			m_changed.insert(device->deviceid);
			QMatrix cmat = matrix(*tablet);
			// 	QProcess::execute("xinput", {"set-prop", "XPPEN Tablet Pen (0)", "--type=float", "Coordinate Transformation Matrix", QString::number(e1x), QString::number(e2x), QString::number(dx), QString::number(e1y), QString::number(e2y), QString::number(dy), "0", "0", "1"});
			float mat[9] = {(float)cmat.m11(), (float)cmat.m21(), (float)cmat.dx(), (float)cmat.m12(), (float)cmat.m22(), (float)cmat.dy(), 0, 0, 1};
			XIChangeProperty(display, device->deviceid, XInternAtom(display, "Coordinate Transformation Matrix", False), XInternAtom(display, "FLOAT", False), 32, PropModeReplace, (unsigned char*)mat, 9);
			//			found = true;
		} else if (auto it = m_changed.find(device->deviceid); it != m_changed.end()) {
			m_changed.erase(it);
			float mat[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
			XIChangeProperty(display, device->deviceid, XInternAtom(display, "Coordinate Transformation Matrix", False), XInternAtom(display, "FLOAT", False), 32, PropModeReplace, (unsigned char*)mat, 9);
		}
	}
	// 	if (!found)
	// 		qDebug() << "Couldn't find device.";
	XIFreeDeviceInfo(info);
	// I don't understand why, but somehow the changed coordinate transformation matrix is not always applied immediately.
	// However, it seems to be applied the next time we get a list of all devices, or when we close the display.
	info = XIQueryDevice(display, XIAllDevices, &ndevices);
	XIFreeDeviceInfo(info);
}
