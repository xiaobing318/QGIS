#include "automerge.h"
#include "qgsgui.h"

QgsAutoMergeDialog::QgsAutoMergeDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	connect(ui.pushButton_Yes, &QPushButton::clicked, this, &QgsAutoMergeDialog::Button_OK_accepted);
	connect(ui.pushButton_Cancel, &QPushButton::clicked, this, &QgsAutoMergeDialog::Button_Cancel_rejected);
}

QgsAutoMergeDialog::~QgsAutoMergeDialog()
{
}

void QgsAutoMergeDialog::Button_OK_accepted()
{
	accept();
}

void QgsAutoMergeDialog::Button_Cancel_rejected()
{
	reject();
}