#ifndef QGS_CONVERT_OPERATIONAREA_AND_SHEET_H
#define QGS_CONVERT_OPERATIONAREA_AND_SHEET_H

#include <QDialog>

#include "ui_convert_operationarea_and_sheet.h"

class QgsConvertOperationAreaAndSheet : public QDialog
{
	Q_OBJECT

public:
	QgsConvertOperationAreaAndSheet(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsConvertOperationAreaAndSheet() override;

public slots:

private:

	Ui_QgsConvertOperationAreaAndSheet ui;

private slots:

	void Button_Convert_accepted();
	void Button_Close_rejected();
};

#endif // QGS_CONVERT_OPERATIONAREA_AND_SHEET_H
