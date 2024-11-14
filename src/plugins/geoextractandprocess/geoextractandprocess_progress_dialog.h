#ifndef QGS_GEOEXTRACTANDPROCESS_PROGRESS_DIALOG_H
#define QGS_GEOEXTRACTANDPROCESS_PROGRESS_DIALOG_H

#include <QDialog>
#include "ui_geoextractandprocess_progress_dialog_base.h"

class QgsGeoExtractAndProcessProgressDialog : public QDialog
{
	Q_OBJECT

public:
	QgsGeoExtractAndProcessProgressDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());

	void setTitle(const QString& title);
	void setCurrentLayer(int layer, int numLayers);
	void setupProgressBar(const QString& format, int maximum);
	void setProgressValue(int value);

private:
	int mProgressUpdate = 0;
	Ui_QgsGeoExtractAndProcessProgressDialogBase ui;
};

#endif // QGS_GEOEXTRACTANDPROCESS_PROGRESS_DIALOG_H
