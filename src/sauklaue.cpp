#include "sauklaue.h"
#include "ui_sauklaue.h"

sauklaue::sauklaue(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::sauklaue)
{
    m_ui->setupUi(this);
}

sauklaue::~sauklaue() = default;
