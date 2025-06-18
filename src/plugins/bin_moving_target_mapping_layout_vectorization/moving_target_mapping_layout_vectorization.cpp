#pragma region "1、包含头文件（减少重复）"
#include "qgsgui.h"
#include "moving_target_mapping_layout_vectorization.h"
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
// 杨小兵-2024-09-06：为了WX系统集成重新集成的【动目标制图】功能，因为在windows上需要包含direct.h头文件才能使用_mkdir函数
#include <direct.h>
#include <filesystem>

/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"
/*-----------------------------------*/

#include "symbology/qgscategorizedsymbolrenderer.h"
#include "qgis.h"
#include "qgsattributes.h"

#include <qgsfillsymbollayer.h>
#include <qgsfillsymbol.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include "qgsmarkersymbollayer.h"

#include <qvalidator.h>
#include <time.h>

#define CLOCKWISE 1
#define COUNTERCLOCKWISE -1

using namespace std;
#pragma endregion


#pragma region "2、全局变量"
/*默认导出地图DPI*/
const int LAYOUT_DPI = 300;
const double OUTER_MAP_INTERIOR_MAP_DISTANCE = 14;       // 内外图廓宽度，单位：毫米
const double KM_LINE_LENGTH = 6;            // 内外图廓方里网刻度线长度，单位：毫米
const double ADJ_KM_LINE_LENGTH = 2;        // 邻带方里网刻度线长度，单位：毫米
const double KM_LINE_TOLERANCE_X = 13;       // X方向方里网刻度线与角点容差，单位：毫米，避免与角点经度绘制重叠
const double KM_LINE_TOLERANCE_Y = 8;       // Y方向方里网刻度线与角点容差，单位：毫米，避免与角点纬度绘制重叠
const double IMAGE_MAPPING_ZERO = 1.0e-3;	// 判断高斯坐标和布局坐标相等容差
const double LEGEND_OFFSET_X = 6;			// 主地图图例左边界横坐标相对地图外图廓右边界距离，单位：毫米
const double MAP_ANNOTATION_OFFSET_X = 6;	// 主地图附注左边界横坐标相对地图外图廓右边界距离，单位：毫米
const double LEGEND_PICTURE_LABEL_DISTANCE = 18;		// 图例图片左边界与图例文本左边界距离，单位：毫米


const double MOSAIC_MAP_WIDTH = 80;			// 鹰眼图宽度，单位：毫米
const double MOSAIC_MAP_HEIGHT = 55;		// 鹰眼图高度，单位：毫米
const double MOSAIC_MAP_LEFT_X = 60;		// 鹰眼图窗口左边界布局坐标，单位：毫米

#pragma endregion


QgsMovingTargetMappingLayoutVectorizationDialog::QgsMovingTargetMappingLayoutVectorizationDialog(QWidget* parent, Qt::WindowFlags fl)
    : QDialog(parent, fl)
{
    ui.setupUi(this);
    QgsGui::enableAutoGeometryRestore(this);

    // 动目标制图
    connect(ui.pushButton_Mapping, &QPushButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::MovingTargetMapping);

    // 关闭
    connect(ui.pushButton_Close, &QPushButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::Close);

    // 打开轨迹点数据
    connect(ui.pushButton_OpenTracePointData, &QPushButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::OpenTracePointData);

    // 打开影像数据
    connect(ui.pushButton_OpenImageData, &QPushButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::OpenImageData);

    // 打开轨迹线数据
    connect(ui.pushButton_OpenTraceLineData, &QPushButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::OpenTraceLineData);

    // 打开鹰眼图数据
    connect(ui.pushButton_OpenEagleEyeData, &QPushButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::OpenEagleEyeData);

    // 保存地图
    connect(ui.pushButton_Save, &QPushButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::MovingTargetMappingDataSave);

    // 页面尺寸下拉选择
    // 隐藏地图尺寸设置
    //connect(ui.comboBox_PageSize, SIGNAL(currentIndexChanged(int)), this, SLOT(ComboPageSizeSelectedChanged()));

    // 纸张方向下拉选择
    //connect(ui.comboBox_PageDirection, SIGNAL(currentIndexChanged(int)), this, SLOT(ComboPageDirectionSelectedChanged()));

    m_qstrTracePointDataPath = "";
    m_qstrTraceLineDataPath = "";
    m_qstrImageDataPath = "";
    m_qstrMappingDataSavePath = "";
    m_qstrEagleEyeDataPath = "";

    restoreState();
    ui.lineEdit_OutputDataPath->setText(m_qstrMappingDataSavePath);
    ui.lineEdit_TracePointVectorDataPath->setText(m_qstrTracePointDataPath);
    ui.lineEdit_TraceLineDataPath->setText(m_qstrTraceLineDataPath);
    ui.lineEdit_ImageDataPath->setText(m_qstrImageDataPath);
    ui.lineEdit_EagleEyeDataPath->setText(m_qstrEagleEyeDataPath);

    InitLineEdit();

    // 经线间隔
    //ui.lineEdit_GeoGridIntervalLon->setText(tr("0.5"));

    // 纬线间隔
    //ui.lineEdit_GeoGridIntervalLat->setText(tr("0.5"));

    // 设置界面初始化状态
    //ui.checkBox_MapAgency->setChecked(true);
    ui.checkBox_Scale->setChecked(true);
    ui.checkBox_NorthArrow->setChecked(true);
    //ui.checkBox_GeoGrid->setChecked(true);
    ui.radioButton_GeoPDF->setChecked(true);

    // 默认横向
    //ui.comboBox_PageDirection->setCurrentIndex(0);
    //m_iPageDirection = 1;

    // 默认尺寸自定义，单位：毫米
    /*ui.comboBox_PageSize->setCurrentIndex(0);
    ui.lineEdit_PageWidth->setText(tr("800"));
    ui.lineEdit_PageHeight->setText(tr("650"));*/

    // 默认比例尺是10000
    ui.lineEdit_Scale->setText(tr("10000"));

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


    //connect(ui.radioButton_SetSizeMode, &QRadioButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::SetMapSizeRadioButton);
    //connect(ui.radioButton_SetScaleMode, &QRadioButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::SetMapScaleRadioButton);
    connect(ui.radioButton_SimpleRender, &QRadioButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::SetSimpleRenderRadioButton);
    connect(ui.radioButton_CategoryRender, &QRadioButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::SetCategoryRenderRadioButton);

    connect(ui.pushButton_SetColor, &QPushButton::clicked, this, &QgsMovingTargetMappingLayoutVectorizationDialog::SetEagleEyeLayerColor);

    // 默认设置比例尺
    //ui.radioButton_SetSizeMode->setChecked(false);
    //ui.radioButton_SetScaleMode->setChecked(true);

    // 默认在右上角
    ui.radioButton_NorthArrowLeftTop->setChecked(false);
    ui.radioButton_NorthArrowRightTop->setChecked(true);

    /*ui.comboBox_PageDirection->setEnabled(false);
    ui.comboBox_PageSize->setEnabled(false);
    ui.lineEdit_PageWidth->setEnabled(false);
    ui.lineEdit_PageHeight->setEnabled(false);*/
    ui.lineEdit_Scale->setEnabled(true);

    // 默认是内部
    ui.radioButton_NeiBu->setChecked(true);
    ui.radioButton_GongKai->setChecked(false);
    ui.radioButton_MiMi->setChecked(false);
    ui.radioButton_JiMi->setChecked(false);

    // 默认☑图例
    ui.checkBox_MapLegend->setChecked(true);

    // 设置鹰眼图默认渲染字段
    ui.lineEdit_RenderField->setText("NAME_ZH");

    // 默认☑鹰眼图
    ui.checkBox_EagleEye->setChecked(true);

    // 默认颜色
    m_qEagleEyeLayerColor = QColor(236, 235, 208);
    m_bSetEagleEyeLayerColor = false;

    // 默认鹰眼图单色渲染
    ui.radioButton_SimpleRender->setChecked(true);
    ui.pushButton_SetColor->setEnabled(true);

    QgsSettings settings;
    settings.setValue(QStringLiteral("svg/searchPathsForSVG"), QStringLiteral("G:/starmap_package/TJ_20240826/TJ/bin/svg"), QgsSettings::Section::NoSection);
}

QgsMovingTargetMappingLayoutVectorizationDialog::~QgsMovingTargetMappingLayoutVectorizationDialog()
{
    // 设置当前保存路径
    QgsSettings settings;

    // 地图输出路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/DataSavePath"), m_qstrMappingDataSavePath, QgsSettings::Section::Plugins);


    // 轨迹点数据文件路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/TracePointDataPath"), m_qstrTracePointDataPath, QgsSettings::Section::Plugins);

    // 轨迹线数据文件路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/TraceLineDataPath"), m_qstrTraceLineDataPath, QgsSettings::Section::Plugins);

    // 影像图数据文件路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/ImageDataPath"), m_qstrImageDataPath, QgsSettings::Section::Plugins);

    // 鹰眼图数据文件路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/EagleEyeDataPath"), m_qstrEagleEyeDataPath, QgsSettings::Section::Plugins);

}

void QgsMovingTargetMappingLayoutVectorizationDialog::Close()
{
    // 设置当前保存路径
    QgsSettings settings;

    // 地图输出路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/DataSavePath"), m_qstrMappingDataSavePath, QgsSettings::Section::Plugins);


    // 轨迹点数据文件路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/TracePointDataPath"), m_qstrTracePointDataPath, QgsSettings::Section::Plugins);

    // 轨迹线数据文件路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/TraceLineDataPath"), m_qstrTraceLineDataPath, QgsSettings::Section::Plugins);

    // 影像图数据文件路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/ImageDataPath"), m_qstrImageDataPath, QgsSettings::Section::Plugins);

    // 鹰眼图数据文件路径
    settings.setValue(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/EagleEyeDataPath"), m_qstrEagleEyeDataPath, QgsSettings::Section::Plugins);

    accept();
}

void QgsMovingTargetMappingLayoutVectorizationDialog::OpenImageData()
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
    m_qstrImageDataPath = filename.toUtf8();

}

QgsLayoutItem* QgsMovingTargetMappingLayoutVectorizationDialog::GetLayoutItemByName(QList<QgsLayoutItem*>& itemList, const QString qstrName)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::GetTracePointDataPath()
{
    return m_qstrTracePointDataPath;
}

QString QgsMovingTargetMappingLayoutVectorizationDialog::GetImageDataPath()
{
    return m_qstrImageDataPath;
}

QString QgsMovingTargetMappingLayoutVectorizationDialog::GetEagleEyeDataPath()
{
    return m_qstrEagleEyeDataPath;
}

QString QgsMovingTargetMappingLayoutVectorizationDialog::GetTraceLineDataPath()
{
    return m_qstrTraceLineDataPath;
}

QString QgsMovingTargetMappingLayoutVectorizationDialog::GetDataSavePath()
{
    return m_qstrMappingDataSavePath;
}

QString QgsMovingTargetMappingLayoutVectorizationDialog::GetMapExportType()
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::GetMapName()
{
    QString qstrMapName = ui.lineEdit_MapName->text();
    return qstrMapName;
}



QString QgsMovingTargetMappingLayoutVectorizationDialog::GetMapDetail()
{
    QString qstrMapDetail = ui.textEdit_MapDetail->toPlainText();
    return qstrMapDetail;
}

QString QgsMovingTargetMappingLayoutVectorizationDialog::GetMovingTargetDetail()
{
    QString qstrMovingTargetDetail = ui.textEdit_MovingTargetDetail->toPlainText();
    return qstrMovingTargetDetail;
}


QString QgsMovingTargetMappingLayoutVectorizationDialog::GetMapAgency()
{
    /*QString qstrMapAgency = ui.lineEdit_MapAgency->text();
    return qstrMapAgency;*/
    return "";
}

bool QgsMovingTargetMappingLayoutVectorizationDialog::GetDrawMapAgencyState()
{
    //bool bChecked = ui.checkBox_MapAgency->isChecked();
    //return bChecked;
    return false;
}

bool QgsMovingTargetMappingLayoutVectorizationDialog::GetDrawScaleState()
{
    bool bChecked = ui.checkBox_Scale->isChecked();
    return bChecked;
}

bool QgsMovingTargetMappingLayoutVectorizationDialog::GetDrawNorthArrowState()
{
    bool bChecked = ui.checkBox_NorthArrow->isChecked();
    return bChecked;
}

//bool QgsMovingTargetMappingLayoutVectorizationDialog::GetDrawGeoGridState()
//{
//    bool bChecked = ui.checkBox_GeoGrid->isChecked();
//    return bChecked;
//}


//void QgsMovingTargetMappingLayoutVectorizationDialog::GetGeoGridInterval(double& dLonInterval, double& dLatInterval)
//{
//    dLonInterval = ui.lineEdit_GeoGridIntervalLon->text().toDouble();
//    dLatInterval = ui.lineEdit_GeoGridIntervalLat->text().toDouble();
//}

void QgsMovingTargetMappingLayoutVectorizationDialog::GetMapSize(double& dWidth, double& dHeight)
{
    //dWidth = ui.lineEdit_PageWidth->text().toDouble();
    //dHeight = ui.lineEdit_PageHeight->text().toDouble();
}



bool QgsMovingTargetMappingLayoutVectorizationDialog::ExportMapToFile(
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

        auto proxyTask = new QgsProxyProgressTask(tr("导出 “%1”").arg(layout->name()));
        QgsApplication::taskManager()->addTask(proxyTask);

        layout->refresh();

        QgsLayoutExporter exporter(layout);
        result = exporter.exportToImage(filePath, settings);
        proxyTask->finalize(result == QgsLayoutExporter::Success);
        proxyTask->deleteLater();
    }

    // 如果导出GeoPDF文件
    else if (qstrExportType == "GeoPDF")
    {
        // 导出pdf设置
        QgsLayoutExporter::PdfExportSettings settings;
        settings.writeGeoPdf = true;
        settings.forceVectorOutput = true;
        settings.appendGeoreference = true;
        settings.exportMetadata = true;

        settings.useIso32000ExtensionFormatGeoreferencing = true;

        settings.dpi = LAYOUT_DPI;

        
        auto proxyTask = new QgsProxyProgressTask(tr("导出 “%1”").arg(layout->name()));
        QgsApplication::taskManager()->addTask(proxyTask);

        // force a refresh, to e.g. update data defined properties, tables, etc
        layout->refresh();

        QgsLayoutExporter exporter(layout);

        // 导出pdf
        result = exporter.exportToPdf(filePath, settings);

        proxyTask->finalize(result == QgsLayoutExporter::Success);
        proxyTask->deleteLater();
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

        
        auto proxyTask = new QgsProxyProgressTask(tr("导出 “%1”").arg(layout->name()));
        QgsApplication::taskManager()->addTask(proxyTask);

        // force a refresh, to e.g. update data defined properties, tables, etc
        layout->refresh();

        QgsLayoutExporter exporter(layout);

        // 导出pdf
        result = exporter.exportToPdf(filePath, settings);

        proxyTask->finalize(result == QgsLayoutExporter::Success);
        proxyTask->deleteLater();
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

void QgsMovingTargetMappingLayoutVectorizationDialog::ConvertMilliMeterToPixel(double w, double h, int dpi, int& x, int& y)
{
    // x,y 为像素数，w,h为毫米
    // 水平方向：x = w * dpi / 25.4
    // 垂直方向：y = h * dpi / 25.4
    x = w * dpi / 25.4;
    y = h * dpi / 25.4;
}

void QgsMovingTargetMappingLayoutVectorizationDialog::ConvertPixeltoMilliMeter(int x, int y, int dpi, double& w, double& h)
{
    // x, y 为像素数，w, h为毫米
    // 水平方向：w = x * 25.4 / dpi
    // 垂直方向：h = y * 25.4 / dpi

    w = x * 25.4 / dpi;
    h = y * 25.4 / dpi;
}

void QgsMovingTargetMappingLayoutVectorizationDialog::restoreState()
{
    const QgsSettings settings;
    m_qstrMappingDataSavePath = settings.value(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/DataSavePath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

    // 轨迹点数据文件路径
    m_qstrTracePointDataPath = settings.value(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/TracePointDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

    // 轨迹线数据文件路径
    m_qstrTraceLineDataPath = settings.value(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/TraceLineDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

    // 影像图数据文件路径
    m_qstrImageDataPath = settings.value(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/ImageDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

    // 鹰眼图数据文件路径
    m_qstrEagleEyeDataPath = settings.value(QStringLiteral("QgsMovingTargetMappingLayoutVectorizationDialog/EagleEyeDataPath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}

double QgsMovingTargetMappingLayoutVectorizationDialog::CalCenterMeridian(double dLong, double dScale)
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

void QgsMovingTargetMappingLayoutVectorizationDialog::GetMBR(int iCount, double* pPoints, double& dLeft, double& dTop, double& dRight, double& dBottom)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::GetEPSGCodeByCenterMeridianAndScale(double dLong, double dScale)
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

void QgsMovingTargetMappingLayoutVectorizationDialog::Degree2DMS(double degree, int& iDegree, int& iMinute, int& iSecond)
{
    iDegree = (int)degree;
    iMinute = (round)((degree - iDegree) * 60);
    iSecond = (round)(((degree - iDegree) * 60 - iMinute) * 60);
}

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatDegree(int iDegree)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatDegree2(int iDegree)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatMinute(int iMinute)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatMinute2(int iMinute)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatSecond(int iSecond)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatSecond2(int iSecond)
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







void QgsMovingTargetMappingLayoutVectorizationDialog::CalGaussBorderCoords(const SE_DRect& dRange,
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

void QgsMovingTargetMappingLayoutVectorizationDialog::CalLayoutBorderPoints(
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

int QgsMovingTargetMappingLayoutVectorizationDialog::CalBandNumber(double dCenterMeridian, double dScale)
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

void QgsMovingTargetMappingLayoutVectorizationDialog::TransformGaussToLayout(SE_DPoint dRefGaussPoint, SE_DPoint dRefLayoutPoint, SE_DPoint dGaussPoint, double dMapUnitToLayoutUnit, SE_DPoint& dLayoutPoint)
{
    dLayoutPoint.dx = dRefLayoutPoint.dx + (dGaussPoint.dx - dRefGaussPoint.dx) * dMapUnitToLayoutUnit;
    dLayoutPoint.dy = dRefLayoutPoint.dy + (dRefGaussPoint.dy - dGaussPoint.dy) * dMapUnitToLayoutUnit;
}

void QgsMovingTargetMappingLayoutVectorizationDialog::MovingTargetMapping()
{
#pragma region "1、界面输入参数检查"
    ////  检查纬度间隔是否为零
    //if (ui.lineEdit_GeoGridIntervalLon->text().toDouble() == 0)
    //{
    //    QMessageBox msgBox;
    //    msgBox.setWindowTitle(tr("动目标专题制图"));
    //    msgBox.setText(tr("经线间隔需大于0，请重新输入！"));
    //    msgBox.setStandardButtons(QMessageBox::Yes);
    //    msgBox.setDefaultButton(QMessageBox::Yes);
    //    msgBox.exec();
    //    return;
    //}
    ////  检查经度间隔是否为零
    //if (ui.lineEdit_GeoGridIntervalLat->text().toDouble() == 0)
    //{
    //    QMessageBox msgBox;
    //    msgBox.setWindowTitle(tr("动目标专题制图"));
    //    msgBox.setText(tr("纬线间隔需大于0，请重新输入！"));
    //    msgBox.setStandardButtons(QMessageBox::Yes);
    //    msgBox.setDefaultButton(QMessageBox::Yes);
    //    msgBox.exec();
    //    return;
    //}
    //  检查输出文件路径是否有效
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
    //  检查轨迹点图层文件是否存在
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
    //  检查轨迹线图层文件是否存在
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
    //  检查影像数据图层文件是否存在
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
#pragma endregion

#pragma region "2、设置绘制线样式"

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

#pragma region "3、获取用户界面参数"
    /*
    一、作用
      以地图外图左上角为参考点，获取基于地图外图廓的所有布局要素的相对参考点位置，用于自动调整影像图各元素位置。
    二、影像制图流程
    （1）根据轨迹线、轨迹点范围裁剪影像数据
    （2）设置地图页面尺寸，计算比例尺，根据比例尺大小确定采用投影坐标系或地理坐标系，小于1:100万使用地理坐标系
    （3）根据界面设置的整饰要素显示状态，设置布局元素item的显示状态
    （4）每个图幅输出工程文件、地图文件（pdf格式或tif格式）
    */

    ui.progressBar->setValue(0);

    //  1、获取输出地图类型（GeoPDF\PDF\tiff）
    QString qstrExportMapType = GetMapExportType();
    //  2、获取地图名称
    QString qstrMapName = GetMapName();
    //  3、是否绘制地图单位
    bool bDrawMapAgency = GetDrawMapAgencyState();
    //  4、获取制图单位
    QString qstrMapAgency = GetMapAgency();
    //  5、获取地图说明
    QString qstrMapDetail = GetMapDetail();
    //  6、获取动目标说明
    QString qstrMovingTargetDetail = GetMovingTargetDetail();
    //  7、是否绘制比例尺
    bool bDrawScale = GetDrawScaleState();
    //  8、是否绘制指北针
    bool bDrawNorthArrow = GetDrawNorthArrowState();
    //  9、是否绘制经纬网
    //bool bDrawGeoGrid = GetDrawGeoGridState();
    //  10、获取经纬网间隔
    //double dLonInterval = 0;
    //double dLatInterval = 0;
    //GetGeoGridInterval(dLonInterval, dLatInterval);
    //  11、获取地图尺寸，单位：毫米
    double dMapWidth = 0;
    double dMapHeight = 0;
    GetMapSize(dMapWidth, dMapHeight);
    // 12、获取专题图布局模板
    QString qstrLayoutTemplatePath = QCoreApplication::applicationDirPath() + "/layout/template/moving_target.qpt";

#pragma endregion

#pragma region "4、开始制图"

#pragma region "4.1、图层列表定义"

    //  获取工程实例
    bool bResult = false;
    QgsProject* pProject = QgsProject::instance();
    //  检查获取工程实例是否成功
    if (!pProject)
    {
        // 记录日志
        QgsMessageLog::logMessage(tr("工程实例获取异常"), tr("动目标专题制图"), Qgis::Critical);
        return;
    }
    //  清空当前工程的所有图层
    pProject->removeAllMapLayers();


    // 添加到工程中的地图图层列表
    QList<QgsMapLayer*> qMapLayerList;          
    qMapLayerList.clear();
    // 主地图加载图层列表
    QList<QgsMapLayer*> qMainMapLayerList;      
    qMainMapLayerList.clear();
    // 鹰眼地图加载图层列表，加载世界政区图
    QList<QgsMapLayer*> qEagleEyeMapLayerList;    
    qEagleEyeMapLayerList.clear();

#pragma endregion

#pragma region "4.2、加载矢量图层（轨迹点图层、轨迹点标注图层、轨迹线图层）"
    /*-------------------------------*/
    /*---------加载矢量图层----------*/
    /*-------------------------------*/
#pragma region "4.2.1、加载轨迹点图层"

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
        dTracePointLayerRect.dleft = qgsPointLayerRect.xMinimum() - qgsPointLayerRect.width() * 0.5;
        dTracePointLayerRect.dright = qgsPointLayerRect.xMaximum() + qgsPointLayerRect.width() * 0.5;
        dTracePointLayerRect.dtop = qgsPointLayerRect.yMaximum() + qgsPointLayerRect.height() * 0.5;
        dTracePointLayerRect.dbottom = qgsPointLayerRect.yMinimum() - qgsPointLayerRect.height() * 0.5;
        qMainMapLayerList.append(pAddedTracePointLayer);
        qMapLayerList.append(pAddedTracePointLayer);
    }

    //  杨小兵-2024-10-17：自动计算经纬线
    // 计算经度和纬度的总跨度
    double lon_span = dTracePointLayerRect.dright - dTracePointLayerRect.dleft;
    double lat_span = dTracePointLayerRect.dtop - dTracePointLayerRect.dbottom;

    // 定义一个辅助函数来计算“友好的”间隔
    auto CalculateNiceInterval = [](double span) -> double {
      // 目标是生成大约3条网格线
      double roughInterval = span / 3.0;
      double exponent = floor(log10(roughInterval));
      double fraction = roughInterval / pow(10.0, exponent);
      double niceFraction;

      if (fraction <= 1.0)
        niceFraction = 1.0;
      else if (fraction <= 2.0)
        niceFraction = 2.0;
      else if (fraction <= 5.0)
        niceFraction = 5.0;
      else
        niceFraction = 10.0;

      return niceFraction * pow(10.0, exponent);
    };

    // 计算经纬网间隔
    //double dLonInterval = CalculateNiceInterval(lon_span);
    //double dLatInterval = CalculateNiceInterval(lat_span);

    // 计算经纬网间隔，使得增加两条经线和两条纬线，将区域均分为三部分
    double dLonInterval = lon_span / 3.0;
    double dLatInterval = lat_span / 3.0;

#pragma endregion

#pragma region "4.2.2、判断是否存在轨迹点标注图层，如果已存在，则直接加载；如果不存在，则复制轨迹点层"

    //  轨迹点标注图层默认不存在
    bool bTracePointLabelLayerExisted = false;

    //  获取输入轨迹点图层所在目录，轨迹点标注层也生成在此目录下
    string strTracePointLayerFileNameTemp = qstrTracePointLayerFilePath.toUtf8().toStdString();
    string strInputTracePointPathTemp = CPLGetDirname(strTracePointLayerFileNameTemp.c_str());

    //  构造轨迹点标注图层路径
    string strLabelPointShpFilePathTemp = strInputTracePointPathTemp + "/trace_point_label.shp";
    //  构建轨迹点标注图层路径（string--->qstring）
    QString qstrLabelPointShpFilePathTemp = QString::fromUtf8(strLabelPointShpFilePathTemp.c_str());

    //  检查构造出来的文件是否存在
    if (QFileInfo::exists(qstrLabelPointShpFilePathTemp))
    {
        bTracePointLabelLayerExisted = true;
    }

    //  如果不存在轨迹点标注图层trace_point_label.shp，则需要创建轨迹点标注图层
    if (!bTracePointLabelLayerExisted)
    {
        //  获取驱动
        CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
        CPLSetConfigOption("SHAPE_ENCODING", "");
        GDALAllRegister();

        //  获取shp驱动
        const char* pszShpDriverName = "ESRI Shapefile";
        GDALDriver* poShpDriver;
        poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
        if (poShpDriver == NULL)
        {
            QgsMessageLog::logMessage(tr("获取ESRI Shapefile驱动失败！"), tr("动目标专题制图"), Qgis::Critical);
            return;
        }

        //  创建数据集
        string strTracePointLayerFileName = qstrTracePointLayerFilePath.toUtf8().toStdString();
        GDALDataset* poDS = (GDALDataset*)GDALOpenEx(strTracePointLayerFileName.c_str(), GDAL_OF_ALL, NULL, NULL, NULL);
        if (!poDS)
        {
            QgsMessageLog::logMessage(tr("打开轨迹点图层失败！"), tr("动目标专题制图"), Qgis::Critical);
            return;
        }

        //  获取数据集中的第一个图层
        OGRLayer* poLayer = poDS->GetLayer(0);
        if (!poLayer)
        {
            // 记录日志
            QgsMessageLog::logMessage(tr("打开轨迹点图层失败！"), tr("动目标专题制图"), Qgis::Critical);
            return;
        }
        //  获取图层的地理参考
        OGRSpatialReference* oLayerSR = poLayer->GetSpatialRef();
        //  获取图层地理类型
        OGRwkbGeometryType geo = poLayer->GetGeomType();
        //  获取图层的名称
        string strLayerName = poLayer->GetName();

        //  获取输入轨迹点图层所在目录，轨迹点标注层也生成在此目录下
        string strInputTracePointPath = CPLGetDirname(strTracePointLayerFileName.c_str());

        //  创建变量用来保存文件夹路径
        string strShpFilePath = strInputTracePointPath;
        string strPointShpFilePath;
#ifdef OS_FAMILY_WINDOWS
        if (std::filesystem::exists(std::filesystem::u8path(strShpFilePath)))
        {
          //  nothing
        }
        else
        {
          std::filesystem::create_directories(std::filesystem::u8path(strShpFilePath));
        }
        //_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
        mkdir(strShpFilePath.c_str(), MODE);
#endif
        OGRFeature* poFeature = NULL;
        int iResult = 0;
        vector<SE_DPoint> vPoints;
        vPoints.clear();
        vector<vector<FieldNameAndValue_MovingTarget>> vPointAttrs;
        vPointAttrs.clear();

        //  重置图层读取
        poLayer->ResetReading();
        while ((poFeature = poLayer->GetNextFeature()) != NULL)
        {
            SE_DPoint xyz;
            vector<FieldNameAndValue_MovingTarget> vFieldValue;   // 当前要素属性值
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
    }

#pragma endregion

#pragma region "4.2.3、加载轨迹点标注图层"

    QgsVectorLayer* pTracePointLabelLayer = nullptr;
    QgsMapLayer* pAddedTracePointLabelLayer = nullptr;

    /*将矢量图层加载到地图图层列表中*/
    QString fileNameVector = qstrLabelPointShpFilePathTemp;
    QFileInfo fileInfoVector(fileNameVector);

    // 获取数据名称
    QString baseNameVector = fileInfoVector.completeBaseName();

    // 通过图层类型获取对应的样式文件全路径，去除所有扩展名的图层名称
    QString qstrBaseName = fileInfoVector.baseName();

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

    qMainMapLayerList.append(pAddedTracePointLabelLayer);
    qMapLayerList.append(pAddedTracePointLabelLayer);
#pragma endregion

#pragma region "4.2.4、加载轨迹线图层"
    //  获取轨迹线图层路径
    QString qstrTraceLineLayerFilePath = GetTraceLineDataPath();
    QgsVectorLayer* pTraceLineLayer = nullptr;
    QgsMapLayer* pAddedTraceLineLayer = nullptr;
    //  判断轨迹线图层是否存在
    if (QFileInfo::exists(qstrTraceLineLayerFilePath))
    {
        //  将矢量图层加载到地图图层列表中
        QString fileNameVector = qstrTraceLineLayerFilePath;
        QFileInfo fileInfoVector(fileNameVector);

        //  获取数据名称
        QString baseNameVector = fileInfoVector.completeBaseName();
        //  通过图层类型获取对应的样式文件全路径，去除所有扩展名的图层名称
        QString qstrBaseName = fileInfoVector.baseName();
        //  获取qml名称
        QString qstrQmlFilePath = QCoreApplication::applicationDirPath() + "/style/theme/moving_target/trace_line.qml";
        //  创建矢量图层
        pTraceLineLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
        if (!pTraceLineLayer->isValid())
        {
            // 记录日志
            QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
            return;
        }

        //  图层已有的样式
        QgsMapLayerStyle oldStyle = pTraceLineLayer->styleManager()->style(pTraceLineLayer->styleManager()->currentStyle());
        QString message;
        bool defaultLoadedFlag = false;
        message = pTraceLineLayer->loadNamedStyle(qstrQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);
        if (!defaultLoadedFlag)
        {
            QgsMessageLog::logMessage((QString(tr("样式文件不存在:")) + qstrQmlFilePath), tr("动目标专题制图"), Qgis::Critical);
            return;
        }

        //  向工程中添加轨迹线图层
        pAddedTraceLineLayer = pProject->addMapLayer(pTraceLineLayer);
        if (!pAddedTraceLineLayer)
        {
            delete pTraceLineLayer;
            // 记录日志
            QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
            return;
        }

        qMapLayerList.append(pAddedTraceLineLayer);
        qMainMapLayerList.append(pAddedTraceLineLayer);
    }

#pragma endregion

#pragma endregion

#pragma region "4.3、加载影像图层"
    /*-------------------------------*/
    /*----------加载影像图层---------*/
    /*-------------------------------*/

    //  先按照制图范围裁切影像数据
    string strImageDataPath = GetImageDataPath().toUtf8().toStdString();
    string strOutputDataPath = GetDataSavePath().toUtf8().toStdString();
    QString qstrRasterFilePath;
    bool bChunkFlag = ChunkImageDataToJPEG(strImageDataPath,
        dTracePointLayerRect.dleft,
        dTracePointLayerRect.dtop,
        dTracePointLayerRect.dright,
        dTracePointLayerRect.dbottom,
        strOutputDataPath,
        qstrRasterFilePath);
    //  检查裁切影像图是否成功
    if (!bChunkFlag)
    {
        // 记录日志
        QgsMessageLog::logMessage((QString(tr("裁切影像图失败:")) + GetImageDataPath()), tr("动目标专题制图"), Qgis::Critical);
        return;
    }


    //  输出影像路径
    QString fileNameRaster = qstrRasterFilePath;
    QFileInfo fileInfoRaster(fileNameRaster);

    //  获取影像数据名称
    QString baseNameRaster = fileInfoRaster.completeBaseName();

    //  根据文件存放路径，栅格的名称创建一个QgsRasterLayer类
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

    qMapLayerList.append(pLayerRaster);
#pragma endregion

#pragma region "4.4、加载图外整饰点、整饰面图层"

    //  图外整饰面图层默认不存在
    bool bLayoutDecPolygonExisted = false;
    //  图外整饰面图层路径
    QString qstrLayoutDecPolygonPath = GetDataSavePath() + "/" + qstrMapName + "/DecPolygon.shp";
    //  获取图外整饰面样式文件路径
    QString qstrLayoutDecPolygonQmlFilePath;
    //  获取布局轮廓文件路径
    GetLayoutDecPolygonQmlFilePath(qstrLayoutDecPolygonQmlFilePath);

    //  判断整饰面图层是否存在
    QgsVectorLayer* pDecPolygonLayer = nullptr;
    QgsMapLayer* pAddedDecPolygonLayer = nullptr;
    if (QFileInfo::exists(qstrLayoutDecPolygonPath))
    {
        bLayoutDecPolygonExisted = true;

        // 将矢量图层加载到地图图层列表中
        QString fileNameVector = qstrLayoutDecPolygonPath;
        QFileInfo fileInfoVector(fileNameVector);
        //  获取数据名称
        QString baseNameVector = fileInfoVector.completeBaseName();
        //  通过图层类型获取对应的样式文件全路径,去除所有扩展名的图层名称
        QString qstrBaseName = fileInfoVector.baseName();
        //  创建矢量图层
        pDecPolygonLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
        if (!pDecPolygonLayer->isValid())
        {
            // 记录日志
            QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("动目标专题制图"), Qgis::Warning);
            return;
        }

        //  图层已有的样式
        QgsMapLayerStyle oldStyle = pDecPolygonLayer->styleManager()->style(pDecPolygonLayer->styleManager()->currentStyle());
        QString message;
        bool defaultLoadedFlag = false;
        message = pDecPolygonLayer->loadNamedStyle(qstrLayoutDecPolygonQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);
        if (!defaultLoadedFlag)
        {
            return;
        }
        //  向工程中添加图层
        pAddedDecPolygonLayer = pProject->addMapLayer(pDecPolygonLayer);
        if (!pAddedDecPolygonLayer)
        {
            delete pDecPolygonLayer;
            // 记录日志
            QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("动目标专题制图"), Qgis::Warning);
            return;
        }

        qMainMapLayerList.append(pAddedDecPolygonLayer);
    }
    else
    {
        bLayoutDecPolygonExisted = false;
    }

#pragma endregion

#pragma region "4.5、加载鹰眼图层"
    /*-------------------------------*/
    /*---------加载鹰眼图层--------*/
    /*-------------------------------*/
    QgsVectorLayer* vecLayerEagleEye = nullptr;
    QgsMapLayer* pLayerEagleEye = nullptr;
    // 左上角投影坐标
    double dProjEagleLeft = 0;
    double dProjEagleTop = 0;
    double dProjEagleRight = 0;
    double dProjEagleBottom = 0;

    // 如果☑鹰眼图
    if (ui.checkBox_EagleEye->isChecked())
    {
        //  获取鹰眼图路径
        QString fileNameEagleEye = ui.lineEdit_EagleEyeDataPath->text();
        QFileInfo fileInfoEagleEye(fileNameEagleEye);

        //  获取数据名称
        QString basenameMosaic = fileInfoEagleEye.completeBaseName();
        vecLayerEagleEye = new QgsVectorLayer(fileNameEagleEye, basenameMosaic, "ogr");
        if (!vecLayerEagleEye->isValid())
        {
            // 记录日志
            QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameEagleEye), tr("动目标专题制图"), Qgis::Warning);;
            return;
        }

#pragma region "使用唯一值渲染镶嵌类别"

        //  渲染字段
        QString qstrRenderField = ui.lineEdit_RenderField->text();

        int currentFieldIndexOf = vecLayerEagleEye->fields().indexOf(qstrRenderField);
          
        //  得到字段值对应的所有属性
        QSet<QVariant> unique = vecLayerEagleEye->uniqueValues(currentFieldIndexOf);
        QVariantList uniqueValues = unique.toList();
        //  获取所有属性对应的类别
        QgsCategoryList cats = QgsCategorizedSymbolRenderer::createCategories(uniqueValues, QgsSymbol::defaultSymbol(vecLayerEagleEye->geometryType()), vecLayerEagleEye, qstrRenderField);
        QColor color;
        int num = 0;

        //  设置不同的符号颜色
        for (auto iter = cats.begin(); iter != cats.end(); ++iter)
        {

            // 如果是简单渲染
            if (ui.radioButton_SimpleRender->isChecked())
            {
                iter->symbol()->setColor(m_qEagleEyeLayerColor);
            }
            else
            {
                // 设置面要素颜色
                QColor clr(rand() % 256, rand() % 256, rand() % 256);
                iter->symbol()->setColor(clr);
            }
        }

        //  加载进QgsCategorizedSymbolRenderer渲染器中
        QgsCategorizedSymbolRenderer* pRender = new QgsCategorizedSymbolRenderer(qstrRenderField, cats);

        //  渲染图层
        vecLayerEagleEye->setRenderer(pRender);

        //  鹰眼图数据范围
        QgsRectangle qgsEagleEyeRect = vecLayerEagleEye->extent();

        //  如果是地理坐标系，需要进行web墨卡托投影
        if (vecLayerEagleEye->crs().isGeographic())
        {
            WebMercator_BL_TO_XY(qgsEagleEyeRect.yMaximum(), qgsEagleEyeRect.xMinimum(), dProjEagleLeft, dProjEagleTop);
            WebMercator_BL_TO_XY(qgsEagleEyeRect.yMinimum(), qgsEagleEyeRect.xMaximum(), dProjEagleRight, dProjEagleBottom);
        }
        else
        {
            dProjEagleLeft = qgsEagleEyeRect.xMinimum();
            dProjEagleTop = qgsEagleEyeRect.yMaximum();
            dProjEagleRight = qgsEagleEyeRect.xMaximum();
            dProjEagleBottom = qgsEagleEyeRect.yMinimum();
        }

#pragma endregion

        //  向工程中添加图层
        pLayerEagleEye = pProject->addMapLayer(vecLayerEagleEye);
        if (!pLayerEagleEye)
        {
            delete vecLayerEagleEye;
            QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameEagleEye), tr("动目标专题制图"), Qgis::Warning);
            return;
        }
    }

#pragma endregion

#pragma region "4.6、加载轨迹点图层到鹰眼图地图控件中"

    //  ☑鹰眼图状态时
    if (ui.checkBox_EagleEye->isChecked())
    {
        //  鹰眼图轨迹点
        QString qstrEagleEyeTracePointLayerFilePath = GetTracePointDataPath();

        //  鹰眼图轨迹点图层对象
        QgsVectorLayer* pEagelEyeTracePointLayer = nullptr;
        QgsMapLayer* pAddedEagleEyeTracePointLayer = nullptr;

        //  判断矢量图层是否存在
        if (QFileInfo::exists(qstrEagleEyeTracePointLayerFilePath))
        {
            //  将矢量图层加载到鹰眼图地图图层列表中
            QString fileNameVector = qstrEagleEyeTracePointLayerFilePath;
            QFileInfo fileInfoVector(fileNameVector);

            //  获取数据名称
            QString baseNameVector = fileInfoVector.completeBaseName();

            //  通过图层类型获取对应的样式文件全路径,去除所有扩展名的图层名称
            QString qstrBaseName = fileInfoVector.baseName();

            //  获取qml名称
            QString qstrQmlFilePath = QCoreApplication::applicationDirPath() + "/style/theme/moving_target/eagle_eye_trace_point.qml";

            pEagelEyeTracePointLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
            if (!pEagelEyeTracePointLayer->isValid())
            {
                // 记录日志
                QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
                return;
            }

            //  图层已有的样式
            QgsMapLayerStyle oldStyle = pEagelEyeTracePointLayer->styleManager()->style(pEagelEyeTracePointLayer->styleManager()->currentStyle());
            QString message;
            bool defaultLoadedFlag = false;
            message = pEagelEyeTracePointLayer->loadNamedStyle(qstrQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

            if (!defaultLoadedFlag)
            {
                QgsMessageLog::logMessage((QString(tr("样式文件不存在:")) + qstrQmlFilePath), tr("动目标专题制图"), Qgis::Critical);
                return;
            }

            //  向工程中添加图层
            pAddedEagleEyeTracePointLayer = pProject->addMapLayer(pEagelEyeTracePointLayer);
            if (!pAddedEagleEyeTracePointLayer)
            {
                delete pEagelEyeTracePointLayer;
                // 记录日志
                QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("动目标专题制图"), Qgis::Critical);
                return;
            }

            qEagleEyeMapLayerList.append(pAddedEagleEyeTracePointLayer);
            qMapLayerList.append(pAddedEagleEyeTracePointLayer);
            if (pLayerEagleEye)
            {
                qMapLayerList.append(pLayerEagleEye);
                qEagleEyeMapLayerList.append(pLayerEagleEye);
            }
        }
    }


#pragma endregion

#pragma region "4.7、加载布局模板"

    /*----------------------------------------*/
    /*【3】根据制图方案设置整饰页面各要素的状态*/
    /*----------------------------------------*/
    //  创建打印布局
    QgsPrintLayout* pPrintLayout = new QgsPrintLayout(QgsProject::instance());
    if (!pPrintLayout)
    {
        // 记录日志
        QgsMessageLog::logMessage(tr("创建打印布局失败！"), tr("动目标专题制图"), Qgis::Critical);
        return;
    }

    // 设置布局名称 - 图幅号
    pPrintLayout->setName(qstrMapName);
    //  初始化默认设置
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

#pragma region "4.8、设置主地图范围及空间参考"

    // 图层的范围
    QgsRectangle qgsMapDisplayRect;
    double dMapScale = 0;
    dMapScale = ui.lineEdit_Scale->text().toDouble();

    //  对数据四个角点进行web墨卡托投影正算,获取轨迹点、轨迹线图层数据范围
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

    //  获取最小外接矩形
    double dMBRLeft = 0;
    double dMBRTop = 0;
    double dMBRRight = 0;
    double dMBRBottom = 0;
    GetMBR(4, dSheetPoints, dMBRLeft, dMBRTop, dMBRRight, dMBRBottom);

    // 修改内容：将地图范围外扩
    // 修改时间：2023-08-31
    // 修改人：Yang ZhiLong
    //dMBRLeft -= dMapScale * 0.02;
    //dMBRRight += dMapScale * 0.08;
    //dMBRBottom -= dMapScale * 0.04;
    //dMBRTop += dMapScale * 0.04/*0.02*/;

    //  杨小兵-2024-10-17
    dMBRLeft -= dMapScale * 0.2;
    dMBRRight += dMapScale * 0.2;
    dMBRBottom -= dMapScale * 0.2;
    dMBRTop += dMapScale * 0.2/*0.02*/;



    qgsMapDisplayRect.setXMinimum(dMBRLeft);
    qgsMapDisplayRect.setXMaximum(dMBRRight);
    qgsMapDisplayRect.setYMinimum(dMBRBottom);
    qgsMapDisplayRect.setYMaximum(dMBRTop);
    qgsMapDisplayRect.scale(1.0);

    //  主地图设置
    QgsLayoutItemMap* pMainMapItem = (QgsLayoutItemMap*)GetLayoutItemByName(qLayoutItemList, "MapItem_MainMap");
    if (!pMainMapItem)
    {
        // 记录日志
        QgsMessageLog::logMessage(tr("主地图控件获取失败！"), tr("动目标专题制图"), Qgis::Critical);
        return;
    }

    //  设置主地图空间参考系
    pMainMapItem->setCrs(QgsCoordinateReferenceSystem(tr("EPSG:3857")));
    pMainMapItem->setScale(dMapScale);
    pMainMapItem->setBackgroundColor(QColor(255, 255, 255, 0));

    double dMapUnit2LayoutUnit = pMainMapItem->mapUnitsToLayoutUnits();
    //  获取地图内图廓（矩形）的左上角点布局坐标
    m_InteriorMapLeftTopLayoutPoint.dx = pMainMapItem->positionWithUnits().x();
    m_InteriorMapLeftTopLayoutPoint.dy = pMainMapItem->positionWithUnits().y();

#pragma region "4.8.1、更新地图内图廓"

    /*
    修改时间：2023-11-27
    修改内容：根据地图比例尺及整饰要素最小尺寸要求，判断是否大于500mm（地图说明+鹰眼图+比例尺+动目标说明）

    修改时间：2023-12-09
    修改内容：如果未选择鹰眼图，则500mm改为400mm
    */

    //  地图画布最小宽度阈值
    double dLimitMapCanvasWidth = 0;
    //  如果选择鹰眼图
    if (ui.checkBox_EagleEye->isChecked())
    {
        dLimitMapCanvasWidth = 500;
    }
    else
    {
        dLimitMapCanvasWidth = 400;
    }


    //  根据比例尺及数据范围计算地图画布大小
    double dMapCanvasWidth = (dMBRRight - dMBRLeft) * dMapUnit2LayoutUnit;
    if (dMapCanvasWidth <= dLimitMapCanvasWidth)
    {
        // 记录日志
        QgsMessageLog::logMessage(tr("当前地图比例尺偏小，请重新设置地图比例尺！"), tr("动目标专题制图"), Qgis::Critical);
        QMessageBox msgBox;
        msgBox.setWindowTitle(tr("动目标专题制图"));
        msgBox.setText(tr("当前地图比例尺偏小，请重新设置地图比例尺！"));
        msgBox.setStandardButtons(QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.exec();
        return;
    }


    //  重新设置地图范围,以地图内图廓为准
    pMainMapItem->attemptSetSceneRect(QRectF(m_InteriorMapLeftTopLayoutPoint.dx,
      m_InteriorMapLeftTopLayoutPoint.dy,
      (dMBRRight - dMBRLeft) * dMapUnit2LayoutUnit,
      (dMBRTop - dMBRBottom) * dMapUnit2LayoutUnit));

    pMainMapItem->setExtent(qgsMapDisplayRect);


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


#pragma endregion

#pragma region "4.8.2、地图内图廓多边形四个布局点坐标A、B、C、D"


    /*
    * 布局坐标分布示意图
    A-------------B
    |			        |
    |			        |
    |		          |
    D-------------C
    */

        //  先算A、B、C、D的投影坐标，然后再计算A、B、C、D的画布坐标
        SE_DPoint dProj_A;
        WebMercator_BL_TO_XY(interiorMapGeoPoint_A.dy, interiorMapGeoPoint_A.dx, dProj_A.dx, dProj_A.dy);

        SE_DPoint dProj_B;
        WebMercator_BL_TO_XY(interiorMapGeoPoint_B.dy, interiorMapGeoPoint_B.dx, dProj_B.dx, dProj_B.dy);

        SE_DPoint dProj_C;
        WebMercator_BL_TO_XY(interiorMapGeoPoint_C.dy, interiorMapGeoPoint_C.dx, dProj_C.dx, dProj_C.dy);

        SE_DPoint dProj_D;
        WebMercator_BL_TO_XY(interiorMapGeoPoint_D.dy, interiorMapGeoPoint_D.dx, dProj_D.dx, dProj_D.dy);



        QRectF mapItemRect = pMainMapItem->rect();

        // 投影坐标到布局坐标的转换

        // 左上角的投影坐标与左上角点的布局坐标一一对应
        SE_DPoint dLeftCornerProjPoint;
        dLeftCornerProjPoint.dx = dMBRLeft;
        dLeftCornerProjPoint.dy = dMBRTop;

        // 左上角的投影坐标与左上角点的布局坐标一一对应
        SE_DPoint dLeftCornerLayoutPoint;
        dLeftCornerLayoutPoint.dx = pMainMapItem->positionWithUnits().x();
        dLeftCornerLayoutPoint.dy = pMainMapItem->positionWithUnits().y();

        // 内图廓左上角A布局坐标值，positionWithUnits()默认为地图窗口左上角布局点坐标
        SE_DPoint interiorMapLayoutPoint_A;
        TransformProjectToLayout(dLeftCornerProjPoint,
            dLeftCornerLayoutPoint,
            dProj_A,
            dMapUnit2LayoutUnit,
            interiorMapLayoutPoint_A);

        // 内图廓右上角B布局坐标值
        SE_DPoint interiorMapLayoutPoint_B;
        TransformProjectToLayout(dLeftCornerProjPoint,
            dLeftCornerLayoutPoint,
            dProj_B,
            dMapUnit2LayoutUnit,
            interiorMapLayoutPoint_B);

        // 内图廓右下角C布局坐标值
        SE_DPoint interiorMapLayoutPoint_C;
        TransformProjectToLayout(dLeftCornerProjPoint,
            dLeftCornerLayoutPoint,
            dProj_C,
            dMapUnit2LayoutUnit,
            interiorMapLayoutPoint_C);

        // 内图廓左下角D布局坐标值
        SE_DPoint interiorMapLayoutPoint_D;
        TransformProjectToLayout(dLeftCornerProjPoint,
            dLeftCornerLayoutPoint,
            dProj_D,
            dMapUnit2LayoutUnit,
            interiorMapLayoutPoint_D);


#pragma endregion

#pragma endregion

#pragma endregion

#pragma region "5、计算经纬网整数倍经线间隔和纬线间隔刻度所在布局点坐标"

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
        |			        |
        |			        |
        |		          |
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

#pragma region "6、绘制内图廓边框"

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

#pragma region "7、绘制外图廓边框"

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

#pragma region "8、绘制经纬网"
        /*
        一、绘制经纬网原则
        * 如果上、下边界整公里X坐标存在不同，则暂不绘制对应X坐标的经纬网；
        * 如果左、右边界整公里Y坐标存在不同，则暂不绘制对应Y坐标的经纬网；
        * 上、下、左、右边界经纬网刻度与上、下、左、右边界线垂直
        * 对于5万比例尺，经纬网刻度、邻带经纬网刻度只需绘制偶数刻度值
        */
#pragma region "8.1、绘制上边界到下边界经纬网及刻度线"

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

#pragma region "8.2、绘制左边界到右边界经纬网及刻度线"

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

#pragma region "8.3、绘制经纬网刻度值"

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
            pLeftBorderLabel_Lat->attemptSetSceneRect(QRectF(layoutLeftBorderPoint.dx - 11.5,
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

#pragma region "9、图外整饰要素调整位置"

#pragma region "外图廓整饰面要素图层【生成地图说明、动目标说明】"

        // add by yangzhilong @2023-08-31
        // 外图廓整饰面要素图层
        OGRLayer* poDecPolygonLayer = NULL;

        //创建结果数据源
        GDALDataset* poLayoutDecorationPolygonDS = NULL;

        // 如果不存在整饰面要素图层，则在此处创建
        if (!bLayoutDecPolygonExisted)
        {
            CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");	// 支持中文路径
            CPLSetConfigOption("SHAPE_ENCODING", "UTF-8");			// 属性表支持中文字段
            GDALAllRegister();

            // 获取shapefile驱动
            const char* pszShpDriverName = "ESRI Shapefile";
            GDALDriver* poShpDriver;
            poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
            if (poShpDriver == NULL)
            {
                QgsMessageLog::logMessage(tr("获取shapefile驱动失败！"), tr("动目标专题制图"), Qgis::Critical);
                return;
            }

            // 结果保存路径
            QString qstrShpFilePath = GetDataSavePath() + "/" + qstrMapName;
            string strShpFilePath = qstrShpFilePath.toUtf8();
            

#ifdef OS_FAMILY_WINDOWS
            if (std::filesystem::exists(std::filesystem::u8path(strShpFilePath)))
            {
              //  nothing
            }
            else
            {
              std::filesystem::create_directories(std::filesystem::u8path(strShpFilePath));
            }
            //_mkdir(strShpFilePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
            mkdir(strShpFilePath.c_str(), MODE);
#endif
            vector<string> vFieldsName;
            vFieldsName.clear();

            vector<OGRFieldType> vFieldType;
            vFieldType.clear();

            vector<int> vFieldWidth;
            vFieldWidth.clear();

            vector<int> vPrecision;
            vPrecision.clear();

            // 点要素字段：
            // TYPE
            vFieldsName.push_back("TYPE");
            vFieldType.push_back(OFTString);
            vFieldWidth.push_back(50);
            vPrecision.push_back(0);

            // 绘制标志，
            // FLAG
            vFieldsName.push_back("FLAG");
            vFieldType.push_back(OFTString);
            vFieldWidth.push_back(10);
            vPrecision.push_back(0);

            // 整饰要素字符串
            vFieldsName.push_back("VALUE");
            vFieldType.push_back(OFTString);
            vFieldWidth.push_back(100);
            vPrecision.push_back(0);


            // 创建结果图层
            OGRSpatialReference pResultSR;		// 结果图层的空间参考，默认CGCS2000坐标系

            // 地理基准CGCS2000
            pResultSR.SetWellKnownGeogCS("EPSG:4490");

            // 创建对应图幅的布局面图层，如:图幅_LayoutPolygon.shp
            // 面要素图层全路径
            string strPolygonShpFilePath = strShpFilePath + "/DecPolygon.shp";
            std::cout << strPolygonShpFilePath << std::endl;
            string strCPGFilePath = strShpFilePath + "/DecPolygon.cpg";
            std::cout << strCPGFilePath << std::endl;

            poLayoutDecorationPolygonDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);

            if (poLayoutDecorationPolygonDS == NULL)
            {
                QgsMessageLog::logMessage(tr("创建整饰面要素图层失败！"), tr("动目标专题制图"), Qgis::Critical);
                return;
            }

            // shp中存储属性信息和几何信息
            poDecPolygonLayer = poLayoutDecorationPolygonDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
            if (!poDecPolygonLayer)
            {
                QgsMessageLog::logMessage(tr("创建整饰面要素图层失败！"), tr("动目标专题制图"), Qgis::Critical);
                return;
            }

            // 创建结果图层属性字段
            int iResult = SetFieldDefn(poDecPolygonLayer, vFieldsName, vFieldType, vFieldWidth, vPrecision);
            if (iResult != 0) {
                QgsMessageLog::logMessage(tr("创建整饰面要素图层属性字段失败！"), tr("动目标专题制图"), Qgis::Critical);
                return;
            }

            if (!CreateShapefileCPG(strCPGFilePath, "UTF8"))
            {
                QgsMessageLog::logMessage(tr("创建整饰面要素图层cpg文件失败！"), tr("动目标专题制图"), Qgis::Critical);
                return;
            }
        }

#pragma endregion

#pragma region "【1】图名"

        // 获取地图名称
        QgsLayoutItemLabel* pTextItem_MapTitleName = (QgsLayoutItemLabel*)GetLayoutItemByName(
            qLayoutItemList,
            "TextItem_MapTitleName");

#pragma region "【1-1】创建整饰要素-动目标地图图名"

        // 创建动目标说明要素，同时设置模板中图名要素不显示
        pTextItem_MapTitleName->setVisibility(false);

        // 如果不存在，需要创建要素
        if (!bLayoutDecPolygonExisted)
        {
            // 动目标地图图名四个角点，依次左上角A、右上角B、右下角C、左下角D
            SE_DPoint dLayoutPoints[4];

            // 右上角
            dLayoutPoints[1].dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2 + 100;
            dLayoutPoints[1].dy = m_dExteriorLeftTopLayoutPoint.dy - 20;

            // 右下角
            dLayoutPoints[2].dx = dLayoutPoints[1].dx;
            dLayoutPoints[2].dy = dLayoutPoints[1].dy + 6;

            // 左下角
            dLayoutPoints[3].dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2 - 100;
            dLayoutPoints[3].dy = dLayoutPoints[1].dy + 6;

            // 左上角
            dLayoutPoints[0].dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2 - 100;
            dLayoutPoints[0].dy = dLayoutPoints[1].dy;


            // 类型
            vector<FieldNameAndValue_MovingTarget> vFieldNameAndValue;
            vFieldNameAndValue.clear();

            FieldNameAndValue_MovingTarget fieldImp;
            fieldImp.strFieldName = "TYPE";
            fieldImp.strFieldValue = "movingtarget_mapname";
            vFieldNameAndValue.push_back(fieldImp);

            fieldImp.strFieldName = "FLAG";
            fieldImp.strFieldValue = "1";
            vFieldNameAndValue.push_back(fieldImp);

            fieldImp.strFieldName = "VALUE";
            fieldImp.strFieldValue = qstrMapName.toUtf8().data();
            vFieldNameAndValue.push_back(fieldImp);

            // 布局坐标转投影坐标
            SE_DPoint dMercatorPoint[4];		// 投影坐标
            double dValue[8] = { 0 };
            SE_DPoint dGeoPoint[4];				// 地理坐标
            vector<SE_DPoint> vPoints;
            vPoints.clear();
            int iRet = 0;

            // 左上角点投影坐标值
            SE_DPoint dRefProjPoint;
            dRefProjPoint.dx = pMainMapItem->extent().xMinimum();
            dRefProjPoint.dy = pMainMapItem->extent().yMaximum();

            // 左上角点布局坐标
            SE_DPoint dRefLayoutPoint;
            dRefLayoutPoint.dx = pMainMapItem->positionWithUnits().x();
            dRefLayoutPoint.dy = pMainMapItem->positionWithUnits().y();


            for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
            {
                TransformLayoutToProjCRS(
                    dRefProjPoint,
                    dRefLayoutPoint,
                    dLayoutPoints[iTempIndex],
                    dMapUnit2LayoutUnit,
                    dMercatorPoint[iTempIndex]);

                dValue[2 * iTempIndex] = dMercatorPoint[iTempIndex].dx;
                dValue[2 * iTempIndex + 1] = dMercatorPoint[iTempIndex].dy;

                // 高斯投影转地理坐标
                iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
                    WebMercator,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    4,
                    dValue);

                if (iRet != 1)
                {
                    continue;
                }

                dGeoPoint[iTempIndex].dx = dValue[2 * iTempIndex];
                dGeoPoint[iTempIndex].dy = dValue[2 * iTempIndex + 1];

                vPoints.push_back(dGeoPoint[iTempIndex]);
            }

            iRet = Set_Polygon(poDecPolygonLayer, vPoints, vFieldNameAndValue);
            if (iRet != 0)
            {
                return;
            }
        }


#pragma endregion

#pragma endregion

#pragma region "【2】地图左下角说明"

        QgsLayoutItemLabel* pTextItem_MapDetail = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_MapDetail");

        /*pTextItem_MapDetail->setText(qstrMapDetail);

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
            27));*/


#pragma region "创建整饰要素-地图说明"

        // 创建地图说明要素，同时设置模板中图名要素不显示
        pTextItem_MapDetail->setVisibility(false);

        // 如果不存在，需要创建要素
        if (!bLayoutDecPolygonExisted)
        {
            // 地图说明四个角点，依次左上角A、右上角B、右下角C、左下角D
            SE_DPoint dLayoutPoints[4];

            // 左上角
            dLayoutPoints[0].dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE;
            dLayoutPoints[0].dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3;

            // 右上角
            dLayoutPoints[1].dx = dLayoutPoints[0].dx + 60;
            dLayoutPoints[1].dy = dLayoutPoints[0].dy;

            // 右下角
            dLayoutPoints[2].dx = dLayoutPoints[0].dx + 60;
            dLayoutPoints[2].dy = dLayoutPoints[0].dy + 30;

            // 左下角
            dLayoutPoints[3].dx = dLayoutPoints[0].dx;
            dLayoutPoints[3].dy = dLayoutPoints[0].dy + 30;


            // 类型
            vector<FieldNameAndValue_MovingTarget> vFieldNameAndValue;
            vFieldNameAndValue.clear();

            FieldNameAndValue_MovingTarget fieldImp;
            fieldImp.strFieldName = "TYPE";
            fieldImp.strFieldValue = "map_detail";
            vFieldNameAndValue.push_back(fieldImp);

            fieldImp.strFieldName = "FLAG";
            fieldImp.strFieldValue = "1";
            vFieldNameAndValue.push_back(fieldImp);

            fieldImp.strFieldName = "VALUE";
            fieldImp.strFieldValue = qstrMapDetail.toUtf8().data();
            vFieldNameAndValue.push_back(fieldImp);

            // 布局坐标转投影坐标
            SE_DPoint dMercatorPoint[4];		// 投影坐标
            double dValue[8] = { 0 };
            SE_DPoint dGeoPoint[4];				// 地理坐标
            vector<SE_DPoint> vPoints;
            vPoints.clear();
            int iRet = 0;

            // 左上角点投影坐标值
            SE_DPoint dRefProjPoint;
            dRefProjPoint.dx = pMainMapItem->extent().xMinimum();
            dRefProjPoint.dy = pMainMapItem->extent().yMaximum();

            // 左上角点布局坐标
            SE_DPoint dRefLayoutPoint;
            dRefLayoutPoint.dx = pMainMapItem->positionWithUnits().x();
            dRefLayoutPoint.dy = pMainMapItem->positionWithUnits().y();


            for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
            {
                TransformLayoutToProjCRS(
                    dRefProjPoint,
                    dRefLayoutPoint,
                    dLayoutPoints[iTempIndex],
                    dMapUnit2LayoutUnit,
                    dMercatorPoint[iTempIndex]);

                dValue[2 * iTempIndex] = dMercatorPoint[iTempIndex].dx;
                dValue[2 * iTempIndex + 1] = dMercatorPoint[iTempIndex].dy;

                // 高斯投影转地理坐标
                iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
                    WebMercator,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    4,
                    dValue);

                if (iRet != 1)
                {
                    continue;
                }

                dGeoPoint[iTempIndex].dx = dValue[2 * iTempIndex];
                dGeoPoint[iTempIndex].dy = dValue[2 * iTempIndex + 1];

                vPoints.push_back(dGeoPoint[iTempIndex]);
            }

            iRet = Set_Polygon(poDecPolygonLayer, vPoints, vFieldNameAndValue);
            if (iRet != 0)
            {
                return;
            }
        }

#pragma endregion





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

#pragma region "【4】图形比例尺"

        // 如果是1:50000比例尺或1:10000比例尺，增加图形比例尺的显示

        QgsLayoutItemPicture* pPictureItem_Scale_10000 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_Scale_10000");
        QgsLayoutItemPicture* pPictureItem_Scale_50000 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_Scale_50000");

        // 如果是绘制状态
        if (bDrawScale)
        {
            // 设置图片模式
            pPictureItem_Scale_10000->setMode(QgsLayoutItemPicture::FormatSVG);
            pPictureItem_Scale_50000->setMode(QgsLayoutItemPicture::FormatSVG);

            QString qstrSvgPath = "";
            // 如果是1万比例尺
            if (dMapScale == SCALE_10K)
            {
                qstrSvgPath = QCoreApplication::applicationDirPath() + "/svg/scale/10000.svg";
                // 设置图片路径
                pPictureItem_Scale_10000->setPicturePath(qstrSvgPath);
                pPictureItem_Scale_10000->setResizeMode(QgsLayoutItemPicture::Zoom);
                pPictureItem_Scale_10000->setVisibility(true);
                pPictureItem_Scale_50000->setVisibility(false);

                // 设置位置
                pPictureItem_Scale_10000->setReferencePoint(QgsLayoutItem::Middle);

                // 设置参考点坐标
                pPictureItem_Scale_10000->attemptMove(QgsLayoutPoint(
                    m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0,
                    m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 15));
            }
            // 如果是5万比例尺
            else if (dMapScale == SCALE_50K)
            {
                qstrSvgPath = QCoreApplication::applicationDirPath() + "/svg/scale/50000.svg";
                // 设置图片路径
                pPictureItem_Scale_50000->setPicturePath(qstrSvgPath);
                pPictureItem_Scale_50000->setResizeMode(QgsLayoutItemPicture::Zoom);
                pPictureItem_Scale_50000->setVisibility(true);
                pPictureItem_Scale_10000->setVisibility(false);

                // 设置参考点坐标
                pPictureItem_Scale_50000->attemptMove(QgsLayoutPoint(
                    m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0,
                    m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 15));
            }
            else
            {
                pPictureItem_Scale_10000->setVisibility(false);
                pPictureItem_Scale_50000->setVisibility(false);
            }
        }
        else
        {
            pPictureItem_Scale_10000->setVisibility(false);
            pPictureItem_Scale_50000->setVisibility(false);
        }

#pragma endregion

#pragma region "【5】制图单位"

        QgsLayoutItemLabel* pTextItem_MappingAgency = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_MappingAgency");
        // 修改时间：2023-10-09
        // 修改内容：制图单位在动目标说明中体现
        pTextItem_MappingAgency->setVisibility(false);

#pragma endregion

#pragma region "【6】动目标说明"

        QgsLayoutItemLabel* pTextItem_MovingTargetDetail = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_MovingTargetDetail");


#pragma region "【6-1】创建整饰要素-动目标说明"

        // 创建动目标说明要素，同时设置模板中图名要素不显示
        pTextItem_MovingTargetDetail->setVisibility(false);

        // 如果不存在，需要创建要素
        if (!bLayoutDecPolygonExisted)
        {
            // 动目标说明四个角点，依次左上角A、右上角B、右下角C、左下角D
            SE_DPoint dLayoutPoints[4];

            // 右上角
            dLayoutPoints[1].dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() - OUTER_MAP_INTERIOR_MAP_DISTANCE;
            dLayoutPoints[1].dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3;

            // 右下角
            dLayoutPoints[2].dx = dLayoutPoints[1].dx;
            dLayoutPoints[2].dy = dLayoutPoints[1].dy + 30;

            // 左下角
            dLayoutPoints[3].dx = dLayoutPoints[1].dx - 60;
            dLayoutPoints[3].dy = dLayoutPoints[1].dy + 30;

            // 左上角
            dLayoutPoints[0].dx = dLayoutPoints[1].dx - 60;
            dLayoutPoints[0].dy = dLayoutPoints[1].dy;


            // 类型
            vector<FieldNameAndValue_MovingTarget> vFieldNameAndValue;
            vFieldNameAndValue.clear();

            FieldNameAndValue_MovingTarget fieldImp;
            fieldImp.strFieldName = "TYPE";
            fieldImp.strFieldValue = "movingtarget_detail";
            vFieldNameAndValue.push_back(fieldImp);

            fieldImp.strFieldName = "FLAG";
            fieldImp.strFieldValue = "1";
            vFieldNameAndValue.push_back(fieldImp);

            fieldImp.strFieldName = "VALUE";
            fieldImp.strFieldValue = qstrMovingTargetDetail.toUtf8().data();
            vFieldNameAndValue.push_back(fieldImp);

            // 布局坐标转投影坐标
            SE_DPoint dMercatorPoint[4];		// 投影坐标
            double dValue[8] = { 0 };
            SE_DPoint dGeoPoint[4];				// 地理坐标
            vector<SE_DPoint> vPoints;
            vPoints.clear();
            int iRet = 0;

            // 左上角点投影坐标值
            SE_DPoint dRefProjPoint;
            dRefProjPoint.dx = pMainMapItem->extent().xMinimum();
            dRefProjPoint.dy = pMainMapItem->extent().yMaximum();

            // 左上角点布局坐标
            SE_DPoint dRefLayoutPoint;
            dRefLayoutPoint.dx = pMainMapItem->positionWithUnits().x();
            dRefLayoutPoint.dy = pMainMapItem->positionWithUnits().y();


            for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
            {
                TransformLayoutToProjCRS(
                    dRefProjPoint,
                    dRefLayoutPoint,
                    dLayoutPoints[iTempIndex],
                    dMapUnit2LayoutUnit,
                    dMercatorPoint[iTempIndex]);

                dValue[2 * iTempIndex] = dMercatorPoint[iTempIndex].dx;
                dValue[2 * iTempIndex + 1] = dMercatorPoint[iTempIndex].dy;

                // 高斯投影转地理坐标
                iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
                    WebMercator,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    4,
                    dValue);

                if (iRet != 1)
                {
                    continue;
                }

                dGeoPoint[iTempIndex].dx = dValue[2 * iTempIndex];
                dGeoPoint[iTempIndex].dy = dValue[2 * iTempIndex + 1];

                vPoints.push_back(dGeoPoint[iTempIndex]);
            }

            iRet = Set_Polygon(poDecPolygonLayer, vPoints, vFieldNameAndValue);
            if (iRet != 0)
            {
                return;
            }
        }


#pragma endregion

        GDALClose(poLayoutDecorationPolygonDS);
#pragma endregion

#pragma region "【7】指北针"

        QgsLayoutItemPicture *pPictureNorthArrowItem = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_NorthArrow");

        // 如果是绘制状态
        if (bDrawNorthArrow)
        {
            pPictureNorthArrowItem->setVisibility(true);

            // 如果选择地图内图廓左上角
            if (ui.radioButton_NorthArrowLeftTop->isChecked())
            {
                pPictureNorthArrowItem->setReferencePoint(QgsLayoutItem::UpperLeft);
                // 调整尺寸
                pPictureNorthArrowItem->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + 5,
                    m_dExteriorLeftTopLayoutPoint.dy + OUTER_MAP_INTERIOR_MAP_DISTANCE + 5));
            }
            // 如果选择地图内图廓右上角
            else
            {
                pPictureNorthArrowItem->setReferencePoint(QgsLayoutItem::UpperRight);
                // 调整尺寸
                pPictureNorthArrowItem->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() - OUTER_MAP_INTERIOR_MAP_DISTANCE - 5,
                    m_dExteriorLeftTopLayoutPoint.dy + OUTER_MAP_INTERIOR_MAP_DISTANCE + 5 ));
            }
        }



#pragma endregion

#pragma region "【8】密级"

        QgsLayoutItemLabel* pTextItem_SecurityClassification = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_SecurityClassification");

        QString qstrSecurityClassification;
        // 公开
        if (ui.radioButton_GongKai->isChecked())
        {
            qstrSecurityClassification = "公开";
        }
        // 内部
        else if (ui.radioButton_NeiBu->isChecked())
        {
            qstrSecurityClassification = "内部";
        }
        // 秘密
        else if (ui.radioButton_MiMi->isChecked())
        {
            qstrSecurityClassification = "秘密";
        }
        // 机密
        else if (ui.radioButton_JiMi->isChecked())
        {
            qstrSecurityClassification = "机密";
        }

        // 设置密级
        pTextItem_SecurityClassification->setText(qstrSecurityClassification);

        // 设置参考点位置
        pTextItem_SecurityClassification->setReferencePoint(QgsLayoutItem::LowerRight);

        // 设置参考点坐标
        pTextItem_SecurityClassification->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() - OUTER_MAP_INTERIOR_MAP_DISTANCE,
            m_dExteriorLeftTopLayoutPoint.dy - 3));

        // 自动调整尺寸
        pTextItem_SecurityClassification->adjustSizeToText();


#pragma endregion

#pragma region "【9】图例标题"

        // 如果显示图例标题
        if (ui.checkBox_MapLegend->isChecked())
        {
            // 图例标题
            QgsLayoutItemLabel* pTextItem_MainMapLegend = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
                "TextItem_MainMapLegend");

            // 设置参考点位置
            pTextItem_MainMapLegend->setReferencePoint(QgsLayoutItem::UpperLeft);

            // 设置参考点坐标
            pTextItem_MainMapLegend->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() + 8,
                m_dExteriorLeftTopLayoutPoint.dy));

            // 自动调整尺寸
            pTextItem_MainMapLegend->adjustSizeToText();
        }


#pragma endregion

#pragma region "【10】图例"

        if (ui.checkBox_MapLegend->isChecked())
        {
            vector<MovingTargetMainMapLegendInfo> vMainMapLegends;
            vMainMapLegends.clear();

#pragma region "【飞机】"

            MovingTargetMainMapLegendInfo legend_plane;
            legend_plane.qstrSymbolCode = "plane";
            QgsLayoutItemPicture* pPictureItem_Plane = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_Plane");
            QgsLayoutItemLabel* pLabelItem_Plane = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_Plane");
            legend_plane.pSymbolPicture = pPictureItem_Plane;
            legend_plane.pSymbolLabel = pLabelItem_Plane;
            vMainMapLegends.push_back(legend_plane);

#pragma endregion

#pragma region "【船舶】"

            MovingTargetMainMapLegendInfo legend_ship;
            legend_ship.qstrSymbolCode = "ship";
            QgsLayoutItemPicture* pPictureItem_Ship = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_Ship");
            QgsLayoutItemLabel* pLabelItem_Ship = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_Ship");
            legend_ship.pSymbolPicture = pPictureItem_Ship;
            legend_ship.pSymbolLabel = pLabelItem_Ship;
            vMainMapLegends.push_back(legend_ship);

#pragma endregion

#pragma region "【车辆】"

            MovingTargetMainMapLegendInfo legend_vehicle;
            legend_ship.qstrSymbolCode = "vehicle";
            QgsLayoutItemPicture* pPictureItem_Vehicle = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_Vehicle");
            QgsLayoutItemLabel* pLabelItem_Vehicle = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_Vehicle");
            legend_vehicle.pSymbolPicture = pPictureItem_Vehicle;
            legend_vehicle.pSymbolLabel = pLabelItem_Vehicle;
            vMainMapLegends.push_back(legend_vehicle);

#pragma endregion

#pragma region "【轨迹线】"

            MovingTargetMainMapLegendInfo legend_trace_line;
            legend_trace_line.qstrSymbolCode = "trace_line";
            QgsLayoutItemPicture* pPictureItem_TraceLine = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_TraceLine");
            QgsLayoutItemLabel* pLabelItem_TraceLine = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TraceLine");
            legend_trace_line.pSymbolPicture = pPictureItem_TraceLine;
            legend_trace_line.pSymbolLabel = pLabelItem_TraceLine;
            vMainMapLegends.push_back(legend_trace_line);

#pragma endregion

            // 依次循环设置图例位置
            // 第一个图例示意图左上角点坐标，横坐标：地图外图廓右边界+6，纵坐标为图例标题下边界+3
            // 图例标题
            QgsLayoutItemLabel* pTextItem_MainMapLegendTemp = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
                "TextItem_MainMapLegend");


            SE_DPoint dFirstLegendLeftTopLayoutPoint;
            dFirstLegendLeftTopLayoutPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() + LEGEND_OFFSET_X;
            dFirstLegendLeftTopLayoutPoint.dy = pTextItem_MainMapLegendTemp->positionWithUnits().y() + pTextItem_MainMapLegendTemp->rect().height() + 3;


            for (int iLegendIndex = 0; iLegendIndex < vMainMapLegends.size(); iLegendIndex++)
            {
                // 图例图片位置
                vMainMapLegends[iLegendIndex].pSymbolPicture->attemptMove(QgsLayoutPoint(dFirstLegendLeftTopLayoutPoint.dx,
                    dFirstLegendLeftTopLayoutPoint.dy + double(iLegendIndex * 6)));

                // 图例文本位置
                vMainMapLegends[iLegendIndex].pSymbolLabel->attemptMove(QgsLayoutPoint(dFirstLegendLeftTopLayoutPoint.dx + LEGEND_PICTURE_LABEL_DISTANCE,
                    dFirstLegendLeftTopLayoutPoint.dy + double(iLegendIndex * 6)));

            }
        }

#pragma endregion

#pragma region "【11】鹰眼图地图"

        /*鹰眼地图设置*/
        QgsLayoutItemMap* pEagleEyeMapItem = (QgsLayoutItemMap*)GetLayoutItemByName(qLayoutItemList, "MapItem_EagleEyeMap");
        if (!pEagleEyeMapItem)
        {
          // 记录日志
          QgsMessageLog::logMessage(tr("镶嵌线地图控件获取失败！"), tr("影像图制图"), Qgis::Critical);
        }

        /*判断是否绘制鹰眼图*/
        if (ui.checkBox_EagleEye->isChecked())
        {
          /*镶嵌线地图只显示镶嵌线图层*/
          pEagleEyeMapItem->setLayers(qEagleEyeMapLayerList);

          // 鹰眼图显示投影矩形范围
          QgsRectangle qgsEagleEyeDisplayRect;
          // 如果鹰眼图是地理坐标系
          if (vecLayerEagleEye->crs().isGeographic())
          {
            pEagleEyeMapItem->setCrs(QgsCoordinateReferenceSystem(tr("EPSG:3857")));

            // 根据鹰眼图数据web墨卡托投影范围进行设置
            qgsEagleEyeDisplayRect.setXMinimum(dProjEagleLeft);
            qgsEagleEyeDisplayRect.setXMaximum(dProjEagleRight);
            qgsEagleEyeDisplayRect.setYMinimum(dProjEagleBottom);
            qgsEagleEyeDisplayRect.setYMaximum(dProjEagleTop);
          }
          else
          {
            pEagleEyeMapItem->setCrs(vecLayerEagleEye->crs());
            qgsEagleEyeDisplayRect.setXMinimum(dProjEagleLeft);
            qgsEagleEyeDisplayRect.setXMaximum(dProjEagleRight);
            qgsEagleEyeDisplayRect.setYMinimum(dProjEagleBottom);
            qgsEagleEyeDisplayRect.setYMaximum(dProjEagleTop);
          }


          pEagleEyeMapItem->setVisibility(true);


          pEagleEyeMapItem->attemptSetSceneRect(QRectF(interiorMapLayoutPoint_D.dx + MOSAIC_MAP_LEFT_X,
            m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3,
            MOSAIC_MAP_WIDTH,
            MOSAIC_MAP_HEIGHT));


          pEagleEyeMapItem->zoomToExtent(qgsEagleEyeDisplayRect);
          pEagleEyeMapItem->setBackgroundColor(QColor(255, 255, 255, 0));
          // 默认显示边框
          pEagleEyeMapItem->setFrameEnabled(true);
        }
        else
        {
          pEagleEyeMapItem->setVisibility(false);
        }

#pragma endregion

#pragma region "将外图廓整饰面要素图层加入到主地图中"

        if (!bLayoutDecPolygonExisted)
        {
          // 整饰面要素图层路径
          QString qstrLayoutLayerDecPolygonPath = GetDataSavePath() + "/" + qstrMapName + "/DecPolygon.shp";

          // 获取布局点样式文件路径
          QString qstrLayoutDecPolygonQmlFilePath;


          GetLayoutDecPolygonQmlFilePath(qstrLayoutDecPolygonQmlFilePath);


          // 判断图层是否存在
          // 如果布局面图层文件存在
          if (QFileInfo::exists(qstrLayoutLayerDecPolygonPath))
          {

            /*将矢量图层加载到地图图层列表中*/
            QString fileNameVector = qstrLayoutLayerDecPolygonPath;
            QFileInfo fileInfoVector(fileNameVector);

            // 获取数据名称
            QString baseNameVector = fileInfoVector.completeBaseName();

            // 通过图层类型获取对应的样式文件全路径
            QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称

            /*获取qml名称*/


            pDecPolygonLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
            if (!pDecPolygonLayer->isValid())
            {
              // 记录日志
              QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("动目标专题制图"), Qgis::Warning);
              return;
            }

            /*图层已有的样式*/
            QgsMapLayerStyle oldStyle = pDecPolygonLayer->styleManager()->style(pDecPolygonLayer->styleManager()->currentStyle());
            QString message;
            bool defaultLoadedFlag = false;
            message = pDecPolygonLayer->loadNamedStyle(qstrLayoutDecPolygonQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

            pAddedDecPolygonLayer = pProject->addMapLayer(pDecPolygonLayer);
            if (!pAddedDecPolygonLayer)
            {
              delete pDecPolygonLayer;
              // 记录日志
              QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("动目标专题制图"), Qgis::Warning);
              return;
            }
            qMapLayerList.append(pAddedDecPolygonLayer);
            qMainMapLayerList.append(pAddedDecPolygonLayer);
          }
        }

#pragma endregion
#pragma endregion

#pragma region "10、导出地图、元数据、工程文件"

        //  确保主地图项包含所有需要的图层，这样，`pMainMapItem`将包含所有加载的图层，包括轨迹线图层，从而确保在导出GeoPDF时所有图层都被渲染
        qMainMapLayerList.append(pLayerRaster);
        pMainMapItem->setLayers(qMainMapLayerList);


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
        //  地图尺寸调整
        dMapWidth = dTracePointLayerRect.GetWidth() * 111000 / dMapScale * 1000.0;		// 单位：毫米
        dMapHeight = dTracePointLayerRect.GetHeight() * 111000 / dMapScale * 1000.0;		// 单位：毫米
        double width = dMapWidth + 500;     // 单位：毫米
        double height = dMapHeight + 500;    // 单位：毫米
        QgsUnitTypes::LayoutUnit units = QgsUnitTypes::LayoutMillimeters;
        QgsLayoutSize pageSize = QgsLayoutSize(width, height, units);
        pPage->setPageSize(pageSize);

        // 导出地图路径下为每个图幅创建一个导出数据文件夹，包括地图文件、工程名称、影像图元数据
        QString qstrMapSavePath = m_qstrMappingDataSavePath + "/" + qstrMapName;
        string strMapSavePath = qstrMapSavePath.toUtf8().data();
#ifdef OS_FAMILY_WINDOWS
        if (std::filesystem::exists(std::filesystem::u8path(strMapSavePath)))
        {
          //  nothing
        }
        else
        {
          std::filesystem::create_directories(std::filesystem::u8path(strMapSavePath));
        }
        //_mkdir(strMapSavePath.c_str());
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

void QgsMovingTargetMappingLayoutVectorizationDialog::ComboPageSizeSelectedChanged()
{
    /*A0：841×1189   B0：1000×1414
      A1：594×841	  B1：707×1000
      A2：420×594    B2：500×707
      A3：297×420    B3：353×500
      A4：210×297    B4：250×353*/
    /*int index = ui.comboBox_PageSize->currentIndex();

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
    }*/

}

void QgsMovingTargetMappingLayoutVectorizationDialog::ComboPageDirectionSelectedChanged()
{
    /*int index = ui.comboBox_PageDirection->currentIndex();
    if (index == 0)
    {
        m_iPageDirection = 1;
    }
    else
    {
        m_iPageDirection = 2;
    }*/

}

void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapNameFontSize()
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

void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapNameFontColor()
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

void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapAgencyFontSize()
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

void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapAgencyFontColor()
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
void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapDetailFontSize()
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
void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapDetailFontColor()
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

void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapMovingTargetFontSize()
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

void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapMovingTargetFontColor()
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

void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapSizeRadioButton()
{
    /*ui.comboBox_PageDirection->setEnabled(true);
    ui.comboBox_PageSize->setEnabled(true);
    ui.lineEdit_PageWidth->setEnabled(true);
    ui.lineEdit_PageHeight->setEnabled(true);
    ui.lineEdit_Scale->setEnabled(false);*/
}

void QgsMovingTargetMappingLayoutVectorizationDialog::SetMapScaleRadioButton()
{
    /*ui.comboBox_PageDirection->setEnabled(false);
    ui.comboBox_PageSize->setEnabled(false);
    ui.lineEdit_PageWidth->setEnabled(false);
    ui.lineEdit_PageHeight->setEnabled(false);
    ui.lineEdit_Scale->setEnabled(true);*/
}

void QgsMovingTargetMappingLayoutVectorizationDialog::SetSimpleRenderRadioButton()
{
    ui.pushButton_SetColor->setEnabled(true);
}

void QgsMovingTargetMappingLayoutVectorizationDialog::SetCategoryRenderRadioButton()
{
    ui.pushButton_SetColor->setEnabled(false);
}

/*根据两点确定的直线计算点坐标Y*/
void QgsMovingTargetMappingLayoutVectorizationDialog::CalCoordYByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputX, double& dOutputY)
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
void QgsMovingTargetMappingLayoutVectorizationDialog::CalCoordXByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputY, double& dOutputX)
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

void QgsMovingTargetMappingLayoutVectorizationDialog::CalGaussBorderCoords_A_B(SE_DPoint dBeginPoint,
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

void QgsMovingTargetMappingLayoutVectorizationDialog::CalGaussBorderCoords_C_B(SE_DPoint dBeginPoint,
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


void QgsMovingTargetMappingLayoutVectorizationDialog::CalGaussBorderCoords_D_C(SE_DPoint dBeginPoint,
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

void QgsMovingTargetMappingLayoutVectorizationDialog::CalGaussBorderCoords_D_A(SE_DPoint dBeginPoint,
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

int QgsMovingTargetMappingLayoutVectorizationDialog::ClockWise(vector<SE_DPoint>& vPoints)
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

SE_DPoint QgsMovingTargetMappingLayoutVectorizationDialog::LineIntersect(SE_DPoint p0,SE_DPoint p1,SE_DPoint p2,SE_DPoint p3)
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

int QgsMovingTargetMappingLayoutVectorizationDialog::CalIndexInVector_XCoord(double dx, vector<SE_DPoint>& vPoints)
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

int QgsMovingTargetMappingLayoutVectorizationDialog::CalIndexInVector_YCoord(double dy, vector<SE_DPoint>& vPoints)
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

void QgsMovingTargetMappingLayoutVectorizationDialog::TransformLayoutToProjCRS(SE_DPoint dRefPoint,
    SE_DPoint dRefLayoutPoint,
    SE_DPoint dLayoutPoint,
    double dMapUnitToLayoutUnit,
    SE_DPoint& dProjPoint)
{
    // 投影横坐标X
    dProjPoint.dx = dRefPoint.dx + (dLayoutPoint.dx - dRefLayoutPoint.dx) / dMapUnitToLayoutUnit;

    // 投影纵坐标Y
    dProjPoint.dy = dRefPoint.dy - (dLayoutPoint.dy - dRefLayoutPoint.dy) / dMapUnitToLayoutUnit;

}

void QgsMovingTargetMappingLayoutVectorizationDialog::CalEndPointByStartPointAndAngle(
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

void QgsMovingTargetMappingLayoutVectorizationDialog::OpenTracePointData()
{
  // 提取目录路径
  QFileInfo fileInfo(m_qstrTracePointDataPath);
  QString curPath = fileInfo.exists() ? fileInfo.absolutePath() : QString();

  // 设置过滤器
  QString filter = tr("Shapefiles (*.shp *.SHP)");
  QString fileName = QFileDialog::getOpenFileName(this, tr("加载轨迹点数据"), curPath, filter);

  if (fileName.isEmpty()) // 如果文件未选择则返回
  {
    // 根据需求决定是否显示提示
    QgsMessageLog::logMessage(tr("请加载轨迹点图层！"), tr("动目标专题图制作"), Qgis::Warning);
    return;
  }

  // 验证文件存在性和格式
  if (!QFile::exists(fileName) || !fileName.endsWith(".shp", Qt::CaseInsensitive)) {
    QgsMessageLog::logMessage(tr("选择的文件无效，请选择一个有效的 Shapefile！"), tr("动目标专题图制作"), Qgis::Warning);
    return;
  }

  // 设置路径
  ui.lineEdit_TracePointVectorDataPath->setText(fileName);
  m_qstrTracePointDataPath = fileName;
}


void QgsMovingTargetMappingLayoutVectorizationDialog::OpenTraceLineData()
{
    // 提取目录路径
    QFileInfo fileInfo(m_qstrTraceLineDataPath);
    QString curPath = fileInfo.exists() ? fileInfo.absolutePath() : QString();


    // 设置过滤器
    QString filter = tr("Shapefiles (*.shp *.SHP)");
    QString fileName = QFileDialog::getOpenFileName(this, tr("加载轨迹线数据"), curPath, filter);

    if (fileName.isEmpty()) // 如果文件未选择则返回
    {
      // 根据需求决定是否显示提示
      QgsMessageLog::logMessage(tr("请加载轨迹线图层！"), tr("动目标专题图制作"), Qgis::Warning);
      return;
    }

    // 验证文件存在性和格式
    if (!QFile::exists(fileName) || !fileName.endsWith(".shp", Qt::CaseInsensitive)) {
      QgsMessageLog::logMessage(tr("选择的文件无效，请选择一个有效的 Shapefile！"), tr("动目标专题图制作"), Qgis::Warning);
      return;
    }


    // 设置路径
    ui.lineEdit_TraceLineDataPath->setText(fileName);
    m_qstrTraceLineDataPath = fileName;
}

void QgsMovingTargetMappingLayoutVectorizationDialog::OpenEagleEyeData()
{
    // 提取目录路径
    QFileInfo fileInfo(m_qstrEagleEyeDataPath);
    QString curPath = fileInfo.exists() ? fileInfo.absolutePath() : QString();


    // 设置过滤器
    QString filter = tr("Shapefiles (*.shp *.SHP)");
    QString fileName = QFileDialog::getOpenFileName(this, tr("加载鹰眼图数据"), curPath, filter);

    if (fileName.isEmpty()) // 如果文件未选择则返回
    {
      // 根据需求决定是否显示提示
      QgsMessageLog::logMessage(tr("请加载鹰眼图图层！"), tr("动目标专题图制作"), Qgis::Warning);
      return;
    }

    // 验证文件存在性和格式
    if (!QFile::exists(fileName) || !fileName.endsWith(".shp", Qt::CaseInsensitive)) {
      QgsMessageLog::logMessage(tr("选择的文件无效，请选择一个有效的 Shapefile！"), tr("动目标专题图制作"), Qgis::Warning);
      return;
    }


    // 设置路径
    ui.lineEdit_EagleEyeDataPath->setText(fileName);
    m_qstrEagleEyeDataPath = fileName;
}

void QgsMovingTargetMappingLayoutVectorizationDialog::MovingTargetMappingDataSave()
{
  // 提取目录路径
  QFileInfo dirInfo(m_qstrMappingDataSavePath);
  QString curPath = dirInfo.exists() ? dirInfo.absolutePath() : QString();

  QString dlgTitle = tr("请选择数据保存路径");
  QString selectedDir = QFileDialog::getExistingDirectory(
    this,
    dlgTitle,
    curPath,
    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (!selectedDir.isEmpty())
  {
    ui.lineEdit_OutputDataPath->setText(selectedDir);
    m_qstrMappingDataSavePath = selectedDir;
  }
}


// 判断目录是否存在
bool QgsMovingTargetMappingLayoutVectorizationDialog::FilePathIsExisted(QString qstrPath)
{
    QDir dir(qstrPath);
    return dir.exists();
}

// 判断文件是否存在
bool QgsMovingTargetMappingLayoutVectorizationDialog::FileIsExisted(QString qstrFilePath)
{
    return QFile::exists(qstrFilePath);
}

// 输入框限制
void QgsMovingTargetMappingLayoutVectorizationDialog::InitLineEdit()
{
    // 图名——限制特殊字符
    ui.lineEdit_MapName->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

    // 制图单位——限制特殊字符
    //ui.lineEdit_MapAgency->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

    // 纸张宽度
    //ui.lineEdit_PageWidth->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

    // 纸张高度
    //ui.lineEdit_PageHeight->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

    // 经纬网间隔
    QDoubleValidator* qDoubleVal_GeoGrid = new QDoubleValidator();
    qDoubleVal_GeoGrid->setRange(0.01, 10, 6);
    qDoubleVal_GeoGrid->setNotation(QDoubleValidator::StandardNotation);
    //ui.lineEdit_GeoGridIntervalLon->setValidator(qDoubleVal_GeoGrid);
    //ui.lineEdit_GeoGridIntervalLat->setValidator(qDoubleVal_GeoGrid);

    // 比例尺
    ui.lineEdit_Scale->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

}

void QgsMovingTargetMappingLayoutVectorizationDialog::WebMercator_BL_TO_XY(double B, double L, double& X, double& Y)
{
    if (B >= 86)
    {
        B = 85.05112878;
    }

    if (B <= -86)
    {
        B = -85.05112878;
    }

    // L、B坐标单位为：度
    double lon = L;
    double lat = B;
    X = lon * 20037508.34 / 180;
    Y = log(tan((90 + lat) * PAI / 360.0)) / (PAI / 180.0);
    Y = Y * 20037508.34 / 180.0;
}

void QgsMovingTargetMappingLayoutVectorizationDialog::WebMercator_XY_TO_BL(double X, double Y, double& B, double& L)
{
    // L、B坐标单位为：度
    L = (X / 20037508.34) * 180.0;
    B = (Y / 20037508.34) * 180.0;
    B = 180.0 / PAI * (2 * atan(exp(B * PAI / 180)) - PAI / 2);
}

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatLonDegree(int iDegree)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatLatDegree(int iDegree)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatLonDegree_Double(double dDegree)
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

QString QgsMovingTargetMappingLayoutVectorizationDialog::FormatLatDegree_Double(double dDegree)
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

//void QgsMovingTargetMappingLayoutVectorizationDialog::CalGeoBorderCoords_A_B(SE_DPoint dBeginPoint,
//    SE_DPoint dEndPoint,
//    double dLonInterval,
//    vector<SE_DPoint>& vGeoBorderCoords)
//{
//    vGeoBorderCoords.clear();
//
//    // X方向起始整间隔数，向上取整
//    int iStartGeo_X = ceil(dBeginPoint.dx / dLonInterval);
//
//    // X方向终止整间隔数
//    int iEndGeo_X = int(dEndPoint.dx / dLonInterval);
//
//    for (int i = iStartGeo_X; i <= iEndGeo_X; i++)
//    {
//        SE_DPoint dPoint;
//        dPoint.dx = i * dLonInterval;
//        dPoint.dy = dBeginPoint.dy;
//
//        // 增加坐标数值有效性判断
//        if (dPoint.dx <= dBeginPoint.dx || dPoint.dx >= dEndPoint.dx)
//        {
//            continue;
//        }
//
//        vGeoBorderCoords.push_back(dPoint);
//    }
//}
//
//void QgsMovingTargetMappingLayoutVectorizationDialog::CalGeoBorderCoords_C_B(SE_DPoint dBeginPoint,
//    SE_DPoint dEndPoint,
//    double dLatInterval,
//    vector<SE_DPoint>& vGeoBorderCoords)
//{
//    vGeoBorderCoords.clear();
//
//    // Y方向起始整数纬度间隔，向上取整
//    int iStartGeo_Y = ceil(dBeginPoint.dy / dLatInterval);
//
//    // Y方向终止整数纬度间隔
//    int iEndGeo_Y = int(dEndPoint.dy / dLatInterval);
//
//    for (int i = iStartGeo_Y; i <= iEndGeo_Y; i++)
//    {
//        SE_DPoint dPoint;
//        dPoint.dy = i * dLatInterval;
//        dPoint.dx = dBeginPoint.dx;
//
//        // 增加坐标数值有效性判断
//        if (dPoint.dy <= dBeginPoint.dy || dPoint.dy >= dEndPoint.dy)
//        {
//            continue;
//        }
//        vGeoBorderCoords.push_back(dPoint);
//    }
//}
//
//void QgsMovingTargetMappingLayoutVectorizationDialog::CalGeoBorderCoords_D_C(SE_DPoint dBeginPoint,
//    SE_DPoint dEndPoint,
//    double dLonInterval,
//    vector<SE_DPoint>& vGeoBorderCoords)
//{
//    vGeoBorderCoords.clear();
//
//    // X方向起始经度间隔整数倍，向上取整
//    int iStartGeo_X = ceil(dBeginPoint.dx / dLonInterval);
//
//    // X方向终止纬度间隔整数倍
//    int iEndGeo_X = int(dEndPoint.dx / dLonInterval);
//
//    for (int i = iStartGeo_X; i <= iEndGeo_X; i++)
//    {
//        SE_DPoint dPoint;
//        dPoint.dx = i * dLonInterval;
//        dPoint.dy = dBeginPoint.dy;
//
//        // 增加坐标数值有效性判断
//        if (dPoint.dx <= dBeginPoint.dx || dPoint.dx >= dEndPoint.dx)
//        {
//            continue;
//        }
//
//        vGeoBorderCoords.push_back(dPoint);
//    }
//}
//
//void QgsMovingTargetMappingLayoutVectorizationDialog::CalGeoBorderCoords_D_A(SE_DPoint dBeginPoint,
//    SE_DPoint dEndPoint,
//    double dLatInterval,
//    vector<SE_DPoint>& vGeoBorderCoords)
//{
//    vGeoBorderCoords.clear();
//
//    // Y方向起始纬度间隔整数倍，向上取整
//    int iStartGeo_Y = ceil(dBeginPoint.dy / dLatInterval);
//
//    // Y方向终止纬度间隔整数倍
//    int iEndGeo_Y = int(dEndPoint.dy / dLatInterval);
//
//    for (int i = iStartGeo_Y; i <= iEndGeo_Y; i++)
//    {
//        SE_DPoint dPoint;
//        dPoint.dy = i * dLatInterval;
//        dPoint.dx = dBeginPoint.dx;
//
//        // 增加坐标数值有效性判断
//        if (dPoint.dy <= dBeginPoint.dy || dPoint.dy >= dEndPoint.dy)
//        {
//            continue;
//        }
//
//        vGeoBorderCoords.push_back(dPoint);
//    }
//}

void QgsMovingTargetMappingLayoutVectorizationDialog::CalGeoBorderCoords_A_B(SE_DPoint dBeginPoint,
  SE_DPoint dEndPoint,
  double dLonInterval,
  vector<SE_DPoint>& vGeoBorderCoords)
{
  vGeoBorderCoords.clear();

  double startLon = dBeginPoint.dx;
  double endLon = dEndPoint.dx;

  // 确保 startLon <= endLon
  if (startLon > endLon)
  {
    std::swap(startLon, endLon);
  }

  // 添加两条经线，将区域均分为三等份
  int numDivisions = 3;

  for (int i = 1; i < numDivisions; ++i)
  {
    SE_DPoint dPoint;
    dPoint.dx = startLon + i * dLonInterval;
    dPoint.dy = dBeginPoint.dy;

    vGeoBorderCoords.push_back(dPoint);
  }
}

void QgsMovingTargetMappingLayoutVectorizationDialog::CalGeoBorderCoords_C_B(SE_DPoint dBeginPoint,
  SE_DPoint dEndPoint,
  double dLatInterval,
  vector<SE_DPoint>& vGeoBorderCoords)
{
  vGeoBorderCoords.clear();

  double startLat = dBeginPoint.dy;
  double endLat = dEndPoint.dy;

  // 确保 startLat <= endLat
  if (startLat > endLat)
  {
    std::swap(startLat, endLat);
  }

  // 添加两条纬线，将区域均分为三等份
  int numDivisions = 3;

  for (int i = 1; i < numDivisions; ++i)
  {
    SE_DPoint dPoint;
    dPoint.dx = dBeginPoint.dx;
    dPoint.dy = startLat + i * dLatInterval;

    vGeoBorderCoords.push_back(dPoint);
  }
}

void QgsMovingTargetMappingLayoutVectorizationDialog::CalGeoBorderCoords_D_C(SE_DPoint dBeginPoint,
  SE_DPoint dEndPoint,
  double dLonInterval,
  vector<SE_DPoint>& vGeoBorderCoords)
{
  vGeoBorderCoords.clear();

  double startLon = dBeginPoint.dx;
  double endLon = dEndPoint.dx;

  // 确保 startLon <= endLon
  if (startLon > endLon)
  {
    std::swap(startLon, endLon);
  }

  // 添加两条经线，将区域均分为三等份
  int numDivisions = 3;

  for (int i = 1; i < numDivisions; ++i)
  {
    SE_DPoint dPoint;
    dPoint.dx = startLon + i * dLonInterval;
    dPoint.dy = dBeginPoint.dy;

    vGeoBorderCoords.push_back(dPoint);
  }
}

void QgsMovingTargetMappingLayoutVectorizationDialog::CalGeoBorderCoords_D_A(SE_DPoint dBeginPoint,
  SE_DPoint dEndPoint,
  double dLatInterval,
  vector<SE_DPoint>& vGeoBorderCoords)
{
  vGeoBorderCoords.clear();

  double startLat = dBeginPoint.dy;
  double endLat = dEndPoint.dy;

  // 确保 startLat <= endLat
  if (startLat > endLat)
  {
    std::swap(startLat, endLat);
  }

  // 添加两条纬线，将区域均分为三等份
  int numDivisions = 3;

  for (int i = 1; i < numDivisions; ++i)
  {
    SE_DPoint dPoint;
    dPoint.dx = dBeginPoint.dx;
    dPoint.dy = startLat + i * dLatInterval;

    vGeoBorderCoords.push_back(dPoint);
  }
}


void QgsMovingTargetMappingLayoutVectorizationDialog::TransformProjectToLayout(SE_DPoint dRefProjectPoint, SE_DPoint dRefLayoutPoint, SE_DPoint dProjPoint, double dMapUnitToLayoutUnit, SE_DPoint& dLayoutPoint)
{
    dLayoutPoint.dx = dRefLayoutPoint.dx + (dProjPoint.dx - dRefProjectPoint.dx) * dMapUnitToLayoutUnit;
    dLayoutPoint.dy = dRefLayoutPoint.dy + (dRefProjectPoint.dy - dProjPoint.dy) * dMapUnitToLayoutUnit;
}

int QgsMovingTargetMappingLayoutVectorizationDialog::Get_Point(OGRFeature* poFeature, SE_DPoint& coordinate, vector<FieldNameAndValue_MovingTarget>& vFieldvalue)
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

        FieldNameAndValue_MovingTarget structFieldValue;
        structFieldValue.strFieldName = strFieldName;
        structFieldValue.strFieldValue = strValue;
        vFieldvalue.push_back(structFieldValue);
    }
    return 0;
}

void QgsMovingTargetMappingLayoutVectorizationDialog::GetLayerFields(OGRLayer* pLayer,
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

int QgsMovingTargetMappingLayoutVectorizationDialog::SetFieldDefn(OGRLayer* poLayer,
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

int QgsMovingTargetMappingLayoutVectorizationDialog::Set_Point(OGRLayer* poLayer, double x, double y, double z, vector<FieldNameAndValue_MovingTarget>& vFieldAndValue)
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

//  杨小兵-2024-10-17：windows平台中的fopen不支持UTF-8字符集编码的路径
//bool QgsMovingTargetMappingLayoutVectorizationDialog::CreateShapefileCPG(
//  string strCPGFilePath,
//  string strEncoding /*= "System"*/)
//{
//    FILE* fp = fopen(strCPGFilePath.c_str(), "w");
//    if (!fp)
//    {
//        return false;
//    }
//
//    fprintf(fp, "%s", strEncoding.c_str());
//
//    fclose(fp);
//
//    return true;
//}

bool QgsMovingTargetMappingLayoutVectorizationDialog::CreateShapefileCPG(
  string strCPGFilePath,
  string strEncoding /*= "System"*/)
{
  const QString& qstrCPGFilePath = QString::fromStdString(strCPGFilePath);
  const QString& qstrEncoding = QString::fromStdString(strEncoding);
  QFile file(qstrCPGFilePath);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    QgsMessageLog::logMessage(tr("无法创建文件：%1").arg(qstrCPGFilePath), tr("动目标专题制图"), Qgis::Critical);
    return false;
  }

  QTextStream out(&file);
  out.setCodec("UTF-8"); // 确保使用 UTF-8 编码写入
  out << qstrEncoding;

  file.close();

  return true;
}


void QgsMovingTargetMappingLayoutVectorizationDialog::GetLayoutDecPointQmlFilePath(	QString& qstrLayoutDecPointQmlPath)
{
    QString qstrQmlPath = "";
    // 运行环境路径
    QString qstrRunDirPath = QCoreApplication::applicationDirPath();

#ifdef OS_FAMILY_WINDOWS

    qstrLayoutDecPointQmlPath = qstrRunDirPath + "\\style\\theme\\moving_target\\layout_decpoint.qml";

#else
    qstrLayoutDecPointQmlPath = qstrRunDirPath + "/style/theme/moving_target/layout_decpoint.qml";

#endif

    qstrLayoutDecPointQmlPath = QDir::toNativeSeparators(qstrLayoutDecPointQmlPath);
}

void QgsMovingTargetMappingLayoutVectorizationDialog::GetLayoutDecPolygonQmlFilePath(QString& qstrLayoutDecPolygonQmlPath)
{
    QString qstrQmlPath = "";
    // 运行环境路径
    QString qstrRunDirPath = QCoreApplication::applicationDirPath();


#ifdef OS_FAMILY_WINDOWS

    qstrLayoutDecPolygonQmlPath = (qstrRunDirPath + "\\style\\theme\\moving_target\\layout_decpolygon.qml").toUtf8();

#else
    qstrLayoutDecPolygonQmlPath = (qstrRunDirPath + "/style/theme/moving_target/layout_decpolygon.qml").toUtf8();

#endif

    qstrLayoutDecPolygonQmlPath = QDir::toNativeSeparators(qstrLayoutDecPolygonQmlPath).toUtf8();
}

int QgsMovingTargetMappingLayoutVectorizationDialog::Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing, vector<FieldNameAndValue_MovingTarget>& vFieldAndValue)
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
    // polygon
    OGRPolygon polygon;
    // 外环
    OGRLinearRing ringOut;
    for (int i = 0; i < OuterRing.size(); i++)
    {
        ringOut.addPoint(OuterRing[i].dx, OuterRing[i].dy, OuterRing[i].dz);
    }
    // 结束点应和起始点相同，保证多边形闭合
    ringOut.closeRings();
    polygon.addRing(&ringOut);

    poFeature->SetGeometry((OGRGeometry*)(&polygon));
    if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
    {
        return -1;
    }
    OGRFeature::DestroyFeature(poFeature);
    return 0;
}

bool QgsMovingTargetMappingLayoutVectorizationDialog::ChunkImageDataToJPEG(
    string strRasterFullName,
    double dLeft,
    double dTop,
    double dRight,
    double dBottom,
    string strOutputPath,
    QString &qstrOutputRasterFileFullName)
{
    // 分块像素个数，JPEG最大支持65500*65500
    double dBlockPixelCount = 65500;

    // 将数据按照60000*60000进行划分范围，每块分别进行jpg输出，然后将分块输出的jpg进行融合成完整的jpg
    // 输入影像图层名称
    string strRasterName = CPLGetBasename(strRasterFullName.c_str());

    // 返回
    int bUsageError = FALSE;
    // 输入列表
    GDALDatasetH pSrcDataSet = GDALOpen(strRasterFullName.c_str(), GA_ReadOnly);
    if (!pSrcDataSet)
    {
        QString qstrTitle = tr("影像数据格式转换工具");
        QString qstrText = tr("打开影像数据文件失败！");
        QMessageBox msgBox;
        msgBox.setWindowTitle(qstrTitle);
        msgBox.setText(qstrText);
        msgBox.setStandardButtons(QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.exec();
        return false;
    }


        //添加命令参数,每次添加一个!!!
        char** argv = nullptr;

        // 无效值设置
        argv = CSLAddString(argv, "-a_nodata");
        argv = CSLAddString(argv, "0");

        // 按像素裁切范围
        argv = CSLAddString(argv, "-projwin");

        char szLeft[50] = { 0 };
        sprintf(szLeft, "%f", dLeft);
        argv = CSLAddString(argv, szLeft);

        char szTop[50] = { 0 };
        sprintf(szTop, "%f", dTop);
        argv = CSLAddString(argv, szTop);

        char szRight[50] = { 0 };
        sprintf(szRight, "%f", dRight);
        argv = CSLAddString(argv, szRight);

        char szBottom[50] = { 0 };
        sprintf(szBottom, "%f", dBottom);
        argv = CSLAddString(argv, szBottom);

        // 输出格式
        argv = CSLAddString(argv, "-of");

        argv = CSLAddString(argv, "GTiff");

        string strMapName = GetMapName().toStdString();
        string strOutputTempPath = strOutputPath + "/" + strMapName;

#ifdef OS_FAMILY_WINDOWS
        if (std::filesystem::exists(std::filesystem::u8path(strOutputTempPath)))
        {
          //  nothing
        }
        else
        {
          std::filesystem::create_directories(std::filesystem::u8path(strOutputTempPath));
        }
        //_mkdir(strOutputTempPath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
        mkdir(strOutputTempPath.c_str(), MODE);
#endif

          string strOutputFullName = strOutputTempPath + "/" + strRasterName + ".tif";


        qstrOutputRasterFileFullName = QString::fromUtf8(strOutputFullName.c_str());

        // 输入数据路径
        argv = CSLAddString(argv, strRasterFullName.c_str());

        // 输出数据路径
        argv = CSLAddString(argv, strOutputFullName.c_str());

        // GDALTranslate
        GDALTranslateOptions* opt = GDALTranslateOptionsNew(argv, NULL);
        if (!opt)
        {
            return false;
        }

        GDALDataset* dst = (GDALDataset*)GDALTranslate(strOutputFullName.c_str(), pSrcDataSet, opt, &bUsageError);
        if (!dst)
        {
            GDALClose(dst);
            return false;
        }

        GDALTranslateOptionsFree(opt);
        CSLDestroy(argv);


    GDALClose(pSrcDataSet);

    return true;
}

void QgsMovingTargetMappingLayoutVectorizationDialog::SetEagleEyeLayerColor()
{
    // 默认黑色
    QColor iniColor = QColor(0, 0, 0);

    QColor color = QColorDialog::getColor(iniColor, this, tr("选择颜色"));
    if (color.isValid())
    {
        m_qEagleEyeLayerColor = color;
        m_bSetEagleEyeLayerColor = true;
    }
}
