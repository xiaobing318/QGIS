#ifndef QGS_AUTOMERGE_DIALOG_H
#define QGS_AUTOMERGE_DIALOG_H

#include <QDialog>

#include "ui_automerge.h"

class QgsAutoMergeDialog : public QDialog
{
	Q_OBJECT

public:
	QgsAutoMergeDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsAutoMergeDialog() override;

private:

	Ui_QgsAutoMergeDlg ui;

private slots:

	void Button_OK_accepted();

	void Button_Cancel_rejected();
};

#endif // QGS_AUTOMERGE_DIALOG_H
