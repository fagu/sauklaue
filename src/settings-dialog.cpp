#include "settings-dialog.h"

#include "settings.h"
#include "tablet.h"

#include <QGridLayout>
#include <QCheckBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QToolButton>

#include <set>

TabletRow::TabletRow(const TabletSettings& tablet, QGridLayout* tabletGrid, int i, bool connected) {
	m_name = tablet.name;
	m_orientation_degrees = tablet.orientation;

	m_enabled = new QCheckBox;
	m_enabled->setChecked(tablet.enabled);
	m_enabled->setIconSize(QSize(32, 32));
	m_enabled->setIcon(QIcon::fromTheme(connected ? "network-connect" : "network-disconnect"));
	m_enabled->setText(tablet.name);
	connect(m_enabled, &QCheckBox::toggled, this, &TabletRow::update);
	tabletGrid->addWidget(m_enabled, i + 1, 0);

	m_rotate_left = new QToolButton;
	m_rotate_left->setIcon(QIcon::fromTheme("object-rotate-left"));
	m_rotate_left->setToolTip(tr("Rotate left"));
	connect(m_rotate_left, &QPushButton::clicked, this, &TabletRow::rotateLeft);
	tabletGrid->addWidget(m_rotate_left, i + 1, 1);

	m_orientation = new QLabel;
	m_orientation->setMinimumWidth(45);
	m_orientation->setAlignment(Qt::AlignRight);
	tabletGrid->addWidget(m_orientation, i + 1, 2);

	m_rotate_right = new QToolButton;
	m_rotate_right->setIcon(QIcon::fromTheme("object-rotate-right"));
	m_rotate_right->setToolTip(tr("Rotate right"));
	connect(m_rotate_right, &QPushButton::clicked, this, &TabletRow::rotateRight);
	tabletGrid->addWidget(m_rotate_right, i + 1, 3);

	m_width = new QDoubleSpinBox;
	m_width->setDecimals(1);
	m_width->setMinimum(0.1);
	m_width->setValue(0.1 * tablet.width);
	tabletGrid->addWidget(m_width, i + 1, 4);

	m_height = new QDoubleSpinBox;
	m_height->setDecimals(1);
	m_height->setMinimum(0.1);
	m_height->setValue(0.1 * tablet.height);
	tabletGrid->addWidget(m_height, i + 1, 5);

	update();
}

void TabletRow::update() {
	m_orientation->setText(QString::number(m_orientation_degrees) + "°");
	m_orientation->setEnabled(m_enabled->isChecked());
	m_rotate_left->setEnabled(m_enabled->isChecked());
	m_rotate_right->setEnabled(m_enabled->isChecked());
	m_width->setEnabled(m_enabled->isChecked());
	m_height->setEnabled(m_enabled->isChecked());
}

void TabletRow::rotateLeft() {
	m_orientation_degrees = (m_orientation_degrees + 270) % 360;
	update();
}

void TabletRow::rotateRight() {
	m_orientation_degrees = (m_orientation_degrees + 90) % 360;
	update();
}

TabletSettings TabletRow::get() const {
	TabletSettings res(m_name);
	res.enabled = m_enabled->isChecked();
	res.orientation = m_orientation_degrees;
	res.width = qRound(m_width->value() * 10);
	res.height = qRound(m_height->value() * 10);
	return res;
}

SettingsDialog::SettingsDialog(QWidget* parent) :
    QDialog(parent) {
	setWindowTitle(tr("Settings"));

	// It can be convenient to try out a setup with the preferences window open.
	// We therefore don't make the window modal.

	QVBoxLayout* layout = new QVBoxLayout(this);

	{
		QLabel* label = new QLabel(tr("Check the external tablet/pen device(s) whose writing area should be mapped only onto the current page. Also input the orientation of the tablet and its width/height ratio (before rotation)."));
		label->setWordWrap(true);
		layout->addWidget(label);
	}

	tabletGrid = new QGridLayout;
	layout->addLayout(tabletGrid);

	QHBoxLayout* bottom = new QHBoxLayout;
	layout->addLayout(bottom);
	{
		QPushButton* button = new QPushButton(QIcon::fromTheme("view-refresh"), tr("Reload"));
		connect(button, &QPushButton::clicked, this, &SettingsDialog::reload);
		bottom->addWidget(button);
	}
	bottom->addStretch();
	{
		QPushButton* button = new QPushButton(QIcon::fromTheme("dialog-ok-apply"), tr("OK"));
		connect(button, &QPushButton::clicked, this, &SettingsDialog::ok);
		button->setDefault(true);
		bottom->addWidget(button);
	}
	{
		QPushButton* button = new QPushButton(QIcon::fromTheme("dialog-ok-apply"), tr("Apply"));
		connect(button, &QPushButton::clicked, this, &SettingsDialog::apply);
		bottom->addWidget(button);
	}
	{
		QPushButton* button = new QPushButton(QIcon::fromTheme("dialog-cancel"), tr("Cancel"));
		connect(button, &QPushButton::clicked, this, &SettingsDialog::cancel);
		bottom->addWidget(button);
	}

	reload();
}

void SettingsDialog::reload() {
	while (QLayoutItem* child = tabletGrid->takeAt(0)) {
		delete child->widget();
		delete child;
	}
	{
		QLabel* label = new QLabel(tr("Device"));
		QFont font = label->font();
		font.setBold(true);
		label->setFont(font);
		tabletGrid->addWidget(label, 0, 0);
	}
	{
		QLabel* label = new QLabel(tr("Rotation"));
		label->setAlignment(Qt::AlignCenter);
		QFont font = label->font();
		font.setBold(true);
		label->setFont(font);
		tabletGrid->addWidget(label, 0, 1, 1, 3);
	}
	{
		QLabel* label = new QLabel(tr("Width"));
		label->setAlignment(Qt::AlignCenter);
		QFont font = label->font();
		font.setBold(true);
		label->setFont(font);
		tabletGrid->addWidget(label, 0, 4);
	}
	{
		QLabel* label = new QLabel(tr("Height"));
		label->setAlignment(Qt::AlignCenter);
		QFont font = label->font();
		font.setBold(true);
		label->setFont(font);
		tabletGrid->addWidget(label, 0, 5);
	}

	std::set<QString> tablet_names;
	std::vector<TabletSettings> tablet_settings = Settings::self()->tablets();
	for (const TabletSettings& tablet : tablet_settings)
		tablet_names.insert(tablet.name);
	m_rows.clear();
	std::set<QString> connected_tablets;
	for (const QString& tablet_name : TabletHandler::self()->device_list()) {
		connected_tablets.insert(tablet_name);
		if (!tablet_names.count(tablet_name)) {
			tablet_settings.emplace_back(tablet_name);
			tablet_names.insert(tablet_name);
		}
	}
	std::sort(tablet_settings.begin(), tablet_settings.end(), [](const TabletSettings& a, const TabletSettings& b) { return a.name < b.name; });
	for (int i = 0; i < (int)tablet_settings.size(); i++) {
		m_rows.push_back(std::make_unique<TabletRow>(tablet_settings[i], tabletGrid, i + 1, connected_tablets.count(tablet_settings[i].name)));
	}
}

void SettingsDialog::ok() {
	apply();
	close();
}

void SettingsDialog::apply() {
	std::vector<TabletSettings> tablets;
	for (const auto& row : m_rows)
		tablets.push_back(row->get());
	Settings::self()->set_tablets(tablets);
	Settings::self()->save();
}

void SettingsDialog::cancel() {
	close();
}
