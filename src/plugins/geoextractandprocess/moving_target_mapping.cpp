#include "qgsgui.h"
#include "moving_target_mapping.h"
#include "qgsproject.h"
#include "qmessagebox.h"
#include "qgslayoutmanager.h"
#include "qgsprintlayout.h"
#include "qgslayout.h"
#include "qgsguiutils.h"
#include "qgsproxyprogresstask.h"
#include "qgsmessagebar.h"
#include "qgslayoutitempage.h"
#include "qgslayoutpagecollection.h"
#include "qgslayoutitemlabel.h"

#include "qgslayoutitemmapitem.h"
#include "qgslayoutitemmapgrid.h"
#include "qgsvectorlayer.h"
#include "qgsrasterlayer.h"
#include "qgsmaplayer.h"
#include "qgslayoutitemlegend.h"
#include "qgslayoutitemscalebar.h"
#include "qgslayertree.h"
#include "qgsmaplayer.h"
#include "qgsmaplayerlegend.h"
#include <qfont.h>
#include "qgsfontutils.h"
#include <qfiledialog.h>
#include <string>
#include <qstandarditemmodel.h>
#include "qgis.h"
#include "qgslinesymbol.h"
#include "CSE_MapSheet.h"
#include "project/cse_geotransformation.h"
#include "cpl_conv.h"
#include "xlsxwriter.h"
#include "qgsmaplayerstylemanager.h"
#include "qgsmaplayerstyle.h"
#include "qgslayoutitempicture.h"
#include "qgslayoutitemshape.h"
#include <QtGui/qtextdocument.h>
#include <QtGui/qtextobject.h>
#include <qtextdocument.h>
#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include <qfontdialog.h>
#include <qcolordialog.h>




/*默认导出地图DPI*/
const int LAYOUT_DPI = 300;
const double OUTER_MAP_INTERIOR_MAP_DISTANCE = 10;       // 内外图廓宽度，单位：毫米
const double KM_LINE_LENGTH = 6;            // 内外图廓方里网刻度线长度，单位：毫米
const double ADJ_KM_LINE_LENGTH = 2;        // 邻带方里网刻度线长度，单位：毫米
const double KM_LINE_TOLERANCE_X = 13;       // X方向方里网刻度线与角点容差，单位：毫米，避免与角点经度绘制重叠
const double KM_LINE_TOLERANCE_Y = 8;       // Y方向方里网刻度线与角点容差，单位：毫米，避免与角点纬度绘制重叠
const double IMAGE_MAPPING_ZERO = 1.0e-3;	// 判断高斯坐标和布局坐标相等容差
const double LEGEND_OFFSET_X = 6;			// 主地图图例左边界横坐标相对地图外图廓右边界距离，单位：毫米
const double MAP_ANNOTATION_OFFSET_X = 6;	// 主地图附注左边界横坐标相对地图外图廓右边界距离，单位：毫米
const double LEGEND_PICTURE_LABEL_DISTANCE = 14;		// 图例图片左边界与图例文本左边界距离，单位：毫米


#define CLOCKWISE 1
#define COUNTERCLOCKWISE -1

using namespace std;

QgsMovingTargetMappingDialog::QgsMovingTargetMappingDialog(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	// 动目标制图
	connect(ui.pushButton_Mapping, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::MovingTargetMapping);

	// 关闭
	connect(ui.pushButton_Close, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::Close);

	// 打开轨迹点数据
	connect(ui.pushButton_OpenTracePointData, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::OpenTracePointData);
	
	// 打开影像数据
	connect(ui.pushButton_OpenImageData, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::OpenImageData);
	
	// 打开轨迹线数据
	connect(ui.pushButton_OpenTraceLineData, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::OpenTraceLineData);
	
	// 保存地图
	connect(ui.pushButton_Save, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::MovingTargetMappingDataSave);
	
	// 页面尺寸下拉选择
	connect(ui.comboBox_PageSize, SIGNAL(currentIndexChanged(int)), this, SLOT(ComboPageSizeSelectedChanged()));

	// 纸张方向下拉选择
	connect(ui.comboBox_PageDirection, SIGNAL(currentIndexChanged(int)), this, SLOT(ComboPageDirectionSelectedChanged()));

	// 设置地图名称字体
	connect(ui.pushButton_MapNameFontSize, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapNameFontSize);

	// 设置地图名称字体颜色
	connect(ui.pushButton_MapNameFontColor, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapNameFontColor);

	// 设置地图制图单位字体
	connect(ui.pushButton_MapAgencyFontSize, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapAgencyFontSize);

	// 设置地图制图单位字体颜色
	connect(ui.pushButton_MapAgencyFontColor, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapAgencyFontColor);

	// 设置地图说明字体
	connect(ui.pushButton_MapDetailsFontSize, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapDetailFontSize);

	// 设置地图说明字体颜色
	connect(ui.pushButton_MapDetailsFontColor, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapDetailFontColor);

	// 设置地图动目标说明字体
	connect(ui.pushButton_MovingTargetDetailsFontSize, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapMovingTargetFontSize);

	// 设置地图动目标说明字体颜色
	connect(ui.pushButton_MovingTargetDetailsFontColor, &QPushButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapMovingTargetFontColor);



	m_qstrTracePointDataPath = "";
	m_qstrTraceLineDataPath = "";
	m_qstrImageDataPath = "";
	m_qstrMappingDataSavePath = "";

	restoreState();
	ui.lineEdit_OutputDataPath->setText(m_qstrMappingDataSavePath);
	ui.lineEdit_TracePointVectorDataPath->setText(m_qstrTracePointDataPath);
	ui.lineEdit_TraceLineDataPath->setText(m_qstrTraceLineDataPath);
	ui.lineEdit_ImageDataPath->setText(m_qstrImageDataPath);

	InitLineEdit();

	// 经纬网
	ui.lineEdit_GeoGridInterval->setText(tr("1"));

	// 设置界面初始化状态
	ui.checkBox_MapAgency->setChecked(true);
	ui.checkBox_Scale->setChecked(true);
	ui.checkBox_NorthArrow->setChecked(true);
	ui.checkBox_GeoGrid->setChecked(true);
	ui.radioButton_GeoPDF->setChecked(true);

	// 默认横向
	ui.comboBox_PageDirection->setCurrentIndex(0);
	m_iPageDirection = 1;

	// 默认尺寸自定义，单位：毫米
	ui.comboBox_PageSize->setCurrentIndex(0);
	ui.lineEdit_PageWidth->setText(tr("800"));	
	ui.lineEdit_PageHeight->setText(tr("650"));

	ui.tabWidget_MovingTargetMapping->setCurrentIndex(0);
	ui.tabWidget_DecorationItem->setCurrentIndex(0);

	// 默认地图名称和制图单位字体及字体颜色未修改状态
	m_bSetMapNameFont = false;
	m_bSetMapNameColor = false;
	m_bSetMapAgencyFont = false;
	m_bSetMapAgencyColor = false;

	m_bSetMapDetailFont = false;
	m_bSetMapDetailColor = false;
	m_bSetMapMovingTargetColor = false;
	m_bSetMapMovingTargetFont = false;


	connect(ui.radioButton_SetSizeMode, &QRadioButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapSizeRadioButton);
	connect(ui.radioButton_SetScaleMode, &QRadioButton::clicked, this, &QgsMovingTargetMappingDialog::SetMapScaleRadioButton);

	// 默认设置尺寸
	ui.radioButton_SetSizeMode->setChecked(true);
	ui.radioButton_SetScaleMode->setChecked(false);
	ui.comboBox_PageDirection->setEnabled(true);
	ui.comboBox_PageSize->setEnabled(true);
	ui.lineEdit_PageWidth->setEnabled(true);
	ui.lineEdit_PageHeight->setEnabled(true);
	ui.lineEdit_Scale->setEnabled(false);
}

QgsMovingTargetMappingDialog::~QgsMovingTargetMappingDialog()
{
	// 设置当前保存路径
	QgsSettings settings;

	// 地图输出路径
	settings.setValue(QStringLiteral("QgsMovingTargetMappingDialog/DataSavePath"), m_qstrMappingDataSavePath, QgsSettings::Section::Plugins);


	// 轨迹点数据文件路径
	settings.setValue(QStringLiteral("QgsMovingTargetMappingDialog/TracePointDataPath"), m_qstrTracePointDataPath, QgsSettings::Section::Plugins);
	
	// 轨迹线数据文件路径
	settings.setValue(QStringLiteral("QgsMovingTargetMappingDialog/TraceLineDataPath"), m_qstrTraceLineDataPath, QgsSettings::Section::Plugins);

	// 影像图数据文件路径
	settings.setValue(QStringLiteral("QgsMovingTargetMappingDialog/ImageDataPath"), m_qstrImageDataPath, QgsSettings::Section::Plugins);

}

void QgsMovingTargetMappingDialog::Close()
{
	// 设置当前保存路径
	QgsSettings settings;

	// 地图输出路径
	settings.setValue(QStringLiteral("QgsMovingTargetMappingDialog/DataSavePath"), m_qstrMappingDataSavePath, QgsSettings::Section::Plugins);


	// 轨迹点数据文件路径
	settings.setValue(QStringLiteral("QgsMovingTargetMappingDialog/TracePointDataPath"), m_qstrTracePointDataPath, QgsSettings::Section::Plugins);

	// 轨迹线数据文件路径
	settings.setValue(QStringLiteral("QgsMovingTargetMappingDialog/TraceLineDataPath"), m_qstrTraceLineDataPath, QgsSettings::Section::Plugins);

	// 影像图数据文件路径
	settings.setValue(QStringLiteral("QgsMovingTargetMappingDialog/ImageDataPath"), m_qstrImageDataPath, QgsSettings::Section::Plugins);

	accept();
}

void QgsMovingTargetMappingDialog::OpenImageData()
{
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString filename = QFileDialog::getOpenFileName(this,
		tr("加载参考影像数据"),
		curPath,
		"image(*.tif *.tiff)");

	// 如果没有选中数据，则返回
	if (filename.isEmpty())
	{
		return;
	}

	ui.lineEdit_ImageDataPath->setText(filename);
	m_qstrImageDataPath = filename;

}



QgsLayoutItem* QgsMovingTargetMappingDialog::GetLayoutItemByName(QList<QgsLayoutItem*>& itemList, const QString qstrName)
{
	for (int i = 0; i < itemList.size(); i++)
	{
		if (itemList[i]->displayName() == qstrName)
		{
			return itemList[i];
		}
	}
	return NULL;
}

QString QgsMovingTargetMappingDialog::GetTracePointDataPath()
{
	return m_qstrTracePointDataPath;
}

QString QgsMovingTargetMappingDialog::GetImageDataPath()
{
	return m_qstrImageDataPath;
}

QString QgsMovingTargetMappingDialog::GetTraceLineDataPath()
{
	return m_qstrTraceLineDataPath;
}

QString QgsMovingTargetMappingDialog::GetDataSavePath()
{
	return m_qstrMappingDataSavePath;
}

QString QgsMovingTargetMappingDialog::GetMapExportType()
{
	if (ui.radioButton_GeoPDF->isChecked())
	{
		return "GeoPDF";
	}
	else if (ui.radioButton_PDF->isChecked())
	{
		return "PDF";
	}
	else if (ui.radioButton_tiff->isChecked())
	{
		return "tiff";
	}
	return "";
}

QString QgsMovingTargetMappingDialog::GetMapName()
{
	QString qstrMapName = ui.lineEdit_MapName->text();
	return qstrMapName;
}



QString QgsMovingTargetMappingDialog::GetMapDetail()
{
	QString qstrMapDetail = ui.textEdit_MapDetail->toPlainText();
	return qstrMapDetail;
}

QString QgsMovingTargetMappingDialog::GetMovingTargetDetail()
{
	QString qstrMovingTargetDetail = ui.textEdit_MovingTargetDetail->toPlainText();
	return qstrMovingTargetDetail;
}


QString QgsMovingTargetMappingDialog::GetMapAgency()
{
	QString qstrMapAgency = ui.lineEdit_MapAgency->text();
	return qstrMapAgency;
}

bool QgsMovingTargetMappingDialog::GetDrawMapAgencyState()
{
	bool bChecked = ui.checkBox_MapAgency->isChecked();
	return bChecked;
}

bool QgsMovingTargetMappingDialog::GetDrawScaleState()
{
	bool bChecked = ui.checkBox_Scale->isChecked();
	return bChecked;
}

bool QgsMovingTargetMappingDialog::GetDrawNorthArrowState()
{
	bool bChecked = ui.checkBox_NorthArrow->isChecked();
	return bChecked;
}

bool QgsMovingTargetMappingDialog::GetDrawGeoGridState()
{
	bool bChecked = ui.checkBox_GeoGrid->isChecked();
	return bChecked;
}


void QgsMovingTargetMappingDialog::GetGeoGridInterval(double& dLonInterval, double& dLatInterval)
{
	dLonInterval = ui.lineEdit_GeoGridInterval->text().toDouble();
	dLatInterval = ui.lineEdit_GeoGridInterval->text().toDouble();
}

void QgsMovingTargetMappingDialog::GetMapSize(double& dWidth, double& dHeight)
{
	dWidth = ui.lineEdit_PageWidth->text().toDouble();
	dHeight = ui.lineEdit_PageHeight->text().toDouble();
}



bool QgsMovingTargetMappingDialog::ExportMapToFile(
	const QString& filePath,
	const QString& qstrExportType,
	QgsPrintLayout* layout)
{
	QgsLayoutExporter::ExportResult result = QgsLayoutExporter::Success;
	QgsTemporaryCursorOverride cursorOverride(Qt::BusyCursor);
	// 如果导出tiff文件
	if (qstrExportType == "tiff")
	{
		// 设置图片导出参数设置
		QgsLayoutExporter::ImageExportSettings settings;

		settings.generateWorldFile = true;
		settings.cropToContents = true;
		// 获取布局页面尺寸
		QgsLayoutPageCollection* pPageCollection = layout->pageCollection();
		pPageCollection->pageCount();
		QgsLayoutItemPage* pPage = pPageCollection->page(0);
		QgsLayoutSize pageSize(QgsUnitTypes::LayoutMillimeters);
		pageSize = pPage->pageSize();

		// x方向像素值
		int x = 0;
		// y方向像素值
		int y = 0;

		// 毫米转像素
		ConvertMilliMeterToPixel(pageSize.width(), pageSize.height(), LAYOUT_DPI, x, y);
		// 设置导出地图像素大小
		settings.imageSize.setWidth(x);
		settings.imageSize.setHeight(y);
		settings.dpi = LAYOUT_DPI;

		QgsProxyProgressTask* proxyTask = new QgsProxyProgressTask(tr("导出 “%1”").arg(layout->name()));
		QgsApplication::taskManager()->addTask(proxyTask);

		layout->refresh();

		QgsLayoutExporter exporter(layout);
		result = exporter.exportToImage(filePath, settings);
		proxyTask->finalize(result == QgsLayoutExporter::Success);
	}

	// 如果导出GeoPDF文件
	else if (qstrExportType == "GeoPDF")
	{
		// 导出pdf设置
		QgsLayoutExporter::PdfExportSettings settings;
		settings.writeGeoPdf = false;
		settings.forceVectorOutput = true;
		settings.appendGeoreference = true;
		settings.exportMetadata = true;

		settings.useIso32000ExtensionFormatGeoreferencing = true;

		settings.dpi = 300;

		QgsTemporaryCursorOverride cursorOverride(Qt::BusyCursor);
		QgsProxyProgressTask* proxyTask = new QgsProxyProgressTask(tr("导出 “%1”").arg(layout->name()));
		QgsApplication::taskManager()->addTask(proxyTask);

		// force a refresh, to e.g. update data defined properties, tables, etc
		layout->refresh();

		QgsLayoutExporter exporter(layout);

		// 导出pdf
		result = exporter.exportToPdf(filePath, settings);

		proxyTask->finalize(result == QgsLayoutExporter::Success);
	}

	// 如果导出PDF文件
	else if (qstrExportType == "PDF")
	{
		// 导出pdf设置
		QgsLayoutExporter::PdfExportSettings settings;
		settings.writeGeoPdf = false;
		settings.forceVectorOutput = true;
		settings.appendGeoreference = true;
		settings.exportMetadata = true;

		settings.useIso32000ExtensionFormatGeoreferencing = true;

		settings.dpi = LAYOUT_DPI;

		QgsTemporaryCursorOverride cursorOverride(Qt::BusyCursor);
		QgsProxyProgressTask* proxyTask = new QgsProxyProgressTask(tr("导出 “%1”").arg(layout->name()));
		QgsApplication::taskManager()->addTask(proxyTask);

		// force a refresh, to e.g. update data defined properties, tables, etc
		layout->refresh();

		QgsLayoutExporter exporter(layout);

		// 导出pdf
		result = exporter.exportToPdf(filePath, settings);

		proxyTask->finalize(result == QgsLayoutExporter::Success);
	}

	QMessageBox msgBox;
	switch (result)
	{
	case QgsLayoutExporter::Success:

		QgsMessageLog::logMessage(tr("成功导出布局到 <a href=\"%1\">%2</a>").arg(QUrl::fromLocalFile(filePath).toString(), QDir::toNativeSeparators(filePath)), tr("动目标专题制图"), Qgis::Success);
		return true;

	case QgsLayoutExporter::PrintError:
	case QgsLayoutExporter::SvgLayerError:
	case QgsLayoutExporter::IteratorError:
	case QgsLayoutExporter::Canceled:
		return false;

	case QgsLayoutExporter::FileError:
		cursorOverride.release();
		QgsMessageLog::logMessage(tr("%1.\n\n文件正在使用").arg(QDir::toNativeSeparators(filePath)), tr("动目标专题制图"), Qgis::Critical);
		return false;

	case QgsLayoutExporter::MemoryError:
		cursorOverride.release();
		QgsMessageLog::logMessage(tr("创建地图导出文件太大导致内存不足"), tr("动目标专题制图"), Qgis::Critical);
		return false;
	}
	return true;
}

void QgsMovingTargetMappingDialog::ConvertMilliMeterToPixel(double w, double h, int dpi, int& x, int& y)
{
	// x,y 为像素数，w,h为毫米
	// 水平方向：x = w * dpi / 25.4
	// 垂直方向：y = h * dpi / 25.4
	x = w * dpi / 25.4;
	y = h * dpi / 25.4;
}

void QgsMovingTargetMappingDialog::ConvertPixeltoMilliMeter(int x, int y, int dpi, double& w, double& h)
{
	// x, y 为像素数，w, h为毫米
	// 水平方向：w = x * 25.4 / dpi
	// 垂直方向：h = y * 25.4 / dpi

	w = x * 25.4 / dpi;
	h = y * 25.4 / dpi;
}

void QgsMovingTargetMappingDialog::restoreState()
{
	const QgsSettings settings;
	m_qstrMappingDataSavePath = settings.value(QStringLiteral("QgsMovingTargetMappingDialog/DataSavePath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 轨迹点数据文件路径
	m_qstrTracePointDataPath = settings.value(QStringLiteral("QgsMovingTargetMappingDialog/TracePointDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 轨迹线数据文件路径
	m_qstrTraceLineDataPath = settings.value(QStringLiteral("QgsMovingTargetMappingDialog/TraceLineDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

	// 影像图数据文件路径
	m_qstrImageDataPath = settings.value(QStringLiteral("QgsMovingTargetMappingDialog/ImageDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();


}

double QgsMovingTargetMappingDialog::CalCenterMeridian(double dLong, double dScale)
{
	double dCenterMeridian = 0;
	// 3度带
	if (dScale == SCALE_10K)
	{
		dCenterMeridian = int((dLong + 1.5) / 3) * 3;
	}
	// 6度带
	else if (dScale == SCALE_50K)
	{
		dCenterMeridian = (int(dLong / 6) + 1) * 6 - 3;
	}
	return dCenterMeridian;
}

void QgsMovingTargetMappingDialog::GetMBR(int iCount, double* pPoints, double& dLeft, double& dTop, double& dRight, double& dBottom)
{
	double dMinX = 1e20;
	double dMaxX = -1e20;
	double dMinY = 1e20;
	double dMaxY = -1e20;

	for (int i = 0; i < iCount; i++)
	{
		if (pPoints[2 * i] > dMaxX)
		{
			dMaxX = pPoints[2 * i];
		}

		if (pPoints[2 * i] < dMinX)
		{
			dMinX = pPoints[2 * i];
		}

		if (pPoints[2 * i + 1] > dMaxY)
		{
			dMaxY = pPoints[2 * i + 1];
		}

		if (pPoints[2 * i + 1] < dMinY)
		{
			dMinY = pPoints[2 * i + 1];
		}
	}

	dLeft = dMinX;
	dRight = dMaxX;
	dTop = dMaxY;
	dBottom = dMinY;
}

QString QgsMovingTargetMappingDialog::GetEPSGCodeByCenterMeridianAndScale(double dLong, double dScale)
{
	QString qstrEPSGCode = "";
	// 如果是1w比例尺，使用3度高斯投影带
	if (dScale == SCALE_10K)
	{
		if (fabs(dLong - 75) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4534";
		}
		else if (fabs(dLong - 78) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4535";
		}
		else if (fabs(dLong - 81) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4536";
		}
		else if (fabs(dLong - 84) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4537";
		}
		else if (fabs(dLong - 87) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4538";
		}
		else if (fabs(dLong - 90) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4539";
		}
		else if (fabs(dLong - 93) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4540";
		}
		else if (fabs(dLong - 96) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4541";
		}
		else if (fabs(dLong - 99) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4542";
		}
		else if (fabs(dLong - 102) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4543";
		}
		else if (fabs(dLong - 105) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4544";
		}
		else if (fabs(dLong - 108) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4545";
		}
		else if (fabs(dLong - 111) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4546";
		}
		else if (fabs(dLong - 114) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4547";
		}
		else if (fabs(dLong - 117) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4548";
		}
		else if (fabs(dLong - 120) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4549";
		}
		else if (fabs(dLong - 123) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4550";
		}
		else if (fabs(dLong - 126) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4551";
		}
		else if (fabs(dLong - 129) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4552";
		}
		else if (fabs(dLong - 132) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4553";
		}
		else if (fabs(dLong - 135) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4554";
		}
	}
	// 如果是5w比例尺，使用6度高斯投影带
	else if (dScale == SCALE_50K)
	{
		if (fabs(dLong - 75) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4502";
		}
		else if (fabs(dLong - 81) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4503";
		}
		else if (fabs(dLong - 87) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4504";
		}
		else if (fabs(dLong - 93) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4505";
		}
		else if (fabs(dLong - 99) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4506";
		}
		else if (fabs(dLong - 105) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4507";
		}
		else if (fabs(dLong - 111) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4508";
		}
		else if (fabs(dLong - 117) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4509";
		}
		else if (fabs(dLong - 123) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4510";
		}
		else if (fabs(dLong - 129) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4511";
		}
		else if (fabs(dLong - 135) < SE_ZERO)
		{
			qstrEPSGCode = "EPSG:4512";
		}
	}
	return qstrEPSGCode;
}

void QgsMovingTargetMappingDialog::Degree2DMS(double degree, int& iDegree, int& iMinute, int& iSecond)
{
	iDegree = (int)degree;
	iMinute = (round)((degree - iDegree) * 60);
	iSecond = (round)(((degree - iDegree) * 60 - iMinute) * 60);
}

QString QgsMovingTargetMappingDialog::FormatDegree(int iDegree)
{
	QString qstrDegree;
	if (iDegree == 0)
	{
		qstrDegree = tr("00°");
	}
	else if (iDegree > 0 && iDegree < 10)
	{
		qstrDegree = tr("0%1°").arg(iDegree);
	}
	else if (iDegree >= 10)
	{
		qstrDegree = tr("%1°").arg(iDegree);
	}

	return qstrDegree;
}

QString QgsMovingTargetMappingDialog::FormatDegree2(int iDegree)
{
	QString qstrDegree;
	if (iDegree == 0)
	{
		qstrDegree = tr("00");
	}
	else if (iDegree > 0 && iDegree < 10)
	{
		qstrDegree = tr("0%1").arg(iDegree);
	}
	else if (iDegree >= 10)
	{
		qstrDegree = tr("%1").arg(iDegree);
	}

	return qstrDegree;
}

QString QgsMovingTargetMappingDialog::FormatMinute(int iMinute)
{
	QString qstrMinute;
	if (iMinute == 0)
	{
		qstrMinute = tr("00′");
	}
	else if (iMinute > 0 && iMinute < 10)
	{
		qstrMinute = tr("0%1′").arg(iMinute);
	}
	else if (iMinute >= 10)
	{
		qstrMinute = tr("%1′").arg(iMinute);
	}

	return qstrMinute;
}

QString QgsMovingTargetMappingDialog::FormatMinute2(int iMinute)
{
	QString qstrMinute;
	if (iMinute == 0)
	{
		qstrMinute = tr("00");
	}
	else if (iMinute > 0 && iMinute < 10)
	{
		qstrMinute = tr("0%1").arg(iMinute);
	}
	else if (iMinute >= 10)
	{
		qstrMinute = tr("%1").arg(iMinute);
	}

	return qstrMinute;
}

QString QgsMovingTargetMappingDialog::FormatSecond(int iSecond)
{
	QString qstrSecond;
	if (iSecond == 0)
	{
		qstrSecond = tr("00″");
	}
	else if (iSecond > 0 && iSecond < 10)
	{
		qstrSecond = tr("0%1″").arg(iSecond);
	}
	else if (iSecond >= 10)
	{
		qstrSecond = tr("%1″").arg(iSecond);
	}

	return qstrSecond;
}

QString QgsMovingTargetMappingDialog::FormatSecond2(int iSecond)
{
	QString qstrSecond;
	if (iSecond == 0)
	{
		qstrSecond = tr("00");
	}
	else if (iSecond > 0 && iSecond < 10)
	{
		qstrSecond = tr("0%1").arg(iSecond);
	}
	else if (iSecond >= 10)
	{
		qstrSecond = tr("%1").arg(iSecond);
	}

	return qstrSecond;
}







void QgsMovingTargetMappingDialog::CalGaussBorderCoords(const SE_DRect& dRange,
	double dKmInternal,
	vector<SE_DPoint>& vLeftBorderCoords,
	vector<SE_DPoint>& vTopBorderCoords,
	vector<SE_DPoint>& vRightBorderCoords,
	vector<SE_DPoint>& vBottomBorderCoords)
{
	vBottomBorderCoords.clear();
	vTopBorderCoords.clear();
	vLeftBorderCoords.clear();
	vRightBorderCoords.clear();

	// X方向起始整公里数，向上取整
	int iStartKm_X = ceil(dRange.dleft / (dKmInternal * 1000.0));

	// X方向终止整公里数，转换为米
	int iEndKm_X = int(dRange.dright / (dKmInternal * 1000.0));

	for (int i = iStartKm_X; i <= iEndKm_X; i++)
	{
		// 下边界整公里高斯投影值
		SE_DPoint dPointBottom;
		dPointBottom.dx = i * (dKmInternal * 1000.0);
		dPointBottom.dy = dRange.dbottom;

		vBottomBorderCoords.push_back(dPointBottom);

		// 上边界整公里高斯投影值
		SE_DPoint dPointTop;
		dPointTop.dx = i * (dKmInternal * 1000.0);
		dPointTop.dy = dRange.dtop;

		vTopBorderCoords.push_back(dPointTop);
	}

	// Y方向起始整公里数，向上取整
	int iStartKm_Y = ceil(dRange.dbottom / (dKmInternal * 1000.0));

	// Y方向终止整公里数
	int iEndKm_Y = int(dRange.dtop / (dKmInternal * 1000.0));

	for (int i = iStartKm_Y; i <= iEndKm_Y; i++)
	{
		// 左边界整公里高斯投影值
		SE_DPoint dPointLeft;
		dPointLeft.dx = dRange.dleft;
		dPointLeft.dy = i * (dKmInternal * 1000.0);

		vLeftBorderCoords.push_back(dPointLeft);

		// 右边界整公里高斯投影值
		SE_DPoint dPointRight;
		dPointRight.dx = dRange.dright;
		dPointRight.dy = i * (dKmInternal * 1000.0);

		vRightBorderCoords.push_back(dPointRight);
	}
}

void QgsMovingTargetMappingDialog::CalLayoutBorderPoints(
	const SE_DPoint& dLeftTopCornerGaussPoint,
	const SE_DPoint& dLeftTopCornerLayoutPoint,
	const SE_DPoint& dLeftBottomCornerGaussPoint,
	const SE_DPoint& dLeftBottomCornerLayoutPoint,
	const SE_DPoint& dRightTopCornerGaussPoint,
	const SE_DPoint& dRightTopCornerLayoutPoint,
	const SE_DPoint& dRightBottomCornerGaussPoint,
	const SE_DPoint& dRightBottomCornerLayoutPoint,
	double dMapUnitToLayoutUnit,
	vector<SE_DPoint>& vLeftBorderGaussCoords,
	vector<SE_DPoint>& vTopBorderGaussCoords,
	vector<SE_DPoint>& vRightBorderGaussCoords,
	vector<SE_DPoint>& vBottomBorderGaussCoords,
	vector<SE_DPoint>& vLeftBorderLayoutCoords,
	vector<SE_DPoint>& vTopBorderLayoutCoords,
	vector<SE_DPoint>& vRightBorderLayoutCoords,
	vector<SE_DPoint>& vBottomBorderLayoutCoords)
{
	vLeftBorderLayoutCoords.clear();
	vRightBorderLayoutCoords.clear();
	vTopBorderLayoutCoords.clear();
	vBottomBorderLayoutCoords.clear();

	// 计算左边界各点的布局位置，从下往上依次存储
	for (int i = 0; i < vLeftBorderGaussCoords.size(); i++)
	{
		// 左边界高斯坐标
		SE_DPoint leftGaussPoint = vLeftBorderGaussCoords[i];

		// 布局左边界坐标
		SE_DPoint leftLayoutPoint;
		leftLayoutPoint.dx = dLeftTopCornerLayoutPoint.dx;
		leftLayoutPoint.dy = dLeftTopCornerLayoutPoint.dy + (dLeftTopCornerGaussPoint.dy - leftGaussPoint.dy) * dMapUnitToLayoutUnit;

		vLeftBorderLayoutCoords.push_back(leftLayoutPoint);
	}

	// 计算右边界各点的布局位置
	for (int i = 0; i < vRightBorderGaussCoords.size(); i++)
	{
		// 右边界高斯坐标
		SE_DPoint rightGaussPoint = vRightBorderGaussCoords[i];

		// 布局右边界坐标
		SE_DPoint rightLayoutPoint;
		rightLayoutPoint.dx = dRightTopCornerLayoutPoint.dx;
		rightLayoutPoint.dy = dRightTopCornerLayoutPoint.dy + (dRightTopCornerGaussPoint.dy - rightGaussPoint.dy) * dMapUnitToLayoutUnit;
		vRightBorderLayoutCoords.push_back(rightLayoutPoint);
	}

	// 计算上边界各点的布局位置
	for (int i = 0; i < vTopBorderGaussCoords.size(); i++)
	{
		// 上边界高斯坐标
		SE_DPoint topGaussPoint = vTopBorderGaussCoords[i];

		// 布局上边界坐标
		SE_DPoint topLayoutPoint;
		topLayoutPoint.dx = dLeftTopCornerLayoutPoint.dx + (topGaussPoint.dx - dLeftTopCornerGaussPoint.dx) * dMapUnitToLayoutUnit;
		topLayoutPoint.dy = dLeftTopCornerLayoutPoint.dy;
		vTopBorderLayoutCoords.push_back(topLayoutPoint);
	}

	// 计算下边界各点的布局位置
	for (int i = 0; i < vBottomBorderGaussCoords.size(); i++)
	{
		// 下边界高斯坐标
		SE_DPoint bottomGaussPoint = vBottomBorderGaussCoords[i];

		// 布局下边界坐标
		SE_DPoint bottomLayoutPoint;
		bottomLayoutPoint.dx = dLeftBottomCornerLayoutPoint.dx + (bottomGaussPoint.dx - dLeftBottomCornerGaussPoint.dx) * dMapUnitToLayoutUnit;
		bottomLayoutPoint.dy = dLeftBottomCornerLayoutPoint.dy;
		vBottomBorderLayoutCoords.push_back(bottomLayoutPoint);
	}
}

int QgsMovingTargetMappingDialog::CalBandNumber(double dCenterMeridian, double dScale)
{
	// 根据中央经线计算带号
	int iBandNumber = 0;
	string strBandType;
	// 5万比例尺
	if (dScale == SCALE_50K)
	{
		iBandNumber = int((dCenterMeridian + 3) / 6);
	}
	// 1万比例尺
	else if (dScale == SCALE_10K)
	{
		iBandNumber = int(dCenterMeridian / 3);
	}

	return iBandNumber;
}







void QgsMovingTargetMappingDialog::TransformGaussToLayout(SE_DPoint dRefGaussPoint, SE_DPoint dRefLayoutPoint, SE_DPoint dGaussPoint, double dMapUnitToLayoutUnit, SE_DPoint& dLayoutPoint)
{
	dLayoutPoint.dx = dRefLayoutPoint.dx + (dGaussPoint.dx - dRefGaussPoint.dx) * dMapUnitToLayoutUnit;
	dLayoutPoint.dy = dRefLayoutPoint.dy + (dRefGaussPoint.dy - dGaussPoint.dy) * dMapUnitToLayoutUnit;
}


void QgsMovingTargetMappingDialog::MovingTargetMapping()
{
	if (ui.lineEdit_GeoGridInterval->text().toDouble() == 0)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("动目标专题制图"));
		msgBox.setText(tr("经纬网间隔需大于0，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	/*判断输入文件及路径的有效性*/
	//------------------------------------//
	if (!FilePathIsExisted(ui.lineEdit_OutputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("动目标专题制图"));
		msgBox.setText(tr("输出目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 轨迹点路径
	if (!FileIsExisted(ui.lineEdit_TracePointVectorDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("动目标专题制图"));
		msgBox.setText(tr("轨迹点数据不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 轨迹线路径
	if (!FileIsExisted(ui.lineEdit_TraceLineDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("动目标专题制图"));
		msgBox.setText(tr("轨迹线数据不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	// 影像数据
	if (!FileIsExisted(ui.lineEdit_ImageDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("动目标专题制图"));
		msgBox.setText(tr("影像数据不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	//------------------------------------//
#pragma region "设置绘制线样式"
	
	// 地图内图廓绘制线的样式
	QVariantMap interiorMapLineMap;
	interiorMapLineMap["line_color"] = "0,0,0,255";
	interiorMapLineMap["line_style"] = "solid";
	interiorMapLineMap["line_width"] = "0.1";
	QgsLineSymbol* interiorMapLineSymbol = QgsLineSymbol::createSimple(interiorMapLineMap);

	// 地图外图廓绘制线的样式
	QVariantMap exteriorMapLineMap;
	exteriorMapLineMap["line_color"] = "0,0,0,255";
	exteriorMapLineMap["line_style"] = "solid";
	exteriorMapLineMap["line_width"] = "0.3";
	QgsLineSymbol* exteriorMapLineSymbol = QgsLineSymbol::createSimple(exteriorMapLineMap);

	// 经纬网刻度线的样式，经纬网线改为黑色
	QVariantMap geoLineBorderMap;
	geoLineBorderMap["line_color"] = "0,0,0,255";
	geoLineBorderMap["line_style"] = "solid";
	geoLineBorderMap["line_width"] = "0.1";
	QgsLineSymbol* geoLineBorderLineSymbol = QgsLineSymbol::createSimple(geoLineBorderMap);

#pragma endregion

	ui.progressBar->setValue(0);

	/*以地图外图左上角为参考点，获取基于地图外图廓的所有布局要素的相对参考点位置，
	用于自动调整影像图各元素位置*/

	/*影像制图流程
	（1）根据轨迹线、轨迹点范围裁剪影像数据
	（2）设置地图页面尺寸，计算比例尺，根据比例尺大小确定采用投影坐标系或地理坐标系，小于1:100万使用地理坐标系
	（3）根据界面设置的整饰要素显示状态，设置布局元素item的显示状态
	（4）每个图幅输出工程文件、地图文件（pdf格式或tif格式）
	*/

	

	// 获取输出地图类型
	QString qstrExportMapType = GetMapExportType();

	/*---------获取整饰要素---------*/
	// 获取地图名称
	QString qstrMapName = GetMapName();

	// 是否绘制地图单位
	bool bDrawMapAgency = GetDrawMapAgencyState();

	// 获取制图单位
	QString qstrMapAgency = GetMapAgency();

	// 获取地图说明
	QString qstrMapDetail = GetMapDetail();

	// 获取动目标说明
	QString qstrMovingTargetDetail = GetMovingTargetDetail();

	// 是否绘制比例尺
	bool bDrawScale = GetDrawScaleState();

	// 是否绘制指北针
	bool bDrawNorthArrow = GetDrawNorthArrowState();

	// 是否绘制经纬网
	bool bDrawGeoGrid = GetDrawGeoGridState();

	// 经纬网间隔
	double dLonInterval = 0;
	double dLatInterval = 0;
	GetGeoGridInterval(dLonInterval, dLatInterval);

	// 获取地图尺寸，单位：毫米
	double dMapWidth = 0;
	double dMapHeight = 0;
	GetMapSize(dMapWidth, dMapHeight);

	/*---------获取专题图布局模板---------*/
	QString qstrLayoutTemplatePath = QCoreApplication::applicationDirPath() + "/layout/template/moving_target.qpt";


#pragma region "开始制图"

#pragma region "图层列表定义"
		
		/*获取工程实例*/
		bool bResult = false;
		QgsProject* pProject = QgsProject::instance();
		if (!pProject)
		{
			// 记录日志
			QgsMessageLog::logMessage(tr("工程实例获取异常"), tr("动目标专题制图"), Qgis::Critical);
			return;
		}

		/*清空当前工程的所有图层*/
		pProject->removeAllMapLayers();


		QList<QgsMapLayer*> qMainMapLayerList;      // 主地图加载图层列表
		qMainMapLayerList.clear();

#pragma endregion

#pragma region "加载影像图层"
		/*-------------------------------*/
		/*----------加载影像图层---------*/
		/*-------------------------------*/
		QString fileNameRaster = GetImageDataPath();
		QFileInfo fileInfoRaster(fileNameRaster);

		// 获取影像数据名称
		QString baseNameRaster = fileInfoRaster.completeBaseName();

		// 根据文件存放路径，栅格的名称创建一个QgsRasterLayer类
		QgsRasterLayer* rasterLayer = new QgsRasterLayer(fileNameRaster, baseNameRaster);
		if (!rasterLayer || !rasterLayer->isValid())
		{
			// 记录日志
			QgsMessageLog::logMessage((QString(tr("栅格图层不合法:")) + fileNameRaster), tr("动目标专题制图"), Qgis::Critical);
			return;
		}

		QgsMapLayer* pLayerRaster = pProject->addMapLayer(rasterLayer);
		if (!pLayerRaster)
		{
			delete rasterLayer;
			// 记录日志
			QgsMessageLog::logMessage((QString(tr("增加栅格图层失败:")) + fileNameRaster), tr("动目标专题制图"), Qgis::Critical);
			return;
		}


#pragma endregion

#pragma region "加载矢量图层"
		/*-------------------------------*/
		/*---------加载矢量图层----------*/
		/*-------------------------------*/
#pragma region "加载轨迹点图层"
		// 轨迹点图层范围
		SE_DRect dTracePointLayerRect;
		QgsCoordinateReferenceSystem qgsTracePointCRS;
		QString qstrTracePointLayerFilePath = GetTracePointDataPath();

		QgsVectorLayer* pTracePointLayer = nullptr;
		QgsMapLayer* pAddedTracePointLayer = nullptr;
		// 判断矢量图层是否存在
		// 如果文件存在
		if (QFileInfo::exists(qstrTracePointLayerFilePath))
		{
			/*将矢量图层加载到地图图层列表中*/
			QString fileNameVector = qstrTracePointLayerFilePath;
			QFileInfo fileInfoVector(fileNameVector);

			// 获取数据名称
			QString baseNameVector = fileInfoVector.completeBaseName();

			// 通过图层类型获取对应的样式文件全路径
			QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称

			/*获取qml名称*/
			QString qstrQmlFilePath = QCoreApplication::applicationDirPath() + "/style/theme/moving_target/trace_point.qml";
			
			pTracePointLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
			if (!pTracePointLayer->isValid())
			{
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
				return;
			}

			qgsTracePointCRS = pTracePointLayer->crs();

			/*图层已有的样式*/
			QgsMapLayerStyle oldStyle = pTracePointLayer->styleManager()->style(pTracePointLayer->styleManager()->currentStyle());
			QString message;
			bool defaultLoadedFlag = false;
			message = pTracePointLayer->loadNamedStyle(qstrQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

			if (!defaultLoadedFlag)
			{
				QgsMessageLog::logMessage((QString(tr("样式文件不存在:")) + qstrQmlFilePath), tr("动目标专题制图"), Qgis::Critical);
				return;
			}

			pAddedTracePointLayer = pProject->addMapLayer(pTracePointLayer);
			if (!pAddedTracePointLayer)
			{
				delete pTracePointLayer;
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
				return;
			}
			
			QgsRectangle qgsPointLayerRect = pAddedTracePointLayer->extent();
			// 地图范围扩大，包括轨迹点边界
			dTracePointLayerRect.dleft = qgsPointLayerRect.xMinimum() - qgsPointLayerRect.width() * 0.1;
			dTracePointLayerRect.dright = qgsPointLayerRect.xMaximum() + qgsPointLayerRect.width() * 0.1;
			dTracePointLayerRect.dtop = qgsPointLayerRect.yMaximum() + qgsPointLayerRect.height() * 0.1;
			dTracePointLayerRect.dbottom = qgsPointLayerRect.yMinimum() - qgsPointLayerRect.height() * 0.1;

			qMainMapLayerList.append(pAddedTracePointLayer);
		}

#pragma endregion

#pragma region "复制轨迹点层，创建并加载轨迹点标注图层trace_point_label.shp"

		// 获取驱动
		CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
		GDALAllRegister();
		// -------------获取shp驱动----------------//
		CPLSetConfigOption("SHAPE_ENCODING", "");			//属性表支持中文字段
		const char* pszShpDriverName = "ESRI Shapefile";
		GDALDriver* poShpDriver;
		poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
		if (poShpDriver == NULL)
		{
			QgsMessageLog::logMessage(tr("获取ESRI Shapefile驱动失败！") , tr("动目标专题制图"), Qgis::Critical);
			return;
		}

		string strTracePointLayerFileName = qstrTracePointLayerFilePath.toLocal8Bit();
		GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strTracePointLayerFileName.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
		if (!poDS)
		{
			QgsMessageLog::logMessage(tr("打开轨迹点图层失败！"), tr("动目标专题制图"), Qgis::Critical);
			return;
		}
		
		// 循环获取每个图层
		OGRLayer* poLayer = poDS->GetLayer(0);
		if (!poLayer) {
			// 记录日志
			QgsMessageLog::logMessage(tr("打开轨迹点图层失败！"), tr("动目标专题制图"), Qgis::Critical);
			return;
		}

		OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();

		OGRwkbGeometryType geo = poLayer->GetGeomType();
		string strLayerName = poLayer->GetName();

		// 获取输入轨迹点图层所在目录，轨迹点标注层也生成在此目录下
		string strInputTracePointPath = CPLGetDirname(strTracePointLayerFileName.c_str());

		string strShpFilePath = strInputTracePointPath;
		string strPointShpFilePath;
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strShpFilePath.c_str(), MODE);
#endif 

		poLayer->ResetReading();
		OGRFeature* poFeature = NULL;
		int iResult = 0;
	
		vector<SE_DPoint> vPoints;
		vPoints.clear();
		vector<vector<FieldNameAndValue>> vPointAttrs;
		vPointAttrs.clear();
		while ((poFeature = poLayer->GetNextFeature()) != NULL)
		{
			SE_DPoint xyz;
			vector<FieldNameAndValue> vFieldValue;   // 当前要素属性值
			vFieldValue.clear();
			iResult = Get_Point(poFeature, xyz, vFieldValue);
			if (iResult != 0) {
				// 记录日志
				continue;
			}
			vPoints.push_back(xyz);

			vPointAttrs.push_back(vFieldValue);
		}
		// 如果点要素大于0，创建图层
		if (vPoints.size() > 0)
		{
			vector<string> vFieldsName;
			vFieldsName.clear();
			
			vector<OGRFieldType> vFieldType;
			vFieldType.clear();
			
			vector<int> vFieldWidth;
			vFieldWidth.clear();
			
			vector<int> vFieldPrecision;
			vFieldPrecision.clear();


			// 获取多尺度数据字段
			GetLayerFields(poLayer,
				vFieldsName,
				vFieldType,
				vFieldWidth,
				vFieldPrecision);

			// 创建轨迹点标注图层
			strPointShpFilePath = strShpFilePath + "/trace_point_label.shp";
			string strCPGFilePath = strShpFilePath + "/trace_point_label.cpg";


			//创建结果数据源
			GDALDataset* poResultDS;
			poResultDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);
			if (poResultDS == NULL)
			{
				// 记录日志
				QgsMessageLog::logMessage(tr("创建轨迹点标注层数据集失败！"), tr("动目标专题制图"), Qgis::Critical);
				return;
			}
			// 根据图层要素类型创建shp文件
			OGRLayer* poResultLayer = NULL;
			// 设置结果图层的空间参考与原始空间参考系一致
			OGRSpatialReference pResultSR = *oLayerSR;

			// shp中存储属性信息和几何信息
			poResultLayer = poResultDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
			if (!poResultLayer)
			{
				// 记录日志
				QgsMessageLog::logMessage(tr("创建轨迹点标注层图层失败！"), tr("动目标专题制图"), Qgis::Critical);
				return;
			}
			// 创建结果图层属性字段
			int iResult = SetFieldDefn(poResultLayer, vFieldsName, vFieldType, vFieldWidth, vFieldPrecision);
			if (iResult != 0)
			{
				return;
			}
			// 创建要素
			for (int i = 0; i < vPoints.size(); i++)
			{
				iResult = Set_Point(poResultLayer,
					vPoints[i].dx,
					vPoints[i].dy,
					vPoints[i].dz,
					vPointAttrs[i]);
				if (iResult != 0) {

					continue;
				}
			}
			// 关闭数据源
			GDALClose(poResultDS);
			CreateShapefileCPG(strCPGFilePath, "System");
		}
		
		GDALClose(poDS);


		QgsVectorLayer* pTracePointLabelLayer = nullptr;
		QgsMapLayer* pAddedTracePointLabelLayer = nullptr;
		// 判断矢量图层是否存在
		// 如果文件存在
		QString qstrTracePointLabelLayer = QString::fromLocal8Bit(strPointShpFilePath.c_str());
		
		if (QFileInfo::exists(qstrTracePointLabelLayer))
		{
			/*将矢量图层加载到地图图层列表中*/
			QString fileNameVector = qstrTracePointLabelLayer;
			QFileInfo fileInfoVector(fileNameVector);

			// 获取数据名称
			QString baseNameVector = fileInfoVector.completeBaseName();

			// 通过图层类型获取对应的样式文件全路径
			QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称

			/*获取qml名称*/
			QString qstrQmlFilePath = QCoreApplication::applicationDirPath() + "/style/theme/moving_target/trace_point_label.qml";

			pTracePointLabelLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
			if (!pTracePointLabelLayer->isValid())
			{
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
				return;
			}

			qgsTracePointCRS = pTracePointLabelLayer->crs();

			/*图层已有的样式*/
			QgsMapLayerStyle oldStyle = pTracePointLabelLayer->styleManager()->style(pTracePointLabelLayer->styleManager()->currentStyle());
			QString message;
			bool defaultLoadedFlag = false;
			message = pTracePointLabelLayer->loadNamedStyle(qstrQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

			if (!defaultLoadedFlag)
			{
				QgsMessageLog::logMessage((QString(tr("样式文件不存在:")) + qstrQmlFilePath), tr("动目标专题制图"), Qgis::Critical);
				return;
			}

			pAddedTracePointLabelLayer = pProject->addMapLayer(pTracePointLabelLayer);
			if (!pAddedTracePointLabelLayer)
			{
				delete pTracePointLabelLayer;
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
				return;
			}

			QgsRectangle qgsPointLayerRect = pAddedTracePointLabelLayer->extent();
			// 地图范围扩大，包括轨迹点边界
			dTracePointLayerRect.dleft = qgsPointLayerRect.xMinimum() - qgsPointLayerRect.width() * 0.1;
			dTracePointLayerRect.dright = qgsPointLayerRect.xMaximum() + qgsPointLayerRect.width() * 0.1;
			dTracePointLayerRect.dtop = qgsPointLayerRect.yMaximum() + qgsPointLayerRect.height() * 0.1;
			dTracePointLayerRect.dbottom = qgsPointLayerRect.yMinimum() - qgsPointLayerRect.height() * 0.1;

			qMainMapLayerList.append(pAddedTracePointLabelLayer);
		}

#pragma endregion


#pragma region "加载轨迹线图层"

		QString qstrTraceLineLayerFilePath = GetTraceLineDataPath();
		QgsVectorLayer* pTraceLineLayer = nullptr;
		QgsMapLayer* pAddedTraceLineLayer = nullptr;
		// 判断矢量图层是否存在
		// 如果文件存在
		if (QFileInfo::exists(qstrTraceLineLayerFilePath))
		{
			/*将矢量图层加载到地图图层列表中*/
			QString fileNameVector = qstrTraceLineLayerFilePath;
			QFileInfo fileInfoVector(fileNameVector);

			// 获取数据名称
			QString baseNameVector = fileInfoVector.completeBaseName();


			// 通过图层类型获取对应的样式文件全路径
			QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称

			/*获取qml名称*/
			QString qstrQmlFilePath = QCoreApplication::applicationDirPath() + "/style/theme/moving_target/trace_line.qml";

			pTraceLineLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
			if (!pTraceLineLayer->isValid())
			{
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
				return;
			}

			/*图层已有的样式*/
			QgsMapLayerStyle oldStyle = pTraceLineLayer->styleManager()->style(pTraceLineLayer->styleManager()->currentStyle());
			QString message;
			bool defaultLoadedFlag = false;
			message = pTraceLineLayer->loadNamedStyle(qstrQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

			if (!defaultLoadedFlag)
			{
				QgsMessageLog::logMessage((QString(tr("样式文件不存在:")) + qstrQmlFilePath), tr("动目标专题制图"), Qgis::Critical);
				return;
			}

			pAddedTraceLineLayer = pProject->addMapLayer(pTraceLineLayer);
			if (!pAddedTraceLineLayer)
			{
				delete pTraceLineLayer;
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
				return;
			}


			qMainMapLayerList.append(pAddedTraceLineLayer);
		}

#pragma endregion


		/*主地图最后加载影像*/
		qMainMapLayerList.append(pLayerRaster);
#pragma endregion




#pragma region "加载布局模板"

		/*----------------------------------------*/
		/*【3】根据制图方案设置整饰页面各要素的状态*/
		/*----------------------------------------*/
		// 创建打印布局
		QgsPrintLayout* pPrintLayout = new QgsPrintLayout(QgsProject::instance());
		if (!pPrintLayout)
		{
			// 记录日志
			QgsMessageLog::logMessage(tr("创建打印布局失败！"), tr("动目标专题制图"), Qgis::Critical);
			return;
		}

		/*设置布局名称 - 图幅号*/
		pPrintLayout->setName(qstrMapName);

		/*初始化默认设置*/
		pPrintLayout->initializeDefaults();

		QFileInfo templateFileInfo(qstrLayoutTemplatePath);
		QFile templateFile(qstrLayoutTemplatePath);
		if (!templateFile.open(QIODevice::ReadOnly))
		{
			QgsMessageLog::logMessage(tr("读取布局模板文件失败！:%1").arg(qstrLayoutTemplatePath), tr("动目标专题制图"), Qgis::Critical);
			return;
		}

		QDomDocument templateDoc;
		QgsReadWriteContext context;
		context.setPathResolver(QgsProject::instance()->pathResolver());

		// 布局模板各要素
		QList< QgsLayoutItem* > qLayoutItemList;
		if (templateDoc.setContent(&templateFile))
		{
			bool ok = false;
			qLayoutItemList = pPrintLayout->loadFromTemplate(templateDoc, context, false, &ok);
			if (!ok)
			{
				// 记录日志
				QgsMessageLog::logMessage(tr("读取布局模板文件失败！:%1").arg(qstrLayoutTemplatePath), tr("动目标专题制图"), Qgis::Critical);
				return;
			}
			else
			{
				// 默认全部设置可选及可见
				for (int j = 0; j < qLayoutItemList.size(); j++)
				{
					qLayoutItemList[j]->setSelected(true);
					qLayoutItemList[j]->setVisibility(true);
				}
			}
		}

#pragma endregion


#pragma region "设置主地图范围及空间参考"

		// 图层的范围
		QgsRectangle qgsMapDisplayRect;

		double dMapScale = 0;

		// 如果设置尺寸模式
		if (ui.radioButton_SetSizeMode->isChecked())
		{
			// 粗略计算地图比例尺，然后决定采用投影坐标或地理坐标显示
			// 如果是地理坐标系
			if (qgsTracePointCRS.isGeographic())
			{
				QgsUnitTypes::DistanceUnit mapUnit = qgsTracePointCRS.mapUnits();
				if (mapUnit == QgsUnitTypes::DistanceDegrees)
				{
					dMapScale = dTracePointLayerRect.GetWidth() * 111000 / (dMapWidth / 1000.0);
				}
			}
			// 如果是投影坐标系
			else
			{
				QgsUnitTypes::DistanceUnit mapUnit = qgsTracePointCRS.mapUnits();
				// 米
				if (mapUnit == QgsUnitTypes::DistanceMeters)
				{
					dMapScale = dTracePointLayerRect.GetWidth() / (dMapWidth / 1000.0);
				}
				// 公里
				else if (mapUnit == QgsUnitTypes::DistanceKilometers)
				{
					dMapScale = dTracePointLayerRect.GetWidth() * 1000.0 / (dMapWidth / 1000.0);
				}
			}

		}

		// 如果设置固定比例尺模式
		else
		{
			dMapScale = ui.lineEdit_Scale->text().toDouble();

			// 地图尺寸调整
			dMapWidth = dTracePointLayerRect.GetWidth() * 111000 / dMapScale * 1000.0 ;		// 单位：毫米
			dMapHeight = dTracePointLayerRect.GetHeight() * 111000 / dMapScale * 1000.0;		// 单位：毫米
		}
		

		/*对数据四个角点进行web墨卡托投影正算*/
		// 获取轨迹点、轨迹线图层数据范围
		double dSheetPoints[8];
		// 左上角点
		dSheetPoints[0] = dTracePointLayerRect.dleft;
		dSheetPoints[1] = dTracePointLayerRect.dtop;

		// 右上角点
		dSheetPoints[2] = dTracePointLayerRect.dright;
		dSheetPoints[3] = dTracePointLayerRect.dtop;

		// 右下角点
		dSheetPoints[4] = dTracePointLayerRect.dright;
		dSheetPoints[5] = dTracePointLayerRect.dbottom;

		// 左下角点
		dSheetPoints[6] = dTracePointLayerRect.dleft;
		dSheetPoints[7] = dTracePointLayerRect.dbottom;

		// 进行WebMercator投影计算
		for (int i = 0; i < 4; ++i)
		{
			double L = dSheetPoints[2 * i];
			double B = dSheetPoints[2 * i + 1];
			double X = 0;
			double Y = 0;
			WebMercator_BL_TO_XY(B, L, X, Y);
			dSheetPoints[2 * i] = X;
			dSheetPoints[2 * i + 1] = Y;
		}

		/*获取最小外接矩形*/
		double dMBRLeft = 0;
		double dMBRTop = 0;
		double dMBRRight = 0;
		double dMBRBottom = 0;
		GetMBR(4, dSheetPoints, dMBRLeft, dMBRTop, dMBRRight, dMBRBottom);

		qgsMapDisplayRect.setXMinimum(dMBRLeft);
		qgsMapDisplayRect.setXMaximum(dMBRRight);
		qgsMapDisplayRect.setYMinimum(dMBRBottom);
		qgsMapDisplayRect.setYMaximum(dMBRTop);
		qgsMapDisplayRect.scale(1.0);

		/*主地图设置*/
		QgsLayoutItemMap* pMainMapItem = (QgsLayoutItemMap*)GetLayoutItemByName(qLayoutItemList, "MapItem_MainMap");
		if (!pMainMapItem)
		{
			// 记录日志
			QgsMessageLog::logMessage(tr("主地图控件获取失败！"), tr("动目标专题制图"), Qgis::Critical);
			return;
		}
			
		/*设置主地图空间参考系*/
		pMainMapItem->setCrs(QgsCoordinateReferenceSystem(tr("EPSG:3857")));
		pMainMapItem->setScale(dMapScale);
		pMainMapItem->setBackgroundColor(QColor(255, 255, 255, 0));

		double dMapUnit2LayoutUnit = pMainMapItem->mapUnitsToLayoutUnits();
		/*获取地图内图廓（矩形）的左上角点布局坐标*/
		m_InteriorMapLeftTopLayoutPoint.dx = pMainMapItem->positionWithUnits().x();
		m_InteriorMapLeftTopLayoutPoint.dy = pMainMapItem->positionWithUnits().y();

#pragma region "更新地图内图廓"

		/*重新设置地图范围*/
		// 以地图内图廓为准
		pMainMapItem->attemptSetSceneRect(QRectF(m_InteriorMapLeftTopLayoutPoint.dx,
			m_InteriorMapLeftTopLayoutPoint.dy,
			(dMBRRight - dMBRLeft) * dMapUnit2LayoutUnit,
			(dMBRTop - dMBRBottom)* dMapUnit2LayoutUnit));

		pMainMapItem->setExtent(qgsMapDisplayRect);
		pMainMapItem->setLayers(qMainMapLayerList);

		
		// 内图廓最小外接矩形左上角点布局坐标
		m_dInteriorLeftTopLayoutPoint.dx = pMainMapItem->positionWithUnits().x();
		m_dInteriorLeftTopLayoutPoint.dy = pMainMapItem->positionWithUnits().y();

		// 地图内图廓多边形四个角点的地理坐标
		// 地图内图廓多边形左上角点地理坐标-A
		SE_DPoint interiorMapGeoPoint_A;
		interiorMapGeoPoint_A.dx = dTracePointLayerRect.dleft;
		interiorMapGeoPoint_A.dy = dTracePointLayerRect.dtop;

		// 地图内图廓多边形右上角点地理坐标-B
		SE_DPoint interiorMapGeoPoint_B;
		interiorMapGeoPoint_B.dx = dTracePointLayerRect.dright;
		interiorMapGeoPoint_B.dy = dTracePointLayerRect.dtop;

		// 地图内图廓多边形右下角点地理坐标-C
		SE_DPoint interiorMapGeoPoint_C;
		interiorMapGeoPoint_C.dx = dTracePointLayerRect.dright;
		interiorMapGeoPoint_C.dy = dTracePointLayerRect.dbottom;

		// 地图内图廓多边形左下角点地理坐标-D
		SE_DPoint interiorMapGeoPoint_D;
		interiorMapGeoPoint_D.dx = dTracePointLayerRect.dleft;
		interiorMapGeoPoint_D.dy = dTracePointLayerRect.dbottom;

		// 地图内图廓矩形左上角地理坐标
		QgsRectangle mapItemRectangle = pMainMapItem->extent();
		m_InteriorMapLeftTopGeoPoint.dx = dTracePointLayerRect.dleft;
		m_InteriorMapLeftTopGeoPoint.dy = dTracePointLayerRect.dtop;

#pragma endregion

#pragma region "地图内图廓多边形四个布局点坐标A、B、C、D"

		// 布局坐标分布示意图
		/*
		A-------------B
		|			  |
		|			  |
		|		      |
		D-------------C
		*/

		QRectF mapItemRect = pMainMapItem->rect();

		// 内图廓左上角A布局坐标值，positionWithUnits()默认为地图窗口左上角布局点坐标
		SE_DPoint interiorMapLayoutPoint_A;
		interiorMapLayoutPoint_A.dx = pMainMapItem->positionWithUnits().x();
		interiorMapLayoutPoint_A.dy = pMainMapItem->positionWithUnits().y();

		// 内图廓右上角B布局坐标值
		SE_DPoint interiorMapLayoutPoint_B;
		interiorMapLayoutPoint_B.dx = pMainMapItem->positionWithUnits().x() + mapItemRect.width();
		interiorMapLayoutPoint_B.dy = pMainMapItem->positionWithUnits().y();

		// 内图廓右下角C布局坐标值
		SE_DPoint interiorMapLayoutPoint_C;
		interiorMapLayoutPoint_C.dx = pMainMapItem->positionWithUnits().x() + mapItemRect.width();
		interiorMapLayoutPoint_C.dy = pMainMapItem->positionWithUnits().y() + mapItemRect.height();

		// 内图廓左下角D布局坐标值
		SE_DPoint interiorMapLayoutPoint_D;
		interiorMapLayoutPoint_D.dx = pMainMapItem->positionWithUnits().x();
		interiorMapLayoutPoint_D.dy = pMainMapItem->positionWithUnits().y() + mapItemRect.height();


#pragma endregion

#pragma endregion



#pragma endregion

#pragma region "计算经纬网整数倍经线间隔和纬线间隔刻度所在布局点坐标"


#pragma region "内图廓A-B经度间隔整数倍刻度点布局坐标"

		// 上边界A-B经度间隔整数倍坐标值集合
		vector<SE_DPoint> vBorderGeoCoords_A_B;
		vBorderGeoCoords_A_B.clear();
		CalGeoBorderCoords_A_B(interiorMapGeoPoint_A, interiorMapGeoPoint_B, dLonInterval, vBorderGeoCoords_A_B);

		// 上边界地理坐标转Web墨卡托投影坐标
		vector<SE_DPoint> vBorderProjectCoords_A_B;
		vBorderProjectCoords_A_B.clear();
		for (int i = 0; i < vBorderGeoCoords_A_B.size(); ++i)
		{
			double L = vBorderGeoCoords_A_B[i].dx;
			double B = vBorderGeoCoords_A_B[i].dy;
			SE_DPoint dOutputPoint;
			WebMercator_BL_TO_XY(B, L, dOutputPoint.dx, dOutputPoint.dy);
			vBorderProjectCoords_A_B.push_back(dOutputPoint);
		}


#pragma endregion

#pragma region "内图廓B-C整数纬度间隔刻度点布局坐标"

		// 右边界B-C整数纬度间隔坐标值集合
		vector<SE_DPoint> vBorderGeoCoords_C_B;
		vBorderGeoCoords_C_B.clear();
		CalGeoBorderCoords_C_B(interiorMapGeoPoint_C, interiorMapGeoPoint_B, dLatInterval, vBorderGeoCoords_C_B);


		// 右边界地理坐标转Web墨卡托投影坐标
		vector<SE_DPoint> vBorderProjectCoords_C_B;
		vBorderProjectCoords_C_B.clear();
		for (int i = 0; i < vBorderGeoCoords_C_B.size(); ++i)
		{
			double L = vBorderGeoCoords_C_B[i].dx;
			double B = vBorderGeoCoords_C_B[i].dy;
			SE_DPoint dOutputPoint;
			WebMercator_BL_TO_XY(B, L, dOutputPoint.dx, dOutputPoint.dy);
			vBorderProjectCoords_C_B.push_back(dOutputPoint);
		}


#pragma endregion


#pragma region "内图廓C-D整数经度间隔刻度点布局坐标"

		// 下边界C-D整数经度间隔坐标值集合
		vector<SE_DPoint> vBorderGeoCoords_D_C;
		vBorderGeoCoords_D_C.clear();
		CalGeoBorderCoords_D_C(interiorMapGeoPoint_D, interiorMapGeoPoint_C, dLonInterval, vBorderGeoCoords_D_C);

		// 下边界地理坐标转Web墨卡托投影坐标
		vector<SE_DPoint> vBorderProjectCoords_D_C;
		vBorderProjectCoords_D_C.clear();
		for (int i = 0; i < vBorderGeoCoords_D_C.size(); ++i)
		{
			double L = vBorderGeoCoords_D_C[i].dx;
			double B = vBorderGeoCoords_D_C[i].dy;
			SE_DPoint dOutputPoint;
			WebMercator_BL_TO_XY(B, L, dOutputPoint.dx, dOutputPoint.dy);
			vBorderProjectCoords_D_C.push_back(dOutputPoint);
		}



#pragma endregion


#pragma region "内图廓A-D整数纬度间隔刻度点布局坐标"

		// 左边界A-D整数纬度间隔坐标值集合
		vector<SE_DPoint> vBorderGeoCoords_D_A;
		vBorderGeoCoords_D_A.clear();
		CalGeoBorderCoords_D_A(interiorMapGeoPoint_D, interiorMapGeoPoint_A, dLatInterval, vBorderGeoCoords_D_A);

		// 左边界地理坐标转Web墨卡托投影坐标
		vector<SE_DPoint> vBorderProjectCoords_D_A;
		vBorderProjectCoords_D_A.clear();
		for (int i = 0; i < vBorderGeoCoords_D_A.size(); ++i)
		{
			double L = vBorderGeoCoords_D_A[i].dx;
			double B = vBorderGeoCoords_D_A[i].dy;
			SE_DPoint dOutputPoint;
			WebMercator_BL_TO_XY(B, L, dOutputPoint.dx, dOutputPoint.dy);
			vBorderProjectCoords_D_A.push_back(dOutputPoint);
		}

#pragma endregion

#pragma region "计算内图廓各边界投影坐标对应的布局点坐标"

		m_InteriorMapLeftTopProjectPoint.dx = dMBRLeft;
		m_InteriorMapLeftTopProjectPoint.dy = dMBRTop;

#pragma region "上边界A-B布局坐标值集合"

		// 上边界A-B布局坐标值集合
		vector<SE_DPoint> vBorderLayoutCoords_A_B;
		vBorderLayoutCoords_A_B.clear();

		for (int iIndex = 0; iIndex < vBorderProjectCoords_A_B.size(); iIndex++)
		{
			SE_DPoint dLayoutPoint;
			// 投影坐标到布局坐标的转换
			TransformProjectToLayout(m_InteriorMapLeftTopProjectPoint,
				m_InteriorMapLeftTopLayoutPoint,
				vBorderProjectCoords_A_B[iIndex],
				dMapUnit2LayoutUnit,
				dLayoutPoint);

			vBorderLayoutCoords_A_B.push_back(dLayoutPoint);
		}

#pragma endregion


#pragma region "右边界B-C布局整公里坐标值集合"

		// 右边界B-C布局整公里坐标值集合
		vector<SE_DPoint> vBorderLayoutCoords_C_B;
		vBorderLayoutCoords_C_B.clear();

		for (int iIndex = 0; iIndex < vBorderProjectCoords_C_B.size(); iIndex++)
		{
			SE_DPoint dLayoutPoint;
			// 投影坐标到布局坐标的转换
			TransformProjectToLayout(m_InteriorMapLeftTopProjectPoint,
				m_InteriorMapLeftTopLayoutPoint,
				vBorderProjectCoords_C_B[iIndex],
				dMapUnit2LayoutUnit,
				dLayoutPoint);

			vBorderLayoutCoords_C_B.push_back(dLayoutPoint);
		}

#pragma endregion

#pragma region "下边界C-D布局整公里坐标值集合"

		// 下边界C-D布局整公里坐标值集合
		vector<SE_DPoint> vBorderLayoutCoords_D_C;
		vBorderLayoutCoords_D_C.clear();

		for (int iIndex = 0; iIndex < vBorderProjectCoords_D_C.size(); iIndex++)
		{
			SE_DPoint dLayoutPoint;
			// 投影坐标到布局坐标的转换
			TransformProjectToLayout(m_InteriorMapLeftTopProjectPoint,
				m_InteriorMapLeftTopLayoutPoint,
				vBorderProjectCoords_D_C[iIndex],
				dMapUnit2LayoutUnit,
				dLayoutPoint);

			vBorderLayoutCoords_D_C.push_back(dLayoutPoint);
		}

#pragma endregion

#pragma region "左边界A-D布局整公里坐标值集合"

		// 左边界A-D布局整公里坐标值集合
		vector<SE_DPoint> vBorderLayoutCoords_D_A;
		vBorderLayoutCoords_D_A.clear();

		for (int iIndex = 0; iIndex < vBorderProjectCoords_D_A.size(); iIndex++)
		{
			SE_DPoint dLayoutPoint;
			// 投影坐标到布局坐标的转换
			TransformProjectToLayout(m_InteriorMapLeftTopProjectPoint,
				m_InteriorMapLeftTopLayoutPoint,
				vBorderProjectCoords_D_A[iIndex],
				dMapUnit2LayoutUnit,
				dLayoutPoint);

			vBorderLayoutCoords_D_A.push_back(dLayoutPoint);
		}

#pragma endregion

#pragma region "计算上、下、左、右外图廓的布局坐标"

		// 外图廓布局坐标分布示意图
		/*
		a-------------b
		|			  |
		|			  |
		|		      |
		d-------------c
		*/

		// 外图廓a点的布局坐标
		SE_DPoint exteriorMapLayoutPoint_a;
		exteriorMapLayoutPoint_a.dx = interiorMapLayoutPoint_A.dx - OUTER_MAP_INTERIOR_MAP_DISTANCE;
		exteriorMapLayoutPoint_a.dy = interiorMapLayoutPoint_A.dy - OUTER_MAP_INTERIOR_MAP_DISTANCE;

		// 外图廓d点的布局坐标
		SE_DPoint exteriorMapLayoutPoint_d;
		exteriorMapLayoutPoint_d.dx = interiorMapLayoutPoint_D.dx - OUTER_MAP_INTERIOR_MAP_DISTANCE;
		exteriorMapLayoutPoint_d.dy = interiorMapLayoutPoint_D.dy + OUTER_MAP_INTERIOR_MAP_DISTANCE;

		// 外图廓c点的布局坐标
		SE_DPoint exteriorMapLayoutPoint_c;
		exteriorMapLayoutPoint_c.dx = interiorMapLayoutPoint_C.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE;
		exteriorMapLayoutPoint_c.dy = interiorMapLayoutPoint_C.dy + OUTER_MAP_INTERIOR_MAP_DISTANCE;


		// 外图廓b点的布局坐标
		SE_DPoint exteriorMapLayoutPoint_b;
		exteriorMapLayoutPoint_b.dx = interiorMapLayoutPoint_B.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE;
		exteriorMapLayoutPoint_b.dy = interiorMapLayoutPoint_B.dy - OUTER_MAP_INTERIOR_MAP_DISTANCE;









#pragma endregion

#pragma region "获取外图廓（矩形区域）最小外接矩形"

		// 获取外图廓（矩形区域）最小外接矩形
		SE_DRect dExteriorMapLayoutMBR;
		double dExteriorMBRPoints[8] = { 0 };
		// 依次存储外图廓a、b、c、d布局坐标
		dExteriorMBRPoints[0] = exteriorMapLayoutPoint_a.dx;
		dExteriorMBRPoints[1] = exteriorMapLayoutPoint_a.dy;

		dExteriorMBRPoints[2] = exteriorMapLayoutPoint_b.dx;
		dExteriorMBRPoints[3] = exteriorMapLayoutPoint_b.dy;

		dExteriorMBRPoints[4] = exteriorMapLayoutPoint_c.dx;
		dExteriorMBRPoints[5] = exteriorMapLayoutPoint_c.dy;

		dExteriorMBRPoints[6] = exteriorMapLayoutPoint_d.dx;
		dExteriorMBRPoints[7] = exteriorMapLayoutPoint_d.dy;

		GetMBR(4,
			dExteriorMBRPoints,
			dExteriorMapLayoutMBR.dleft,
			dExteriorMapLayoutMBR.dtop,
			dExteriorMapLayoutMBR.dright,
			dExteriorMapLayoutMBR.dbottom);

		m_dExteriorMapLayoutMBR = dExteriorMapLayoutMBR;

		// 外图廓左上角布局坐标
		m_dExteriorLeftTopLayoutPoint.dx = dExteriorMapLayoutMBR.dleft;
		m_dExteriorLeftTopLayoutPoint.dy = dExteriorMapLayoutMBR.dbottom;

		// 外图廓左下角布局坐标
		m_dExteriorLeftBottomLayoutPoint.dx = dExteriorMapLayoutMBR.dleft;
		m_dExteriorLeftBottomLayoutPoint.dy = dExteriorMapLayoutMBR.dtop;

		// 外图廓右上角布局坐标
		m_dExteriorRightTopLayoutPoint.dx = dExteriorMapLayoutMBR.dright;
		m_dExteriorRightTopLayoutPoint.dy = dExteriorMapLayoutMBR.dbottom;

		// 外图廓右下角布局坐标
		m_dExteriorRightBottomLayoutPoint.dx = dExteriorMapLayoutMBR.dright;
		m_dExteriorRightBottomLayoutPoint.dy = dExteriorMapLayoutMBR.dtop;

#pragma endregion

#pragma region "计算外图廓整公里刻度布局点坐标"

#pragma region "计算上边界a-b整公里刻度布局点坐标"

		vector<SE_DPoint> vBorderLayoutCoords_a_b;
		vBorderLayoutCoords_a_b.clear();

		for (int iIndex = 0; iIndex < vBorderLayoutCoords_A_B.size(); iIndex++)
		{
			double dX = vBorderLayoutCoords_A_B[iIndex].dx;
			double dY = exteriorMapLayoutPoint_a.dy;

			vBorderLayoutCoords_a_b.push_back(SE_DPoint(dX, dY));
		}

#pragma endregion


#pragma region "计算下边界c-d整公里刻度布局点坐标"

		vector<SE_DPoint> vBorderLayoutCoords_d_c;
		vBorderLayoutCoords_d_c.clear();

		for (int iIndex = 0; iIndex < vBorderLayoutCoords_D_C.size(); iIndex++)
		{
			double dX = vBorderLayoutCoords_D_C[iIndex].dx;
			double dY = exteriorMapLayoutPoint_c.dy;

			vBorderLayoutCoords_d_c.push_back(SE_DPoint(dX, dY));
		}

#pragma endregion

#pragma region "计算左边界a-d整公里刻度布局点坐标"

		vector<SE_DPoint> vBorderLayoutCoords_d_a;
		vBorderLayoutCoords_d_a.clear();

		for (int iIndex = 0; iIndex < vBorderLayoutCoords_D_A.size(); iIndex++)
		{
			double dX = exteriorMapLayoutPoint_a.dx;
			double dY = vBorderLayoutCoords_D_A[iIndex].dy;

			vBorderLayoutCoords_d_a.push_back(SE_DPoint(dX, dY));
		}

#pragma endregion


#pragma region "计算右边界b-c整公里刻度布局点坐标"

		vector<SE_DPoint> vBorderLayoutCoords_c_b;
		vBorderLayoutCoords_c_b.clear();

		for (int iIndex = 0; iIndex < vBorderLayoutCoords_C_B.size(); iIndex++)
		{
			double dX = exteriorMapLayoutPoint_b.dx;
			double dY = vBorderLayoutCoords_C_B[iIndex].dy;

			vBorderLayoutCoords_c_b.push_back(SE_DPoint(dX, dY));
		}

#pragma endregion


#pragma endregion



#pragma endregion






#pragma endregion

#pragma region "绘制内图廓边框"

		// 内图廓A-B
		QgsLayoutItemPolyline* pInteriorMapBorder_A_B = nullptr;
		QPolygonF pPolygon_A_B;
		pPolygon_A_B.append(QPointF(interiorMapLayoutPoint_A.dx, interiorMapLayoutPoint_A.dy));
		pPolygon_A_B.append(QPointF(interiorMapLayoutPoint_B.dx, interiorMapLayoutPoint_B.dy));
		pInteriorMapBorder_A_B = new QgsLayoutItemPolyline(pPolygon_A_B, pPrintLayout);
		pInteriorMapBorder_A_B->setSymbol(interiorMapLineSymbol);
		pPrintLayout->addLayoutItem(pInteriorMapBorder_A_B);

		// 内图廓B-C
		QgsLayoutItemPolyline* pInteriorMapBorder_B_C = nullptr;
		QPolygonF pPolygon_B_C;
		pPolygon_B_C.append(QPointF(interiorMapLayoutPoint_B.dx, interiorMapLayoutPoint_B.dy));
		pPolygon_B_C.append(QPointF(interiorMapLayoutPoint_C.dx, interiorMapLayoutPoint_C.dy));
		pInteriorMapBorder_B_C = new QgsLayoutItemPolyline(pPolygon_B_C, pPrintLayout);
		pInteriorMapBorder_B_C->setSymbol(interiorMapLineSymbol);
		pPrintLayout->addLayoutItem(pInteriorMapBorder_B_C);

		// 内图廓C-D
		QgsLayoutItemPolyline* pInteriorMapBorder_C_D = nullptr;
		QPolygonF pPolygon_C_D;
		pPolygon_C_D.append(QPointF(interiorMapLayoutPoint_C.dx, interiorMapLayoutPoint_C.dy));
		pPolygon_C_D.append(QPointF(interiorMapLayoutPoint_D.dx, interiorMapLayoutPoint_D.dy));
		pInteriorMapBorder_C_D = new QgsLayoutItemPolyline(pPolygon_C_D, pPrintLayout);
		pInteriorMapBorder_C_D->setSymbol(interiorMapLineSymbol);
		pPrintLayout->addLayoutItem(pInteriorMapBorder_C_D);


		// 内图廓A-D
		QgsLayoutItemPolyline* pInteriorMapBorder_A_D = nullptr;
		QPolygonF pPolygon_A_D;
		pPolygon_A_D.append(QPointF(interiorMapLayoutPoint_A.dx, interiorMapLayoutPoint_A.dy));
		pPolygon_A_D.append(QPointF(interiorMapLayoutPoint_D.dx, interiorMapLayoutPoint_D.dy));
		pInteriorMapBorder_A_D = new QgsLayoutItemPolyline(pPolygon_A_D, pPrintLayout);
		pInteriorMapBorder_A_D->setSymbol(interiorMapLineSymbol);
		pPrintLayout->addLayoutItem(pInteriorMapBorder_A_D);

#pragma endregion

#pragma region "绘制外图廓边框"

		// 外图廓a-b
		QgsLayoutItemPolyline* pExteriorMapBorder_a_b = nullptr;
		QPolygonF pPolygon_a_b;
		pPolygon_a_b.append(QPointF(exteriorMapLayoutPoint_a.dx, exteriorMapLayoutPoint_a.dy));
		pPolygon_a_b.append(QPointF(exteriorMapLayoutPoint_b.dx, exteriorMapLayoutPoint_b.dy));
		pExteriorMapBorder_a_b = new QgsLayoutItemPolyline(pPolygon_a_b, pPrintLayout);
		pExteriorMapBorder_a_b->setSymbol(exteriorMapLineSymbol);
		pPrintLayout->addLayoutItem(pExteriorMapBorder_a_b);

		// 外图廓b-c
		QgsLayoutItemPolyline* pExteriorMapBorder_b_c = nullptr;
		QPolygonF pPolygon_b_c;
		pPolygon_b_c.append(QPointF(exteriorMapLayoutPoint_b.dx, exteriorMapLayoutPoint_b.dy));
		pPolygon_b_c.append(QPointF(exteriorMapLayoutPoint_c.dx, exteriorMapLayoutPoint_c.dy));
		pExteriorMapBorder_b_c = new QgsLayoutItemPolyline(pPolygon_b_c, pPrintLayout);
		pExteriorMapBorder_b_c->setSymbol(exteriorMapLineSymbol);
		pPrintLayout->addLayoutItem(pExteriorMapBorder_b_c);

		// 外图廓c-d
		QgsLayoutItemPolyline* pExteriorMapBorder_c_d = nullptr;
		QPolygonF pPolygon_c_d;
		pPolygon_c_d.append(QPointF(exteriorMapLayoutPoint_c.dx, exteriorMapLayoutPoint_c.dy));
		pPolygon_c_d.append(QPointF(exteriorMapLayoutPoint_d.dx, exteriorMapLayoutPoint_d.dy));
		pExteriorMapBorder_c_d = new QgsLayoutItemPolyline(pPolygon_c_d, pPrintLayout);
		pExteriorMapBorder_c_d->setSymbol(exteriorMapLineSymbol);
		pPrintLayout->addLayoutItem(pExteriorMapBorder_c_d);

		// 外图廓a-d
		QgsLayoutItemPolyline* pExteriorMapBorder_a_d = nullptr;
		QPolygonF pPolygon_a_d;
		pPolygon_a_d.append(QPointF(exteriorMapLayoutPoint_a.dx, exteriorMapLayoutPoint_a.dy));
		pPolygon_a_d.append(QPointF(exteriorMapLayoutPoint_d.dx, exteriorMapLayoutPoint_d.dy));
		pExteriorMapBorder_a_d = new QgsLayoutItemPolyline(pPolygon_a_d, pPrintLayout);
		pExteriorMapBorder_a_d->setSymbol(exteriorMapLineSymbol);
		pPrintLayout->addLayoutItem(pExteriorMapBorder_a_d);

#pragma endregion




		/*绘制经纬网原则
		* 如果上、下边界整公里X坐标存在不同，则暂不绘制对应X坐标的经纬网；
		* 如果左、右边界整公里Y坐标存在不同，则暂不绘制对应Y坐标的经纬网；
		* 上、下、左、右边界经纬网刻度与上、下、左、右边界线垂直
		* 对于5万比例尺，经纬网刻度、邻带经纬网刻度只需绘制偶数刻度值
		*/


#pragma region "绘制经纬网"

		// 绘制经纬网

#pragma region "绘制上边界到下边界经纬网及刻度线"

		/*-------------------------------------*/
		/*   绘制上边界到下边界经纬网及刻度线  */
		/*-------------------------------------*/

		// X方向经纬网
		// 从左往右依次绘制上边界刻度
		QgsLayoutItemPolyline** pKmLine_X_Top = new QgsLayoutItemPolyline * [vBorderLayoutCoords_A_B.size()];
		if (pKmLine_X_Top)
		{
			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_A_B.size(); iPointIndex++)
			{
				// 内图廓上边界整间隔刻度点
				SE_DPoint layoutTopBorderPoint = vBorderLayoutCoords_A_B[iPointIndex];

				// 查找当前点x值是否在内图廓下边界整刻度点中x值有相同的
				int iIndexOfX = CalIndexInVector_XCoord(layoutTopBorderPoint.dx, vBorderLayoutCoords_D_C);

				// 如果索引值不为-1，说明有相同x值的点
				if (iIndexOfX != -1)
				{
					// 内图廓下边界整整间隔刻度点索引
					SE_DPoint layoutBottomBorderPoint = vBorderLayoutCoords_D_C[iIndexOfX];
					QPolygonF polygon;
					// 整公里刻度线与外图廓上边界交叉点
					polygon.append(QPointF(vBorderLayoutCoords_a_b[iPointIndex].dx, vBorderLayoutCoords_a_b[iPointIndex].dy));
					// 整公里刻度线与外图廓下边界交叉点
					polygon.append(QPointF(vBorderLayoutCoords_d_c[iIndexOfX].dx, vBorderLayoutCoords_d_c[iIndexOfX].dy));

					pKmLine_X_Top[iPointIndex] = new QgsLayoutItemPolyline(polygon, pPrintLayout);
					pKmLine_X_Top[iPointIndex]->setSymbol(geoLineBorderLineSymbol);
					pPrintLayout->addLayoutItem(pKmLine_X_Top[iPointIndex]);

				}

				else // 待完善：是否需要与左、右边界进行相交绘制
				{

				}
			}

		}


		// 从左往右依次绘制下边界刻度
		QgsLayoutItemPolyline** pKmLine_X_Bottom = new QgsLayoutItemPolyline * [vBorderLayoutCoords_D_C.size()];
		if (pKmLine_X_Bottom)
		{
			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_D_C.size(); iPointIndex++)
			{
				// 内图廓下边界整公里刻度点
				SE_DPoint layoutBottomBorderPoint = vBorderLayoutCoords_D_C[iPointIndex];

				// 查找当前点x值是否在内图廓上边界整刻度点中x值有相同的
				int iIndexOfX = CalIndexInVector_XCoord(layoutBottomBorderPoint.dx, vBorderLayoutCoords_A_B);

				// 如果索引值不为-1，说明有相同x值的点，已经绘制过，直接跳过
				if (iIndexOfX != -1)
				{
					continue;
				}

				else // 待完善：是否需要与左、右边界进行相交绘制
				{

				}
			}
		}


#pragma endregion

#pragma region "绘制左边界到右边界经纬网及刻度线"

		/*-------------------------------------*/
		/*   绘制左边界到右边界经纬网及刻度线  */
		/*-------------------------------------*/
		// 从下往上绘制左边界Y方向经纬网
		QgsLayoutItemPolyline** pKmLine_Y_Left = new QgsLayoutItemPolyline * [vBorderLayoutCoords_D_A.size()];
		for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_D_A.size(); iPointIndex++)
		{
			// 内图廓左边界整间隔刻度点
			SE_DPoint layoutLeftBorderPoint = vBorderLayoutCoords_D_A[iPointIndex];

			// 查找当前点y值是否在内图廓右边界整刻度点中y值有相同的
			int iIndexOfY = CalIndexInVector_YCoord(layoutLeftBorderPoint.dy, vBorderLayoutCoords_C_B);

			// 如果索引值不为-1，说明有相同y值的点
			if (iIndexOfY != -1)
			{
				// 内图廓右边界整公里刻度点
				SE_DPoint layoutRightBorderPoint = vBorderLayoutCoords_C_B[iIndexOfY];
				QPolygonF polygon;
				// 整间隔刻度线与外图廓左边界交叉点
				polygon.append(QPointF(vBorderLayoutCoords_d_a[iPointIndex].dx, vBorderLayoutCoords_d_a[iPointIndex].dy));

				// 整间隔刻度线与外图廓右边界交叉点
				polygon.append(QPointF(vBorderLayoutCoords_c_b[iIndexOfY].dx, vBorderLayoutCoords_c_b[iIndexOfY].dy));

				pKmLine_Y_Left[iPointIndex] = new QgsLayoutItemPolyline(polygon, pPrintLayout);
				pKmLine_Y_Left[iPointIndex]->setSymbol(geoLineBorderLineSymbol);
				pPrintLayout->addLayoutItem(pKmLine_Y_Left[iPointIndex]);


			}
			else  // 待完善：是否需要与上、下边界进行相交绘制
			{

			}
		}


		// 从下往上绘制右边界Y方向经纬网

		for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_C_B.size(); iPointIndex++)
		{
			// 内图廓右边界整间隔刻度点
			SE_DPoint layoutRightBorderPoint = vBorderLayoutCoords_C_B[iPointIndex];

			// 查找当前点y值是否在内图廓左边界整刻度点中y值有相同的
			int iIndexOfY = CalIndexInVector_YCoord(layoutRightBorderPoint.dy, vBorderLayoutCoords_D_A);

			// 如果索引值不为-1，说明有相同y值的点，已绘制，跳过
			if (iIndexOfY != -1)
			{
				continue;
			}
			else  // 待完善：是否需要与上、下边界进行相交绘制
			{

			}
		}
#pragma endregion

#pragma region "绘制经纬网刻度值"


		/*-----------------------------------------------------*/
		/*   绘制上边界、下边界、左边界、右边界经纬网刻度值    */
		/*-----------------------------------------------------*/

#pragma region "绘制内图廓上边界整度刻度值"

		// 上边界经度整数值
		QgsLayoutItemLabel* pTopBorderLabel_Lon = nullptr;
		// 设置字体及大小
		QFont fontGeoCoordValue;
		fontGeoCoordValue.setFamily(QStringLiteral("等线"));
		fontGeoCoordValue.setPixelSize(35);
		fontGeoCoordValue.setBold(true);

		/*记录需要绘制百公里经纬网数值的第一个点和最后一个点索引值*/
		// 第一个需要绘制百公里及带号的上、下边界整公里经纬网点索引
		int iBeginMarkIndex_X_Top = 0;                                      // 默认索引为0
		int iEndMarkIndex_X_Top = vBorderLayoutCoords_A_B.size() - 1;        // 默认索引为最后一个点

		/*绘制内图廓上边界整公里刻度值*/
		for (int iPointIndex = iBeginMarkIndex_X_Top; iPointIndex <= iEndMarkIndex_X_Top; iPointIndex++)
		{
			// 内图廓上边界整公里刻度点
			SE_DPoint layoutTopBorderPoint = vBorderLayoutCoords_A_B[iPointIndex];
			SE_DPoint layoutTopBorderGeoPoint = vBorderGeoCoords_A_B[iPointIndex];

			// 绘制经度值
			pTopBorderLabel_Lon = new QgsLayoutItemLabel(pPrintLayout);
			if (!pTopBorderLabel_Lon)
			{
				// 记录日志
				continue;
			}

			pTopBorderLabel_Lon->setFont(fontGeoCoordValue);

			// 如果经度大于0，显示30°E，经度等于0，显示0°，经度小于0，显示30°W
			QString qstrLon = FormatLonDegree_Double(layoutTopBorderGeoPoint.dx);

			pTopBorderLabel_Lon->setText(qstrLon);

			pTopBorderLabel_Lon->attemptSetSceneRect(QRectF(layoutTopBorderPoint.dx + 0.15,
				layoutTopBorderPoint.dy - 5,
				20,
				20));
			pPrintLayout->addLayoutItem(pTopBorderLabel_Lon);

		}

#pragma endregion

#pragma region "绘制内图廓下边界整公里刻度值"

#pragma region "绘制内图廓下边界整公里刻度值"

		/*记录需要绘制百公里经纬网数值的第一个点和最后一个点索引值*/
		// 第一个需要绘制百公里及带号的上、下边界整公里经纬网点索引
		int iBeginMarkIndex_X_Bottom = 0;                                      // 默认索引为0
		int iEndMarkIndex_X_Bottom = vBorderLayoutCoords_D_C.size() - 1;        // 默认索引为最后一个点

		// 下边界十位整间隔数值
		QgsLayoutItemLabel* pBottomBorderLabel_10km = nullptr;

		/*绘制内图廓下边界整间隔刻度值*/
		for (int iPointIndex = iBeginMarkIndex_X_Bottom; iPointIndex <= iEndMarkIndex_X_Bottom; iPointIndex++)
		{
			// 内图廓下边界整间隔刻度点
			SE_DPoint layoutBottomBorderPoint = vBorderLayoutCoords_D_C[iPointIndex];
			SE_DPoint layoutBottomBorderGeoPoint = vBorderGeoCoords_D_C[iPointIndex];

			// 绘制十位公里数值
			pBottomBorderLabel_10km = new QgsLayoutItemLabel(pPrintLayout);
			if (!pBottomBorderLabel_10km)
			{
				// 记录日志
				continue;
			}

			pBottomBorderLabel_10km->setFont(fontGeoCoordValue);

			QString qstrLon = FormatLonDegree_Double(layoutBottomBorderGeoPoint.dx);
			pBottomBorderLabel_10km->setText(qstrLon);

			pBottomBorderLabel_10km->attemptSetSceneRect(QRectF(layoutBottomBorderPoint.dx + 0.15,
				layoutBottomBorderPoint.dy + 0.1,
				20,
				20));
			pPrintLayout->addLayoutItem(pBottomBorderLabel_10km);

		}


#pragma endregion



#pragma endregion

#pragma region "绘制内图廓左边界经纬网数值"

		/*--------------------------------------*/
		/*         绘制左边界经纬网数值         */
		/*--------------------------------------*/

		// 左边界十位整公里数值
		QgsLayoutItemLabel* pLeftBorderLabel_Lat = nullptr;

		// 第一个需要绘制千公里+百公里的左边界整公里经纬网点索引
		int iBeginMarkIndex_Y_Left = 0;                                       // 默认索引为0
		int iEndMarkIndex_Y_Left = vBorderLayoutCoords_D_A.size() - 1;        // 默认索引为最后一个点


		/*绘制内图廓左边界整公里刻度值*/
		for (int iPointIndex = iBeginMarkIndex_Y_Left; iPointIndex <= iEndMarkIndex_Y_Left; iPointIndex++)
		{
			// 内图廓左边界整公里刻度点布局坐标值
			SE_DPoint layoutLeftBorderPoint = vBorderLayoutCoords_D_A[iPointIndex];

			// 内图廓左边界整公里地理坐标值
			SE_DPoint layoutLeftBorderGeoPoint = vBorderGeoCoords_D_A[iPointIndex];


			// 左边界绘制十位公里数值
			pLeftBorderLabel_Lat = new QgsLayoutItemLabel(pPrintLayout);
			if (!pLeftBorderLabel_Lat)
			{
				// 记录日志
				continue;
			}

			pLeftBorderLabel_Lat->setFont(fontGeoCoordValue);
			QString qstrLat = FormatLatDegree_Double(layoutLeftBorderGeoPoint.dy);
			pLeftBorderLabel_Lat->setText(qstrLat);


			// 纬度刻度值
			pLeftBorderLabel_Lat->attemptSetSceneRect(QRectF(layoutLeftBorderPoint.dx - 9.5,
				layoutLeftBorderPoint.dy - 5,
				20,
				20));


			pPrintLayout->addLayoutItem(pLeftBorderLabel_Lat);

		}
		/*-------------【end】----------------*/


#pragma endregion

#pragma region "绘制内图廓右边界经纬网数值"
		/*--------------------------------------*/
		/*         绘制右边界经纬网数值         */
		/*--------------------------------------*/
		/*-------------【begin】----------------*/

		// 第一个需要绘制千公里+百公里的右边界整公里经纬网点索引
		int iBeginMarkIndex_Y_Right = 0;                                       // 默认索引为0
		int iEndMarkIndex_Y_Right = vBorderLayoutCoords_C_B.size() - 1;        // 默认索引为最后一个点

		// 右边界十位整公里数值
		QgsLayoutItemLabel* pRightBorderLabel_10km = nullptr;

		/*绘制内图廓右边界整公里刻度值*/
		for (int iPointIndex = iBeginMarkIndex_Y_Right; iPointIndex <= iEndMarkIndex_Y_Right; iPointIndex++)
		{
			// 内图廓右边界整公里刻度点布局坐标值
			SE_DPoint layoutRightBorderPoint = vBorderLayoutCoords_C_B[iPointIndex];

			// 内图廓右边界整公里地理坐标值
			SE_DPoint layoutRightBorderGeoPoint = vBorderGeoCoords_C_B[iPointIndex];

			// 右边界绘制十位公里数值
			pRightBorderLabel_10km = new QgsLayoutItemLabel(pPrintLayout);
			if (!pRightBorderLabel_10km)
			{
				// 记录日志
				continue;
			}

			pRightBorderLabel_10km->setFont(fontGeoCoordValue);
			QString qstrLat = FormatLatDegree_Double(layoutRightBorderGeoPoint.dy);
			pRightBorderLabel_10km->setText(qstrLat);


			// 10km刻度值
			pRightBorderLabel_10km->attemptSetSceneRect(QRectF(layoutRightBorderPoint.dx + 0.5,
				layoutRightBorderPoint.dy - 5,
				20,
				20));

			pPrintLayout->addLayoutItem(pRightBorderLabel_10km);

		}
		/*-------------【end】-----------------*/
#pragma endregion



#pragma endregion

		


#pragma endregion


#pragma region "图外整饰要素调整位置"



#pragma region "【1】更新地图标题-图名位置"

		// 获取地图标题-图幅名称
		QgsLayoutItemLabel* pTextItem_MapTitleName = (QgsLayoutItemLabel*)GetLayoutItemByName(
			qLayoutItemList,
			"TextItem_MapTitleName");

		((QgsLayoutItemLabel*)pTextItem_MapTitleName)->setText(qstrMapName);

		// 如果设置了字体和颜色
		if (m_bSetMapNameFont)
		{
			((QgsLayoutItemLabel*)pTextItem_MapTitleName)->setFont(m_qMapNameFont);
		}

		if (m_bSetMapNameColor)
		{
			((QgsLayoutItemLabel*)pTextItem_MapTitleName)->setFontColor(m_qMapNameColor);
		}



		// 设置参考点坐标
		pTextItem_MapTitleName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2,
			m_dExteriorLeftTopLayoutPoint.dy - 8));

		// 自动调整尺寸
		pTextItem_MapTitleName->adjustSizeToText();



#pragma endregion

#pragma region "【2】地图左下角说明"

		QgsLayoutItemLabel* pTextItem_MapDetail = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_MapDetail");
			
		pTextItem_MapDetail->setText(qstrMapDetail);
		
		// 如果设置了字体和颜色
		if (m_bSetMapDetailFont)
		{
			pTextItem_MapDetail->setFont(m_qMapDetailFont);
		}

		if (m_bSetMapDetailColor)
		{
			pTextItem_MapDetail->setFontColor(m_qMapDetailColor);
		}

		pTextItem_MapDetail->setVisibility(true);

		// 调整尺寸
		pTextItem_MapDetail->attemptSetSceneRect(QRectF(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE,
			m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3,
			60,
			27));
		

		


#pragma endregion

#pragma region "【3】比例尺文字"

		QgsLayoutItemLabel* pTextItem_Scale = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_Scale");

		// 如果是绘制状态
		if (bDrawScale)
		{
			// 计算出的比例尺
			double dScaleTemp = int(dMapScale / 100.0) * 100.0;
			QString qstrScale = tr("1:%1").arg(dScaleTemp);
			pTextItem_Scale->setText(qstrScale);
			pTextItem_Scale->setVisibility(true);

			// 设置位置
			pTextItem_Scale->setReferencePoint(QgsLayoutItem::LowerMiddle);

			// 设置参考点坐标
			pTextItem_Scale->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 9));

			// 设置水平对齐方式
			pTextItem_Scale->setHAlign(Qt::AlignHCenter);

		}
		else
		{
			pTextItem_Scale->setVisibility(false);
		}
		

#pragma endregion

#pragma region "【4】制图单位"
	
		QgsLayoutItemLabel* pTextItem_MappingAgency = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_MappingAgency");
		// 如果是绘制状态
		if (bDrawMapAgency)
		{
			pTextItem_MappingAgency->setText(qstrMapAgency);

			// 如果设置了字体和颜色
			if (m_bSetMapAgencyFont)
			{
				pTextItem_MappingAgency->setFont(m_qMapAgencyFont);
			}

			if (m_bSetMapAgencyColor)
			{
				pTextItem_MappingAgency->setFontColor(m_qMapAgencyColor);
			}


			pTextItem_MappingAgency->setVisibility(true);

			// 设置位置
			pTextItem_MappingAgency->setReferencePoint(QgsLayoutItem::Middle);

			// 设置参考点坐标
			pTextItem_MappingAgency->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 28));

			// 自动调整尺寸
			pTextItem_MappingAgency->adjustSizeToText();

		}
		else
		{
			pTextItem_MappingAgency->setVisibility(false);
		}


#pragma endregion

#pragma region "动目标说明"

		QgsLayoutItemLabel* pTextItem_MovingTargetDetail = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_MovingTargetDetail");
		
		// 增加对话框设置动目标说明
		pTextItem_MovingTargetDetail->setText(qstrMovingTargetDetail);

		// 如果设置了字体和颜色
		if (m_bSetMapMovingTargetFont)
		{
			pTextItem_MovingTargetDetail->setFont(m_qMapMovingTargetFont);
		}

		if (m_bSetMapMovingTargetColor)
		{
			pTextItem_MovingTargetDetail->setFontColor(m_qMapMovingTargetColor);
		}



		pTextItem_MovingTargetDetail->setVisibility(true);

		// 设置位置，右上角参考点
		pTextItem_MovingTargetDetail->setReferencePoint(QgsLayoutItem::UpperRight);

		// 调整尺寸
		pTextItem_MovingTargetDetail->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() - OUTER_MAP_INTERIOR_MAP_DISTANCE,
			m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3));


#pragma endregion

#pragma region "指北针"

		QgsLayoutItemPicture *pPictureNorthArrowItem = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_NorthArrow");

		// 如果是绘制状态
		if (bDrawNorthArrow)
		{		
			pPictureNorthArrowItem->setVisibility(true);
			pPictureNorthArrowItem->setReferencePoint(QgsLayoutItem::LowerRight);
			// 调整尺寸
			pPictureNorthArrowItem->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() - OUTER_MAP_INTERIOR_MAP_DISTANCE,
				m_dExteriorLeftTopLayoutPoint.dy - 8));

		}



#pragma endregion


#pragma endregion








#pragma region "导出地图、元数据、工程文件"
		
		/*【4】每个图幅输出工程文件、元数据文件、地图文件（pdf格式或tiff格式）*/
		QgsLayoutPageCollection* pPageCollection = pPrintLayout->pageCollection();

		/*默认获取第1页*/
		QgsLayoutItemPage* pPage = pPageCollection->page(0);
		if (!pPage)
		{
			// 记录日志
			QgsMessageLog::logMessage(tr("获取布局页面失败！"), tr("动目标专题制图"), Qgis::Critical);
			return;
		}

		// TODO:默认页面尺寸，可通过配置文件获取
		double width = dMapWidth + 200;     // 单位：毫米
		double height = dMapHeight + 500;    // 单位：毫米
		QgsUnitTypes::LayoutUnit units = QgsUnitTypes::LayoutMillimeters;
		QgsLayoutSize pageSize = QgsLayoutSize(width, height, units);
		pPage->setPageSize(pageSize);

		// 导出地图路径下为每个图幅创建一个导出数据文件夹，包括地图文件、工程名称、影像图元数据
		QString qstrMapSavePath = m_qstrMappingDataSavePath + "/" + qstrMapName;
		string strMapSavePath = qstrMapSavePath.toLocal8Bit();
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strMapSavePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strMapSavePath.c_str(), MODE);
#endif

		// 导出地图文件全路径
		QString qstrMapFileName;
		if (qstrExportMapType == "GeoPDF"
			|| qstrExportMapType == "PDF")
		{
			qstrMapFileName = qstrMapSavePath + "/" + qstrMapName + ".pdf";
		}
		else if (qstrExportMapType == "tiff")
		{
			qstrMapFileName = qstrMapSavePath + "/" + qstrMapName + ".tiff";
		}

		/*-------------------根据导出类型导出地图-----------*/

		bResult = ExportMapToFile(qstrMapFileName, qstrExportMapType, pPrintLayout);
		if (!bResult)
		{
			QgsMessageLog::logMessage(tr("导出地图文件失败！"), tr("动目标专题制图"), Qgis::Critical);
		}


		/*---------------导出工程文件--------------*/
		pProject->setFilePathStorage(Qgis::FilePathType::Absolute);
		QString qstrProjectName = qstrMapSavePath + "/" + qstrMapName + ".qgz";

		QFileInfo info(qstrProjectName);
		pProject->setFileName(qstrProjectName);

		bResult = pProject->write();
		if (!bResult)
		{
			QgsMessageLog::logMessage(tr("%1工程文件保存失败！").arg(qstrProjectName), tr("动目标专题制图"), Qgis::Critical);
			return;
		}

		

















	

#pragma endregion

	ui.progressBar->setValue(100);














	

#pragma endregion




}

void QgsMovingTargetMappingDialog::ComboPageSizeSelectedChanged()
{
	/*A0：841×1189   B0：1000×1414
	  A1：594×841	  B1：707×1000
	  A2：420×594    B2：500×707
	  A3：297×420    B3：353×500
	  A4：210×297    B4：250×353*/
	int index = ui.comboBox_PageSize->currentIndex();

	// 横向
	if (m_iPageDirection == 1)
	{
		// A4
		if (index == 1)
		{
			ui.lineEdit_PageWidth->setText(tr("297"));
			ui.lineEdit_PageHeight->setText(tr("210"));
		}
		// A3
		else if (index == 2)
		{
			ui.lineEdit_PageWidth->setText(tr("420"));
			ui.lineEdit_PageHeight->setText(tr("297"));
		}
		// A2
		else if (index == 3)
		{
			ui.lineEdit_PageWidth->setText(tr("594"));
			ui.lineEdit_PageHeight->setText(tr("420"));
		}
		// A1
		else if (index == 4)
		{
			ui.lineEdit_PageWidth->setText(tr("841"));
			ui.lineEdit_PageHeight->setText(tr("594"));
		}
		// A0
		else if (index == 5)
		{
			ui.lineEdit_PageWidth->setText(tr("1189"));
			ui.lineEdit_PageHeight->setText(tr("841"));
		}

		// B4
		else if (index == 6)
		{
			ui.lineEdit_PageWidth->setText(tr("353"));
			ui.lineEdit_PageHeight->setText(tr("250"));
		}
		// B3
		else if (index == 7)
		{
			ui.lineEdit_PageWidth->setText(tr("500"));
			ui.lineEdit_PageHeight->setText(tr("353"));
		}
		// B2
		else if (index == 8)
		{
			ui.lineEdit_PageWidth->setText(tr("707"));
			ui.lineEdit_PageHeight->setText(tr("500"));
		}
		// B1
		else if (index == 9)
		{
			ui.lineEdit_PageWidth->setText(tr("1000"));
			ui.lineEdit_PageHeight->setText(tr("707"));
		}
		// B0
		else if (index == 10)
		{
			ui.lineEdit_PageWidth->setText(tr("1414"));
			ui.lineEdit_PageHeight->setText(tr("1000"));
		}
	}
	// 纵向
	else if (m_iPageDirection == 2)
	{
		// A4
		if (index == 1)
		{
			ui.lineEdit_PageWidth->setText(tr("210"));
			ui.lineEdit_PageHeight->setText(tr("297"));
		}
		// A3
		else if (index == 2)
		{
			ui.lineEdit_PageWidth->setText(tr("297"));
			ui.lineEdit_PageHeight->setText(tr("420"));
		}
		// A2
		else if (index == 3)
		{
			ui.lineEdit_PageWidth->setText(tr("420"));
			ui.lineEdit_PageHeight->setText(tr("594"));
		}
		// A1
		else if (index == 4)
		{
			ui.lineEdit_PageWidth->setText(tr("594"));
			ui.lineEdit_PageHeight->setText(tr("841"));
		}
		// A0
		else if (index == 5)
		{
			ui.lineEdit_PageWidth->setText(tr("841"));
			ui.lineEdit_PageHeight->setText(tr("1189"));
		}

		// B4
		else if (index == 6)
		{
			ui.lineEdit_PageWidth->setText(tr("250"));
			ui.lineEdit_PageHeight->setText(tr("353"));
		}
		// B3
		else if (index == 7)
		{
			ui.lineEdit_PageWidth->setText(tr("353"));
			ui.lineEdit_PageHeight->setText(tr("500"));
		}
		// B2
		else if (index == 8)
		{
			ui.lineEdit_PageWidth->setText(tr("500"));
			ui.lineEdit_PageHeight->setText(tr("707"));
		}
		// B1
		else if (index == 9)
		{
			ui.lineEdit_PageWidth->setText(tr("707"));
			ui.lineEdit_PageHeight->setText(tr("1000"));
		}
		// B0
		else if (index == 10)
		{
			ui.lineEdit_PageWidth->setText(tr("1000"));
			ui.lineEdit_PageHeight->setText(tr("1414"));
		}
	}
		
}

void QgsMovingTargetMappingDialog::ComboPageDirectionSelectedChanged()
{
	int index = ui.comboBox_PageDirection->currentIndex();
	if (index == 0)
	{
		m_iPageDirection = 1;
	}
	else
	{
		m_iPageDirection = 2;
	}

}

void QgsMovingTargetMappingDialog::SetMapNameFontSize()
{
	QFont iniFont = QFont(QStringLiteral("等线"));
	bool ok = false;
	QFont font = QFontDialog::getFont(&ok, iniFont, this, tr("选择字体"));
	if (ok)
	{
		m_qMapNameFont = font;
		m_bSetMapNameFont = true;
	}
}

void QgsMovingTargetMappingDialog::SetMapNameFontColor()
{
	// 默认黑色
	QColor iniColor = QColor(0, 0, 0);

	QColor color = QColorDialog::getColor(iniColor,this,tr("选择颜色"));
	if (color.isValid())
	{
		m_qMapNameColor = color;
		m_bSetMapNameColor = true;
	}
}

void QgsMovingTargetMappingDialog::SetMapAgencyFontSize()
{
	QFont iniFont = QFont(QStringLiteral("等线"));
	bool ok = false;
	QFont font = QFontDialog::getFont(&ok, iniFont,this,tr("选择字体"));
	if (ok)
	{
		m_qMapAgencyFont = font;
		m_bSetMapAgencyFont = true;
	}
}

void QgsMovingTargetMappingDialog::SetMapAgencyFontColor()
{
	// 默认黑色
	QColor iniColor = QColor(0, 0, 0);

	QColor color = QColorDialog::getColor(iniColor, this, tr("选择颜色"));
	if (color.isValid())
	{
		m_qMapAgencyColor = color;
		m_bSetMapAgencyColor = true;
	}
}

// 设置地图说明字体大小
void QgsMovingTargetMappingDialog::SetMapDetailFontSize()
{
	QFont iniFont = QFont(QStringLiteral("等线"));
	bool ok = false;
	QFont font = QFontDialog::getFont(&ok, iniFont, this, tr("选择字体"));
	if (ok)
	{
		m_qMapDetailFont = font;
		m_bSetMapDetailFont = true;
	}
}

// 设置地图说明字体颜色
void QgsMovingTargetMappingDialog::SetMapDetailFontColor()
{
	// 默认黑色
	QColor iniColor = QColor(0, 0, 0);

	QColor color = QColorDialog::getColor(iniColor, this, tr("选择颜色"));
	if (color.isValid())
	{
		m_qMapDetailColor = color;
		m_bSetMapDetailColor = true;
	}
}

void QgsMovingTargetMappingDialog::SetMapMovingTargetFontSize()
{
	QFont iniFont = QFont(QStringLiteral("等线"));
	bool ok = false;
	QFont font = QFontDialog::getFont(&ok, iniFont, this, tr("选择字体"));
	if (ok)
	{
		m_qMapMovingTargetFont = font;
		m_bSetMapMovingTargetFont = true;
	}
}

void QgsMovingTargetMappingDialog::SetMapMovingTargetFontColor()
{
	// 默认黑色
	QColor iniColor = QColor(0, 0, 0);

	QColor color = QColorDialog::getColor(iniColor, this, tr("选择颜色"));
	if (color.isValid())
	{
		m_qMapMovingTargetColor = color;
		m_bSetMapMovingTargetColor = true;
	}
}

void QgsMovingTargetMappingDialog::SetMapSizeRadioButton()
{
	ui.comboBox_PageDirection->setEnabled(true);
	ui.comboBox_PageSize->setEnabled(true);
	ui.lineEdit_PageWidth->setEnabled(true);
	ui.lineEdit_PageHeight->setEnabled(true);
	ui.lineEdit_Scale->setEnabled(false);
}

void QgsMovingTargetMappingDialog::SetMapScaleRadioButton()
{
	ui.comboBox_PageDirection->setEnabled(false);
	ui.comboBox_PageSize->setEnabled(false);
	ui.lineEdit_PageWidth->setEnabled(false);
	ui.lineEdit_PageHeight->setEnabled(false);
	ui.lineEdit_Scale->setEnabled(true);
}


/*根据两点确定的直线计算点坐标Y*/
void QgsMovingTargetMappingDialog::CalCoordYByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputX, double& dOutputY)
{
	// 如果两点Y坐标相同
	if (fabs(dPoint1.dy - dPoint2.dy) <= IMAGE_MAPPING_ZERO)
	{
		dOutputY = dPoint1.dy;
	}
	// 如果两点Y坐标不同，X坐标相同（垂直）
	else if (fabs(dPoint1.dy - dPoint2.dy) > IMAGE_MAPPING_ZERO && fabs(dPoint1.dx - dPoint2.dx) < IMAGE_MAPPING_ZERO)
	{
		dOutputY = dPoint1.dy;
	}
	// 如果两点Y坐标不同，X坐标不同
	else if (fabs(dPoint1.dy - dPoint2.dy) > IMAGE_MAPPING_ZERO && fabs(dPoint1.dx - dPoint2.dx) > IMAGE_MAPPING_ZERO)
	{
		dOutputY = (dInputX - dPoint2.dx) * (dPoint1.dy - dPoint2.dy) / (dPoint1.dx - dPoint2.dx) + dPoint2.dy;
	}
}

/*根据两点确定的直线计算点坐标X*/
void QgsMovingTargetMappingDialog::CalCoordXByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputY, double& dOutputX)
{
	// 如果两点Y坐标相同
	if (fabs(dPoint1.dy - dPoint2.dy) <= IMAGE_MAPPING_ZERO)
	{
		dOutputX = dPoint1.dx;
	}
	// 如果两点Y坐标不同，X坐标相同（垂直）
	else if (fabs(dPoint1.dy - dPoint2.dy) > IMAGE_MAPPING_ZERO && fabs(dPoint1.dx - dPoint2.dx) < IMAGE_MAPPING_ZERO)
	{
		dOutputX = dPoint1.dx;
	}
	// 如果两点Y坐标不同，X坐标不同
	else if (fabs(dPoint1.dy - dPoint2.dy) > IMAGE_MAPPING_ZERO && fabs(dPoint1.dx - dPoint2.dx) > IMAGE_MAPPING_ZERO)
	{
		dOutputX = (dInputY - dPoint2.dy) * (dPoint1.dx - dPoint2.dx) / (dPoint1.dy - dPoint2.dy) + dPoint2.dx;
	}
}

void QgsMovingTargetMappingDialog::CalGaussBorderCoords_A_B(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// X方向起始整公里数，向上取整
	int iStartKm_X = ceil(dBeginPoint.dx / 1000.0);

	// X方向终止整公里数
	int iEndKm_X = int(dEndPoint.dx / 1000.0);

	for (int i = iStartKm_X; i <= iEndKm_X; i++)
	{
		SE_DPoint dPoint;
		dPoint.dx = i * 1000.0;
		CalCoordYByTwoPoint(dBeginPoint, dEndPoint, dPoint.dx, dPoint.dy);
		vGaussBorderCoords.push_back(dPoint);
	}
}


void QgsMovingTargetMappingDialog::CalGaussBorderCoords_C_B(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// Y方向起始整公里数，向上取整
	int iStartKm_Y = ceil(dBeginPoint.dy / 1000.0);

	// Y方向终止整公里数
	int iEndKm_Y = int(dEndPoint.dy / 1000.0);

	for (int i = iStartKm_Y; i <= iEndKm_Y; i++)
	{
		SE_DPoint dPoint;
		dPoint.dy = i * 1000.0;
		CalCoordXByTwoPoint(dBeginPoint, dEndPoint, dPoint.dy, dPoint.dx);
		vGaussBorderCoords.push_back(dPoint);
	}
}


void QgsMovingTargetMappingDialog::CalGaussBorderCoords_D_C(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// X方向起始整公里数，向上取整
	int iStartKm_X = ceil(dBeginPoint.dx / 1000.0);

	// X方向终止整公里数
	int iEndKm_X = int(dEndPoint.dx / 1000.0);

	for (int i = iStartKm_X; i <= iEndKm_X; i++)
	{
		SE_DPoint dPoint;
		dPoint.dx = i * 1000.0;
		CalCoordYByTwoPoint(dBeginPoint, dEndPoint, dPoint.dx, dPoint.dy);
		vGaussBorderCoords.push_back(dPoint);
	}
}


void QgsMovingTargetMappingDialog::CalGaussBorderCoords_D_A(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	vector<SE_DPoint>& vGaussBorderCoords)
{
	vGaussBorderCoords.clear();

	// Y方向起始整公里数，向上取整
	int iStartKm_Y = ceil(dBeginPoint.dy / 1000.0);

	// Y方向终止整公里数
	int iEndKm_Y = int(dEndPoint.dy / 1000.0);

	for (int i = iStartKm_Y; i <= iEndKm_Y; i++)
	{
		SE_DPoint dPoint;
		dPoint.dy = i * 1000.0;
		CalCoordXByTwoPoint(dBeginPoint, dEndPoint, dPoint.dy, dPoint.dx);
		vGaussBorderCoords.push_back(dPoint);
	}
}



int QgsMovingTargetMappingDialog::ClockWise(vector<SE_DPoint>& vPoints)
{
	int i, j, k;
	int count = 0;
	double z;

	if (vPoints.size() < 3)
		return 0;

	int n = vPoints.size();

	for (i = 0; i < n; i++) {
		j = (i + 1) % n;
		k = (i + 2) % n;
		z = (vPoints[j].dx - vPoints[i].dx) * (vPoints[k].dy - vPoints[j].dy);
		z -= (vPoints[j].dy - vPoints[i].dy) * (vPoints[k].dx - vPoints[j].dx);
		if (z < 0)
			count--;
		else if (z > 0)
			count++;
	}

	if (count > 0)			// 逆时针
		return COUNTERCLOCKWISE;
	else if (count < 0)		// 顺时针
		return CLOCKWISE;
	else
		return 0;
}


SE_DPoint QgsMovingTargetMappingDialog::LineIntersect(SE_DPoint p0,SE_DPoint p1,SE_DPoint p2,SE_DPoint p3)
{
	SE_DPoint dIntersectPoint;
	double A1 = p1.dy - p0.dy;
	double B1 = p0.dx - p1.dx;
	double C1 = A1 * p0.dx + B1 * p0.dy;
	double A2 = p3.dy - p2.dy;
	double B2 = p2.dx - p3.dx;
	double C2 = A2 * p2.dx + B2 * p2.dy;
	double denominator = A1 * B2 - A2 * B1;

	if (denominator == 0)
	{
		return dIntersectPoint;
	}

	// 对于外图廓四个多边形结点，此处两条线不可能平行
	
	dIntersectPoint.dx = (B2 * C1 - B1 * C2) / denominator;
	dIntersectPoint.dy = (A1 * C2 - A2 * C1) / denominator;
	return dIntersectPoint;
}

int QgsMovingTargetMappingDialog::CalIndexInVector_XCoord(double dx, vector<SE_DPoint>& vPoints)
{
	for (int i = 0; i < vPoints.size(); i++)
	{
		if (fabs(dx - vPoints[i].dx) < IMAGE_MAPPING_ZERO)
		{
			return i;
		}
	}

	return -1;
}

int QgsMovingTargetMappingDialog::CalIndexInVector_YCoord(double dy, vector<SE_DPoint>& vPoints)
{
	for (int i = 0; i < vPoints.size(); i++)
	{
		if (fabs(dy - vPoints[i].dy) < IMAGE_MAPPING_ZERO)
		{
			return i;
		}
	}

	return -1;
}

void QgsMovingTargetMappingDialog::TransformLayoutToGauss(SE_DPoint dRefGaussPoint, 
	SE_DPoint dRefLayoutPoint, 
	SE_DPoint dLayoutPoint, 
	double dMapUnitToLayoutUnit, 
	SE_DPoint& dGaussPoint)
{
	// 高斯横坐标X
	dGaussPoint.dx = dRefGaussPoint.dx + (dLayoutPoint.dx - dRefLayoutPoint.dx) / dMapUnitToLayoutUnit;

	// 高斯纵坐标Y
	dGaussPoint.dy = dRefGaussPoint.dy - (dLayoutPoint.dy - dRefLayoutPoint.dy) / dMapUnitToLayoutUnit;

}

void QgsMovingTargetMappingDialog::CalEndPointByStartPointAndAngle(
	SE_DPoint dStartPoint, 
	double dLength, 
	double dAngle, 
	SE_DPoint& dEndPoint)
{
	// 角度转换为弧度
	dAngle *= DEGREE_2_RADIAN;
	if (dAngle >= 0)
	{
		dEndPoint.dx = dStartPoint.dx + dLength * sin(fabs(dAngle));
		dEndPoint.dy = dStartPoint.dy - dLength * cos(fabs(dAngle));
	}
	else
	{
		dEndPoint.dx = dStartPoint.dx - dLength * sin(fabs(dAngle));
		dEndPoint.dy = dStartPoint.dy - dLength * cos(fabs(dAngle));
	}

}

void QgsMovingTargetMappingDialog::OpenTracePointData()
{
	// 选择文件夹
	QString curPath = m_qstrTracePointDataPath;
	QString fileName = QFileDialog::getOpenFileName(this, tr("加载轨迹点数据"), curPath, "*.shp *.SHP");
	if (fileName.isEmpty()) //如果文件未选择则返回
	{
		QgsMessageLog::logMessage(tr("请加载轨迹点图层！"), tr("动目标专题图制作"), Qgis::Warning);
		return;
	}
	ui.lineEdit_TracePointVectorDataPath->setText(fileName);
	m_qstrTracePointDataPath = fileName;
}

void QgsMovingTargetMappingDialog::OpenTraceLineData()
{
	// 选择文件夹
	QString curPath = m_qstrTraceLineDataPath; 
	QString fileName = QFileDialog::getOpenFileName(this, tr("加载轨迹点数据"), curPath, "*.shp *.SHP");
	if (fileName.isEmpty()) //如果文件未选择则返回
	{
		QgsMessageLog::logMessage(tr("请加载轨迹点图层！"), tr("动目标专题图制作"), Qgis::Warning);
		return;
	}
	ui.lineEdit_TraceLineDataPath->setText(fileName);
	m_qstrTraceLineDataPath = fileName;
}

void QgsMovingTargetMappingDialog::MovingTargetMappingDataSave()
{
	// 选择文件夹
	QString curPath = m_qstrMappingDataSavePath; 
	QString dlgTile = QObject::tr("请选择数据保存路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputDataPath->setText(selectedDir);
		m_qstrMappingDataSavePath = selectedDir;
	}
}


// 判断目录是否存在
bool QgsMovingTargetMappingDialog::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}


// 判断文件是否存在
bool QgsMovingTargetMappingDialog::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}

// 输入框限制
void QgsMovingTargetMappingDialog::InitLineEdit()
{
	// 图名——限制特殊字符
	ui.lineEdit_MapName->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

	// 制图单位——限制特殊字符
	ui.lineEdit_MapAgency->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

	// 纸张宽度
	ui.lineEdit_PageWidth->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

	// 纸张高度
	ui.lineEdit_PageHeight->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

	// 经纬网间隔
	QDoubleValidator* qDoubleVal_GeoGrid = new QDoubleValidator();
	qDoubleVal_GeoGrid->setRange(0.01, 10, 6);
	qDoubleVal_GeoGrid->setNotation(QDoubleValidator::StandardNotation);
	ui.lineEdit_GeoGridInterval->setValidator(qDoubleVal_GeoGrid);

	// 比例尺
	ui.lineEdit_Scale->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

}



void QgsMovingTargetMappingDialog::WebMercator_BL_TO_XY(double B, double L, double& X, double& Y)
{
	// L、B坐标单位为：度
	double lon = L;
	double lat = B;
	X = lon * 20037508.34 / 180;
	Y = log(tan((90 + lat) * PAI / 360.0)) / (PAI / 180.0);
	Y = Y * 20037508.34 / 180.0;
}

void QgsMovingTargetMappingDialog::WebMercator_XY_TO_BL(double X, double Y, double& B, double& L)
{
	// L、B坐标单位为：度
	L = (X / 20037508.34) * 180.0;
	B = (Y / 20037508.34) * 180.0;
	B = 180.0 / PAI * (2 * atan(exp(B * PAI / 180)) - PAI / 2);
}

QString QgsMovingTargetMappingDialog::FormatLonDegree(int iDegree)
{
	QString qstrDegree;
	if (iDegree == 0)
	{
		qstrDegree = tr("0°");
	}
	else if (iDegree > 0)
	{
		qstrDegree = tr("%1°E").arg(iDegree);
	}
	else
	{
		qstrDegree = tr("%1°W").arg(abs(iDegree));
	}

	return qstrDegree;
}

QString QgsMovingTargetMappingDialog::FormatLatDegree(int iDegree)
{
	QString qstrDegree;
	if (iDegree == 0)
	{
		qstrDegree = tr("0°");
	}
	else if (iDegree > 0)
	{
		qstrDegree = tr("%1°N").arg(iDegree);
	}
	else
	{
		qstrDegree = tr("%1°S").arg(abs(iDegree));
	}

	return qstrDegree;
}


QString QgsMovingTargetMappingDialog::FormatLonDegree_Double(double dDegree)
{
	QString qstrDegree;
	if (dDegree == 0)
	{
		qstrDegree = tr("0°");
	}
	else if (dDegree > 0)
	{
		qstrDegree = tr("%1°E").arg(dDegree);
	}
	else
	{
		qstrDegree = tr("%1°W").arg(fabs(dDegree));
	}

	return qstrDegree;
}

QString QgsMovingTargetMappingDialog::FormatLatDegree_Double(double dDegree)
{
	QString qstrDegree;
	if (dDegree == 0)
	{
		qstrDegree = tr("0°");
	}
	else if (dDegree > 0)
	{
		qstrDegree = tr("%1°N").arg(dDegree);
	}
	else
	{
		qstrDegree = tr("%1°S").arg(fabs(dDegree));
	}

	return qstrDegree;
}



void QgsMovingTargetMappingDialog::CalGeoBorderCoords_A_B(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	double dLonInterval,
	vector<SE_DPoint>& vGeoBorderCoords)
{
	vGeoBorderCoords.clear();

	// X方向起始整间隔数，向上取整
	int iStartGeo_X = ceil(dBeginPoint.dx / dLonInterval);

	// X方向终止整间隔数
	int iEndGeo_X = int(dEndPoint.dx / dLonInterval);

	for (int i = iStartGeo_X; i <= iEndGeo_X; i++)
	{
		SE_DPoint dPoint;
		dPoint.dx = i * dLonInterval;
		dPoint.dy = dBeginPoint.dy;
		vGeoBorderCoords.push_back(dPoint);
	}
}


void QgsMovingTargetMappingDialog::CalGeoBorderCoords_C_B(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	double dLatInterval,
	vector<SE_DPoint>& vGeoBorderCoords)
{
	vGeoBorderCoords.clear();

	// Y方向起始整数纬度间隔，向上取整
	int iStartGeo_Y = ceil(dBeginPoint.dy / dLatInterval);

	// Y方向终止整数纬度间隔
	int iEndGeo_Y = int(dEndPoint.dy / dLatInterval);

	for (int i = iStartGeo_Y; i <= iEndGeo_Y; i++)
	{
		SE_DPoint dPoint;
		dPoint.dy = i * dLatInterval;
		dPoint.dx = dBeginPoint.dx;
		vGeoBorderCoords.push_back(dPoint);
	}
}


void QgsMovingTargetMappingDialog::CalGeoBorderCoords_D_C(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	double dLonInterval,
	vector<SE_DPoint>& vGeoBorderCoords)
{
	vGeoBorderCoords.clear();

	// X方向起始经度间隔整数倍，向上取整
	int iStartGeo_X = ceil(dBeginPoint.dx / dLonInterval);

	// X方向终止纬度间隔整数倍
	int iEndGeo_X = int(dEndPoint.dx / dLonInterval);

	for (int i = iStartGeo_X; i <= iEndGeo_X; i++)
	{
		SE_DPoint dPoint;
		dPoint.dx = i * dLonInterval;
		dPoint.dy = dBeginPoint.dy;
		vGeoBorderCoords.push_back(dPoint);
	}
}


void QgsMovingTargetMappingDialog::CalGeoBorderCoords_D_A(SE_DPoint dBeginPoint,
	SE_DPoint dEndPoint,
	double dLatInterval,
	vector<SE_DPoint>& vGeoBorderCoords)
{
	vGeoBorderCoords.clear();

	// Y方向起始纬度间隔整数倍，向上取整
	int iStartGeo_Y = ceil(dBeginPoint.dy / dLatInterval);

	// Y方向终止纬度间隔整数倍
	int iEndGeo_Y = int(dEndPoint.dy / dLatInterval);

	for (int i = iStartGeo_Y; i <= iEndGeo_Y; i++)
	{
		SE_DPoint dPoint;
		dPoint.dy = i * dLatInterval;
		dPoint.dx = dBeginPoint.dx;
		vGeoBorderCoords.push_back(dPoint);
	}
}


void QgsMovingTargetMappingDialog::TransformProjectToLayout(SE_DPoint dRefProjectPoint, SE_DPoint dRefLayoutPoint, SE_DPoint dProjPoint, double dMapUnitToLayoutUnit, SE_DPoint& dLayoutPoint)
{
	dLayoutPoint.dx = dRefLayoutPoint.dx + (dProjPoint.dx - dRefProjectPoint.dx) * dMapUnitToLayoutUnit;
	dLayoutPoint.dy = dRefLayoutPoint.dy + (dRefProjectPoint.dy - dProjPoint.dy) * dMapUnitToLayoutUnit;
}


int QgsMovingTargetMappingDialog::Get_Point(OGRFeature* poFeature, SE_DPoint& coordinate, vector<FieldNameAndValue>& vFieldvalue)
{
	if (!poFeature) {
		return -1;
	}
	OGRGeometry* poGeometry = poFeature->GetGeometryRef();

	if (!poGeometry)
	{
		// 跳过
		return -1;
	}

	//将几何结构转换成点类型
	OGRPoint* poPoint = (OGRPoint*)poGeometry;
	coordinate.dx = poPoint->getX();
	coordinate.dy = poPoint->getY();
	coordinate.dz = poPoint->getZ();
	for (int iField = 0; iField < poFeature->GetFieldCount(); iField++)
	{
		OGRFieldDefn* pField = poFeature->GetFieldDefnRef(iField);
		string strFieldName = pField->GetNameRef();
		string strValue = poFeature->GetFieldAsString(iField);

		FieldNameAndValue structFieldValue;
		structFieldValue.strFieldName = strFieldName;
		structFieldValue.strFieldValue = strValue;
		vFieldvalue.push_back(structFieldValue);
	}
	return 0;
}


void QgsMovingTargetMappingDialog::GetLayerFields(OGRLayer* pLayer,
	vector<string>& vFieldname,
	vector<OGRFieldType>& vFieldtype,
	vector<int>& vFieldwidth,
	vector<int>& vFieldPrecision)
{
	/*获取图层的属性表结构*/
	OGRFeatureDefn* pFeatureDefn = pLayer->GetLayerDefn();
	int iFieldCount = pFeatureDefn->GetFieldCount();
	for (int iAttr = 0; iAttr < iFieldCount; iAttr++)
	{
		OGRFieldDefn* pField = pFeatureDefn->GetFieldDefn(iAttr);

		vFieldname.push_back(pField->GetNameRef());
		vFieldtype.push_back(pField->GetType());
		vFieldwidth.push_back(pField->GetWidth());
		vFieldPrecision.push_back(pField->GetPrecision());	
	}
}


int QgsMovingTargetMappingDialog::SetFieldDefn(OGRLayer* poLayer, 
	vector<string> fieldname, 
	vector<OGRFieldType> fieldtype, 
	vector<int> fieldwidth,
	vector<int> fieldPrecision)
{
	OGRFieldDefn* pField = nullptr;
	for (int i = 0; i < fieldname.size(); i++)
	{
		pField = new OGRFieldDefn(fieldname[i].c_str(), fieldtype[i]);
		if (!pField)
		{
			continue;
		}
		if (fieldwidth[i] == 0)
		{
			fieldwidth[i] = 10;
		}

		// 设置字段宽度及精度，实际操作需要根据不同字段设置不同长度
		pField->SetWidth(fieldwidth[i]);	
		pField->SetPrecision(fieldPrecision[i]);
		OGRErr err = poLayer->CreateField(pField);

		if (err != OGRERR_NONE)
		{
			printf("CreateField failed!\n");
			return -1;
		}
	}
	return 0;
}


int QgsMovingTargetMappingDialog::Set_Point(OGRLayer* poLayer, double x, double y, double z, vector<FieldNameAndValue>& vFieldAndValue)
{
	OGRFeature* poFeature;
	if (poLayer == NULL)
		return -1;
	poFeature = OGRFeature::CreateFeature(poLayer->GetLayerDefn());
	// 根据提供的字段值vector对相应字段赋值
	for (int i = 0; i < vFieldAndValue.size(); i++)
	{
		poFeature->SetField(vFieldAndValue[i].strFieldName.c_str(), vFieldAndValue[i].strFieldValue.c_str());
	}

	OGRPoint point;
	point.setX(x);
	point.setY(y);
	point.setZ(z);
	poFeature->SetGeometry((OGRGeometry*)(&point));

	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);

	return 0;
}




bool QgsMovingTargetMappingDialog::CreateShapefileCPG(string strCPGFilePath, string strEncoding /*= "System"*/)
{
	FILE* fp = fopen(strCPGFilePath.c_str(), "w");
	if (!fp)
	{
		return false;
	}

	fprintf(fp, "%s", strEncoding.c_str());

	fclose(fp);

	return true;
}
