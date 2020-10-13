#ifndef SETTINGS_H
#define SETTINGS_H

#include "util.h"
#include "config.h"

#include <QString>
#include <QDialog>
#include <QGridLayout>
#include <QCheckBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>

#include <map>
#include <optional>

class Tablet;

class TabletSetting {
public:
	QString name; // Name of the input device
	bool enabled;
	int orientation; // Rotation in degrees
	int width, height;
	TabletSetting(QString _name) : name(_name), enabled(false), orientation(0), width(100), height(100) {}
	TabletSetting(const TabletSetting&) = default;
};

class Settings : public ConfigGenerated {
	Q_OBJECT
public:
	static Settings *self();
	std::vector<TabletSetting> tablets() const;
	void set_tablets(const std::vector<TabletSetting> &tablets);
	std::optional<TabletSetting> tablet(const QString name) const;
private:
	Settings();
protected:
	virtual void usrRead();
	virtual bool usrSave();
private:
	std::map<QString,TabletSetting> m_tablets;
	std::vector<TabletSetting> m_tabletsvec;
};

class TabletRow : public QObject {
	Q_OBJECT
public:
	TabletRow(const TabletSetting& tablet, QGridLayout *tabletGrid, int i, bool connected);
	TabletSetting get() const;
private slots:
	void update();
	void rotateRight();
	void rotateLeft();
private:
	QString m_name;
	QCheckBox *m_enabled;
	QLabel *m_orientation;
	int m_orientation_degrees;
	QPushButton *m_rotate_left;
	QPushButton *m_rotate_right;
	QDoubleSpinBox *m_width;
	QDoubleSpinBox *m_height;
};

class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	SettingsDialog(QWidget *parent = nullptr);
private slots:
	void reload();
	void ok();
	void cancel();
private:
	QGridLayout *tabletGrid;
	std::vector<std::unique_ptr<TabletRow> > m_rows;
};

#endif
