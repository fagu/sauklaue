#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include "all-types.h"
#include "src/tablet.h"

#include <QObject>
#include <QString>
#include <QDialog>
#include <KConfigDialog>

class QGridLayout;
class QCheckBox;
class QLabel;
class QDoubleSpinBox;
class QToolButton;
class QSpinBox;
class QShowEvent;

class TabletRow : public QObject {
	Q_OBJECT
public:
	TabletRow(const TabletSettings& tablet, QGridLayout* tabletGrid, int i, bool connected);
	TabletSettings get() const;

private:
	void update();
	void rotateRight();
	void rotateLeft();

	QString m_name;
	QCheckBox* m_enabled;
	QLabel* m_orientation;
	int m_orientation_degrees;
	QToolButton* m_rotate_left;
	QToolButton* m_rotate_right;
	QDoubleSpinBox* m_width;
	QDoubleSpinBox* m_height;
	QCheckBox* m_both_sides;
};

class SettingsGeneralWidget : public QWidget {
	Q_OBJECT
public:
	SettingsGeneralWidget(QWidget* parent = nullptr);
};

class SettingsTabletWidget : public QWidget {
	Q_OBJECT
public:
	SettingsTabletWidget(TabletHandler* tablet_handler, QWidget* parent = nullptr);
	void reset();
	void updateSettings();

private:
	TabletHandler* m_tablet_handler;
	QGridLayout* tabletGrid;
	std::vector<std::unique_ptr<TabletRow> > m_rows;
};

class SettingsDialog : public KConfigDialog {
	Q_OBJECT
public:
	SettingsDialog(TabletHandler* tablet_handler, QWidget* parent = nullptr);

protected:
	void updateWidgets() override;
	void updateWidgetsDefault() override;
	void updateSettings() override;
	bool hasChanged() override;
	bool isDefault() override;

	void pageChanged();

private:
	SettingsTabletWidget* tablet;
	KPageWidgetItem* tabletItem;
	QPushButton* reloadButton;
};

#endif
