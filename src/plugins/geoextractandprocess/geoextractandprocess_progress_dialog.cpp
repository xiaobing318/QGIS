#include "geoextractandprocess_progress_dialog.h"

QgsGeoExtractAndProcessProgressDialog::QgsGeoExtractAndProcessProgressDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
}

void QgsGeoExtractAndProcessProgressDialog::setTitle(const QString& title)
{
	setWindowTitle(title);
}

void QgsGeoExtractAndProcessProgressDialog::setCurrentLayer(int layer, int numLayers)
{
	//label->setText( tr( "Layer %1 of %2.." ).arg( layer ).arg( numLayers ) );
	ui.progressBar->reset();
}

void QgsGeoExtractAndProcessProgressDialog::setupProgressBar(const QString& format, int maximum)
{
	ui.progressBar->setFormat(format);
	ui.progressBar->setRange(0, maximum);
	ui.progressBar->reset();

	mProgressUpdate = maximum / 100;
	if (mProgressUpdate == 0)
	{
		mProgressUpdate = 1;
	}
}

void QgsGeoExtractAndProcessProgressDialog::setProgressValue(int value)
{
	// update progress every nth feature for faster processing
	if (value == ui.progressBar->maximum() || value % mProgressUpdate == 0)
	{
		ui.progressBar->setValue(value);
	}
}