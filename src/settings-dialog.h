#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include "all-types.h"

#include <QObject>
#include <QString>
#include <QDialog>

class QGridLayout;
class QCheckBox;
class QLabel;
class QDoubleSpinBox;
class QPushButton;

class TabletRow : public QObject {
	Q_OBJECT
public:
	TabletRow(const TabletSettings& tablet, QGridLayout* tabletGrid, int i, bool connected);
	TabletSettings get() const;
private slots:
	void update();
	void rotateRight();
	void rotateLeft();

private:
	QString m_name;
	QCheckBox* m_enabled;
	QLabel* m_orientation;
	int m_orientation_degrees;
	QPushButton* m_rotate_left;
	QPushButton* m_rotate_right;
	QDoubleSpinBox* m_width;
	QDoubleSpinBox* m_height;
};

class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	SettingsDialog(QWidget* parent = nullptr);
private slots:
	void reload();
	void ok();
	void apply();
	void cancel();

private:
	QGridLayout* tabletGrid;
	std::vector<std::unique_ptr<TabletRow> > m_rows;
};

#endif
