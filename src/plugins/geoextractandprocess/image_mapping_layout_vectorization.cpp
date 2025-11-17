#include "qgsgui.h"
#include "image_mapping_layout_vectorization.h"
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

#include "symbology/qgscategorizedsymbolrenderer.h"
#include "qgis.h"
#include "qgsattributes.h"

#include <qgsfillsymbollayer.h>
#include <qgsfillsymbol.h>
#include <qgssinglesymbolrenderer.h>


/*----------------GDAL--------------*/
#include "gdal.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "gdal_utils.h"
/*-----------------------------------*/


/*默认导出地图DPI*/
const int LAYOUT_DPI = 300;
const double OUTER_MAP_INTERIOR_MAP_DISTANCE = 6;       // 内外图廓宽度，单位：毫米
const double KM_LINE_LENGTH = 6;            // 内外图廓方里网刻度线长度，单位：毫米
const double ADJ_KM_LINE_LENGTH = 1.5;        // 邻带方里网刻度线长度，单位：毫米，标准规范为1.5毫米
const double KM_LINE_TOLERANCE_X = 13;       // X方向方里网刻度线与角点容差，单位：毫米，避免与角点经度绘制重叠
const double KM_LINE_TOLERANCE_Y = 8;       // Y方向方里网刻度线与角点容差，单位：毫米，避免与角点纬度绘制重叠
const double IMAGE_MAPPING_ZERO = 1.0e-3;	// 判断高斯坐标和布局坐标相等容差
const double LEGEND_OFFSET_X = 6;			// 主地图图例左边界横坐标相对地图外图廓右边界距离，单位：毫米
const double GRAY_CIRLE_OFFSET_X = 6;			// 5万灰色圆形左边界相对地图外图廓右边界距离，单位：毫米
const double MAP_ANNOTATION_OFFSET_X = 6;	// 主地图附注左边界横坐标相对地图外图廓右边界距离，单位：毫米
const double LEGEND_PICTURE_LABEL_DISTANCE = 14;		// 图例图片左边界与图例文本左边界距离，单位：毫米
const double MOSAIC_LEGEND_LEFT_TOP_X = 158;		// 镶嵌线图例左上角布局坐标，单位：毫米
const double MOSAIC_MAP_WIDTH = 45;		// 镶嵌线图例左上角布局坐标，单位：毫米
const double MOSAIC_MAP_HEIGHT = 38;		// 镶嵌线图例左上角布局坐标，单位：毫米
const double MOSAIC_MAP_LEFT_X = 56;		// 镶嵌线地图窗口左边界布局坐标，单位：毫米
const double ADJ_KM_LINE_LENGTH_RIGHT = 4.5;            // 邻带方里网右边界绘制刻度与外图廓间距，单位：毫米


#define CLOCKWISE 1
#define COUNTERCLOCKWISE -1

using namespace std;

QgsImageMappingDialog_LayoutVectorization::QgsImageMappingDialog_LayoutVectorization(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);

	m_mCode_Names.clear();
	m_mName_Codes.clear();

	// 初始化要素类编码名称映射表
	InitFeatureClassNameCodeTable();

	// 设置影像制图方案默认☑数据源选项卡
	ui.tabWidget_ImageMappingScheme->setCurrentIndex(0);

	// 整饰要素默认☑图名选项卡
	ui.tabWidget_DecorationItem->setCurrentIndex(0);

	// 设置要素选择tabWidget默认☑状态
	// 图层对话框默认☑陆地交通层选项库
	ui.tabWidget_LayerType->setCurrentIndex(0);

	// 陆地交通层默认☑铁路选项卡
	ui.tabWidget_FeatureClass_D->setCurrentIndex(0);

	// 水域陆地层默认☑河流选项卡
	ui.tabWidget_FeatureClass_F->setCurrentIndex(0);

	// 陆地地貌层默认☑等高线选项卡
	ui.tabWidget_FeatureClass_J->setCurrentIndex(0);

	// 境界与政区层默认☑国界及领海选项卡
	ui.tabWidget_FeatureClass_K->setCurrentIndex(0);

	// 注记层默认☑居民地名称选项卡
	ui.tabWidget_FeatureClass_R->setCurrentIndex(0);

	ui.lineEdit_LayoutTemplatePath->setEnabled(false);

	// 初始化所有要素类列表为不选中状态
	m_vStrLayerName.push_back("D");
	m_vStrLayerName.push_back("F");
	m_vStrLayerName.push_back("J");
	m_vStrLayerName.push_back("K");
	m_vStrLayerName.push_back("R");
	for (int i = 0; i < m_vStrLayerName.size(); i++)
	{
		SetListWidgetAllUnSelectedStatus(m_vStrLayerName[i]);
	}

	m_vImageMappingSchema.clear();
	// 初始化对话框
	InitImageMappingSchema();

	// 整饰要素矢量化流程
	connect(ui.pushButton_ImageMapping, &QPushButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::ImageMapping_LayoutVectorization);

	connect(ui.pushButton_Close, &QPushButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::Close);

	connect(ui.pushButton_OpenVectorData, &QPushButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::OpenVectorData);
	connect(ui.pushButton_OpenImageData, &QPushButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::OpenImageData);
	connect(ui.pushButton_OpenMosaicData, &QPushButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::OpenMosaicData);
	connect(ui.pushButton_Save, &QPushButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::ImageMappingDataSave);
	connect(ui.listView_ImageMappingSchema, SIGNAL(clicked(const QModelIndex&)), this, SLOT(UpdateUIByImageMappingSchema(const QModelIndex&)));

	connect(ui.radioButton_LonLatRange, &QRadioButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::LonLatRadioButton_Click);
	connect(ui.radioButton_SheetList, &QRadioButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::SheetListRadioButton_Click);

	connect(ui.pushButton_LoadSheetList, &QPushButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::LoadSheetListFile);
	connect(ui.pushButton_LoadMappingSchema, &QPushButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::LoadMappingSchema);
	connect(ui.pushButton_SaveMappingSchema, &QPushButton::clicked, this, &QgsImageMappingDialog_LayoutVectorization::SaveMappingSchema);


	/*设置制图范围页面控件状态*/
	// 默认图幅列表方式
	ui.radioButton_SheetList->setEnabled(true);
	ui.radioButton_SheetList->setChecked(true);

	// 默认秘密级别
	ui.radioButton_MiMi->setEnabled(true);
	ui.radioButton_MiMi->setChecked(true);

	ui.lineEdit_Left->setEnabled(false);
	ui.lineEdit_Top->setEnabled(false);
	ui.lineEdit_Right->setEnabled(false);
	ui.lineEdit_Bottom->setEnabled(false);
	ui.plainTextEdit_SheetList->setEnabled(true);
	ui.pushButton_LoadSheetList->setEnabled(true);

	ui.lineEdit_MosaicDataRenderField->setText("catalog_id");
	ui.lineEdit_MosaicDataAcqDataField->setText("acq_date");
	ui.lineEdit_MosaicDataSensorField->setText("sensor");

	restoreState();
	ui.lineEdit_OutputDataPath->setText(m_qstrImageMappingDataSavePath);

	InitLineEdit();
}

QgsImageMappingDialog_LayoutVectorization::~QgsImageMappingDialog_LayoutVectorization()
{
	m_vImageMappingSchema.clear();
	m_mCode_Names.clear();
	m_mName_Codes.clear();

	// 设置当前保存路径
	QgsSettings settings;

	// 地图输出路径
	settings.setValue(QStringLiteral("QgsImageMappingDialog_LayoutVectorization/ImageMappingDataSavePath"), m_qstrImageMappingDataSavePath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("QgsImageMappingDialog_LayoutVectorization/ImageMappingSchemaSavePath"), m_qstrImageMappingSchemaSavePath, QgsSettings::Section::Plugins);
}

void QgsImageMappingDialog_LayoutVectorization::Close()
{
	accept();
}



void QgsImageMappingDialog_LayoutVectorization::OpenVectorData()
{
	// 选择文件夹
	QString curPath = m_qstrVectorDataPath;
	QString dlgTile = QObject::tr("请选择矢量数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_VectorDataPath->setText(selectedDir);
		m_qstrVectorDataPath = selectedDir;
	}
}

void QgsImageMappingDialog_LayoutVectorization::OpenImageData()
{
	// 选择文件夹
	QString curPath = m_qstrImageDataPath;
	QString dlgTile = QObject::tr("请选择影像数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_ImageDataPath->setText(selectedDir);
		m_qstrImageDataPath = selectedDir;
	}
}

void QgsImageMappingDialog_LayoutVectorization::OpenMosaicData()
{
	// 选择文件夹
	QString curPath = m_qstrMosaicDataPath;
	QString dlgTile = QObject::tr("请选择镶嵌线数据存储路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_MosaicDataPath->setText(selectedDir);
		m_qstrMosaicDataPath = selectedDir;
	}
}

void QgsImageMappingDialog_LayoutVectorization::ImageMappingDataSave()
{
	// 选择文件夹
	QString curPath = m_qstrImageMappingDataSavePath;
	QString dlgTile = QObject::tr("请选择数据保存路径");
	QString selectedDir = QFileDialog::getExistingDirectory(
		this,
		dlgTile,
		curPath,
		QFileDialog::ShowDirsOnly);

	if (!selectedDir.isEmpty())
	{
		ui.lineEdit_OutputDataPath->setText(selectedDir);
		m_qstrImageMappingDataSavePath = selectedDir;
	}
}

void QgsImageMappingDialog_LayoutVectorization::UpdateUIByImageMappingSchema(const QModelIndex& index)
{
	//点击获取当前索引index的内容
	QString qstrSchemaName = ui.listView_ImageMappingSchema->model()->data(index).toString();

	for (int i = 0; i < m_vImageMappingSchema.size(); i++)
	{
		// 根据所选制图方案更新对话框ui
		if (m_vImageMappingSchema[i].qstrName == qstrSchemaName)
		{
			m_ImageMappingSchema = m_vImageMappingSchema[i];
			SetUIByImageMappingSchema(m_vImageMappingSchema[i]);
		}
	}
}

void QgsImageMappingDialog_LayoutVectorization::LonLatRadioButton_Click()
{
	ui.lineEdit_Left->setEnabled(true);
	ui.lineEdit_Top->setEnabled(true);
	ui.lineEdit_Right->setEnabled(true);
	ui.lineEdit_Bottom->setEnabled(true);
	ui.plainTextEdit_SheetList->setEnabled(false);
	ui.pushButton_LoadSheetList->setEnabled(false);
}

void QgsImageMappingDialog_LayoutVectorization::SheetListRadioButton_Click()
{
	ui.lineEdit_Left->setEnabled(false);
	ui.lineEdit_Top->setEnabled(false);
	ui.lineEdit_Right->setEnabled(false);
	ui.lineEdit_Bottom->setEnabled(false);
	ui.plainTextEdit_SheetList->setEnabled(true);
	ui.pushButton_LoadSheetList->setEnabled(true);
}


QgsLayoutItem* QgsImageMappingDialog_LayoutVectorization::GetLayoutItemByName(QList<QgsLayoutItem*>& itemList, const QString qstrName)
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

QString QgsImageMappingDialog_LayoutVectorization::GetVectorDataPath()
{
	m_qstrVectorDataPath = ui.lineEdit_VectorDataPath->text();
	return m_qstrVectorDataPath;
}

QString QgsImageMappingDialog_LayoutVectorization::GetImageDataPath()
{
	m_qstrImageDataPath = ui.lineEdit_ImageDataPath->text();
	return m_qstrImageDataPath;
}

QString QgsImageMappingDialog_LayoutVectorization::GetMosaicDataPath()
{
	m_qstrMosaicDataPath = ui.lineEdit_MosaicDataPath->text();
	return m_qstrMosaicDataPath;
}

QString QgsImageMappingDialog_LayoutVectorization::GetDataSavePath()
{
	m_qstrImageMappingDataSavePath = ui.lineEdit_OutputDataPath->text();
	return m_qstrImageMappingDataSavePath;
}

QString QgsImageMappingDialog_LayoutVectorization::GetMapExportType()
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

QString QgsImageMappingDialog_LayoutVectorization::GetMainTitleSheet()
{
	/*QString qstrMainTitleSheet = ui.lineEdit_MapName_MainTitleSheet->text();
	return qstrMainTitleSheet;*/
	return "";
}

QString QgsImageMappingDialog_LayoutVectorization::GetMainTitleName()
{
	/*QString qstrMainTitleName = ui.lineEdit_MapName_MainTitleName->text();
	return qstrMainTitleName;*/
	return "";
}

QString QgsImageMappingDialog_LayoutVectorization::GetSubTitleProvinceName()
{
	/*QString qstrSubTitleProvinceName = ui.lineEdit_MapName_SubTitleProvince->text();
	return qstrSubTitleProvinceName;*/
	return "";
}

QString QgsImageMappingDialog_LayoutVectorization::GetSubTitleCountyName()
{
	/*QString qstrSubTitleCountyName = ui.lineEdit_MapName_SubTitleCounty->text();
	return qstrSubTitleCountyName;*/
	return "";
}

QString QgsImageMappingDialog_LayoutVectorization::GetSecurityClassification()
{
	if (ui.radioButton_JueMi->isChecked())
	{
		return tr("绝密");
	}
	else if (ui.radioButton_JiMi->isChecked())
	{
		return tr("机密");
	}
	else if (ui.radioButton_MiMi->isChecked())
	{
		return tr("秘密");
	}
	return "";
}

QString QgsImageMappingDialog_LayoutVectorization::GetMapDetail()
{
	QString qstrMapDetail = ui.textEdit_MapDetail->toPlainText();
	return qstrMapDetail;
}

QString QgsImageMappingDialog_LayoutVectorization::GetMapAgency()
{
	QString qstrMapAgency = ui.lineEdit_MapAgency->text();
	return qstrMapAgency;
}

QString QgsImageMappingDialog_LayoutVectorization::GetLeftTopCornerSheet()
{
	QString qstrLeftTopCornerSheet;// = ui.lineEdit_LeftTopSheet->text();
	return qstrLeftTopCornerSheet;
}

QString QgsImageMappingDialog_LayoutVectorization::GetRightBottomCornerSheet()
{
	QString qstrRightBottomCornerSheet;// = ui.lineEdit_RightBottomSheet->text();
	return qstrRightBottomCornerSheet;
}

QString QgsImageMappingDialog_LayoutVectorization::GetScale()
{
	QString qstrScale = ui.lineEdit_Scale->text();
	return qstrScale;
}

double QgsImageMappingDialog_LayoutVectorization::GetMagenticAngle()
{
	return 0;
	//return ui.lineEdit_MagenticAngle->text().toDouble();
}

double QgsImageMappingDialog_LayoutVectorization::GetMeridianConvergenceAngle()
{
	return 0;
	//return ui.lineEdit_MeridianConvergenceAngle->text().toDouble();
}

QString QgsImageMappingDialog_LayoutVectorization::GetMapAnnotation()
{
	QString qstrMapAnnotation = ui.lineEdit_Annotations->text();
	return qstrMapAnnotation;
}

QString QgsImageMappingDialog_LayoutVectorization::GetLayoutTemplateName()
{
	QString qstrLayoutTemplateName = ui.listWidget_LayoutTemplate->currentItem()->text();
	return qstrLayoutTemplateName;
}

QString QgsImageMappingDialog_LayoutVectorization::GetLayoutTemplateFullPath()
{
	QString qstrLayoutTemplateFullPath = ui.lineEdit_LayoutTemplatePath->text();
	return qstrLayoutTemplateFullPath;
}

void QgsImageMappingDialog_LayoutVectorization::GetImageMappingRange(double& dLeft, double& dTop, double& dRight, double& dBottom)
{
	QString qstrLeft = ui.lineEdit_Left->text();
	dLeft = qstrLeft.toDouble();

	QString qstrRight = ui.lineEdit_Right->text();
	dRight = qstrRight.toDouble();

	QString qstrTop = ui.lineEdit_Top->text();
	dTop = qstrTop.toDouble();

	QString qstrBottom = ui.lineEdit_Bottom->text();
	dBottom = qstrBottom.toDouble();
}

void QgsImageMappingDialog_LayoutVectorization::SetListWidgetAllUnSelectedStatus(const QString& strLayerName)
{
	// D、F、J、K、R
	// 存储list控件
	vector<QListWidget*> vQListWidget;
	vQListWidget.clear();

	// 陆地交通
	if (strLayerName == "D")
	{
		vQListWidget.push_back(ui.listWidget_TieLu);
		vQListWidget.push_back(ui.listWidget_GongLu);
		vQListWidget.push_back(ui.listWidget_QiTaDaoLu);
	}

	// 水域/陆地
	if (strLayerName == "F")
	{
		vQListWidget.push_back(ui.listWidget_HeLiu);
		vQListWidget.push_back(ui.listWidget_HuPoShuiKuChiTang);
		vQListWidget.push_back(ui.listWidget_LuDiDaoYuHaiYang);
}

	// 陆地地貌及土质
	if (strLayerName == "J")
	{
		vQListWidget.push_back(ui.listWidget_DengGaoXian);
		vQListWidget.push_back(ui.listWidget_DiMaoGaoCheng);
	}

	// 境界与政区
	if (strLayerName == "K")
	{
		vQListWidget.push_back(ui.listWidget_GuoJieJiLingHai);
		vQListWidget.push_back(ui.listWidget_GuoNeiJingJie);
	}

	// 注记
	if (strLayerName == "R")
	{
		vQListWidget.push_back(ui.listWidget_JuMinDiMingCheng);
	}

	// 遍历list控件,设置选中状态
	for (int i = 0; i < vQListWidget.size(); i++)
	{
		// 每个list的元素个数
		int iCount = vQListWidget[i]->count();
		for (int iRow = 0; iRow < iCount; iRow++)
		{
			// 设置每个元素为选中状态不选中状态
			vQListWidget[i]->item(iRow)->setCheckState(Qt::Unchecked);
		}
	}
}

// 初始化要素类名称和编码的映射表
void QgsImageMappingDialog_LayoutVectorization::InitFeatureClassNameCodeTable()
{
	// 获取映射表配置参数
	QString qstrPath = QCoreApplication::applicationDirPath();

#ifdef OS_FAMILY_WINDOWS

	qstrPath += "\\config\\featureclass_name_code.cfg";

#else

	qstrPath += "/config/featureclass_name_code.cfg";

#endif

	string strPath = qstrPath.toLocal8Bit();
	FILE* fp = fopen(strPath.c_str(), "r");
	if (!fp)
	{
		// 记录日志
		return;
	}

	int iCount = 0;		// 要素类对照个数
	char szTemp[300] = { 0 };
	QString qstrCode;
	QString qstrName;
	fscanf(fp, "%d", &iCount);
	for (int i = 0; i < iCount; i++)
	{
		// 要素类编码
		fscanf(fp, "%s", szTemp);
		qstrCode = szTemp;
		memset(szTemp, 0, 300);
		// 要素类名称
		fscanf(fp, "%s", szTemp);
		qstrName = szTemp;
		memset(szTemp, 0, 300);

		m_mName_Codes.insert(MAP_FeatureClassName_Code::value_type(qstrName, qstrCode));
		m_mCode_Names.insert(MAP_FeatureClassCode_Name::value_type(qstrCode, qstrName));
	}

	fclose(fp);
}

// 获取图层字段选择状态
void QgsImageMappingDialog_LayoutVectorization::GetLayerFeatureClassChecked(string strLayerType, vector<string>& vFeatureClassCode)
{
	// D、F、J、K、R
	vFeatureClassCode.clear();
	// 存储list控件
	vector<QListWidget*> vQListWidget;
	vQListWidget.clear();

	/*根据图层类型，获取要素类名称选中状态，将选中的要素类加入到列表中*/

	// 陆地交通
	if (strLayerType == "D")
	{
		// D的list控件包括：
		/* 1、listWidget_TieLu
		   2、listWidget_GongLu
		   3、listWidget_QiTaDaoLu
		*/

		vQListWidget.push_back(ui.listWidget_TieLu);
		vQListWidget.push_back(ui.listWidget_GongLu);
		vQListWidget.push_back(ui.listWidget_QiTaDaoLu);
	}

	// 水域/陆地
	if (strLayerType == "F")
	{
		// F的list控件包括：
		/*
		   1、listWidget_HeLiu
		   2、listWidget_HuPoShuiKuChiTang
		   3、listWidget_LuDiDaoYuHaiYang
		*/

		vQListWidget.push_back(ui.listWidget_HeLiu);
		vQListWidget.push_back(ui.listWidget_HuPoShuiKuChiTang);
		vQListWidget.push_back(ui.listWidget_LuDiDaoYuHaiYang);
	}

	// 陆地地貌及土质
	if (strLayerType == "J")
	{
		// J的list控件包括：
		/* 1、listWidget_DengGaoXian
		   2、listWidget_DiMaoGaoCheng
		*/

		vQListWidget.push_back(ui.listWidget_DengGaoXian);
		vQListWidget.push_back(ui.listWidget_DiMaoGaoCheng);
	}

	// 境界与政区
	if (strLayerType == "K")
	{
		// K的list控件包括：
		/* 1、listWidget_GuoJieJiLingHai
		   2、listWidget_GuoNeiJingJie
		   3、listWidget_XingZhengQu
		   4、listWidget_QiTaJieXian
		*/

		vQListWidget.push_back(ui.listWidget_GuoJieJiLingHai);
		vQListWidget.push_back(ui.listWidget_GuoNeiJingJie);
	}

	// 注记
	if (strLayerType == "R")
	{
		// R的list控件包括：
		/* 1、listWidget_JuMinDiMingCheng
		   2、listWidget_XingZhengQuMingCheng
		   3、listWidget_ZhuanYouMingCheng
		   4、listWidget_HaiYangHuPoHeChuan
		   5、listWidget_AoDiPenDi
		   6、listWidget_DaoBanDao
		   7、listWidget_ShanMaiShanLing
		   8、listWidget_DuLiGaoDi
		   9、listWidget_QiTaZhuJi
		*/

		vQListWidget.push_back(ui.listWidget_JuMinDiMingCheng);
	}

	for (int i = 0; i < vQListWidget.size(); i++)
	{
		// 每个list的元素个数
		int iCount = vQListWidget[i]->count();
		for (int iRow = 0; iRow < iCount; iRow++)
		{
			QListWidgetItem* pItem = vQListWidget[i]->item(iRow);
			// 如果当前list元素为☑状态
			if (pItem->checkState() == Qt::Checked)
			{
				QString qstrText = pItem->text();
				QString qstrName = qstrText;
				QString qstrCode;
				// 通过映射表获取编码
				MAP_FeatureClassName_Code::iterator iter = m_mName_Codes.find(qstrName);
				if (iter != m_mName_Codes.end())
				{
					qstrCode = iter->second;
					string strCode = qstrCode.toStdString();
					vFeatureClassCode.push_back(strCode);
				}
			}
		}
	}
}

void QgsImageMappingDialog_LayoutVectorization::InitImageMappingSchema()
{
	// 获取运行环境路径
	QString qstrExePath = QCoreApplication::applicationDirPath();
	QString qstrSchemaPath = qstrExePath + "/layout/schema";

	m_vImageMappingSchema.clear();

	// 读取运行路径下所有的xml文件
	QStringList xmlFileNameList = GetFileNamesByExtName(qstrSchemaPath, "*.xml");
	for (int i = 0; i < xmlFileNameList.size(); i++)
	{
		QString xmlFullPath = qstrSchemaPath + "/" + xmlFileNameList[i];

		vector<ImageMappingSchema> vSchemaList;
		vSchemaList.clear();
		bool bResult = LoadImageMappingSchemaXml(xmlFullPath, vSchemaList);
		if (!bResult)
		{
			// 记录日志
			return;
		}

		for (int j = 0; j < vSchemaList.size(); j++)
		{
			m_vImageMappingSchema.push_back(vSchemaList[j]);
		}
	}

	// 布局模板列表框增加显示已有模板
	for (int i = 0; i < m_vImageMappingSchema.size(); i++)
	{
		ui.listWidget_LayoutTemplate->addItem(m_vImageMappingSchema[i].qstrLayoutTemplateName);
	}

	// 默认显示第一个模板
	ui.listWidget_LayoutTemplate->setCurrentRow(0);

	QStandardItemModel* model = new QStandardItemModel(this);
	// 根据配置文件设置界面
	for (int i = 0; i < m_vImageMappingSchema.size(); i++)
	{
		ImageMappingSchema param = m_vImageMappingSchema[i];
		// 设置对话框中各部件的内容
		QStandardItem* item = new QStandardItem(param.qstrName);
		model->appendRow(item);
	}

	ui.listView_ImageMappingSchema->setModel(model);

	if (ui.listView_ImageMappingSchema->model()->rowCount() > 0)
	{
		//默认选中第一行
		QModelIndex qindex = ui.listView_ImageMappingSchema->model()->index(0, 0);
		ui.listView_ImageMappingSchema->setCurrentIndex(qindex);

		// 默认显示第一个制图方案
		SetUIByImageMappingSchema(m_vImageMappingSchema[0]);
		m_ImageMappingSchema = m_vImageMappingSchema[0];
	}
}

QStringList QgsImageMappingDialog_LayoutVectorization::GetFileNamesByExtName(const QString& path, const QString& extName)
{
	QDir dir(path);
	QStringList nameFilters;
	nameFilters << extName;
	QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
	return files;
}

bool QgsImageMappingDialog_LayoutVectorization::LoadImageMappingSchemaXml(const QString& xmlPath, vector<ImageMappingSchema>& vSchema)
{
	QDomDocument doc;
	QFile file(xmlPath);
	if (!file.open(QIODevice::ReadOnly))
	{
		return false;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return false;
	}
	file.close();

	// 读取根节点 Schema_List
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   Schema 节点
	QDomNode schemaNode = root.firstChild();
	int i = 0;
	int j = 0;
	int k = 0;

	vSchema.clear();
	while (!schemaNode.isNull())
	{
		// 循环遍历每个子节点
		ImageMappingSchema param;

		// 节点元素名称
		QString tagName = schemaNode.toElement().tagName();

		// 节点标记查找
		if (tagName == "Schema")
		{
			// Schema的子节点nodeList
			QDomNodeList schemaNodeList = schemaNode.childNodes();

			for (i = 0; i < schemaNodeList.size(); i++)
			{
				QDomNode schemaChildNode = schemaNodeList.at(i);
				QDomElement schemaChildElement = schemaChildNode.toElement();

				// 制图方案名称
				if (schemaChildElement.tagName() == "Name")
				{
					param.qstrName = schemaChildElement.text();
				}

				// 矢量数据路径
				else if (schemaChildElement.tagName() == "VectorDataPath")
				{
					param.qstrVectorDataPath = schemaChildElement.text();
				}

				// 影像数据路径
				else if (schemaChildElement.tagName() == "ImageDataPath")
				{
					param.qstrImageDataPath = schemaChildElement.text();
				}

				// 镶嵌线数据路径
				else if (schemaChildElement.tagName() == "MosaicDataPath")
				{
					param.qstrMosaicDataPath = schemaChildElement.text();
				}

				// 制图范围
				else if (schemaChildElement.tagName() == "MappingRange")
				{
					QDomNamedNodeMap attrMap = schemaChildNode.attributes();

					// 左边界经度
					QDomNode leftNode = attrMap.namedItem("Left");
					param.dLeft = leftNode.toAttr().value().toDouble();

					// 上边界纬度
					QDomNode topNode = attrMap.namedItem("Top");
					param.dTop = topNode.toAttr().value().toDouble();

					// 右边界经度
					QDomNode rightNode = attrMap.namedItem("Right");
					param.dRight = rightNode.toAttr().value().toDouble();

					// 下边界纬度
					QDomNode bottomNode = attrMap.namedItem("Bottom");
					param.dBottom = bottomNode.toAttr().value().toDouble();
				}

				// 要素类图层列表
				else if (schemaChildElement.tagName() == "FeatureLayers")
				{
					// FeatureLayers的子节点Layer列表
					QDomNodeList layersList = schemaChildNode.childNodes();

					// 循环获取每个Layer节点
					for (j = 0; j < layersList.size(); j++)
					{
						// 单图层要素类参数
						FeatureClassParam fcParam;

						QDomNode featureLayersChildNode = layersList.at(j);
						QDomElement featureLayersChildElement = featureLayersChildNode.toElement();

						// 获取Layer节点下的子节点
						QDomNodeList layerList = featureLayersChildNode.childNodes();
						for (k = 0; k < layerList.size(); k++)
						{
							QDomNode layerChildNode = layerList.at(k);
							QDomElement layerChildElement = layerChildNode.toElement();
							if (layerChildElement.tagName() == "LayerName")
							{
								fcParam.qstrLayerName = layerChildElement.text();
							}

							else if (layerChildElement.tagName() == "FeatureClassCodes")
							{
								// 获取字段列表
								QDomNodeList FeatureClassList = layerChildNode.childNodes();
								for (int index = 0; index < FeatureClassList.size(); index++)
								{
									QDomNode FeatureClassNode = FeatureClassList.at(index);
									QDomElement fieldElement = FeatureClassNode.toElement();
									QString qstrFeatureClass = fieldElement.text();
									fcParam.vFeatureClassCode.push_back(qstrFeatureClass);
								}
							}
						}

						param.vFeatureClass.push_back(fcParam);
					}
				}

				// 布局模板路径
				else if (schemaChildElement.tagName() == "LayoutTemplatePath")
				{
					param.qstrLayoutTemplatePath = schemaChildElement.text();
				}

				// 布局模板名称
				else if (schemaChildElement.tagName() == "LayoutTemplateName")
				{
					param.qstrLayoutTemplateName = schemaChildElement.text();
				}

				// 布局整饰要素
				else if (schemaChildElement.tagName() == "LayoutItemParam")
				{
					// LayoutItemParam的子节点item列表
					QDomNodeList itemsList = schemaChildNode.childNodes();

					// 循环获取每个Layer节点
					for (j = 0; j < itemsList.size(); j++)
					{
						QDomNode itemNode = itemsList.at(j);
						QDomElement itemElement = itemNode.toElement();

						// 主标题-图名
						if (itemElement.tagName() == "MapTitleName")
						{
							param.layoutItemParam.qstrMapTitleName = itemElement.text();
						}

						// 地图副标题-省名
						else if (itemElement.tagName() == "MapSubTitleProvince")
						{
							param.layoutItemParam.qstrMapSubTitleProvince = itemElement.text();
						}

						// 地图副标题-县名
						else if (itemElement.tagName() == "MapSubTitleCounty")
						{
							param.layoutItemParam.qstrMapSubTitleCounty = itemElement.text();
						}

						// 是否显示副标题
						else if (itemElement.tagName() == "MapSubTitleChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bMapSubTitleChecked = true;
							}
							else
							{
								param.layoutItemParam.bMapSubTitleChecked = false;
							}
						}

						// 密级
						else if (itemElement.tagName() == "SecurityClassification")
						{
							param.layoutItemParam.qstrSecurityClassification = itemElement.text();
						}

						// 地图说明
						else if (itemElement.tagName() == "MapDetails")
						{
							param.layoutItemParam.qstrMapDetails = itemElement.text();
						}

						// 是否显示地图说明
						else if (itemElement.tagName() == "MapDetailsChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bMapDetailsChecked = true;
							}
							else
							{
								param.layoutItemParam.bMapDetailsChecked = false;
							}
						}

						// 制图单位
						else if (itemElement.tagName() == "MapAgency")
						{
							param.layoutItemParam.qstrMapAgency = itemElement.text();
						}

						// 是否显示制图单位
						else if (itemElement.tagName() == "MapAgencyChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bMapAgencyChecked = true;
							}
							else
							{
								param.layoutItemParam.bMapAgencyChecked = false;
							}
						}

						// 左上角图幅号
						else if (itemElement.tagName() == "LeftTopCornerSheet")
						{
							param.layoutItemParam.qstrLeftTopCornerSheet = itemElement.text();
						}

						// 是否显示左上角图幅号
						else if (itemElement.tagName() == "LeftTopCornerSheetChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bLeftTopCornerSheetChecked = true;
							}
							else
							{
								param.layoutItemParam.bLeftTopCornerSheetChecked = false;
							}
						}

						// 右下角图幅号
						else if (itemElement.tagName() == "RightBottomCornerSheet")
						{
							param.layoutItemParam.qstrRightBottomCornerSheet = itemElement.text();
						}

						// 是否显示右下角图幅号
						else if (itemElement.tagName() == "RightBottomCornerSheetChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bRightBottomCornerSheetChecked = true;
							}
							else
							{
								param.layoutItemParam.bRightBottomCornerSheetChecked = false;
							}
						}

						// 比例尺
						else if (itemElement.tagName() == "Scale")
						{
							param.layoutItemParam.qstrScale = itemElement.text();
						}

						// 是否显示比例尺
						else if (itemElement.tagName() == "ScaleChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bScaleChecked = true;
							}
							else
							{
								param.layoutItemParam.bScaleChecked = false;
							}
						}

						// 是否显示方里网
						else if (itemElement.tagName() == "KmNetChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bKmNetChecked = true;
							}
							else
							{
								param.layoutItemParam.bKmNetChecked = false;
							}
						}

						// 是否显示坡度尺
						else if (itemElement.tagName() == "SlopeRulerChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bSlopeRulerChecked = true;
							}
							else
							{
								param.layoutItemParam.bSlopeRulerChecked = false;
							}
						}

						// 是否显示三北方向线
						else if (itemElement.tagName() == "NorthDirectionLinesChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bNorthDirectionLinesChecked = true;
							}
							else
							{
								param.layoutItemParam.bNorthDirectionLinesChecked = false;
							}
						}

						// 磁偏角
						else if (itemElement.tagName() == "MagenticAngle")
						{
							param.layoutItemParam.dMagenticAngle = itemElement.text().toDouble();
						}

						// 子午线收敛角
						else if (itemElement.tagName() == "MeridianConvergenceAngle")
						{
							param.layoutItemParam.dMeridianConvergenceAngle = itemElement.text().toDouble();
						}

						// 是否显示接图表
						else if (itemElement.tagName() == "SheetListChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bSheetListChecked = true;
							}
							else
							{
								param.layoutItemParam.bSheetListChecked = false;
							}
						}

						// 是否显示镶嵌线地图
						else if (itemElement.tagName() == "MosaicMapChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bMosaicMapChecked = true;
							}
							else
							{
								param.layoutItemParam.bMosaicMapChecked = false;
							}
						}

						// 是否显示主图图例
						else if (itemElement.tagName() == "MainMapLegendChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bMainMapLegendChecked = true;
							}
							else
							{
								param.layoutItemParam.bMainMapLegendChecked = false;
							}
						}

						// 是否显示镶嵌线地图图例
						else if (itemElement.tagName() == "MosaicMapLegendChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bMosaicMapLegendChecked = true;
							}
							else
							{
								param.layoutItemParam.bMosaicMapLegendChecked = false;
							}
						}

						// 地图附注
						else if (itemElement.tagName() == "MapAnnotations")
						{
							param.layoutItemParam.qstrMapAnnotations = itemElement.text();
						}

						// 是否显示地图附注
						else if (itemElement.tagName() == "AnnotationsChecked")
						{
							if (itemElement.text() == "true")
							{
								param.layoutItemParam.bAnnotationsChecked = true;
							}
							else
							{
								param.layoutItemParam.bAnnotationsChecked = false;
							}
						}
					}
				}

				// 地图导出类型
				else if (schemaChildElement.tagName() == "MapExportType")
				{
					param.qstrMapExportType = schemaChildElement.text();
				}

				// 地图输出路径
				else if (schemaChildElement.tagName() == "applicationDirPath")
				{
					param.qstrMapExportPath = schemaChildElement.text();
				}
			}
		}

		vSchema.push_back(param);

		// 读取下一个Schema节点
		schemaNode = schemaNode.nextSibling();
	}

	return true;
}

void QgsImageMappingDialog_LayoutVectorization::SetUIByImageMappingSchema(const ImageMappingSchema& schema)
{
	// 获取运行环境路径
	QString qstrExePath = QCoreApplication::applicationDirPath();
	QString qstrSchemaPath = qstrExePath + "/layout/schema";
	QString qstrTemplatePath = qstrExePath + "/layout/template";

	/*------设置数据源------*/
	// 矢量数据路径
	ui.lineEdit_VectorDataPath->setText(schema.qstrVectorDataPath);

	// 影像数据路径
	ui.lineEdit_ImageDataPath->setText(schema.qstrImageDataPath);

	// 镶嵌线数据路径
	ui.lineEdit_MosaicDataPath->setText(schema.qstrMosaicDataPath);

	/*------设置制图范围------*/
	// 左边界经度
	QString qstrLeft = tr("%1").arg(schema.dLeft);
	ui.lineEdit_Left->setText(qstrLeft);

	// 上边界纬度
	QString qstrTop = tr("%1").arg(schema.dTop);
	ui.lineEdit_Top->setText(qstrTop);

	// 右边界经度
	QString qstrRight = tr("%1").arg(schema.dRight);
	ui.lineEdit_Right->setText(qstrRight);

	// 下边界纬度
	QString qstrBottom = tr("%1").arg(schema.dBottom);
	ui.lineEdit_Bottom->setText(qstrBottom);

	/*------设置矢量要素选取------*/
	// 根据制图方案设置
	for (int i = 0; i < schema.vFeatureClass.size(); i++)
	{
		FeatureClassParam param = schema.vFeatureClass[i];
		SetListWidgetStatus(param);
	}

	/*------设置布局模板------*/
	// 加载布局模板
	if (schema.qstrLayoutTemplatePath == "applicationDirPath")
	{
		ui.lineEdit_LayoutTemplatePath->setText(qstrTemplatePath + "/" + schema.qstrLayoutTemplateName);
	}

	// 模板个数
	int iTemplateCount = ui.listWidget_LayoutTemplate->count();
	for (int iRow = 0; iRow < iTemplateCount; iRow++)
	{
		// 获取每个元素
		QString qstrText = ui.listWidget_LayoutTemplate->item(iRow)->text();

		// 设置对应的布局模板为选中状态
		if (qstrText == schema.qstrLayoutTemplateName)
		{
			ui.listWidget_LayoutTemplate->setCurrentRow(iRow);
		}
	}

	/*------设置布局整饰要素------*/
	// 地图主标题-图名
	//ui.lineEdit_MapName_MainTitleName->setText(schema.layoutItemParam.qstrMapTitleName);

	// 地图副标题-省名
	//ui.lineEdit_MapName_SubTitleProvince->setText(schema.layoutItemParam.qstrMapSubTitleProvince);

	// 地图副标题-县名
	//ui.lineEdit_MapName_SubTitleCounty->setText(schema.layoutItemParam.qstrMapSubTitleCounty);

	// 密级
	if (schema.layoutItemParam.qstrSecurityClassification == "绝密")
	{
		ui.radioButton_JueMi->setChecked(true);
	}
	else if (schema.layoutItemParam.qstrSecurityClassification == "机密")
	{
		ui.radioButton_JiMi->setChecked(true);
	}
	else if (schema.layoutItemParam.qstrSecurityClassification == "秘密")
	{
		ui.radioButton_MiMi->setChecked(true);
	}

	// 地图说明
	ui.textEdit_MapDetail->setText(schema.layoutItemParam.qstrMapDetails);

	// 是否绘制制图单位
	if (schema.layoutItemParam.bMapAgencyChecked)
	{
		ui.checkBox_MapAgency->setChecked(true);
		ui.lineEdit_MapAgency->setEnabled(true);
		ui.lineEdit_MapAgency->setText(schema.layoutItemParam.qstrMapAgency);
	}
	else
	{
		ui.checkBox_MapAgency->setChecked(false);
		ui.lineEdit_MapAgency->setEnabled(false);
		ui.lineEdit_MapAgency->setText(schema.layoutItemParam.qstrMapAgency);
	}

	// 是否绘制左上角图幅号
	if (schema.layoutItemParam.bLeftTopCornerSheetChecked)
	{
		ui.checkBox_LeftTopSheet->setChecked(true);
		//ui.lineEdit_LeftTopSheet->setEnabled(true);
		//ui.lineEdit_LeftTopSheet->setText(schema.layoutItemParam.qstrLeftTopCornerSheet);
	}
	else
	{
		ui.checkBox_LeftTopSheet->setChecked(false);
		//ui.lineEdit_LeftTopSheet->setEnabled(false);
		//ui.lineEdit_LeftTopSheet->setText(schema.layoutItemParam.qstrLeftTopCornerSheet);
	}

	// 是否绘制右下角图幅号
	if (schema.layoutItemParam.bRightBottomCornerSheetChecked)
	{
		ui.checkBox_RightBottomSheet->setChecked(true);
		//ui.lineEdit_RightBottomSheet->setEnabled(true);
		//ui.lineEdit_RightBottomSheet->setText(schema.layoutItemParam.qstrRightBottomCornerSheet);
	}
	else
	{
		ui.checkBox_RightBottomSheet->setChecked(false);
		//ui.lineEdit_RightBottomSheet->setEnabled(false);
		//ui.lineEdit_RightBottomSheet->setText(schema.layoutItemParam.qstrRightBottomCornerSheet);
	}

	// 是否绘制比例尺
	if (schema.layoutItemParam.bScaleChecked)
	{
		ui.checkBox_Scale->setChecked(true);
		ui.lineEdit_Scale->setEnabled(true);
		ui.lineEdit_Scale->setText(schema.layoutItemParam.qstrScale);
	}
	else
	{
		ui.checkBox_Scale->setChecked(false);
		ui.lineEdit_Scale->setEnabled(false);
		ui.lineEdit_Scale->setText(schema.layoutItemParam.qstrScale);
	}

	// 是否绘制方里网
	if (schema.layoutItemParam.bKmNetChecked)
	{
		ui.checkBox_KmNet->setChecked(true);
	}
	else
	{
		ui.checkBox_KmNet->setChecked(false);
	}

	// 是否绘制坡度尺
	/*if (schema.layoutItemParam.bSlopeRulerChecked)
	{
		ui.checkBox_SlopeRuler->setChecked(true);
	}
	else
	{
		ui.checkBox_SlopeRuler->setChecked(false);
	}*/

	// 是否绘制三北方向线
	if (schema.layoutItemParam.bNorthDirectionLinesChecked)
	{
		ui.checkBox_NorthDirectionLines->setChecked(true);
		//ui.lineEdit_MagenticAngle->setEnabled(true);
		//QString qstrMagenticAngle = tr("%1").arg(schema.layoutItemParam.dMagenticAngle);
		//ui.lineEdit_MagenticAngle->setText(qstrMagenticAngle);

		//ui.lineEdit_MeridianConvergenceAngle->setEnabled(true);
		//QString qstrMeridianConvergenceAngle = tr("%1").arg(schema.layoutItemParam.dMeridianConvergenceAngle);
		//ui.lineEdit_MeridianConvergenceAngle->setText(qstrMeridianConvergenceAngle);
	}
	else
	{
		ui.checkBox_NorthDirectionLines->setChecked(false);
		//ui.lineEdit_MagenticAngle->setEnabled(false);
		//QString qstrMagenticAngle = tr("%1").arg(schema.layoutItemParam.dMagenticAngle);
		//ui.lineEdit_MagenticAngle->setText(qstrMagenticAngle);

		//ui.lineEdit_MeridianConvergenceAngle->setEnabled(false);
		//QString qstrMeridianConvergenceAngle = tr("%1").arg(schema.layoutItemParam.dMeridianConvergenceAngle);
		//ui.lineEdit_MeridianConvergenceAngle->setText(qstrMeridianConvergenceAngle);
	}

	// 是否绘制接图表
	if (schema.layoutItemParam.bSheetListChecked)
	{
		ui.checkBox_SheetList->setChecked(true);
	}
	else
	{
		ui.checkBox_SheetList->setChecked(false);
	}

	// 是否绘制镶嵌线缩略图
	if (schema.layoutItemParam.bMosaicMapChecked)
	{
		ui.checkBox_MosaicMap->setChecked(true);
	}
	else
	{
		ui.checkBox_MosaicMap->setChecked(false);
	}

	// 是否绘制主图图例
	if (schema.layoutItemParam.bMainMapLegendChecked)
	{
		ui.checkBox_MainMapLegend->setChecked(true);
	}
	else
	{
		ui.checkBox_MainMapLegend->setChecked(false);
	}

	// 是否绘制镶嵌线图例
	if (schema.layoutItemParam.bMosaicMapLegendChecked)
	{
		ui.checkBox_MosaicMapLegend->setChecked(true);
	}
	else
	{
		ui.checkBox_MosaicMapLegend->setChecked(false);
	}

	// 是否绘制地图附注
	if (schema.layoutItemParam.bAnnotationsChecked)
	{
		ui.checkBox_Annotations->setChecked(true);
		ui.lineEdit_Annotations->setEnabled(true);
		ui.lineEdit_Annotations->setText(schema.layoutItemParam.qstrMapAnnotations);
	}
	else
	{
		ui.checkBox_Annotations->setChecked(false);
		ui.lineEdit_Annotations->setEnabled(false);
		ui.lineEdit_Annotations->setText(schema.layoutItemParam.qstrMapAnnotations);
	}

	/*------地图输出格式------*/
	if (schema.qstrMapExportType == "GeoPDF")
	{
		ui.radioButton_GeoPDF->setChecked(true);
	}
	else if (schema.qstrMapExportType == "PDF")
	{
		ui.radioButton_PDF->setChecked(true);
	}
	else if (schema.qstrMapExportType == "tiff")
	{
		ui.radioButton_tiff->setChecked(true);
	}

	/*------地图输出路径------*/
	ui.lineEdit_OutputDataPath->setText(schema.qstrMapExportPath);
}

// 设置各图层要素类列表框状态
void QgsImageMappingDialog_LayoutVectorization::SetListWidgetStatus(FeatureClassParam& param)
{
	// 图层：D、F、J、K、R
	// 存储list控件
	vector<QListWidget*> vQListWidget;
	vQListWidget.clear();

	// 陆地交通
	if (param.qstrLayerName == "D")
	{
		vQListWidget.push_back(ui.listWidget_TieLu);
		vQListWidget.push_back(ui.listWidget_GongLu);
		vQListWidget.push_back(ui.listWidget_QiTaDaoLu);
	}

	// 水域/陆地
	if (param.qstrLayerName == "F")
	{
		vQListWidget.push_back(ui.listWidget_HeLiu);
		vQListWidget.push_back(ui.listWidget_HuPoShuiKuChiTang);
		vQListWidget.push_back(ui.listWidget_LuDiDaoYuHaiYang);
	}

	// 陆地地貌及土质
	if (param.qstrLayerName == "J")
	{
		vQListWidget.push_back(ui.listWidget_DengGaoXian);
		vQListWidget.push_back(ui.listWidget_DiMaoGaoCheng);
	}

	// 境界与政区
	if (param.qstrLayerName == "K")
	{
		vQListWidget.push_back(ui.listWidget_GuoJieJiLingHai);
		vQListWidget.push_back(ui.listWidget_GuoNeiJingJie);
	}

	// 注记
	if (param.qstrLayerName == "R")
	{
		vQListWidget.push_back(ui.listWidget_JuMinDiMingCheng);
	}

	// 遍历list控件，判断每个item名称是否在要素类名称列表中，如果在，则设置选中状态
	for (int i = 0; i < vQListWidget.size(); i++)
	{
		// 每个list的元素个数
		int iCount = vQListWidget[i]->count();
		for (int iRow = 0; iRow < iCount; iRow++)
		{
			// 获取每个元素
			QString qstrText = vQListWidget[i]->item(iRow)->text();

			// 判断每个元素是否在要素类列表中，如果在，则设置状态为选中
			if (bIsInFeatureClasses(qstrText, param.vFeatureClassCode))
			{
				vQListWidget[i]->item(iRow)->setCheckState(Qt::Checked);
			}
		}
	}
}

void QgsImageMappingDialog_LayoutVectorization::GetListWidgetStatus(const QString& qstrLayerType, FeatureClassParam& param)
{
	param.qstrLayerName = qstrLayerType;
	// 图层：D、F、J、K、R
	// 存储list控件
	vector<QListWidget*> vQListWidget;
	vQListWidget.clear();

	// 陆地交通
	if (qstrLayerType == "D")
	{
		vQListWidget.push_back(ui.listWidget_TieLu);
		vQListWidget.push_back(ui.listWidget_GongLu);
		vQListWidget.push_back(ui.listWidget_QiTaDaoLu);
	}

	// 水域/陆地
	if (qstrLayerType == "F")
	{
		vQListWidget.push_back(ui.listWidget_HeLiu);
		vQListWidget.push_back(ui.listWidget_HuPoShuiKuChiTang);
		vQListWidget.push_back(ui.listWidget_LuDiDaoYuHaiYang);
	}

	// 陆地地貌及土质
	if (qstrLayerType == "J")
	{
		vQListWidget.push_back(ui.listWidget_DengGaoXian);
		vQListWidget.push_back(ui.listWidget_DiMaoGaoCheng);
	}

	// 境界与政区
	if (qstrLayerType == "K")
	{
		vQListWidget.push_back(ui.listWidget_GuoJieJiLingHai);
		vQListWidget.push_back(ui.listWidget_GuoNeiJingJie);
	}

	// 注记
	if (qstrLayerType == "R")
	{
		vQListWidget.push_back(ui.listWidget_JuMinDiMingCheng);
	}

	// 遍历list控件，判断每个item名称是否在要素类名称列表中，如果在，则设置选中状态
	for (int i = 0; i < vQListWidget.size(); i++)
	{
		// 每个list的元素个数
		int iCount = vQListWidget[i]->count();
		for (int iRow = 0; iRow < iCount; iRow++)
		{
			if (vQListWidget[i]->item(iRow)->checkState() == Qt::Checked)
			{
				// 获取每个元素
				QString qstrText = vQListWidget[i]->item(iRow)->text();
				QString qstrCode = "";
				// 获取对应编码
				MAP_FeatureClassName_Code::iterator iter = m_mName_Codes.find(qstrText);
				if (iter != m_mName_Codes.end())
				{
					qstrCode = iter->second;
				}
				param.vFeatureClassCode.push_back(qstrCode);
			}
		}
	}
}

// 判断列表中元素是否在要素类列表中
bool QgsImageMappingDialog_LayoutVectorization::bIsInFeatureClasses(QString qstrText, vector<QString>& vFeatureClassCode)
{
	for (int i = 0; i < vFeatureClassCode.size(); i++)
	{
		QString qstrCode = vFeatureClassCode[i];
		QString qstrName;
		MAP_FeatureClassCode_Name::iterator iter = m_mCode_Names.find(qstrCode);
		if (iter != m_mCode_Names.end())
		{
			qstrName = iter->second;
		}

		if (qstrText == qstrName)
		{
			return true;
		}
	}

	return false;
}

bool QgsImageMappingDialog_LayoutVectorization::ExportMapToFile(
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
		settings.writeGeoPdf = true;
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

		QgsMessageLog::logMessage(tr("成功导出布局到 <a href=\"%1\">%2</a>").arg(QUrl::fromLocalFile(filePath).toString(), QDir::toNativeSeparators(filePath)), tr("影像图制图"), Qgis::Success);
		return true;

	case QgsLayoutExporter::PrintError:
	case QgsLayoutExporter::SvgLayerError:
	case QgsLayoutExporter::IteratorError:
	case QgsLayoutExporter::Canceled:
		return false;

	case QgsLayoutExporter::FileError:
		cursorOverride.release();
		QgsMessageLog::logMessage(tr("%1.\n\n文件正在使用").arg(QDir::toNativeSeparators(filePath)), tr("影像图制图"), Qgis::Critical);
		return false;

	case QgsLayoutExporter::MemoryError:
		cursorOverride.release();
		QgsMessageLog::logMessage(tr("创建地图导出文件太大导致内存不足"), tr("影像图制图"), Qgis::Critical);
		return false;
	}
	return true;
}

void QgsImageMappingDialog_LayoutVectorization::ConvertMilliMeterToPixel(double w, double h, int dpi, int& x, int& y)
{
	// x,y 为像素数，w,h为毫米
	// 水平方向：x = w * dpi / 25.4
	// 垂直方向：y = h * dpi / 25.4
	x = w * dpi / 25.4;
	y = h * dpi / 25.4;
}

void QgsImageMappingDialog_LayoutVectorization::ConvertPixeltoMilliMeter(int x, int y, int dpi, double& w, double& h)
{
	// x, y 为像素数，w, h为毫米
	// 水平方向：w = x * 25.4 / dpi
	// 垂直方向：h = y * 25.4 / dpi

	w = x * 25.4 / dpi;
	h = y * 25.4 / dpi;
}

void QgsImageMappingDialog_LayoutVectorization::restoreState()
{
	const QgsSettings settings;
	m_qstrImageMappingDataSavePath = settings.value(QStringLiteral("QgsImageMappingDialog_LayoutVectorization/ImageMappingDataSavePath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();
	m_qstrImageMappingSchemaSavePath = settings.value(QStringLiteral("QgsImageMappingDialog_LayoutVectorization/ImageMappingSchemaSavePath"), QDir::homePath(), QgsSettings::Section::Plugins).toString();

}

void QgsImageMappingDialog_LayoutVectorization::GetSheetListByMappingRange(const QString& qstrTemplateName,
	double dLeft,
	double dTop,
	double dRight,
	double dBottom,
	vector<QString>& qstrSheetList)
{
	qstrSheetList.clear();
	CSE_MapSheet mapSheet;
	vector<string> vSheetList;
	vSheetList.clear();

	// 如果是1w制图模板
	if (qstrTemplateName == "1w_layout.qpt")
	{
		// 获取1万比例尺图幅列表
		mapSheet.get_name_from_bbox(SCALE_10K,
			"D",
			dLeft,
			dBottom,
			dRight,
			dTop,
			vSheetList);
	}
	// 如果是5w制图模板
	else if (qstrTemplateName == "5w_layout.qpt")
	{
		// 获取5万比例尺图幅列表
		mapSheet.get_name_from_bbox(SCALE_50K,
			"D",
			dLeft,
			dBottom,
			dRight,
			dTop,
			vSheetList);
	}

	for (int i = 0; i < vSheetList.size(); i++)
	{
		QString qstrSheet = QString::fromLocal8Bit(vSheetList[i].c_str());
		qstrSheetList.push_back(qstrSheet);
	}
}

void QgsImageMappingDialog_LayoutVectorization::GetVectorLayerPathList(const ImageMappingSchema& schema,
	const QString& qstrSheet,
	vector<QString>& vPath)
{
	vPath.clear();
	for (int i = 0; i < schema.vFeatureClass.size(); i++)
	{
		// 点图层
		QString qstrLayerFilePath = schema.qstrVectorDataPath
			+ "/"
			+ qstrSheet
			+ "/"
			+ qstrSheet
			+ "_"
			+ schema.vFeatureClass[i].qstrLayerName
			+ "_point.shp";

		vPath.push_back(qstrLayerFilePath);

		// 线图层
		qstrLayerFilePath = schema.qstrVectorDataPath
			+ "/"
			+ qstrSheet
			+ "/"
			+ qstrSheet
			+ "_"
			+ schema.vFeatureClass[i].qstrLayerName
			+ "_line.shp";

		vPath.push_back(qstrLayerFilePath);

		// 面图层
		qstrLayerFilePath = schema.qstrVectorDataPath
			+ "/"
			+ qstrSheet
			+ "/"
			+ qstrSheet
			+ "_"
			+ schema.vFeatureClass[i].qstrLayerName
			+ "_polygon.shp";

		vPath.push_back(qstrLayerFilePath);
	}
}

double QgsImageMappingDialog_LayoutVectorization::CalCenterMeridian(double dLong, double dScale)
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

void QgsImageMappingDialog_LayoutVectorization::GetMBR(int iCount, double* pPoints, double& dLeft, double& dTop, double& dRight, double& dBottom)
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

QString QgsImageMappingDialog_LayoutVectorization::GetEPSGCodeByCenterMeridianAndScale(double dLong, double dScale)
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

void QgsImageMappingDialog_LayoutVectorization::Degree2DMS(double degree, int& iDegree, int& iMinute, int& iSecond)
{
	iDegree = (int)degree;
	iMinute = (round)((degree - iDegree) * 60);
	iSecond = (round)(((degree - iDegree) * 60 - iMinute) * 60);
}

bool QgsImageMappingDialog_LayoutVectorization::ExportImageMetaData(const QString& filePath,
	ImageMappingSchema& schema,
	double dLeft,
	double dRight,
	double dTop,
	double dBottom,
	double dValue[],
	double dCenterMeridian,
	double dImageResolution)
{
	// 经度范围
	// 度转度分秒
	// 左边界经度
	int iLeftDegree, iLeftMinute, iLeftSecond;
	int iRightDegree, iRightMinute, iRightSecond;
	int iTopDegree, iTopMinute, iTopSecond;
	int iBottomDegree, iBottomMinute, iBottomSecond;

	Degree2DMS(dLeft, iLeftDegree, iLeftMinute, iLeftSecond);
	Degree2DMS(dRight, iRightDegree, iRightMinute, iRightSecond);
	Degree2DMS(dTop, iTopDegree, iTopMinute, iTopSecond);
	Degree2DMS(dBottom, iBottomDegree, iBottomMinute, iBottomSecond);

	// 经度范围
	QString qstrLonRang = tr("%1%2%3-%4%5%6").arg(iLeftDegree).arg(FormatMinute2(iLeftMinute)).arg(FormatMinute2(iLeftSecond))
		.arg(iRightDegree).arg(FormatMinute2(iRightMinute)).arg(FormatMinute2(iRightSecond));

	// 纬度范围
	QString qstrLatRang = tr("%1%2%3-%4%5%6").arg(iBottomDegree).arg(FormatMinute2(iBottomMinute)).arg(FormatMinute2(iBottomSecond))
		.arg(FormatDegree2(iTopDegree)).arg(FormatMinute2(iTopMinute)).arg(FormatMinute2(iTopSecond));

	// 根据中央经线计算带号
	int iBandNumber = 0;
	string strBandType;
	if (m_ImageMappingSchema.layoutItemParam.qstrScale == "1:50000")
	{
		strBandType = "6度带";
		iBandNumber = int((dCenterMeridian + 3) / 6);
	}
	else if (m_ImageMappingSchema.layoutItemParam.qstrScale == "1:10000")
	{
		strBandType = "3度带";
		iBandNumber = int(dCenterMeridian / 3);
	}
	QString qstrBandNumber = tr("%1").arg(iBandNumber);

	// 西北角点[0]、[1]
	// 东北角点[2]、[3]
	// 东南角点[4]、[5]
	// 西南角点[6]、[7]

	// 西南图廓角点X坐标dValue[6]
	QString qstrSouthWestX = tr("%1").arg(iBandNumber * 1000000.0 + dValue[6]);

	// 西南图廓角点Y坐标dValue[7]
	QString qstrSouthWestY = tr("%1").arg(dValue[7]);

	// 西北图廓角点X坐标dValue[0]
	QString qstrNorthWestX = tr("%1").arg(iBandNumber * 1000000.0 + dValue[0]);

	// 西北图廓角点Y坐标dValue[1]
	QString qstrNorthWestY = tr("%1").arg(dValue[1]);

	// 东北图廓角点X坐标dValue[2]
	QString qstrNorthEastX = tr("%1").arg(iBandNumber * 1000000.0 + dValue[2]);

	// 东北图廓角点Y坐标dValue[3]
	QString qstrNorthEastY = tr("%1").arg(dValue[3]);

	// 东南图廓角点X坐标dValue[4]
	QString qstrSouthEastX = tr("%1").arg(iBandNumber * 1000000.0 + dValue[4]);

	// 东南图廓角点Y坐标dValue[5]
	QString qstrSouthEastY = tr("%1").arg(dValue[5]);

	/*读取元数据项配置文件*/
	string strCategory;
	string strName;
	string strType;
	string strLength;
	string strValue;
	ImageMetaData metaData;
	metaData.vItems.clear();

	/*创建图幅元数据*/
	// 创建xlsx文件并添加一个表格页
	string strImageMetaDataPath = filePath.toLocal8Bit();
	lxw_workbook* workbook = workbook_new(strImageMetaDataPath.c_str());
	if (!workbook)
	{
		QgsMessageLog::logMessage((QString(tr("创建元数据文件失败:")) + filePath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	/*添加新的表格页*/
	lxw_worksheet* worksheet = workbook_add_worksheet(workbook, NULL);
	if (!worksheet)
	{
		QgsMessageLog::logMessage((QString(tr("添加表格失败:")) + filePath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	/* 增加xlsx格式表格 */
	lxw_format* format = workbook_add_format(workbook);
	if (!format)
	{
		QgsMessageLog::logMessage((QString(tr("设置单元格格式失败:")) + filePath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	/*设置粗体*/
	// format_set_bold(format);
	/*设置对齐方式*/
	//format_set_align(format, LXW_ALIGN_CENTER);
	//format_set_align(format, LXW_ALIGN_VERTICAL_CENTER);
	/* 设置列的宽度 */
	// 第一列宽度20（类别）
	worksheet_set_column(worksheet, 0, 0, 20, NULL);

	// 第二列宽度30（数据项名称）
	worksheet_set_column(worksheet, 1, 1, 30, NULL);

	// 第三列宽度10（数据类型）
	worksheet_set_column(worksheet, 2, 2, 10, NULL);

	// 第四列宽度10（长度）
	worksheet_set_column(worksheet, 3, 3, 10, NULL);

	// 第五列宽度30（值）
	worksheet_set_column(worksheet, 4, 4, 30, NULL);

	/*写第一行标题值*/
	// 类别
	worksheet_write_string(worksheet, 0, 0, "类别", NULL);

	// 数据项名称
	worksheet_write_string(worksheet, 0, 1, "数据项名称", NULL);

	// 数据类型
	worksheet_write_string(worksheet, 0, 2, "数据类型", NULL);

	// 长度
	//string strLength = qstrLength.toLocal8Bit();
	worksheet_write_string(worksheet, 0, 3, "长度", NULL);

	// 值
	//string strValue = qstrValue.toLocal8Bit();
	worksheet_write_string(worksheet, 0, 4, "值", NULL);

	string strSheet = m_ImageMappingSchema.layoutItemParam.qstrLeftTopCornerSheet.toLocal8Bit();

	// 依次设置元数据项
	ImageMetaDataItem item;

	//--------【产品信息】---------//
	item.strCategory = "产品信息";
	item.strName = "元数据文件名";
	item.strType = "字符型";
	item.iLength = 20;
	item.strValue = strSheet + ".xlsx";
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "产品名称";
	item.strType = "字符型";
	item.iLength = 20;
	if (m_ImageMappingSchema.layoutItemParam.qstrScale == "1:50000")
	{
		item.strValue = "1:50 000数字正射影像图";
	}
	else if (m_ImageMappingSchema.layoutItemParam.qstrScale == "1:10000")
	{
		item.strValue = "1:10 000数字正射影像图";
	}
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "图名";
	item.strType = "字符型";
	item.iLength = 20;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "图号";
	item.strType = "字符型";
	item.iLength = 20;
	item.strValue = strSheet;
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "基本等高距";
	item.strType = "数值型";
	item.iLength = 10;
	if (m_ImageMappingSchema.layoutItemParam.qstrScale == "1:50000")
	{
		item.strValue = "10";
	}
	else if (m_ImageMappingSchema.layoutItemParam.qstrScale == "1:10000")
	{
		item.strValue = "5";
	}
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "等深线";
	item.strType = "数值型";
	item.iLength = 10;
	item.strValue = "NULL";
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "产品生产单位";
	item.strType = "字符型";
	item.iLength = 20;
	item.strValue = "";
	metaData.vItems.push_back(item);

	auto now = std::chrono::system_clock::now();
	//通过不同精度获取相差的毫秒数
	uint64_t dis_millseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
		- std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() * 1000;
	time_t tt = std::chrono::system_clock::to_time_t(now);
	auto time_tm = localtime(&tt);
	char strTime[25] = { 0 };
	sprintf(strTime, "%d-%02d-%02d %02d:%02d:%02d ", time_tm->tm_year + 1900,
		time_tm->tm_mon + 1, time_tm->tm_mday, time_tm->tm_hour,
		time_tm->tm_min, time_tm->tm_sec);

	item.strCategory = "产品信息";
	item.strName = "产品生产时间";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = strTime;
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "产品出版单位";
	item.strType = "字符型";
	item.iLength = 20;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "产品出版时间";
	item.strType = "字符型";
	item.iLength = 10;
	//item.strValue = strTime;
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "数据量";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "数据格式";
	item.strType = "字符型";
	item.iLength = 10;
	if (m_ImageMappingSchema.qstrMapExportType == "GeoPDF")
	{
		item.strValue = "栅格GeoPDF";
	}
	else if (m_ImageMappingSchema.qstrMapExportType == "PDF")
	{
		item.strValue = "栅格PDF";
	}
	else if (m_ImageMappingSchema.qstrMapExportType == "tiff")
	{
		item.strValue = "栅格tiff";
	}
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "图廓角点经度范围";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrLonRang.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "图廓角点纬度范围";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrLatRang.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "西南图廓角点X坐标";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrSouthWestX.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "西南图廓角点Y坐标";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrSouthWestY.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "西北图廓角点X坐标";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrNorthWestX.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "西北图廓角点Y坐标";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrNorthWestY.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "东北图廓角点X坐标";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrNorthEastX.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "东北图廓角点Y坐标";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrNorthEastY.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "东南图廓角点X坐标";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrSouthEastX.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "东南图廓角点Y坐标";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = qstrSouthEastY.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "参照数据格式编号";
	item.strType = "字符型";
	item.iLength = 20;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "参照图式编号";
	item.strType = "字符型";
	item.iLength = 20;
	item.strValue = "GJB 873A-2008";
	metaData.vItems.push_back(item);

	item.strCategory = "产品信息";
	item.strName = "参照要素分类编码编号";
	item.strType = "字符型";
	item.iLength = 20;
	item.strValue = "GJB 1839B-2012";
	metaData.vItems.push_back(item);

	item.strCategory = "安全信息";
	item.strName = "密级";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "安全信息";
	item.strName = "数据是否加密";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "";
	metaData.vItems.push_back(item);

	//--------【空间坐标信息】---------//
	item.strCategory = "空间坐标信息";
	item.strName = "参考椭球";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "CGCS2000椭球";
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "平面坐标系";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "2000中国大地坐标系";
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "高程系统";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "正常高";
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "高程基准";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "1985国家高程基准";
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "深度基准";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "NULL";
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "地图投影";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "高斯-克吕格投影";
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "中央经线";
	item.strType = "字符型";
	item.iLength = 10;
	QString qstrCenterMeridian = tr("%1").arg(dCenterMeridian);
	item.strValue = qstrCenterMeridian.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "分带方式";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = strBandType;
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "投影带号";
	item.strType = "整型";
	item.iLength = 4;
	item.strValue = qstrBandNumber.toLocal8Bit();
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "磁偏角";
	item.strType = "数值型";
	item.iLength = 10;
	item.strValue = "NULL";
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "磁坐偏角";
	item.strType = "数值型";
	item.iLength = 10;
	item.strValue = "NULL";
	metaData.vItems.push_back(item);

	item.strCategory = "空间坐标信息";
	item.strName = "坐标纵线偏角";
	item.strType = "数值型";
	item.iLength = 10;
	item.strValue = "NULL";
	metaData.vItems.push_back(item);

	//--------【数据质量信息】---------//
	item.strCategory = "数据质量信息";
	item.strName = "东边接边情况";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "已接";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "西边接边情况";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "已接";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "南边接边情况";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "已接";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "北边接边情况";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "已接";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "平面位置中误差";
	item.strType = "数值型";
	item.iLength = 20;
	item.strValue = "NULL";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "属性精度";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "符合要求";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "逻辑一致性";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "一致";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "图幅形式";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "满幅";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "接边质量评价";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "合格";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "质量评分";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "NULL";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "数据质量总评价";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "数据验收单位";
	item.strType = "字符型";
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "数据质量信息";
	item.strName = "数据验收时间";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "";
	metaData.vItems.push_back(item);

	// --------【其他信息】---------//
	item.strCategory = "其他信息";
	item.strName = "主要数据源";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "卫星遥感影像";
	metaData.vItems.push_back(item);

	item.strCategory = "其他信息";
	item.strName = "数据采集设备及软件";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "1";
	metaData.vItems.push_back(item);

	item.strCategory = "其他信息";
	item.strName = "总层数";
	item.strType = "整型";
	item.iLength = 10;
	item.strValue = "1";
	metaData.vItems.push_back(item);

	item.strCategory = "其他信息";
	item.strName = "影像色彩";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "彩色";
	metaData.vItems.push_back(item);

	item.strCategory = "其他信息";
	item.strName = "影像分辨率";
	item.strType = "字符型";
	item.iLength = 10;
	QString qstrImageRes = tr("%1").arg(dImageResolution);
	item.strValue = qstrImageRes.toLocal8Bit();
	metaData.vItems.push_back(item);

	//---------【分发信息】---------//
	item.strCategory = "分发信息";
	item.strName = "分发介质";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "分发信息";
	item.strName = "分发单位联系电话";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "分发信息";
	item.strName = "分发单位通信地址";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "分发信息";
	item.strName = "分发单位名称";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "";
	metaData.vItems.push_back(item);

	item.strCategory = "分发信息";
	item.strName = "分发日期";
	item.strType = "字符型";
	item.iLength = 10;
	item.strValue = "";
	metaData.vItems.push_back(item);
	//------------------------------------//
	string strTemp;
	int iLength = 0;
	/*循环写元数据项*/
	for (int i = 0; i < metaData.vItems.size(); i++)
	{
		strTemp = metaData.vItems[i].strCategory;
		worksheet_write_string(worksheet, i + 1, 0, strTemp.c_str(), NULL);

		strTemp = metaData.vItems[i].strName;
		worksheet_write_string(worksheet, i + 1, 1, strTemp.c_str(), NULL);

		strTemp = metaData.vItems[i].strType;
		worksheet_write_string(worksheet, i + 1, 2, strTemp.c_str(), NULL);

		int iLength = metaData.vItems[i].iLength;
		worksheet_write_number(worksheet, i + 1, 3, iLength, NULL);

		strTemp = metaData.vItems[i].strValue;

		worksheet_write_string(worksheet, i + 1, 4, strTemp.c_str(), NULL);
	}

	// 合并行
	lxw_error lxwerr = worksheet_merge_range(worksheet, 1,
		0, 25,
		0, "产品信息",
		NULL);

	if (lxwerr != LXW_NO_ERROR)
	{
		QgsMessageLog::logMessage((QString(tr("合并单元格失败！")) + filePath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	lxwerr = worksheet_merge_range(worksheet, 26,
		0, 27,
		0, "安全信息",
		NULL);

	if (lxwerr != LXW_NO_ERROR)
	{
		QgsMessageLog::logMessage((QString(tr("合并单元格失败！")) + filePath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	lxwerr = worksheet_merge_range(worksheet, 28,
		0, 39,
		0, "空间坐标信息",
		NULL);

	if (lxwerr != LXW_NO_ERROR)
	{
		QgsMessageLog::logMessage((QString(tr("合并单元格失败！")) + filePath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	lxwerr = worksheet_merge_range(worksheet, 40,
		0, 52,
		0, "数据质量信息",
		NULL);

	if (lxwerr != LXW_NO_ERROR)
	{
		QgsMessageLog::logMessage((QString(tr("合并单元格失败！")) + filePath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	lxwerr = worksheet_merge_range(worksheet, 53,
		0, 57,
		0, "其他信息",
		NULL);

	if (lxwerr != LXW_NO_ERROR)
	{
		QgsMessageLog::logMessage((QString(tr("合并单元格失败！")) + filePath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	lxwerr = worksheet_merge_range(worksheet, 58,
		0, 62,
		0, "分发信息",
		NULL);

	if (lxwerr != LXW_NO_ERROR)
	{
		QgsMessageLog::logMessage((QString(tr("合并单元格失败！")) + filePath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	workbook_close(workbook);
	return true;
}

QString QgsImageMappingDialog_LayoutVectorization::FormatDegree(int iDegree)
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

QString QgsImageMappingDialog_LayoutVectorization::FormatDegree2(int iDegree)
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

QString QgsImageMappingDialog_LayoutVectorization::FormatMinute(int iMinute)
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

QString QgsImageMappingDialog_LayoutVectorization::FormatMinute2(int iMinute)
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

QString QgsImageMappingDialog_LayoutVectorization::FormatSecond(int iSecond)
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

QString QgsImageMappingDialog_LayoutVectorization::FormatSecond2(int iSecond)
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

bool QgsImageMappingDialog_LayoutVectorization::LoadImageMappingMetaDataConfigFile(string& strCategory,
	string& strName,
	string& strType,
	string& strLength,
	string& strValue,
	ImageMetaData& data)
{
	// 配置文件路径
	// 获取映射表配置参数
	QString qstrMetaDataConfigPath = QCoreApplication::applicationDirPath();

#ifdef OS_FAMILY_WINDOWS

	qstrMetaDataConfigPath += "\\config\\image_mapping\\image_metadata.cfg";

#else

	qstrMetaDataConfigPath += "/config/image_mapping/image_metadata.cfg";

#endif

	string strMetaDataConfigPath = qstrMetaDataConfigPath.toLocal8Bit();
	FILE* fp = fopen(strMetaDataConfigPath.c_str(), "r");
	if (!fp)
	{
		// 记录日志
		QgsMessageLog::logMessage((QString(tr("元数据配置文件打开失败:")) + qstrMetaDataConfigPath), tr("影像图制图"), Qgis::Critical);
		return false;
	}

	// 依次读取标题：类别、数据项名称、数据类型、长度、值
	//---------------------------------------//
	char szTitle[500] = { 0 };
	memset(szTitle, 0, 500);
	// 类别
	fscanf(fp, "%s", szTitle);
	strCategory = szTitle;
	memset(szTitle, 0, 500);

	// 数据项名称
	fscanf(fp, "%s", szTitle);
	strName = szTitle;
	memset(szTitle, 0, 500);

	// 数据类型
	fscanf(fp, "%s", szTitle);
	strType = szTitle;
	memset(szTitle, 0, 500);

	// 长度
	fscanf(fp, "%s", szTitle);
	strLength = szTitle;
	memset(szTitle, 0, 500);

	// 值
	fscanf(fp, "%s", szTitle);
	strValue = szTitle;
	memset(szTitle, 0, 500);

	//---------------------------------------//

	int iCount = 0;
	char szTemp[500] = { 0 };
	string strTempCategory;       // 类别
	string strTempName;           // 名称
	string strTempType;           // 类型

	data.vItems.clear();

	// 元数据项个数
	fscanf(fp, "%d", &iCount);
	for (int i = 0; i < iCount; i++)
	{
		// 类别
		fscanf(fp, "%s", szTemp);
		strTempCategory = szTemp;
		memset(szTemp, 0, 500);

		// 数据项名称
		fscanf(fp, "%s", szTemp);
		strTempName = szTemp;
		memset(szTemp, 0, 500);

		// 类型
		fscanf(fp, "%s", szTemp);
		strTempType = szTemp;
		memset(szTemp, 0, 500);

		// 长度
		int iLength = 0;
		fscanf(fp, "%d", &iLength);

		ImageMetaDataItem item;
		item.strCategory = strTempCategory;
		item.strName = strTempName;
		item.strType = strTempType;
		item.iLength = iLength;
		data.vItems.push_back(item);
	}

	fclose(fp);

	return true;
}

void QgsImageMappingDialog_LayoutVectorization::DrawGridOfNeighboringZone(QString qstrSheet,
	QgsPrintLayout* pLayout,
	QList< QgsLayoutItem* > layoutItemList,
	QList<QgsMapLayer*> mapLayerList,
	QString qstrEPSGCode,
	double dScale)
{
	// 新建地图布局要素
	//QgsLayoutItemMap* pNeighborMap;

	/*map = QgsLayoutItemMap(layout)
		map.setRect(20, 20, 20, 20)
		ms.setExtent(rect)
		map.setExtent(rect)
		map.setBackgroundColor(QColor(255, 255, 255, 0))
		layout.addLayoutItem(map)*/
}

QString QgsImageMappingDialog_LayoutVectorization::GetQmlFilePathFromLayerName(double dScale, const QString& qstrLayerName)
{
	QString qstrQmlPath = "";
	// 运行环境路径
	QString qstrRunDirPath = QCoreApplication::applicationDirPath();

	// 从图层名称中提取出图层类型_几何类型，如：DN02501024_D_line.shp中获取D_line
	int nPos = qstrLayerName.indexOf("_", 0, Qt::CaseInsensitive);
	QString qstrLayerTypeAndGeometry = qstrLayerName.mid(nPos + 1);

	QString qstrScale = "";

	// 5万比例尺
	if (dScale == SCALE_50K)
	{
		qstrScale = "50000";
	}
	// 1万比例尺
	else if (dScale == SCALE_10K)
	{
		qstrScale = "10000";
	}

#ifdef OS_FAMILY_WINDOWS

	qstrQmlPath = qstrRunDirPath + "\\style\\gjb\\" + qstrScale + "\\" + qstrLayerTypeAndGeometry + ".qml";

#else
	qstrQmlPath = qstrRunDirPath + "/style/gjb/" + qstrScale + "/" + qstrLayerTypeAndGeometry + ".qml";

#endif

	qstrQmlPath = QDir::toNativeSeparators(qstrQmlPath);
	return qstrQmlPath;
}

QString QgsImageMappingDialog_LayoutVectorization::GetMosaicQmlFilePath(double dScale)
{
	QString qstrQmlPath = "";
	// 运行环境路径
	QString qstrRunDirPath = QCoreApplication::applicationDirPath();

	QString qstrScale = "";

	// 5万比例尺
	if (dScale == SCALE_50K)
	{
		qstrScale = "50000";
	}
	// 1万比例尺
	else if (dScale == SCALE_10K)
	{
		qstrScale = "10000";
	}

#ifdef OS_FAMILY_WINDOWS

	qstrQmlPath = qstrRunDirPath + "\\style\\gjb\\" + qstrScale + "\\" + "mosaic.qml";

#else
	qstrQmlPath = qstrRunDirPath + "/style/gjb/" + qstrScale + "/" + "mosaic.qml";

#endif

	qstrQmlPath = QDir::toNativeSeparators(qstrQmlPath);
	return qstrQmlPath;
}

void QgsImageMappingDialog_LayoutVectorization::GetLayoutQmlFilePath(double dScale,
	QString& qstrLayoutPointQmlPath,
	QString& qstrLayoutLineQmlPath)
{
	QString qstrQmlPath = "";
	// 运行环境路径
	QString qstrRunDirPath = QCoreApplication::applicationDirPath();

	QString qstrScale = "";

	// 5万比例尺
	if (dScale == SCALE_50K)
	{
		qstrScale = "50000";
	}
	// 1万比例尺
	else if (dScale == SCALE_10K)
	{
		qstrScale = "10000";
	}

#ifdef OS_FAMILY_WINDOWS

	qstrLayoutPointQmlPath = qstrRunDirPath + "\\style\\gjb\\" + qstrScale + "\\" + "layout_point.qml";
	qstrLayoutLineQmlPath = qstrRunDirPath + "\\style\\gjb\\" + qstrScale + "\\" + "layout_line.qml";

#else
	qstrLayoutPointQmlPath = qstrRunDirPath + "/style/gjb/" + qstrScale + "/" + "layout_point.qml";
	qstrLayoutLineQmlPath = qstrRunDirPath + "/style/gjb/" + qstrScale + "/" + "layout_line.qml";

#endif

	qstrLayoutPointQmlPath = QDir::toNativeSeparators(qstrLayoutPointQmlPath);
	qstrLayoutLineQmlPath = QDir::toNativeSeparators(qstrLayoutLineQmlPath);
}

void QgsImageMappingDialog_LayoutVectorization::GetLayoutDecPointQmlFilePath(double dScale,
	QString& qstrLayoutDecPointQmlPath)
{
	QString qstrQmlPath = "";
	// 运行环境路径
	QString qstrRunDirPath = QCoreApplication::applicationDirPath();

	QString qstrScale = "";

	// 5万比例尺
	if (dScale == SCALE_50K)
	{
		qstrScale = "50000";
	}
	// 1万比例尺
	else if (dScale == SCALE_10K)
	{
		qstrScale = "10000";
	}

#ifdef OS_FAMILY_WINDOWS

	qstrLayoutDecPointQmlPath = qstrRunDirPath + "\\style\\gjb\\" + qstrScale + "\\" + "layout_decpoint.qml";

#else
	qstrLayoutDecPointQmlPath = qstrRunDirPath + "/style/gjb/" + qstrScale + "/" + "layout_decpoint.qml";

#endif

	qstrLayoutDecPointQmlPath = QDir::toNativeSeparators(qstrLayoutDecPointQmlPath);
}

void QgsImageMappingDialog_LayoutVectorization::GetLayoutDecPolygonQmlFilePath(double dScale,
	QString& qstrLayoutDecPolygonQmlPath)
{
	QString qstrQmlPath = "";
	// 运行环境路径
	QString qstrRunDirPath = QCoreApplication::applicationDirPath();

	QString qstrScale = "";

	// 5万比例尺
	if (dScale == SCALE_50K)
	{
		qstrScale = "50000";
	}
	// 1万比例尺
	else if (dScale == SCALE_10K)
	{
		qstrScale = "10000";
	}

#ifdef OS_FAMILY_WINDOWS

	qstrLayoutDecPolygonQmlPath = qstrRunDirPath + "\\style\\gjb\\" + qstrScale + "\\" + "layout_decpolygon.qml";

#else
	qstrLayoutDecPolygonQmlPath = qstrRunDirPath + "/style/gjb/" + qstrScale + "/" + "layout_decpolygon.qml";

#endif

	qstrLayoutDecPolygonQmlPath = QDir::toNativeSeparators(qstrLayoutDecPolygonQmlPath);
}

void QgsImageMappingDialog_LayoutVectorization::CalGaussBorderCoords(const SE_DRect& dRange,
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
	int iStartKm_X = ceil(dRange.dleft / 1000.0);

	// X方向终止整公里数
	int iEndKm_X = int(dRange.dright / 1000.0);

	for (int i = iStartKm_X; i <= iEndKm_X; i++)
	{
		// 下边界整公里高斯投影值
		SE_DPoint dPointBottom;
		dPointBottom.dx = i * 1000.0;
		dPointBottom.dy = dRange.dbottom;

		vBottomBorderCoords.push_back(dPointBottom);

		// 上边界整公里高斯投影值
		SE_DPoint dPointTop;
		dPointTop.dx = i * 1000.0;
		dPointTop.dy = dRange.dtop;

		vTopBorderCoords.push_back(dPointTop);
	}

	// Y方向起始整公里数，向上取整
	int iStartKm_Y = ceil(dRange.dbottom / 1000.0);

	// Y方向终止整公里数
	int iEndKm_Y = int(dRange.dtop / 1000.0);

	for (int i = iStartKm_Y; i <= iEndKm_Y; i++)
	{
		// 左边界整公里高斯投影值
		SE_DPoint dPointLeft;
		dPointLeft.dx = dRange.dleft;
		dPointLeft.dy = i * 1000.0;

		vLeftBorderCoords.push_back(dPointLeft);

		// 右边界整公里高斯投影值
		SE_DPoint dPointRight;
		dPointRight.dx = dRange.dright;
		dPointRight.dy = i * 1000.0;

		vRightBorderCoords.push_back(dPointRight);
	}
}

void QgsImageMappingDialog_LayoutVectorization::CalLayoutBorderPoints(
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

int QgsImageMappingDialog_LayoutVectorization::CalBandNumber(double dCenterMeridian, double dScale)
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

int QgsImageMappingDialog_LayoutVectorization::bIsAdjProjectBandSheet(double dScale, const QString& qstrSheet, double dCenterMeridian)
{
	// 当前投影带左边界经度值，单位：度
	double dProjectBandLeft = 0;

	// 当前投影带右边界经度值，单位：度
	double dProjectBandRight = 0;

	// 如果比例尺是5万，采用6°分带
	if (dScale == SCALE_50K)
	{
		dProjectBandLeft = dCenterMeridian - 3;
		dProjectBandRight = dCenterMeridian + 3;
	}
	// 如果比例尺是1万，采用3°分带
	else if (dScale == SCALE_10K)
	{
		dProjectBandLeft = dCenterMeridian - 1.5;
		dProjectBandRight = dCenterMeridian + 1.5;
	}

	// 获取当前图幅的经纬度范围
	CSE_MapSheet mapSheet;
	string strSheet = qstrSheet.toLocal8Bit();
	double dLeft = 0;
	double dTop = 0;
	double dRight = 0;
	double dBottom = 0;
	mapSheet.get_box(strSheet, dLeft, dTop, dRight, dBottom);

	// 判断当前图幅中心经度与投影带左边界经度差是否在4个图幅范围内，如果在，说明当前图幅为西侧邻带；
	// 如果不是，继续判断当前图幅中心经度与投影带右边界是否在1个图幅内，如果在，说明当前图幅为东侧邻带；
	double dCenterLon = (dLeft + dRight) * 0.5;
	double dSheetWidth = dRight - dLeft;

	// 西侧邻带
	if (fabs(dCenterLon - dProjectBandLeft) < 4 * dSheetWidth)
	{
		return 1;
	}
	else
	{
		// 非邻带
		if (fabs(dCenterLon - dProjectBandRight) > dSheetWidth)
		{
			return 0;
		}
		// 东侧邻带
		else
		{
			return 2;
		}
	}
	return 0;
}

void QgsImageMappingDialog_LayoutVectorization::DrawAdjKmNet(QgsPrintLayout* pPrintLayout,
	vector<SE_DPoint>& vOuterLeftBorderLayoutCoords,
	vector<SE_DPoint>& vOuterTopBorderLayoutCoords,
	vector<SE_DPoint>& vOuterRightBorderLayoutCoords,
	vector<SE_DPoint>& vOuterBottomBorderLayoutCoords,
	vector<SE_DPoint>& vLeftBorderAdjGaussCoords,
	vector<SE_DPoint>& vTopBorderAdjGaussCoords,
	vector<SE_DPoint>& vRightBorderAdjGaussCoords,
	vector<SE_DPoint>& vBottomBorderAdjGaussCoords,
	double dCenterMeridian,
	double dScale,
	QgsLineSymbol* kmLineBorderLine)
{
	/*-----------------------------------------------------*/
	/*   绘制上边界、下边界、左边界、右边界方里网刻度值    */
	/*-----------------------------------------------------*/
	/*绘制刻度值规则：
	（1）外图廓最左侧方里网纵线和最右侧方里网纵线需要写带号+百公里数+十位公里数值；
	（2）其它方里网纵线只需写十位公里数值；
	（3）上边界参考点位置为左下角，坐标值为对应上边界整公里刻度点；
	（4）下边界参考点位置为左上角，坐标值为对应下边界整公里刻度点；
	（5）外图廓最上侧方里网横线和最下侧方里网纵线需要写千位百位+十位公里数值；
	（6）左边界参考点位置为右下角，坐标值为对应左边界整公里刻度点；
	（7）右边界参考点位置为左下角，坐标值为对应右边界整公里刻度点；
	*/

#pragma region "绘制上边界"

	// 上边界带号
	QgsLayoutItemLabel* pTopBorderLabel_BandNumber = nullptr;
	// 设置字体及大小
	QFont fontBandNumber;
	fontBandNumber.setFamily(QStringLiteral("等线"));
	fontBandNumber.setPixelSize(21);        // 单位：像素
	fontBandNumber.setBold(true);

	// 上边界百公里数值
	QgsLayoutItemLabel* pTopBorderLabel_100km = nullptr;
	// 设置字体及大小
	QFont font100km;
	font100km.setFamily(QStringLiteral("等线"));
	font100km.setPixelSize(21);		// 7.1P≈21像素
	font100km.setBold(true);

	// 上边界十位整公里数值
	QgsLayoutItemLabel* pTopBorderLabel_10km = nullptr;
	// 设置字体及大小
	QFont font10km;
	font10km.setFamily(QStringLiteral("等线"));
	font10km.setPixelSize(35);
	font10km.setBold(true);

	/*记录需要绘制百公里方里网数值的第一个点和最后一个点索引值*/
	// 第一个需要绘制百公里及带号的上边界整公里方里网点索引
	int iBeginMarkIndex_X_Top = 0;                                           // 默认索引为0
	int iEndMarkIndex_X_Top = vOuterTopBorderLayoutCoords.size() - 1;        // 默认索引为最后一个点

#pragma region "计算上边界百公里及带号绘制的索引"


	for (int iPointIndex = 0; iPointIndex < vOuterTopBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutTopBorderPoint = vOuterTopBorderLayoutCoords[iPointIndex];
		if (layoutTopBorderPoint.dx - m_dInteriorMapLayoutPoint_A.dx >= 0)
		{
			iBeginMarkIndex_X_Top = iPointIndex;
			break;
		}
	}

	for (int iPointIndex = 0; iPointIndex < vOuterTopBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutTopBorderPoint = vOuterTopBorderLayoutCoords[iPointIndex];
		if (layoutTopBorderPoint.dx - m_dInteriorMapLayoutPoint_B.dx >= 0)
		{
			iEndMarkIndex_X_Top = iPointIndex - 1;
			break;
		}
	}


#pragma endregion




	/*绘制外图廓上边界整公里刻度值*/
	for (int iPointIndex = iBeginMarkIndex_X_Top; iPointIndex <= iEndMarkIndex_X_Top; iPointIndex++)
	{
		//--------------------------------------------------------------------------//
		// 修改时间：2023-03-22
		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		//---------------------【开始】----------------------------------------------//
		int i10kmValue = int(vTopBorderAdjGaussCoords[iPointIndex].dx / 1000.0);
		if (dScale == SCALE_50K
			&& iPointIndex != iBeginMarkIndex_X_Top
			&& iPointIndex != iEndMarkIndex_X_Top
			&& i10kmValue % 2 != 0)
		{
			continue;
		}
		//----------------------【结束】---------------------------------------------//


		// 外图廓上边界整公里刻度点
		SE_DPoint layoutTopBorderPoint = vOuterTopBorderLayoutCoords[iPointIndex];
		SE_DPoint layoutTopBorderGaussPoint = vTopBorderAdjGaussCoords[iPointIndex];
		// 绘制十位公里数值
		pTopBorderLabel_10km = new QgsLayoutItemLabel(pPrintLayout);
		if (!pTopBorderLabel_10km)
		{
			// 记录日志
			continue;
		}

		pTopBorderLabel_10km->setFont(font10km);
		QString qstr10km = tr("%1").arg(int(layoutTopBorderGaussPoint.dx / 1000.0));
		qstr10km = qstr10km.right(2);
		pTopBorderLabel_10km->setText(qstr10km);

		pTopBorderLabel_10km->attemptSetSceneRect(QRectF(layoutTopBorderPoint.dx + 0.15,
			layoutTopBorderPoint.dy - 3.5,
			5,
			5));


		pPrintLayout->addLayoutItem(pTopBorderLabel_10km);

		// 绘制百公里和带号
		if (iPointIndex == iBeginMarkIndex_X_Top ||
			iPointIndex == iEndMarkIndex_X_Top)
		{
			//-------------------------------------------------//
			// 百公里
			//-------------------------------------------------//
			pTopBorderLabel_100km = new QgsLayoutItemLabel(pPrintLayout);
			if (!pTopBorderLabel_100km)
			{
				// 记录日志
				continue;
			}

			pTopBorderLabel_100km->setFont(font100km);
			QString qstr100km = tr("%1").arg(int(layoutTopBorderGaussPoint.dx / 100000.0));
			pTopBorderLabel_100km->setText(qstr100km);

			pTopBorderLabel_100km->attemptSetSceneRect(QRectF(layoutTopBorderPoint.dx - 1.5,
				layoutTopBorderPoint.dy - 2.5,
				3,
				3));


			pPrintLayout->addLayoutItem(pTopBorderLabel_100km);
			//--------------------------------------------------//

			//-------------------------------------------------//
			// 高斯投影带号
			//-------------------------------------------------//
			pTopBorderLabel_BandNumber = new QgsLayoutItemLabel(pPrintLayout);
			if (!pTopBorderLabel_BandNumber)
			{
				// 记录日志
				continue;
			}

			pTopBorderLabel_BandNumber->setFont(fontBandNumber);
			int iBandNumber = CalBandNumber(dCenterMeridian, dScale);
			QString qstrBandNumber = tr("%1").arg(iBandNumber);
			pTopBorderLabel_BandNumber->setText(qstrBandNumber);


			pTopBorderLabel_BandNumber->attemptSetSceneRect(QRectF(layoutTopBorderPoint.dx - 4,
				layoutTopBorderPoint.dy - 3.3,
				4,
				4));


			pPrintLayout->addLayoutItem(pTopBorderLabel_BandNumber);
			//--------------------------------------------------//
		}
	}

#pragma endregion

#pragma region "绘制下边界"

	/*记录需要绘制百公里方里网数值的第一个点和最后一个点索引值*/
	// 第一个需要绘制百公里及带号的下边界整公里方里网点索引
	int iBeginMarkIndex_X_Bottom = 0;												// 默认索引为0
	int iEndMarkIndex_X_Bottom = vOuterBottomBorderLayoutCoords.size() - 1;         // 默认索引为最后一个点

#pragma region "计算下边界百公里及带号绘制的索引"


	for (int iPointIndex = 0; iPointIndex < vOuterBottomBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutBottomBorderPoint = vOuterBottomBorderLayoutCoords[iPointIndex];
		if (layoutBottomBorderPoint.dx - m_dInteriorMapLayoutPoint_D.dx >= 0)
		{
			iBeginMarkIndex_X_Bottom = iPointIndex;
			break;
		}
	}

	for (int iPointIndex = 0; iPointIndex < vOuterBottomBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutBottomBorderPoint = vOuterBottomBorderLayoutCoords[iPointIndex];
		if (layoutBottomBorderPoint.dx - m_dInteriorMapLayoutPoint_C.dx >= 0)
		{
			iEndMarkIndex_X_Bottom = iPointIndex - 1;
			break;
		}
	}

#pragma endregion


	// 下边界带号
	QgsLayoutItemLabel* pBottomBorderLabel_BandNumber = nullptr;

	// 下边界百公里数值
	QgsLayoutItemLabel* pBottomBorderLabel_100km = nullptr;

	// 下边界十位整公里数值
	QgsLayoutItemLabel* pBottomBorderLabel_10km = nullptr;

	/*绘制内图廓下边界整公里刻度值*/
	for (int iPointIndex = iBeginMarkIndex_X_Bottom; iPointIndex <= iEndMarkIndex_X_Bottom; iPointIndex++)
	{
		//--------------------------------------------------------------------------//
		// 修改时间：2023-03-22
		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		//---------------------【开始】----------------------------------------------//
		int i10kmValue = int(vBottomBorderAdjGaussCoords[iPointIndex].dx / 1000.0);
		if (dScale == SCALE_50K
			&& iPointIndex != iBeginMarkIndex_X_Bottom
			&& iPointIndex != iEndMarkIndex_X_Bottom
			&& i10kmValue % 2 != 0)
		{
			continue;
		}
		//----------------------【结束】---------------------------------------------//



		// 内图廓下边界整公里刻度点
		SE_DPoint layoutBottomBorderPoint = vOuterBottomBorderLayoutCoords[iPointIndex];
		SE_DPoint layoutBottomBorderGaussPoint = vBottomBorderAdjGaussCoords[iPointIndex];

		// 绘制十位公里数值
		pBottomBorderLabel_10km = new QgsLayoutItemLabel(pPrintLayout);
		if (!pBottomBorderLabel_10km)
		{
			// 记录日志
			continue;
		}

		pBottomBorderLabel_10km->setFont(font10km);
		QString qstr10km = tr("%1").arg(int(layoutBottomBorderGaussPoint.dx / 1000.0));
		qstr10km = qstr10km.right(2);
		pBottomBorderLabel_10km->setText(qstr10km);

		// 10km刻度值
		pBottomBorderLabel_10km->attemptSetSceneRect(QRectF(layoutBottomBorderPoint.dx + 0.15,
			layoutBottomBorderPoint.dy + 0.1,
			5,
			5));


		pPrintLayout->addLayoutItem(pBottomBorderLabel_10km);

		// 绘制百公里和带号
		if (iPointIndex == iBeginMarkIndex_X_Bottom ||
			iPointIndex == iEndMarkIndex_X_Bottom)
		{
			//-------------------------------------------------//
			// 百公里
			//-------------------------------------------------//
			pBottomBorderLabel_100km = new QgsLayoutItemLabel(pPrintLayout);
			if (!pBottomBorderLabel_100km)
			{
				// 记录日志
				continue;
			}

			pBottomBorderLabel_100km->setFont(font100km);
			QString qstr100km = tr("%1").arg(int(layoutBottomBorderGaussPoint.dx / 100000.0));
			pBottomBorderLabel_100km->setText(qstr100km);


			// 100km刻度值
			pBottomBorderLabel_100km->attemptSetSceneRect(QRectF(layoutBottomBorderPoint.dx - 1.5,
				layoutBottomBorderPoint.dy + 1.2,
				3,
				3));


			pPrintLayout->addLayoutItem(pBottomBorderLabel_100km);
			//--------------------------------------------------//

			//-------------------------------------------------//
			// 高斯投影带号
			//-------------------------------------------------//
			pBottomBorderLabel_BandNumber = new QgsLayoutItemLabel(pPrintLayout);
			if (!pBottomBorderLabel_BandNumber)
			{
				// 记录日志
				continue;
			}

			pBottomBorderLabel_BandNumber->setFont(fontBandNumber);
			int iBandNumber = CalBandNumber(dCenterMeridian, dScale);
			QString qstrBandNumber = tr("%1").arg(iBandNumber);
			pBottomBorderLabel_BandNumber->setText(qstrBandNumber);


			// 带号
			pBottomBorderLabel_BandNumber->attemptSetSceneRect(QRectF(layoutBottomBorderPoint.dx - 4,
				layoutBottomBorderPoint.dy + 0.1,
				4,
				4));



			pPrintLayout->addLayoutItem(pBottomBorderLabel_BandNumber);
			//--------------------------------------------------//
		}
	}
#pragma endregion

#pragma region "绘制左边界"
	/*--------------------------------------*/
	/*         绘制左边界方里网数值         */
	/*--------------------------------------*/
	/*-------------【begin】----------------*/
	// 设置字体及大小
	QFont font100km_Y;      // 左右边界千公里+百公里
	font100km_Y.setFamily(QStringLiteral("等线"));
	font100km_Y.setPixelSize(21);
	font100km_Y.setBold(true);

	// 左边界千公里+百公里数值
	QgsLayoutItemLabel* pLeftBorderLabel_100km = nullptr;

	// 左边界十位整公里数值
	QgsLayoutItemLabel* pLeftBorderLabel_10km = nullptr;

	/*记录需要绘制千公里+百公里方里网数值的第一个点和最后一个点索引值*/
	// 第一个需要绘制千公里+百公里的左边界整公里方里网点索引
	int iBeginMarkIndex_Y_Left = 0;			// 下边界高斯坐标，默认索引为0
	int iEndMarkIndex_Y_Left = vOuterLeftBorderLayoutCoords.size() - 1;        // 上边界高斯坐标


	for (int iPointIndex = 0; iPointIndex < vOuterLeftBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutLeftBorderPoint = vOuterLeftBorderLayoutCoords[iPointIndex];
		if (layoutLeftBorderPoint.dy - m_dInteriorMapLayoutPoint_D.dy <= 0)
		{
			iBeginMarkIndex_Y_Left = iPointIndex;
			break;
		}
	}

	for (int iPointIndex = 0; iPointIndex < vOuterLeftBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutLeftBorderPoint = vOuterLeftBorderLayoutCoords[iPointIndex];
		if (layoutLeftBorderPoint.dy - m_dInteriorMapLayoutPoint_A.dy <= 0)
		{
			iEndMarkIndex_Y_Left = iPointIndex - 1;
			break;
		}
	}


	/*绘制外图廓左边界整公里刻度值*/
	for (int iPointIndex = iBeginMarkIndex_Y_Left; iPointIndex <= iEndMarkIndex_Y_Left; iPointIndex++)
	{
		//--------------------------------------------------------------------------//
		// 修改时间：2023-03-22
		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		//---------------------【开始】----------------------------------------------//
		int i10kmValue = int(vLeftBorderAdjGaussCoords[iPointIndex].dy / 1000.0);
		if (dScale == SCALE_50K
			&& iPointIndex != iBeginMarkIndex_Y_Left
			&& iPointIndex != iEndMarkIndex_Y_Left
			&& i10kmValue % 2 != 0)
		{
			continue;
		}
		//----------------------【结束】---------------------------------------------//



		// 外图廓左边界整公里刻度点布局坐标值
		SE_DPoint layoutLeftBorderPoint = vOuterLeftBorderLayoutCoords[iPointIndex];

		// 外图廓左边界整公里高斯坐标值
		SE_DPoint layoutLeftBorderGaussPoint = vLeftBorderAdjGaussCoords[iPointIndex];

		// 左边界绘制十位公里数值
		pLeftBorderLabel_10km = new QgsLayoutItemLabel(pPrintLayout);
		if (!pLeftBorderLabel_10km)
		{
			// 记录日志
			continue;
		}

		pLeftBorderLabel_10km->setFont(font10km);
		QString qstr10km = tr("%1").arg(int(layoutLeftBorderGaussPoint.dy / 1000.0));
		qstr10km = qstr10km.right(2);
		pLeftBorderLabel_10km->setText(qstr10km);


		// 10km刻度值
		pLeftBorderLabel_10km->attemptSetSceneRect(QRectF(layoutLeftBorderPoint.dx - 3.9,
			layoutLeftBorderPoint.dy - 3.35,
			5,
			5));

		/*-----------------------------------------------*/
		// 修改时间：2023-03-11
		// 修改内容：邻带方里网刻度值旋转

		pLeftBorderLabel_10km->rotateItem(-90, QPointF(layoutLeftBorderPoint.dx - pLeftBorderLabel_10km->rect().width() / 2,
			layoutLeftBorderPoint.dy - pLeftBorderLabel_10km->rect().height() / 2));

		/*-----------------------------------------------*/



		pPrintLayout->addLayoutItem(pLeftBorderLabel_10km);

		// 绘制千公里+百公里
		if (iPointIndex == iBeginMarkIndex_Y_Left ||
			iPointIndex == iEndMarkIndex_Y_Left ||
			qstr10km == "00")
		{
			//-------------------------------------------------//
			// 千公里+百公里
			//-------------------------------------------------//
			pLeftBorderLabel_100km = new QgsLayoutItemLabel(pPrintLayout);
			if (!pLeftBorderLabel_100km)
			{
				// 记录日志
				continue;
			}

			pLeftBorderLabel_100km->setFont(font100km_Y);
			/*QString qstr100km = tr("%1").arg(int(layoutLeftBorderGaussPoint.dy / 100000.0));*/
			QString qstr100km;
			int i100km = abs(int(layoutLeftBorderGaussPoint.dy / 100000.0));
			// 千位补0
			if (i100km < 10 && i100km > 0)
			{
				qstr100km = tr("0%1").arg(int(layoutLeftBorderGaussPoint.dy / 100000.0));
			}
			else if (i100km == 0)
			{
				qstr100km = tr("00");
			}
			else if (i100km >= 10)
			{
				qstr100km = tr("%1").arg(int(layoutLeftBorderGaussPoint.dy / 100000.0));
			}



			pLeftBorderLabel_100km->setText(qstr100km);

			// 100km刻度值
			/*2023-03-11 邻带千百公里刻度调整
			*/
			//-------------------------------------------//
			pLeftBorderLabel_100km->attemptSetSceneRect(QRectF(layoutLeftBorderPoint.dx - 3,
				layoutLeftBorderPoint.dy - 3,
				3.5,
				3.5));

			pLeftBorderLabel_100km->rotateItem(-90, QPointF(layoutLeftBorderPoint.dx,
				layoutLeftBorderPoint.dy));
			//-------------------------------------------//


			pPrintLayout->addLayoutItem(pLeftBorderLabel_100km);
			//--------------------------------------------------//
		}
	}
	/*-------------【end】----------------*/
#pragma endregion

#pragma region "绘制右边界"
	 /*--------------------------------------*/
	/*         绘制右边界方里网数值         */
	/*--------------------------------------*/
	/*-------------【begin】----------------*/

	// 右边界千公里+百公里数值
	QgsLayoutItemLabel* pRightBorderLabel_100km = nullptr;

	// 右边界十位整公里数值
	QgsLayoutItemLabel* pRightBorderLabel_10km = nullptr;



	/*记录需要绘制千公里+百公里方里网数值的第一个点和最后一个点索引值*/
// 第一个需要绘制千公里+百公里的右边界整公里方里网点索引
	int iBeginMarkIndex_Y_Right = 0;			// 下边界高斯坐标，默认索引为0
	int iEndMarkIndex_Y_Right = vOuterRightBorderLayoutCoords.size() - 1;        // 上边界高斯坐标


	for (int iPointIndex = 0; iPointIndex < vOuterRightBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutRightBorderPoint = vOuterRightBorderLayoutCoords[iPointIndex];
		if (layoutRightBorderPoint.dy - m_dInteriorMapLayoutPoint_C.dy <= 0)
		{
			iBeginMarkIndex_Y_Right = iPointIndex;
			break;
		}
	}

	for (int iPointIndex = 0; iPointIndex < vOuterRightBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutRightBorderPoint = vOuterRightBorderLayoutCoords[iPointIndex];
		if (layoutRightBorderPoint.dy - m_dInteriorMapLayoutPoint_B.dy <= 0)
		{
			iEndMarkIndex_Y_Right = iPointIndex - 1;
			break;
		}
	}


	/*绘制外图廓右边界整公里刻度值*/
	for (int iPointIndex = iBeginMarkIndex_Y_Right; iPointIndex <= iEndMarkIndex_Y_Right; iPointIndex++)
	{
		//--------------------------------------------------------------------------//
		// 修改时间：2023-03-22
		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		//---------------------【开始】----------------------------------------------//
		int i10kmValue = int(vRightBorderAdjGaussCoords[iPointIndex].dy / 1000.0);
		if (dScale == SCALE_50K
			&& iPointIndex != iBeginMarkIndex_Y_Right
			&& iPointIndex != iEndMarkIndex_Y_Right
			&& i10kmValue % 2 != 0)
		{
			continue;
		}
		//----------------------【结束】---------------------------------------------//


		// 外图廓右边界整公里刻度点布局坐标值
		SE_DPoint layoutRightBorderPoint = vOuterRightBorderLayoutCoords[iPointIndex];

		// 外图廓右边界整公里高斯坐标值
		SE_DPoint layoutRightBorderGaussPoint = vRightBorderAdjGaussCoords[iPointIndex];

		// 右边界绘制十位公里数值
		pRightBorderLabel_10km = new QgsLayoutItemLabel(pPrintLayout);
		if (!pRightBorderLabel_10km)
		{
			// 记录日志
			continue;
		}

		pRightBorderLabel_10km->setFont(font10km);
		QString qstr10km = tr("%1").arg(int(layoutRightBorderGaussPoint.dy / 1000.0));
		qstr10km = qstr10km.right(2);
		pRightBorderLabel_10km->setText(qstr10km);



		// 10km刻度值
		pRightBorderLabel_10km->attemptSetSceneRect(QRectF(layoutRightBorderPoint.dx + ADJ_KM_LINE_LENGTH_RIGHT - 3.9,
			layoutRightBorderPoint.dy - 3.35,
			5,
			5));

		/*-----------------------------------------------*/
		// 修改时间：2023-03-11
		// 修改内容：邻带方里网刻度值旋转

		pRightBorderLabel_10km->rotateItem(-90, QPointF(layoutRightBorderPoint.dx + ADJ_KM_LINE_LENGTH_RIGHT - pLeftBorderLabel_10km->rect().width() / 2,
			layoutRightBorderPoint.dy - pRightBorderLabel_10km->rect().height() / 2));

		/*-----------------------------------------------*/


		pPrintLayout->addLayoutItem(pRightBorderLabel_10km);

		// 绘制千公里+百公里
		if (iPointIndex == iBeginMarkIndex_Y_Right ||
			iPointIndex == iEndMarkIndex_Y_Right ||
			qstr10km == "00")
		{
			//-------------------------------------------------//
			// 千公里+百公里
			//-------------------------------------------------//
			pRightBorderLabel_100km = new QgsLayoutItemLabel(pPrintLayout);
			if (!pRightBorderLabel_100km)
			{
				// 记录日志
				continue;
			}

			pRightBorderLabel_100km->setFont(font100km_Y);
			/*QString qstr100km = tr("%1").arg(int(layoutRightBorderGaussPoint.dy / 100000.0));*/
			QString qstr100km;
			int i100km = abs(int(layoutRightBorderGaussPoint.dy / 100000.0));
			// 千位补0
			if (i100km < 10 && i100km > 0)
			{
				qstr100km = tr("0%1").arg(int(layoutRightBorderGaussPoint.dy / 100000.0));
			}
			else if (i100km == 0)
			{
				qstr100km = tr("00");
			}
			else if (i100km >= 10)
			{
				qstr100km = tr("%1").arg(int(layoutRightBorderGaussPoint.dy / 100000.0));
			}

			pRightBorderLabel_100km->setText(qstr100km);

			// 100km刻度值

			/*2023-03-11 邻带千百公里刻度调整
			*/
			//-------------------------------------------//
			pRightBorderLabel_100km->attemptSetSceneRect(QRectF(layoutRightBorderPoint.dx + ADJ_KM_LINE_LENGTH_RIGHT - 3,
				layoutRightBorderPoint.dy - 3,
				3.5,
				3.5));

			pRightBorderLabel_100km->rotateItem(-90, QPointF(layoutRightBorderPoint.dx + ADJ_KM_LINE_LENGTH_RIGHT,
				layoutRightBorderPoint.dy));
			//-------------------------------------------//


			pPrintLayout->addLayoutItem(pRightBorderLabel_100km);
			//--------------------------------------------------//
		}
	}
	/*-------------【end】-----------------*/
#pragma endregion

#pragma region "绘制刻度线"

	/*-------------------------------------*/
	/*           绘制上边界刻度线          */
	/*-------------------------------------*/
	// 外图廓上边界刻度
	QgsLayoutItemPolyline* pAdjKmLine_Top = nullptr;
	for (int iPointIndex = iBeginMarkIndex_X_Top; iPointIndex <= iEndMarkIndex_X_Top; iPointIndex++)
	{
		// 外图廓上边界整公里刻度点
		SE_DPoint layoutTopBorderPoint = vOuterTopBorderLayoutCoords[iPointIndex];

		QPolygonF polygon;

		// 邻带方里网外图廓上边界刻度线
		polygon.append(QPointF(layoutTopBorderPoint.dx, layoutTopBorderPoint.dy - ADJ_KM_LINE_LENGTH));

		// 邻带方里网整公里刻度线与外图廓上边界交叉点
		polygon.append(QPointF(layoutTopBorderPoint.dx, layoutTopBorderPoint.dy));

		pAdjKmLine_Top = new QgsLayoutItemPolyline(polygon, pPrintLayout);
		pAdjKmLine_Top->setSymbol(kmLineBorderLine);
		pPrintLayout->addLayoutItem(pAdjKmLine_Top);
	}

	/*-------------------------------------*/
	/*           绘制下边界刻度线          */
	/*-------------------------------------*/
	// 外图廓下边界刻度
	QgsLayoutItemPolyline* pAdjKmLine_Bottom = nullptr;
	for (int iPointIndex = iBeginMarkIndex_X_Bottom; iPointIndex <= iEndMarkIndex_X_Bottom; iPointIndex++)
	{
		// 外图廓下边界整公里刻度点
		SE_DPoint layoutBottomBorderPoint = vOuterBottomBorderLayoutCoords[iPointIndex];

		QPolygonF polygon;

		// 邻带方里网外图廓下边界刻度线
		polygon.append(QPointF(layoutBottomBorderPoint.dx, layoutBottomBorderPoint.dy + ADJ_KM_LINE_LENGTH));

		// 邻带方里网整公里刻度线与外图廓下边界交叉点
		polygon.append(QPointF(layoutBottomBorderPoint.dx, layoutBottomBorderPoint.dy));

		pAdjKmLine_Bottom = new QgsLayoutItemPolyline(polygon, pPrintLayout);
		pAdjKmLine_Bottom->setSymbol(kmLineBorderLine);
		pPrintLayout->addLayoutItem(pAdjKmLine_Bottom);
	}

	/*-------------------------------------*/
	/*           绘制左边界刻度线          */
	/*-------------------------------------*/
	// 外图廓左边界刻度
	QgsLayoutItemPolyline* pAdjKmLine_Left = nullptr;
	for (int iPointIndex = iBeginMarkIndex_Y_Left; iPointIndex <= iEndMarkIndex_Y_Left; iPointIndex++)
	{
		// 外图廓左边界整公里刻度点
		SE_DPoint layoutLeftBorderPoint = vOuterLeftBorderLayoutCoords[iPointIndex];

		QPolygonF polygon;

		// 邻带方里网外图廓左边界刻度线
		polygon.append(QPointF(layoutLeftBorderPoint.dx - ADJ_KM_LINE_LENGTH, layoutLeftBorderPoint.dy));

		// 邻带方里网整公里刻度线与外图廓左边界交叉点
		polygon.append(QPointF(layoutLeftBorderPoint.dx, layoutLeftBorderPoint.dy));

		pAdjKmLine_Left = new QgsLayoutItemPolyline(polygon, pPrintLayout);
		pAdjKmLine_Left->setSymbol(kmLineBorderLine);
		pPrintLayout->addLayoutItem(pAdjKmLine_Left);
	}

	/*-------------------------------------*/
	/*           绘制左边界刻度线          */
	/*-------------------------------------*/
	// 外图廓右边界刻度
	QgsLayoutItemPolyline* pAdjKmLine_Right = nullptr;
	for (int iPointIndex = iBeginMarkIndex_Y_Right; iPointIndex <= iEndMarkIndex_Y_Right; iPointIndex++)
	{
		// 外图廓右边界整公里刻度点
		SE_DPoint layoutRightBorderPoint = vOuterRightBorderLayoutCoords[iPointIndex];

		QPolygonF polygon;

		// 邻带方里网外图廓右边界刻度线
		polygon.append(QPointF(layoutRightBorderPoint.dx + ADJ_KM_LINE_LENGTH, layoutRightBorderPoint.dy));

		// 邻带方里网整公里刻度线与外图廓右边界交叉点
		polygon.append(QPointF(layoutRightBorderPoint.dx, layoutRightBorderPoint.dy));

		pAdjKmLine_Right = new QgsLayoutItemPolyline(polygon, pPrintLayout);
		pAdjKmLine_Right->setSymbol(kmLineBorderLine);
		pPrintLayout->addLayoutItem(pAdjKmLine_Right);
	}

#pragma endregion
}

void QgsImageMappingDialog_LayoutVectorization::DrawAdjKmNet_CreateLayer(
	OGRLayer *pLayoutPointLayer,
	OGRLayer * pLayoutLineLayer,
	SE_DPoint dRefGaussPoint,
	SE_DPoint dRefLayoutPoint,
	vector<SE_DPoint>& vOuterLeftBorderLayoutCoords,
	vector<SE_DPoint>& vOuterTopBorderLayoutCoords,
	vector<SE_DPoint>& vOuterRightBorderLayoutCoords,
	vector<SE_DPoint>& vOuterBottomBorderLayoutCoords,
	vector<SE_DPoint>& vLeftBorderAdjGaussCoords,
	vector<SE_DPoint>& vTopBorderAdjGaussCoords,
	vector<SE_DPoint>& vRightBorderAdjGaussCoords,
	vector<SE_DPoint>& vBottomBorderAdjGaussCoords,
	double dCenterMeridian,
	double dScale,
	double dMapUnit2LayoutUnit)
{
	/*-----------------------------------------------------*/
	/*   绘制上边界、下边界、左边界、右边界方里网刻度值    */
	/*-----------------------------------------------------*/
	/*绘制刻度值规则：
	（1）外图廓最左侧方里网纵线和最右侧方里网纵线需要写带号+百公里数+十位公里数值；
	（2）其它方里网纵线只需写十位公里数值；
	（3）上边界参考点位置为左下角，坐标值为对应上边界整公里刻度点；
	（4）下边界参考点位置为左上角，坐标值为对应下边界整公里刻度点；
	（5）外图廓最上侧方里网横线和最下侧方里网纵线需要写千位百位+十位公里数值；
	（6）左边界参考点位置为右下角，坐标值为对应左边界整公里刻度点；
	（7）右边界参考点位置为左下角，坐标值为对应右边界整公里刻度点；
	*/

#pragma region "绘制上边界"

	/*记录需要绘制百公里方里网数值的第一个点和最后一个点索引值*/
	// 第一个需要绘制百公里及带号的上边界整公里方里网点索引
	int iBeginMarkIndex_X_Top = 0;                                           // 默认索引为0
	int iEndMarkIndex_X_Top = vOuterTopBorderLayoutCoords.size() - 1;        // 默认索引为最后一个点

#pragma region "计算上边界百公里及带号绘制的索引"


	for (int iPointIndex = 0; iPointIndex < vOuterTopBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutTopBorderPoint = vOuterTopBorderLayoutCoords[iPointIndex];
		if (layoutTopBorderPoint.dx - m_dInteriorMapLayoutPoint_A.dx >= 0)
		{
			iBeginMarkIndex_X_Top = iPointIndex;
			break;
		}
	}

	for (int iPointIndex = 0; iPointIndex < vOuterTopBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutTopBorderPoint = vOuterTopBorderLayoutCoords[iPointIndex];
		if (layoutTopBorderPoint.dx - m_dInteriorMapLayoutPoint_B.dx >= 0)
		{
			iEndMarkIndex_X_Top = iPointIndex - 1;
			break;
		}
	}


#pragma endregion




	/*绘制外图廓上边界整公里刻度值*/
	for (int iPointIndex = iBeginMarkIndex_X_Top; iPointIndex <= iEndMarkIndex_X_Top; iPointIndex++)
	{
		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		// 每个要素的属性信息
		vector<FieldNameAndValue_Imp> vFieldNameAndValue;
		vFieldNameAndValue.clear();



		SE_DPoint layoutTopBorderGaussPoint = vTopBorderAdjGaussCoords[iPointIndex];

		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		int i10kmValue = int(vTopBorderAdjGaussCoords[iPointIndex].dx / 1000.0);
		if (dScale == SCALE_50K
			&& iPointIndex != iBeginMarkIndex_X_Top
			&& iPointIndex != iEndMarkIndex_X_Top
			&& i10kmValue % 2 != 0)
		{
			continue;
		}

		QString qstr10km = tr("%1").arg(int(layoutTopBorderGaussPoint.dx / 1000.0));
		qstr10km = qstr10km.right(2);

		// 生成“十公里刻度值”点要素
		// 属性信息
		// 类型
		FieldNameAndValue_Imp fieldImp;
		fieldImp.strFieldName = "TYPE";
		fieldImp.strFieldValue = "adj_top_ten_km";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "FLAG";
		fieldImp.strFieldValue = "1";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "VALUE";
		fieldImp.strFieldValue = qstr10km.toLocal8Bit();
		vFieldNameAndValue.push_back(fieldImp);

		SE_DPoint dGaussPoint;
		// 布局坐标转高斯坐标
		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			vOuterTopBorderLayoutCoords[iPointIndex],
			dMapUnit2LayoutUnit,
			dGaussPoint);

		double dValues[2];
		dValues[0] = dGaussPoint.dx;
		dValues[1] = dGaussPoint.dy;

		// 高斯投影转地理坐标
		int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dValues);

		if (iRet != 1)
		{
			continue;
		}

		iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
		if (iRet != 0)
		{
			continue;
		}

		// 绘制百公里和带号
		if (iPointIndex == iBeginMarkIndex_X_Top ||
			iPointIndex == iEndMarkIndex_X_Top)
		{
			QString qstr100km = tr("%1").arg(int(layoutTopBorderGaussPoint.dx / 100000.0));

			vFieldNameAndValue.clear();

			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "adj_top_hundred_km";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = qstr100km.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}

			// 带号
			int iBandNumber = CalBandNumber(dCenterMeridian, dScale);
			QString qstrBandNumber = tr("%1").arg(iBandNumber);

			vFieldNameAndValue.clear();

			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "adj_top_band_number";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = qstrBandNumber.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}
	}

#pragma endregion

#pragma region "绘制下边界"

	/*记录需要绘制百公里方里网数值的第一个点和最后一个点索引值*/
	// 第一个需要绘制百公里及带号的下边界整公里方里网点索引
	int iBeginMarkIndex_X_Bottom = 0;												// 默认索引为0
	int iEndMarkIndex_X_Bottom = vOuterBottomBorderLayoutCoords.size() - 1;         // 默认索引为最后一个点

#pragma region "计算下边界百公里及带号绘制的索引"


	for (int iPointIndex = 0; iPointIndex < vOuterBottomBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutBottomBorderPoint = vOuterBottomBorderLayoutCoords[iPointIndex];
		if (layoutBottomBorderPoint.dx - m_dInteriorMapLayoutPoint_D.dx >= 0)
		{
			iBeginMarkIndex_X_Bottom = iPointIndex;
			break;
		}
	}

	for (int iPointIndex = 0; iPointIndex < vOuterBottomBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutBottomBorderPoint = vOuterBottomBorderLayoutCoords[iPointIndex];
		if (layoutBottomBorderPoint.dx - m_dInteriorMapLayoutPoint_C.dx >= 0)
		{
			iEndMarkIndex_X_Bottom = iPointIndex - 1;
			break;
		}
	}

#pragma endregion


	/*绘制内图廓下边界整公里刻度值*/
	for (int iPointIndex = iBeginMarkIndex_X_Bottom; iPointIndex <= iEndMarkIndex_X_Bottom; iPointIndex++)
	{
		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		// 每个要素的属性信息
		vector<FieldNameAndValue_Imp> vFieldNameAndValue;
		vFieldNameAndValue.clear();


		SE_DPoint layoutBottomBorderGaussPoint = vBottomBorderAdjGaussCoords[iPointIndex];

		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		int i10kmValue = int(vBottomBorderAdjGaussCoords[iPointIndex].dx / 1000.0);
		if (dScale == SCALE_50K
			&& iPointIndex != iBeginMarkIndex_X_Bottom
			&& iPointIndex != iEndMarkIndex_X_Bottom
			&& i10kmValue % 2 != 0)
		{
			continue;
		}

		QString qstr10km = tr("%1").arg(int(layoutBottomBorderGaussPoint.dx / 1000.0));
		qstr10km = qstr10km.right(2);

		// 生成“十公里刻度值”点要素
		// 属性信息
		// 类型
		FieldNameAndValue_Imp fieldImp;
		fieldImp.strFieldName = "TYPE";
		fieldImp.strFieldValue = "adj_bottom_ten_km";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "FLAG";
		fieldImp.strFieldValue = "1";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "VALUE";
		fieldImp.strFieldValue = qstr10km.toLocal8Bit();
		vFieldNameAndValue.push_back(fieldImp);


		SE_DPoint dGaussPoint;
		// 布局坐标转高斯坐标
		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			vOuterBottomBorderLayoutCoords[iPointIndex],
			dMapUnit2LayoutUnit,
			dGaussPoint);

		double dValues[2];
		dValues[0] = dGaussPoint.dx;
		dValues[1] = dGaussPoint.dy;


		// 高斯投影转地理坐标
		int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dValues);

		if (iRet != 1)
		{
			continue;
		}

		iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
		if (iRet != 0)
		{
			continue;
		}

		// 绘制百公里和带号
		if (iPointIndex == iBeginMarkIndex_X_Bottom ||
			iPointIndex == iEndMarkIndex_X_Bottom)
		{
			QString qstr100km = tr("%1").arg(int(layoutBottomBorderGaussPoint.dx / 100000.0));

			vFieldNameAndValue.clear();

			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "adj_bottom_hundred_km";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = qstr100km.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}

			// 带号
			int iBandNumber = CalBandNumber(dCenterMeridian, dScale);
			QString qstrBandNumber = tr("%1").arg(iBandNumber);

			vFieldNameAndValue.clear();

			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "adj_bottom_band_number";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = qstrBandNumber.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}
	}
#pragma endregion

#pragma region "绘制左边界"
	/*--------------------------------------*/
	/*         绘制左边界方里网数值         */
	/*--------------------------------------*/


	/*记录需要绘制千公里+百公里方里网数值的第一个点和最后一个点索引值*/
	// 第一个需要绘制千公里+百公里的左边界整公里方里网点索引
	int iBeginMarkIndex_Y_Left = 0;			// 下边界高斯坐标，默认索引为0
	int iEndMarkIndex_Y_Left = vOuterLeftBorderLayoutCoords.size() - 1;        // 上边界高斯坐标


	for (int iPointIndex = 0; iPointIndex < vOuterLeftBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutLeftBorderPoint = vOuterLeftBorderLayoutCoords[iPointIndex];
		if (layoutLeftBorderPoint.dy - m_dInteriorMapLayoutPoint_D.dy <= 0)
		{
			iBeginMarkIndex_Y_Left = iPointIndex;
			break;
		}
	}

	for (int iPointIndex = 0; iPointIndex < vOuterLeftBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutLeftBorderPoint = vOuterLeftBorderLayoutCoords[iPointIndex];
		if (layoutLeftBorderPoint.dy - m_dInteriorMapLayoutPoint_A.dy <= 0)
		{
			iEndMarkIndex_Y_Left = iPointIndex - 1;
			break;
		}
	}


	/*绘制外图廓左边界整公里刻度值*/
	for (int iPointIndex = iBeginMarkIndex_Y_Left; iPointIndex <= iEndMarkIndex_Y_Left; iPointIndex++)
	{
		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值

		// 每个要素的属性信息
		vector<FieldNameAndValue_Imp> vFieldNameAndValue;
		vFieldNameAndValue.clear();


		SE_DPoint layoutLeftBorderGaussPoint = vLeftBorderAdjGaussCoords[iPointIndex];

		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		int i10kmValue = int(vLeftBorderAdjGaussCoords[iPointIndex].dy / 1000.0);
		if (dScale == SCALE_50K
			&& iPointIndex != iBeginMarkIndex_Y_Left
			&& iPointIndex != iEndMarkIndex_Y_Left
			&& i10kmValue % 2 != 0)
		{
			continue;
		}

		QString qstr10km = tr("%1").arg(int(layoutLeftBorderGaussPoint.dy / 1000.0));
		qstr10km = qstr10km.right(2);

		// 生成“十公里刻度值”点要素
		// 属性信息
		// 类型
		FieldNameAndValue_Imp fieldImp;
		fieldImp.strFieldName = "TYPE";
		fieldImp.strFieldValue = "adj_left_ten_km";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "FLAG";
		fieldImp.strFieldValue = "1";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "VALUE";
		fieldImp.strFieldValue = qstr10km.toLocal8Bit();
		vFieldNameAndValue.push_back(fieldImp);

		SE_DPoint dGaussPoint;
		// 布局坐标转高斯坐标
		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			vOuterLeftBorderLayoutCoords[iPointIndex],
			dMapUnit2LayoutUnit,
			dGaussPoint);

		double dValues[2];
		dValues[0] = dGaussPoint.dx;
		dValues[1] = dGaussPoint.dy;


		// 高斯投影转地理坐标
		int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dValues);

		if (iRet != 1)
		{
			continue;
		}

		iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
		if (iRet != 0)
		{
			continue;
		}

		// 绘制百公里
		if (iPointIndex == iBeginMarkIndex_Y_Left ||
			iPointIndex == iEndMarkIndex_Y_Left ||
			qstr10km == "00")
		{
			int i100km = abs(int(layoutLeftBorderGaussPoint.dy / 100000.0));
			// 千位补0
			QString qstr100km;
			if (i100km < 10 && i100km > 0)
			{
				qstr100km = tr("0%1").arg(int(layoutLeftBorderGaussPoint.dy / 100000.0));
			}
			else if (i100km == 0)
			{
				qstr100km = tr("00");
			}
			else if (i100km >= 10)
			{
				qstr100km = tr("%1").arg(int(layoutLeftBorderGaussPoint.dy / 100000.0));
			}

			vFieldNameAndValue.clear();

			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "adj_left_hundred_km";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = qstr100km.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}

		}
	}
	/*-------------【end】----------------*/
#pragma endregion

#pragma region "绘制右边界"
	 /*--------------------------------------*/
	/*         绘制右边界方里网数值         */
	/*--------------------------------------*/



	/*记录需要绘制千公里+百公里方里网数值的第一个点和最后一个点索引值*/
	// 第一个需要绘制千公里+百公里的右边界整公里方里网点索引
	int iBeginMarkIndex_Y_Right = 0;			// 下边界高斯坐标，默认索引为0
	int iEndMarkIndex_Y_Right = vOuterRightBorderLayoutCoords.size() - 1;        // 上边界高斯坐标


	for (int iPointIndex = 0; iPointIndex < vOuterRightBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutRightBorderPoint = vOuterRightBorderLayoutCoords[iPointIndex];
		if (layoutRightBorderPoint.dy - m_dInteriorMapLayoutPoint_C.dy <= 0)
		{
			iBeginMarkIndex_Y_Right = iPointIndex;
			break;
		}
	}

	for (int iPointIndex = 0; iPointIndex < vOuterRightBorderLayoutCoords.size(); iPointIndex++)
	{
		SE_DPoint layoutRightBorderPoint = vOuterRightBorderLayoutCoords[iPointIndex];
		if (layoutRightBorderPoint.dy - m_dInteriorMapLayoutPoint_B.dy <= 0)
		{
			iEndMarkIndex_Y_Right = iPointIndex - 1;
			break;
		}
	}


	/*绘制外图廓右边界整公里刻度值*/
	for (int iPointIndex = iBeginMarkIndex_Y_Right; iPointIndex <= iEndMarkIndex_Y_Right; iPointIndex++)
	{
		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		// 每个要素的属性信息
		vector<FieldNameAndValue_Imp> vFieldNameAndValue;
		vFieldNameAndValue.clear();

		SE_DPoint layoutRightBorderGaussPoint = vRightBorderAdjGaussCoords[iPointIndex];

		// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
		int i10kmValue = int(vRightBorderAdjGaussCoords[iPointIndex].dy / 1000.0);
		if (dScale == SCALE_50K
			&& iPointIndex != iBeginMarkIndex_Y_Right
			&& iPointIndex != iEndMarkIndex_Y_Right
			&& i10kmValue % 2 != 0)
		{
			continue;
		}

		QString qstr10km = tr("%1").arg(int(layoutRightBorderGaussPoint.dy / 1000.0));
		qstr10km = qstr10km.right(2);

		// 生成“十公里刻度值”点要素
		// 属性信息
		// 类型
		FieldNameAndValue_Imp fieldImp;
		fieldImp.strFieldName = "TYPE";
		fieldImp.strFieldValue = "adj_right_ten_km";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "FLAG";
		fieldImp.strFieldValue = "1";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "VALUE";
		fieldImp.strFieldValue = qstr10km.toLocal8Bit();
		vFieldNameAndValue.push_back(fieldImp);


		SE_DPoint dGaussPoint;
		// 布局坐标转高斯坐标
		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			vOuterRightBorderLayoutCoords[iPointIndex],
			dMapUnit2LayoutUnit,
			dGaussPoint);

		double dValues[2];
		dValues[0] = dGaussPoint.dx;
		dValues[1] = dGaussPoint.dy;


		// 高斯投影转地理坐标
		int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dValues);

		if (iRet != 1)
		{
			continue;
		}

		iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
		if (iRet != 0)
		{
			continue;
		}

		// 绘制百公里
		if (iPointIndex == iBeginMarkIndex_Y_Right ||
			iPointIndex == iEndMarkIndex_Y_Right ||
			qstr10km == "00")
		{
			int i100km = abs(int(layoutRightBorderGaussPoint.dy / 100000.0));
			// 千位补0
			QString qstr100km;
			if (i100km < 10 && i100km > 0)
			{
				qstr100km = tr("0%1").arg(int(layoutRightBorderGaussPoint.dy / 100000.0));
			}
			else if (i100km == 0)
			{
				qstr100km = tr("00");
			}
			else if (i100km >= 10)
			{
				qstr100km = tr("%1").arg(int(layoutRightBorderGaussPoint.dy / 100000.0));
			}

			vFieldNameAndValue.clear();

			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "adj_right_hundred_km";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = qstr100km.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			iRet = Set_Point(pLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}

		}

	}

#pragma endregion

#pragma region "绘制刻度线"



	/*-------------------------------------*/
	/*           绘制上边界刻度线          */
	/*-------------------------------------*/
	// 外图廓上边界刻度
	for (int iPointIndex = iBeginMarkIndex_X_Top; iPointIndex <= iEndMarkIndex_X_Top; iPointIndex++)
	{
		// 外图廓上边界整公里刻度点
		SE_DPoint layoutTopBorderPoint = vOuterTopBorderLayoutCoords[iPointIndex];

		// 起始布局点
		SE_DPoint dLayoutPointFrom;
		dLayoutPointFrom.dx = layoutTopBorderPoint.dx;
		dLayoutPointFrom.dy = layoutTopBorderPoint.dy - ADJ_KM_LINE_LENGTH;

		// 终止布局点
		SE_DPoint dLayoutPointTo;
		dLayoutPointTo.dx = layoutTopBorderPoint.dx;
		dLayoutPointTo.dy = layoutTopBorderPoint.dy;

		// 类型
		vector<FieldNameAndValue_Imp> vFieldNameAndValue;
		vFieldNameAndValue.clear();

		FieldNameAndValue_Imp fieldImp;
		fieldImp.strFieldName = "TYPE";
		fieldImp.strFieldValue = "adj_top_line";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "FLAG";
		fieldImp.strFieldValue = "1";
		vFieldNameAndValue.push_back(fieldImp);

		// 布局坐标转投影坐标
		SE_DPoint dFromGaussPoint;
		double dFromValue[2] = { 0 };

		SE_DPoint dToGaussPoint;
		double dToValue[2] = { 0 };

		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			dLayoutPointFrom,
			dMapUnit2LayoutUnit,
			dFromGaussPoint);

		dFromValue[0] = dFromGaussPoint.dx;
		dFromValue[1] = dFromGaussPoint.dy;

		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			dLayoutPointTo,
			dMapUnit2LayoutUnit,
			dToGaussPoint);

		dToValue[0] = dToGaussPoint.dx;
		dToValue[1] = dToGaussPoint.dy;

		// 高斯投影转地理坐标
		int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dFromValue);

		if (iRet != 1)
		{
			continue;
		}

		iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dToValue);

		if (iRet != 1)
		{
			continue;
		}

		SE_DPoint dFromGeoPoint;
		dFromGeoPoint.dx = dFromValue[0];
		dFromGeoPoint.dy = dFromValue[1];

		SE_DPoint dToGeoPoint;
		dToGeoPoint.dx = dToValue[0];
		dToGeoPoint.dy = dToValue[1];

		vector<SE_DPoint> vLine;
		vLine.clear();
		vLine.push_back(dFromGeoPoint);
		vLine.push_back(dToGeoPoint);
		iRet = Set_LineString(pLayoutLineLayer, vLine, vFieldNameAndValue);
		if (iRet != 0)
		{
			continue;
		}
	}

	/*-------------------------------------*/
	/*           绘制下边界刻度线          */
	/*-------------------------------------*/
	// 外图廓下边界刻度
	QgsLayoutItemPolyline* pAdjKmLine_Bottom = nullptr;
	for (int iPointIndex = iBeginMarkIndex_X_Bottom; iPointIndex <= iEndMarkIndex_X_Bottom; iPointIndex++)
	{
		// 外图廓下边界整公里刻度点
		SE_DPoint layoutBottomBorderPoint = vOuterBottomBorderLayoutCoords[iPointIndex];

		// 起始布局点
		SE_DPoint dLayoutPointFrom;
		dLayoutPointFrom.dx = layoutBottomBorderPoint.dx;
		dLayoutPointFrom.dy = layoutBottomBorderPoint.dy + ADJ_KM_LINE_LENGTH;

		// 终止布局点
		SE_DPoint dLayoutPointTo;
		dLayoutPointTo.dx = layoutBottomBorderPoint.dx;
		dLayoutPointTo.dy = layoutBottomBorderPoint.dy;

		// 类型
		vector<FieldNameAndValue_Imp> vFieldNameAndValue;
		vFieldNameAndValue.clear();

		FieldNameAndValue_Imp fieldImp;
		fieldImp.strFieldName = "TYPE";
		fieldImp.strFieldValue = "adj_bottom_line";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "FLAG";
		fieldImp.strFieldValue = "1";
		vFieldNameAndValue.push_back(fieldImp);

		// 布局坐标转投影坐标
		SE_DPoint dFromGaussPoint;
		double dFromValue[2] = { 0 };

		SE_DPoint dToGaussPoint;
		double dToValue[2] = { 0 };

		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			dLayoutPointFrom,
			dMapUnit2LayoutUnit,
			dFromGaussPoint);

		dFromValue[0] = dFromGaussPoint.dx;
		dFromValue[1] = dFromGaussPoint.dy;

		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			dLayoutPointTo,
			dMapUnit2LayoutUnit,
			dToGaussPoint);

		dToValue[0] = dToGaussPoint.dx;
		dToValue[1] = dToGaussPoint.dy;

		// 高斯投影转地理坐标
		int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dFromValue);

		if (iRet != 1)
		{
			continue;
		}

		iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dToValue);

		if (iRet != 1)
		{
			continue;
		}

		SE_DPoint dFromGeoPoint;
		dFromGeoPoint.dx = dFromValue[0];
		dFromGeoPoint.dy = dFromValue[1];

		SE_DPoint dToGeoPoint;
		dToGeoPoint.dx = dToValue[0];
		dToGeoPoint.dy = dToValue[1];

		vector<SE_DPoint> vLine;
		vLine.clear();
		vLine.push_back(dFromGeoPoint);
		vLine.push_back(dToGeoPoint);
		iRet = Set_LineString(pLayoutLineLayer, vLine, vFieldNameAndValue);
		if (iRet != 0)
		{
			continue;
		}
	}

	/*-------------------------------------*/
	/*           绘制左边界刻度线          */
	/*-------------------------------------*/
	// 外图廓左边界刻度
	QgsLayoutItemPolyline* pAdjKmLine_Left = nullptr;
	for (int iPointIndex = iBeginMarkIndex_Y_Left; iPointIndex <= iEndMarkIndex_Y_Left; iPointIndex++)
	{
		// 外图廓左边界整公里刻度点
		SE_DPoint layoutLeftBorderPoint = vOuterLeftBorderLayoutCoords[iPointIndex];

		// 起始布局点
		SE_DPoint dLayoutPointFrom;
		dLayoutPointFrom.dx = layoutLeftBorderPoint.dx - ADJ_KM_LINE_LENGTH;
		dLayoutPointFrom.dy = layoutLeftBorderPoint.dy;

		// 终止布局点
		SE_DPoint dLayoutPointTo;
		dLayoutPointTo.dx = layoutLeftBorderPoint.dx;
		dLayoutPointTo.dy = layoutLeftBorderPoint.dy;

		// 类型
		vector<FieldNameAndValue_Imp> vFieldNameAndValue;
		vFieldNameAndValue.clear();

		FieldNameAndValue_Imp fieldImp;
		fieldImp.strFieldName = "TYPE";
		fieldImp.strFieldValue = "adj_left_line";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "FLAG";
		fieldImp.strFieldValue = "1";
		vFieldNameAndValue.push_back(fieldImp);

		// 布局坐标转投影坐标
		SE_DPoint dFromGaussPoint;
		double dFromValue[2] = { 0 };

		SE_DPoint dToGaussPoint;
		double dToValue[2] = { 0 };

		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			dLayoutPointFrom,
			dMapUnit2LayoutUnit,
			dFromGaussPoint);

		dFromValue[0] = dFromGaussPoint.dx;
		dFromValue[1] = dFromGaussPoint.dy;

		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			dLayoutPointTo,
			dMapUnit2LayoutUnit,
			dToGaussPoint);

		dToValue[0] = dToGaussPoint.dx;
		dToValue[1] = dToGaussPoint.dy;

		// 高斯投影转地理坐标
		int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dFromValue);

		if (iRet != 1)
		{
			continue;
		}

		iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dToValue);

		if (iRet != 1)
		{
			continue;
		}

		SE_DPoint dFromGeoPoint;
		dFromGeoPoint.dx = dFromValue[0];
		dFromGeoPoint.dy = dFromValue[1];

		SE_DPoint dToGeoPoint;
		dToGeoPoint.dx = dToValue[0];
		dToGeoPoint.dy = dToValue[1];

		vector<SE_DPoint> vLine;
		vLine.clear();
		vLine.push_back(dFromGeoPoint);
		vLine.push_back(dToGeoPoint);
		iRet = Set_LineString(pLayoutLineLayer, vLine, vFieldNameAndValue);
		if (iRet != 0)
		{
			continue;
		}
	}

	/*-------------------------------------*/
	/*           绘制左边界刻度线          */
	/*-------------------------------------*/
	// 外图廓右边界刻度
	QgsLayoutItemPolyline* pAdjKmLine_Right = nullptr;
	for (int iPointIndex = iBeginMarkIndex_Y_Right; iPointIndex <= iEndMarkIndex_Y_Right; iPointIndex++)
	{
		// 外图廓右边界整公里刻度点
		SE_DPoint layoutRightBorderPoint = vOuterRightBorderLayoutCoords[iPointIndex];

		// 起始布局点
		SE_DPoint dLayoutPointFrom;
		dLayoutPointFrom.dx = layoutRightBorderPoint.dx + ADJ_KM_LINE_LENGTH;
		dLayoutPointFrom.dy = layoutRightBorderPoint.dy;

		// 终止布局点
		SE_DPoint dLayoutPointTo;
		dLayoutPointTo.dx = layoutRightBorderPoint.dx;
		dLayoutPointTo.dy = layoutRightBorderPoint.dy;

		// 类型
		vector<FieldNameAndValue_Imp> vFieldNameAndValue;
		vFieldNameAndValue.clear();

		FieldNameAndValue_Imp fieldImp;
		fieldImp.strFieldName = "TYPE";
		fieldImp.strFieldValue = "adj_right_line";
		vFieldNameAndValue.push_back(fieldImp);

		fieldImp.strFieldName = "FLAG";
		fieldImp.strFieldValue = "1";
		vFieldNameAndValue.push_back(fieldImp);

		// 布局坐标转投影坐标
		SE_DPoint dFromGaussPoint;
		double dFromValue[2] = { 0 };

		SE_DPoint dToGaussPoint;
		double dToValue[2] = { 0 };

		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			dLayoutPointFrom,
			dMapUnit2LayoutUnit,
			dFromGaussPoint);

		dFromValue[0] = dFromGaussPoint.dx;
		dFromValue[1] = dFromGaussPoint.dy;

		TransformLayoutToGauss(dRefGaussPoint,
			dRefLayoutPoint,
			dLayoutPointTo,
			dMapUnit2LayoutUnit,
			dToGaussPoint);

		dToValue[0] = dToGaussPoint.dx;
		dToValue[1] = dToGaussPoint.dy;

		// 高斯投影转地理坐标
		int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dFromValue);

		if (iRet != 1)
		{
			continue;
		}

		iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			1,
			dToValue);

		if (iRet != 1)
		{
			continue;
		}

		SE_DPoint dFromGeoPoint;
		dFromGeoPoint.dx = dFromValue[0];
		dFromGeoPoint.dy = dFromValue[1];

		SE_DPoint dToGeoPoint;
		dToGeoPoint.dx = dToValue[0];
		dToGeoPoint.dy = dToValue[1];

		vector<SE_DPoint> vLine;
		vLine.clear();
		vLine.push_back(dFromGeoPoint);
		vLine.push_back(dToGeoPoint);
		iRet = Set_LineString(pLayoutLineLayer, vLine, vFieldNameAndValue);
		if (iRet != 0)
		{
			continue;
		}
	}

#pragma endregion
}

void QgsImageMappingDialog_LayoutVectorization::GetRadioChecked(bool& bLonLatRange, bool& bSheetList)
{
	if (ui.radioButton_LonLatRange->isChecked())
	{
		bLonLatRange = true;
	}
	else
	{
		bLonLatRange = false;
	}

	if (ui.radioButton_SheetList->isChecked())
	{
		bSheetList = true;
	}
	else
	{
		bSheetList = false;
	}
}

void QgsImageMappingDialog_LayoutVectorization::GetSheetListFromPlainTextEdit(vector<QString>& qstrSheetList)
{
	qstrSheetList.clear();
	QTextDocument* doc = ui.plainTextEdit_SheetList->document();  // 文本对象
	int iBlockCount = doc->blockCount();    //回车符是一个block

	for (int i = 0; i < iBlockCount; i++)
	{
		QTextBlock textLine = doc->findBlockByNumber(i); // 文本中的一段
		QString str = textLine.text();
		qstrSheetList.push_back(str);
	}
}

bool QgsImageMappingDialog_LayoutVectorization::LoadLayoutItemPositionsConfig(double dScale, vector<LayoutItemPositions>& vLayoutItemPositions)
{
	// 配置文件路径
	QString qstrLayoutItemConfigPath = QCoreApplication::applicationDirPath();

	// 5万比例尺
	if (dScale == SCALE_50K)
	{
		qstrLayoutItemConfigPath += "/layout/position/layout_item_positions_5w.xml";
	}
	// 1万比例尺
	else if (dScale == SCALE_10K)
	{
		qstrLayoutItemConfigPath += "/layout/position/layout_item_positions_1w.xml";
	}

	vLayoutItemPositions.clear();

	/*读取xml配置文件*/
	QDomDocument doc;
	QFile file(qstrLayoutItemConfigPath);
	if (!file.open(QIODevice::ReadOnly))
	{
		return false;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return false;
	}
	file.close();

	// 读取根节点 LayoutItem_List
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   LayoutItem 节点
	QDomNode layoutItemNode = root.firstChild();
	int i = 0;
	int j = 0;
	int k = 0;

	while (!layoutItemNode.isNull())
	{
		// 循环遍历每个子节点
		LayoutItemPositions item;

		// 节点元素名称
		QString tagName = layoutItemNode.toElement().tagName();

		// 节点标记查找
		if (tagName == "LayoutItem")
		{
			// LayoutItem的子节点nodeList
			QDomNodeList layoutItemNodeList = layoutItemNode.childNodes();

			for (i = 0; i < layoutItemNodeList.size(); i++)
			{
				QDomNode layoutItemChildNode = layoutItemNodeList.at(i);
				QDomElement layoutItemChildElement = layoutItemChildNode.toElement();

				// 布局元素名称
				if (layoutItemChildElement.tagName() == "Name")
				{
					item.qstrName = layoutItemChildElement.text();
				}

				// 横坐标方向偏移值
				else if (layoutItemChildElement.tagName() == "Left_offset")
				{
					item.dLeft_offset = layoutItemChildElement.text().toDouble();
				}

				// 纵坐标方向偏移值
				else if (layoutItemChildElement.tagName() == "Top_offset")
				{
					item.dTop_offset = layoutItemChildElement.text().toDouble();
				}

				// 布局元素宽度
				else if (layoutItemChildElement.tagName() == "Width")
				{
					item.dWidth = layoutItemChildElement.text().toDouble();
				}

				// 布局元素高度
				else if (layoutItemChildElement.tagName() == "Height")
				{
					item.dHeight = layoutItemChildElement.text().toDouble();
				}

				// 布局元素参考点位置
				else if (layoutItemChildElement.tagName() == "ReferencePoint")
				{
					item.qstrReferencePoint = layoutItemChildElement.text();
				}
			}
		}

		vLayoutItemPositions.push_back(item);

		// 读取下一个Schema节点
		layoutItemNode = layoutItemNode.nextSibling();
	}

	return true;
}

void QgsImageMappingDialog_LayoutVectorization::GetLayoutItemPositionByName(const QString& qstrName,
	vector<LayoutItemPositions>& vItem,
	LayoutItemPositions& item)
{
	for (int i = 0; i < vItem.size(); i++)
	{
		if (qstrName == vItem[i].qstrName)
		{
			item = vItem[i];
			break;
		}
	}
}

QgsLayoutItem::ReferencePoint QgsImageMappingDialog_LayoutVectorization::GetReferencePoint(const QString& qstrReferencePoint)
{
	// 左上角
	if (qstrReferencePoint == "UpperLeft")
	{
		return QgsLayoutItem::UpperLeft;
	}

	// 上中
	else if (qstrReferencePoint == "UpperMiddle")
	{
		return QgsLayoutItem::UpperMiddle;
	}

	// 右上角
	else if (qstrReferencePoint == "UpperRight")
	{
		return QgsLayoutItem::UpperRight;
	}

	// 中左
	else if (qstrReferencePoint == "MiddleLeft")
	{
		return QgsLayoutItem::MiddleLeft;
	}

	// 正中
	else if (qstrReferencePoint == "Middle")
	{
		return QgsLayoutItem::Middle;
	}

	// 中右
	else if (qstrReferencePoint == "MiddleRight")
	{
		return QgsLayoutItem::MiddleRight;
	}

	// 左下角
	else if (qstrReferencePoint == "LowerLeft")
	{
		return QgsLayoutItem::LowerLeft;
	}

	// 中下
	else if (qstrReferencePoint == "LowerMiddle")
	{
		return QgsLayoutItem::LowerMiddle;
	}

	// 右下角
	else if (qstrReferencePoint == "LowerRight")
	{
		return QgsLayoutItem::LowerRight;
	}

	// 默认左上角
	return QgsLayoutItem::UpperLeft;
}

bool QgsImageMappingDialog_LayoutVectorization::ReadSMSFile(string strSMSPath, int iKey, string& strValue)
{
	FILE* fp = fopen(strSMSPath.c_str(), "r");
	if (!fp) {
		return false;
	}
	int itemp = 0;
	char temp1[500] = "";
	char temp2[500] = "";
	int i = 0;
	for (i = 1; i < iKey; i++)
	{
		fscanf(fp, "%d", &itemp);
		fscanf(fp, "%s", temp1);
		fscanf(fp, "%s", temp2);
	}
	fscanf(fp, "%d", &itemp);
	fscanf(fp, "%s", temp1);
	fscanf(fp, "%s", temp2);
	strValue = temp2;
	fclose(fp);
	return true;
}

void QgsImageMappingDialog_LayoutVectorization::TransformGaussToLayout(SE_DPoint dRefGaussPoint, SE_DPoint dRefLayoutPoint, SE_DPoint dGaussPoint, double dMapUnitToLayoutUnit, SE_DPoint& dLayoutPoint)
{
	dLayoutPoint.dx = dRefLayoutPoint.dx + (dGaussPoint.dx - dRefGaussPoint.dx) * dMapUnitToLayoutUnit;
	dLayoutPoint.dy = dRefLayoutPoint.dy + (dRefGaussPoint.dy - dGaussPoint.dy) * dMapUnitToLayoutUnit;
}


void QgsImageMappingDialog_LayoutVectorization::LoadSheetListFile()
{
	// 打开txt文件
	// 选择文件夹
	QString curPath = QCoreApplication::applicationDirPath();
	QString fileName = QFileDialog::getOpenFileName(this, tr("加载图幅列表文件"), curPath, "*.txt *.TXT");
	if (fileName.isEmpty()) // 如果文件未选择则返回
	{
		return;
	}

	string strFileName = fileName.toLocal8Bit();

	FILE* fp = fopen(strFileName.c_str(), "r");
	if (!fp)
	{
		// 记录日志
		QMessageBox msgBox1;
		msgBox1.setWindowTitle(tr("加载图幅列表文件"));
		msgBox1.setText(tr("读取图幅列表文件失败！"));
		msgBox1.setStandardButtons(QMessageBox::Yes);
		msgBox1.setDefaultButton(QMessageBox::Yes);
		msgBox1.exec();
		return;
	}

	vector<string> vSheetList;
	vSheetList.clear();

	while (!feof(fp))
	{
		char szLineBuffer[256] = "";
		fgets(szLineBuffer, sizeof(szLineBuffer) - 1, fp);

		// 去除"\r\n"字符，最后一行不需要去"\r\n"
		string strSheet = szLineBuffer;

		if (strstr(strSheet.c_str(), "\r") == NULL
			&& strstr(strSheet.c_str(), "\n") == NULL)
		{
			vSheetList.push_back(strSheet);
		}
		else
		{
			strSheet = strSheet.substr(0, strSheet.length() - 2);
			vSheetList.push_back(strSheet);
		}
	}
	fclose(fp);

	for (int i = 0; i < vSheetList.size(); i++)
	{
		QString qstrTemp = QString::fromLocal8Bit(vSheetList[i].c_str());
		ui.plainTextEdit_SheetList->appendPlainText(qstrTemp);
	}
}

void QgsImageMappingDialog_LayoutVectorization::LoadMappingSchema()
{
	// 配置文件默认从运行环境加载
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = tr("选择制图方案");
	QString filter = tr("xml 文件(*.xml)");
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	vector<ImageMappingSchema> vSchema;
	vSchema.clear();

	if (!LoadImageMappingSchemaXml(strFileName, vSchema))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("影像图制图"));
		msgBox.setText(tr("制图方案文件加载失败，请确认制图方案文件正确性！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	// 将制图方案添加到列表中
	for (int i = 0; i < vSchema.size(); ++i)
	{
		m_vImageMappingSchema.push_back(vSchema[i]);
	}

	// 先清空列表，然后重新添加到列表中
	ui.listWidget_LayoutTemplate->clear();

	// 布局模板列表框增加新增制图方案
	for (int i = 0; i < m_vImageMappingSchema.size(); i++)
	{
		ui.listWidget_LayoutTemplate->addItem(m_vImageMappingSchema[i].qstrLayoutTemplateName);
	}

	// 默认显示新增加的制图方案
	ui.listWidget_LayoutTemplate->setCurrentRow(0);

	QStandardItemModel* model = new QStandardItemModel(this);
	// 根据配置文件设置界面
	for (int i = 0; i < m_vImageMappingSchema.size(); i++)
	{
		ImageMappingSchema param = m_vImageMappingSchema[i];
		// 设置对话框中各部件的内容
		QStandardItem* item = new QStandardItem(param.qstrName);
		model->appendRow(item);
	}

	ui.listView_ImageMappingSchema->setModel(model);

	if (ui.listView_ImageMappingSchema->model()->rowCount() > 0)
	{
		//默认选中第一行
		QModelIndex qindex = ui.listView_ImageMappingSchema->model()->index(0, 0);
		ui.listView_ImageMappingSchema->setCurrentIndex(qindex);

		// 默认显示第一个制图方案
		SetUIByImageMappingSchema(m_vImageMappingSchema[0]);
		m_ImageMappingSchema = m_vImageMappingSchema[0];
	}


}

void QgsImageMappingDialog_LayoutVectorization::SaveMappingSchema()
{
	// 获取当前选择的制图方案
	int iCurrentIndex = ui.listView_ImageMappingSchema->selectionModel()->currentIndex().row();
	ImageMappingSchema schema = m_vImageMappingSchema[iCurrentIndex];

	// D、F、J、K、R
	vector<string> vLayerType;
	vLayerType.push_back("D");
	vLayerType.push_back("F");
	vLayerType.push_back("J");
	vLayerType.push_back("K");
	vLayerType.push_back("R");

	schema.vFeatureClass.clear();
	for (int i = 0; i < vLayerType.size(); ++i)
	{
		FeatureClassParam fcParam;
		vector<string> vFeatureClass;
		vFeatureClass.clear();
		GetLayerFeatureClassChecked(vLayerType[i], vFeatureClass);

		fcParam.qstrLayerName = QString::fromLocal8Bit(vLayerType[i].c_str());
		for (int j = 0; j < vFeatureClass.size(); ++j)
		{
			fcParam.vFeatureClassCode.push_back(QString::fromLocal8Bit(vFeatureClass[j].c_str()));
		}

		schema.vFeatureClass.push_back(fcParam);
	}


	// QDomDocument类
	QDomDocument doc;
	QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
	doc.appendChild(instruction);

	// 创建根节点  QDomElemet元素:Schema_List
	QDomElement root = doc.createElement("Schema_List");

	// 添加根节点
	doc.appendChild(root);

	// 创建Schema节点
	/*子节点
	<Name>
	<VectorDataPath>
	<ImageDataPath>
	<MosaicDataPath>
	<MappingRange>
	<FeatureLayers>
	<LayoutTemplatePath>
	<LayoutTemplateName>
	<LayoutItemParam>
	<MapExportType>
	<MapExportPath>
	*/
	QDomElement SchemaNode = doc.createElement("Schema");
	root.appendChild(SchemaNode);

	// 【1】创建Name节点
	QDomElement NameNode = doc.createElement("Name");
	SchemaNode.appendChild(NameNode);
	//QDomText strNameText = doc.createTextNode(schema.qstrName);
	//NameNode.appendChild(strNameText);

	// 【2】创建VectorDataPath节点
	QDomElement VectorDataPathNode = doc.createElement("VectorDataPath");
	SchemaNode.appendChild(VectorDataPathNode);
	QDomText strVectorDataPathText = doc.createTextNode(schema.qstrVectorDataPath);
	VectorDataPathNode.appendChild(strVectorDataPathText);

	// 【3】创建ImageDataPath节点
	QDomElement ImageDataPathNode = doc.createElement("ImageDataPath");
	SchemaNode.appendChild(ImageDataPathNode);
	QDomText strImageDataPathText = doc.createTextNode(schema.qstrImageDataPath);
	ImageDataPathNode.appendChild(strImageDataPathText);

	// 【4】创建MosaicDataPath节点
	QDomElement MosaicDataPathNode = doc.createElement("MosaicDataPath");
	SchemaNode.appendChild(MosaicDataPathNode);
	QDomText strMosaicDataPathText = doc.createTextNode(schema.qstrMosaicDataPath);
	MosaicDataPathNode.appendChild(strMosaicDataPathText);

	// 【5】创建MappingRange节点
	QDomElement MappingRangeNode = doc.createElement("MappingRange");

	// 制图范围使用属性节点
	MappingRangeNode.setAttribute(tr("Left"), schema.dLeft);
	MappingRangeNode.setAttribute(tr("Top"), schema.dTop);
	MappingRangeNode.setAttribute(tr("Right"), schema.dRight);
	MappingRangeNode.setAttribute(tr("Bottom"), schema.dBottom);
	SchemaNode.appendChild(MappingRangeNode);

	// 【6】创建FeatureLayers节点
	QDomElement FeatureLayersNode = doc.createElement("FeatureLayers");
	SchemaNode.appendChild(FeatureLayersNode);
	for (int i = 0; i < schema.vFeatureClass.size(); ++i)
	{
		// 生成Layer节点
		QDomElement LayerNode = doc.createElement("Layer");
		FeatureLayersNode.appendChild(LayerNode);

		// 生成LayerName节点
		QDomElement LayerNameNode = doc.createElement("LayerName");
		LayerNode.appendChild(LayerNameNode);
		QDomText strLayerNameText = doc.createTextNode(schema.vFeatureClass[i].qstrLayerName);
		LayerNameNode.appendChild(strLayerNameText);

		// 生成FeatureClassCodes节点
		QDomElement FeatureClassCodesNode = doc.createElement("FeatureClassCodes");
		LayerNode.appendChild(FeatureClassCodesNode);
		for (int j = 0; j < schema.vFeatureClass[i].vFeatureClassCode.size(); ++j)
		{
			// 生成FeatureClassCode节点
			QDomElement FeatureClassCodeNode = doc.createElement("FeatureClassCode");
			FeatureClassCodesNode.appendChild(FeatureClassCodeNode);
			QDomText strFeatureClassCodeText = doc.createTextNode(schema.vFeatureClass[i].vFeatureClassCode[j]);
			FeatureClassCodeNode.appendChild(strFeatureClassCodeText);
		}
	}

	// 【7】创建LayoutTemplatePath节点
	QDomElement LayoutTemplatePathNode = doc.createElement("LayoutTemplatePath");
	SchemaNode.appendChild(LayoutTemplatePathNode);
	QDomText strLayoutTemplatePathText = doc.createTextNode(schema.qstrLayoutTemplatePath);
	LayoutTemplatePathNode.appendChild(strLayoutTemplatePathText);

	// 【8】创建LayoutTemplateName节点
	QDomElement LayoutTemplateNameNode = doc.createElement("LayoutTemplateName");
	SchemaNode.appendChild(LayoutTemplateNameNode);
	QDomText strLayoutTemplateNameText = doc.createTextNode(schema.qstrLayoutTemplateName);
	LayoutTemplateNameNode.appendChild(strLayoutTemplateNameText);

	// 【9】创建LayoutItemParam节点
	QDomElement LayoutItemParamNode = doc.createElement("LayoutItemParam");
	SchemaNode.appendChild(LayoutItemParamNode);

	// 创建LayoutItemParam子节点：
	/*<MapTitleName>
	<MapSubTitleProvince>
	<MapSubTitleCounty>
	<MapSubTitleChecked>
	<SecurityClassification>
	------------------------------
	<MapDetails>
	<MapDetailsChecked>
	<MapAgency>
	<MapAgencyChecked>
	<LeftTopCornerSheet>
	------------------------------
	<LeftTopCornerSheetChecked>
	<RightBottomCornerSheet>
	<RightBottomCornerSheetChecked>
	<Scale>
	<ScaleChecked>
	------------------------------
	<KmNetChecked>
	<SlopeRulerChecked>
	<NorthDirectionLinesChecked>
	<SheetListChecked>
	<MosaicMapChecked>
	------------------------------
	<MainMapLegendChecked>
	<MosaicMapLegendChecked>
	<MapAnnotations>
	------------------------------
	<AnnotationsChecked>

	*/

	// 【10】MapTitleName
	QDomElement MapTitleNameNode = doc.createElement("MapTitleName");
	LayoutItemParamNode.appendChild(MapTitleNameNode);
	QDomText strMapTitleNameText = doc.createTextNode(schema.layoutItemParam.qstrMapTitleName);
	MapTitleNameNode.appendChild(strMapTitleNameText);

	// 【11】MapSubTitleProvince
	QDomElement MapSubTitleProvinceNode = doc.createElement("MapSubTitleProvince");
	LayoutItemParamNode.appendChild(MapSubTitleProvinceNode);
	QDomText strMapSubTitleProvinceText = doc.createTextNode(schema.layoutItemParam.qstrMapSubTitleProvince);
	MapSubTitleProvinceNode.appendChild(strMapSubTitleProvinceText);

	// 【12】MapSubTitleCounty
	QDomElement MapSubTitleCountyNode = doc.createElement("MapSubTitleCounty");
	LayoutItemParamNode.appendChild(MapSubTitleCountyNode);
	QDomText strMapSubTitleCountyText = doc.createTextNode(schema.layoutItemParam.qstrMapSubTitleCounty);
	MapSubTitleCountyNode.appendChild(strMapSubTitleCountyText);

	// 【13】MapSubTitleChecked
	QDomElement MapSubTitleCheckedNode = doc.createElement("MapSubTitleChecked");
	LayoutItemParamNode.appendChild(MapSubTitleCheckedNode);

	if (schema.layoutItemParam.bMapSubTitleChecked)
	{
		QDomText strMapSubTitleCheckedText = doc.createTextNode("true");
		MapSubTitleCheckedNode.appendChild(strMapSubTitleCheckedText);
	}
	else
	{
		QDomText strMapSubTitleCheckedText = doc.createTextNode("false");
		MapSubTitleCheckedNode.appendChild(strMapSubTitleCheckedText);
	}

	// 【14】SecurityClassification
	QDomElement SecurityClassificationNode = doc.createElement("SecurityClassification");
	LayoutItemParamNode.appendChild(SecurityClassificationNode);
	QDomText strSecurityClassificationText = doc.createTextNode(schema.layoutItemParam.qstrSecurityClassification);
	SecurityClassificationNode.appendChild(strSecurityClassificationText);

	// 【15】MapDetails
	QDomElement MapDetailsNode = doc.createElement("MapDetails");
	LayoutItemParamNode.appendChild(MapDetailsNode);
	QDomText strMapDetailsText = doc.createTextNode(schema.layoutItemParam.qstrMapDetails);
	MapDetailsNode.appendChild(strMapDetailsText);

	// 【16】MapDetailsChecked
	QDomElement MapDetailsCheckedNode = doc.createElement("MapDetailsChecked");
	LayoutItemParamNode.appendChild(MapDetailsCheckedNode);
	if (schema.layoutItemParam.bMapDetailsChecked)
	{
		QDomText strMapDetailsCheckedText = doc.createTextNode("true");
		MapDetailsCheckedNode.appendChild(strMapDetailsCheckedText);
	}
	else
	{
		QDomText strMapDetailsCheckedText = doc.createTextNode("false");
		MapDetailsCheckedNode.appendChild(strMapDetailsCheckedText);
	}

	// 【17】MapAgency
	QDomElement MapAgencyNode = doc.createElement("MapAgency");
	LayoutItemParamNode.appendChild(MapAgencyNode);
	QDomText strMapAgencyText = doc.createTextNode(schema.layoutItemParam.qstrMapAgency);
	MapAgencyNode.appendChild(strMapAgencyText);

	// 【18】MapAgencyChecked
	QDomElement MapAgencyCheckedNode = doc.createElement("MapAgencyChecked");
	LayoutItemParamNode.appendChild(MapAgencyCheckedNode);
	if (schema.layoutItemParam.bMapAgencyChecked)
	{
		QDomText strMapAgencyCheckedText = doc.createTextNode("true");
		MapAgencyCheckedNode.appendChild(strMapAgencyCheckedText);
	}
	else
	{
		QDomText strMapAgencyCheckedText = doc.createTextNode("false");
		MapAgencyCheckedNode.appendChild(strMapAgencyCheckedText);
	}

	// 【19】LeftTopCornerSheet
	QDomElement LeftTopCornerSheetNode = doc.createElement("LeftTopCornerSheet");
	LayoutItemParamNode.appendChild(LeftTopCornerSheetNode);
	QDomText strLeftTopCornerSheetText = doc.createTextNode(schema.layoutItemParam.qstrLeftTopCornerSheet);
	LeftTopCornerSheetNode.appendChild(strLeftTopCornerSheetText);

	// 【20】LeftTopCornerSheetChecked
	QDomElement LeftTopCornerSheetCheckedNode = doc.createElement("LeftTopCornerSheetChecked");
	LayoutItemParamNode.appendChild(LeftTopCornerSheetCheckedNode);
	if (schema.layoutItemParam.bLeftTopCornerSheetChecked)
	{
		QDomText strLeftTopCornerSheetCheckedText = doc.createTextNode("true");
		LeftTopCornerSheetCheckedNode.appendChild(strLeftTopCornerSheetCheckedText);
	}
	else
	{
		QDomText strLeftTopCornerSheetCheckedText = doc.createTextNode("false");
		LeftTopCornerSheetCheckedNode.appendChild(strLeftTopCornerSheetCheckedText);
	}

	// 【21】RightBottomCornerSheet
	QDomElement RightBottomCornerSheetNode = doc.createElement("RightBottomCornerSheet");
	LayoutItemParamNode.appendChild(RightBottomCornerSheetNode);
	QDomText strRightBottomCornerSheetText = doc.createTextNode(schema.layoutItemParam.qstrRightBottomCornerSheet);
	RightBottomCornerSheetNode.appendChild(strRightBottomCornerSheetText);

	// 【22】RightBottomCornerSheetChecked
	QDomElement RightBottomCornerSheetCheckedNode = doc.createElement("RightBottomCornerSheetChecked");
	LayoutItemParamNode.appendChild(RightBottomCornerSheetCheckedNode);
	if (schema.layoutItemParam.bRightBottomCornerSheetChecked)
	{
		QDomText strRightBottomCornerSheetCheckedText = doc.createTextNode("true");
		RightBottomCornerSheetCheckedNode.appendChild(strRightBottomCornerSheetCheckedText);
	}
	else
	{
		QDomText strRightBottomCornerSheetCheckedText = doc.createTextNode("false");
		RightBottomCornerSheetCheckedNode.appendChild(strRightBottomCornerSheetCheckedText);
	}

	// 【23】Scale
	QDomElement ScaleNode = doc.createElement("Scale");
	LayoutItemParamNode.appendChild(ScaleNode);
	QDomText strScaleText = doc.createTextNode(schema.layoutItemParam.qstrScale);
	ScaleNode.appendChild(strScaleText);

	// 【24】ScaleChecked
	QDomElement ScaleCheckedNode = doc.createElement("ScaleChecked");
	LayoutItemParamNode.appendChild(ScaleCheckedNode);
	if (schema.layoutItemParam.bScaleChecked)
	{
		QDomText strScaleCheckedText = doc.createTextNode("true");
		ScaleCheckedNode.appendChild(strScaleCheckedText);
	}
	else
	{
		QDomText strScaleCheckedText = doc.createTextNode("false");
		ScaleCheckedNode.appendChild(strScaleCheckedText);
	}

	// 【25】KmNetChecked
	QDomElement KmNetCheckedNode = doc.createElement("KmNetChecked");
	LayoutItemParamNode.appendChild(KmNetCheckedNode);
	if (schema.layoutItemParam.bKmNetChecked)
	{
		QDomText strKmNetCheckedText = doc.createTextNode("true");
		KmNetCheckedNode.appendChild(strKmNetCheckedText);
	}
	else
	{
		QDomText strKmNetCheckedText = doc.createTextNode("false");
		KmNetCheckedNode.appendChild(strKmNetCheckedText);
	}

	// 【26】SlopeRulerChecked
	QDomElement SlopeRulerCheckedNode = doc.createElement("SlopeRulerChecked");
	LayoutItemParamNode.appendChild(SlopeRulerCheckedNode);
	if (schema.layoutItemParam.bSlopeRulerChecked)
	{
		QDomText strSlopeRulerCheckedText = doc.createTextNode("true");
		SlopeRulerCheckedNode.appendChild(strSlopeRulerCheckedText);
	}
	else
	{
		QDomText strSlopeRulerCheckedText = doc.createTextNode("false");
		SlopeRulerCheckedNode.appendChild(strSlopeRulerCheckedText);
	}

	// 【27】NorthDirectionLinesChecked
	QDomElement NorthDirectionLinesCheckedNode = doc.createElement("NorthDirectionLinesChecked");
	LayoutItemParamNode.appendChild(NorthDirectionLinesCheckedNode);
	if (schema.layoutItemParam.bNorthDirectionLinesChecked)
	{
		QDomText strNorthDirectionLinesCheckedText = doc.createTextNode("true");
		NorthDirectionLinesCheckedNode.appendChild(strNorthDirectionLinesCheckedText);
	}
	else
	{
		QDomText strNorthDirectionLinesCheckedText = doc.createTextNode("false");
		NorthDirectionLinesCheckedNode.appendChild(strNorthDirectionLinesCheckedText);
	}

	// 【28】SheetListChecked
	QDomElement SheetListCheckedNode = doc.createElement("SheetListChecked");
	LayoutItemParamNode.appendChild(SheetListCheckedNode);
	if (schema.layoutItemParam.bSheetListChecked)
	{
		QDomText strSheetListCheckedText = doc.createTextNode("true");
		SheetListCheckedNode.appendChild(strSheetListCheckedText);
	}
	else
	{
		QDomText strSheetListCheckedText = doc.createTextNode("false");
		SheetListCheckedNode.appendChild(strSheetListCheckedText);
	}

	// 【29】MosaicMapChecked
	QDomElement MosaicMapCheckedNode = doc.createElement("MosaicMapChecked");
	LayoutItemParamNode.appendChild(MosaicMapCheckedNode);
	if (schema.layoutItemParam.bMosaicMapChecked)
	{
		QDomText strMosaicMapCheckedText = doc.createTextNode("true");
		MosaicMapCheckedNode.appendChild(strMosaicMapCheckedText);
	}
	else
	{
		QDomText strMosaicMapCheckedText = doc.createTextNode("false");
		MosaicMapCheckedNode.appendChild(strMosaicMapCheckedText);
	}

	// 【30】MainMapLegendChecked
	QDomElement MainMapLegendCheckedNode = doc.createElement("MainMapLegendChecked");
	LayoutItemParamNode.appendChild(MainMapLegendCheckedNode);
	if (schema.layoutItemParam.bMainMapLegendChecked)
	{
		QDomText strMainMapLegendCheckedText = doc.createTextNode("true");
		MainMapLegendCheckedNode.appendChild(strMainMapLegendCheckedText);
	}
	else
	{
		QDomText strMainMapLegendCheckedText = doc.createTextNode("false");
		MainMapLegendCheckedNode.appendChild(strMainMapLegendCheckedText);
	}

	// 【31】MosaicMapLegendChecked
	QDomElement MosaicMapLegendCheckedNode = doc.createElement("MosaicMapLegendChecked");
	LayoutItemParamNode.appendChild(MosaicMapLegendCheckedNode);
	if (schema.layoutItemParam.bMosaicMapLegendChecked)
	{
		QDomText strMosaicMapLegendCheckedText = doc.createTextNode("true");
		MosaicMapLegendCheckedNode.appendChild(strMosaicMapLegendCheckedText);
	}
	else
	{
		QDomText strMosaicMapLegendCheckedText = doc.createTextNode("false");
		MosaicMapLegendCheckedNode.appendChild(strMosaicMapLegendCheckedText);
	}

	// 【32】MapAnnotations
	QDomElement MapAnnotationsNode = doc.createElement("MapAnnotations");
	LayoutItemParamNode.appendChild(MapAnnotationsNode);
	QDomText strMapAnnotationsText = doc.createTextNode(schema.layoutItemParam.qstrMapAnnotations);
	MapAnnotationsNode.appendChild(strMapAnnotationsText);

	// 【33】AnnotationsChecked
	QDomElement AnnotationsCheckedNode = doc.createElement("AnnotationsChecked");
	LayoutItemParamNode.appendChild(AnnotationsCheckedNode);
	if (schema.layoutItemParam.bAnnotationsChecked)
	{
		QDomText strAnnotationsCheckedText = doc.createTextNode("true");
		AnnotationsCheckedNode.appendChild(strAnnotationsCheckedText);
	}
	else
	{
		QDomText strAnnotationsCheckedText = doc.createTextNode("false");
		AnnotationsCheckedNode.appendChild(strAnnotationsCheckedText);
	}

	// 【34】创建MapExportType节点
	QDomElement MapExportTypeNode = doc.createElement("MapExportType");
	SchemaNode.appendChild(MapExportTypeNode);
	QDomText strMapExportTypeText = doc.createTextNode(schema.qstrMapExportType);
	MapExportTypeNode.appendChild(strMapExportTypeText);


	// 【35】创建MapExportPath节点
	QDomElement MapExportPathNode = doc.createElement("MapExportPath");
	SchemaNode.appendChild(MapExportPathNode);
	QDomText strMapExportPathText = doc.createTextNode(schema.qstrMapExportPath);
	MapExportPathNode.appendChild(strMapExportPathText);

	// 配置文件默认保存在当前运行环境下
	QString curPath = m_qstrImageMappingSchemaSavePath;
	QString dlgTitle = tr("保存配置文件");
	QString filter = tr("xml 文件(*.xml)");
	QString strFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!strFileName.isEmpty())
	{
		ui.lineEdit_OutputMappingSchemaPath->setText(strFileName);
		m_qstrImageMappingSchemaSavePath = strFileName;
		string strBaseName = CPLGetBasename(strFileName.toLocal8Bit());

		QFile file(strFileName);
		QDomText strNameText = doc.createTextNode(QString::fromLocal8Bit(strBaseName.c_str()));
		NameNode.appendChild(strNameText);

		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
			return;
		QTextStream out(&file);
		out.setCodec("UTF-8");
		doc.save(out, 4, QDomNode::EncodingFromTextStream);
		file.close();

	}
}


/*根据两点确定的直线计算点坐标Y*/
void QgsImageMappingDialog_LayoutVectorization::CalCoordYByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputX, double& dOutputY)
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
void QgsImageMappingDialog_LayoutVectorization::CalCoordXByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputY, double& dOutputX)
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

void QgsImageMappingDialog_LayoutVectorization::CalGaussBorderCoords_A_B(SE_DPoint dBeginPoint,
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


void QgsImageMappingDialog_LayoutVectorization::CalGaussBorderCoords_C_B(SE_DPoint dBeginPoint,
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


void QgsImageMappingDialog_LayoutVectorization::CalGaussBorderCoords_D_C(SE_DPoint dBeginPoint,
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


void QgsImageMappingDialog_LayoutVectorization::CalGaussBorderCoords_D_A(SE_DPoint dBeginPoint,
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



int QgsImageMappingDialog_LayoutVectorization::ClockWise(vector<SE_DPoint>& vPoints)
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


SE_DPoint QgsImageMappingDialog_LayoutVectorization::LineIntersect(SE_DPoint p0,SE_DPoint p1,SE_DPoint p2,SE_DPoint p3)
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

int QgsImageMappingDialog_LayoutVectorization::CalIndexInVector_XCoord(double dx, vector<SE_DPoint>& vPoints)
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

int QgsImageMappingDialog_LayoutVectorization::CalIndexInVector_YCoord(double dy, vector<SE_DPoint>& vPoints)
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

void QgsImageMappingDialog_LayoutVectorization::TransformLayoutToGauss(SE_DPoint dRefGaussPoint,
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

void QgsImageMappingDialog_LayoutVectorization::CalEndPointByStartPointAndAngle(
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

void QgsImageMappingDialog_LayoutVectorization::ConvertMilFromIntToString(int iMil, QString& qstrMil)
{
	// 如果密位数为个位数
	if (iMil < 10)
	{
		qstrMil = tr("（0-0%1）").arg(iMil);
	}
	// 如果密位为10到99
	else if (iMil < 100 && iMil >= 10)
	{
		qstrMil = tr("（0-%1）").arg(iMil);
	}
	// 如果密位为100到999
	else if (iMil >= 100 && iMil <= 999)
	{
		QString qstrTemp = tr("%1").arg(iMil);
		// "-"前半部分数字
		QString qstrFirst = qstrTemp.left(1);

		// "-"后半部分数字
		QString qstrSecond = qstrTemp.right(2);

		qstrMil = tr("（0%1-%2）").arg(qstrFirst).arg(qstrSecond);
	}
	// 如果密位大于等于1000
	else
	{
		QString qstrTemp = tr("%1").arg(iMil);
		// "-"前半部分数字
		QString qstrFirst = qstrTemp.left(2);

		// "-"后半部分数字
		QString qstrSecond = qstrTemp.right(2);

		qstrMil = tr("（%1-%2）").arg(qstrFirst).arg(qstrSecond);
	}
}




QList<QgsStringMap> QgsImageMappingDialog_LayoutVectorization::GetAttributeValue(QgsVectorLayer* layer)
{
	QgsStringMap featureValue;
	QList<QgsStringMap> featureValues;
	int i = 0, nFeatureCount = 0, nFieldsCount = 0;
	if (layer == NULL)
	{
		return featureValues;
	}
	// 图层要素个数
	layer->selectAll();
	QgsFeatureList featurelist = layer->selectedFeatures();
	nFeatureCount = featurelist.size();
	QgsFeature feature;
	QString fieldName, fieldValue;

	// 图层字段信息
	QgsFields qgsFields = layer->fields();

	// 字段个数
	nFieldsCount = qgsFields.count();
	for (i = 0; i < nFeatureCount; i++)
	{
		feature = featurelist.at(i);

		// 获取当前要素的属性集合
		QgsAttributes qgsAttrs = feature.attributes();

		QgsField field;
		// 获取每个字段的属性值
		for (int j = 0; j < nFieldsCount; j++)
		{
			field = qgsFields[j];
			fieldName = field.name();

			fieldValue = qgsAttrs[j].toString();
			featureValue.insert(fieldName, fieldValue);
		}
		featureValues.push_back(featureValue);
	}
	return featureValues;
}

void QgsImageMappingDialog_LayoutVectorization::GetAcqDataAndSensorByCateName(QList<QgsStringMap>& qAttributeList,
	const QString& qstrCateName,
	const QString& qstrCateField,
	const QString& qstrAcaDataField,
	const QString& qstrSensorField,
	QString& qstrAcqDataValue,
	QString& qstrSensorValue)
{
	for (int i = 0; i < qAttributeList.size(); i++)
	{
		QgsStringMap qAttrMap = qAttributeList.at(i);

		// 类别属性值
		QString qstrCateValue = qAttrMap.value(qstrCateField);

		if (qstrCateValue == qstrCateName)
		{
			// 摄影时间
			qstrAcqDataValue = qAttrMap.value(qstrAcaDataField);

			// 传感器
			qstrSensorValue = qAttrMap.value(qstrSensorField);

			break;
		}
	}
}


// 判断目录是否存在
bool QgsImageMappingDialog_LayoutVectorization::FilePathIsExisted(QString qstrPath)
{
	QDir dir(qstrPath);
	return dir.exists();
}


// 判断文件是否存在
bool QgsImageMappingDialog_LayoutVectorization::FileIsExisted(QString qstrFilePath)
{
	return QFile::exists(qstrFilePath);
}


void QgsImageMappingDialog_LayoutVectorization::InitLineEdit()
{
	// 上边界纬度
	ui.lineEdit_Top->setValidator(new QRegExpValidator(QRegExp("^-?(90|[1-8]?\\d(\\.\\d+)?)$")));

	// 下边界纬度
	ui.lineEdit_Bottom->setValidator(new QRegExpValidator(QRegExp("^-?(90|[1-8]?\\d(\\.\\d+)?)$")));

	// 左边界经度
	ui.lineEdit_Left->setValidator(new QRegExpValidator(QRegExp("^-?(180|1?[0-7]?\\d(\\.\\d+)?)$")));

	// 右边界经度
	ui.lineEdit_Right->setValidator(new QRegExpValidator(QRegExp("^-?(180|1?[0-7]?\\d(\\.\\d+)?)$")));

	// 类别编号字段
	ui.lineEdit_MosaicDataRenderField->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

	// 拍摄时间字段
	ui.lineEdit_MosaicDataAcqDataField->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

	// 传感器字段
	ui.lineEdit_MosaicDataSensorField->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

	// 制图单位
	ui.lineEdit_MapAgency->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

	// 比例尺
	ui.lineEdit_Scale->setValidator(new QRegExpValidator(QRegExp("^[\u4e00-\u9fa5a-zA-Z0-9_]+$")));

}


bool QgsImageMappingDialog_LayoutVectorization::TranslateFromTif2Jpg(QString qstrFilePath, QString qstrSheetNumber)
{
	// 使用GDALTranslate执行栅格格式转换
	//添加命令参数,每次添加一个!!!
	char** argv = nullptr;

	// 像素类型
	argv = CSLAddString(argv, "-ot");
	argv = CSLAddString(argv, "Byte");

	// 输出格式
	argv = CSLAddString(argv, "-of");
	argv = CSLAddString(argv, "JPEG");

	string strFilePath = qstrFilePath.toLocal8Bit();
	string strSheetNumber = qstrSheetNumber.toLocal8Bit();

	// 输入tif文件全路径
	string strRasterFullName = strFilePath + "/" + strSheetNumber + ".tif";

	// 输出jpg文件全路径
	string strOutputFullName = strFilePath + "/" + strSheetNumber + ".jpg";

	// 输入数据路径
	argv = CSLAddString(argv, strRasterFullName.c_str());

	// 输出数据路径
	argv = CSLAddString(argv, strOutputFullName.c_str());

	// 返回
	int bUsageError = FALSE;
	// 输入列表
	GDALDatasetH pSrcDataSet = GDALOpen(strRasterFullName.c_str(), GA_ReadOnly);
	if (!pSrcDataSet)
	{
		return false;
	}

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
	}

	GDALTranslateOptionsFree(opt);
	CSLDestroy(argv);
	return true;
}


void QgsImageMappingDialog_LayoutVectorization::GetFeatureClassesByLayerType(
	vector<FeatureClassParam> &vParams,
	QString qstrLayerType,
	vector<QString> &vCodes)
{
	vCodes.clear();

	for (int i = 0; i < vParams.size(); ++i)
	{
		if (qstrLayerType == vParams[i].qstrLayerName)
		{
			vCodes = vParams[i].vFeatureClassCode;
			break;
		}
	}
}



// 影像图制图-图廓内要素矢量化（方里网、方里网刻度值等）
void QgsImageMappingDialog_LayoutVectorization::ImageMapping_LayoutVectorization()
{
	// 修改内容：影像图制图-图廓内要素矢量化（方里网、方里网刻度值等）
	// 修改时间：2023-07-25

	/*判断数据路径的有效性*/
	//-------------------------------------//
	if (!FilePathIsExisted(ui.lineEdit_VectorDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("影像图制图"));
		msgBox.setText(tr("矢量数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FilePathIsExisted(ui.lineEdit_ImageDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("影像图制图"));
		msgBox.setText(tr("影像数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FilePathIsExisted(ui.lineEdit_MosaicDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("影像图制图"));
		msgBox.setText(tr("镶嵌线数据目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	if (!FilePathIsExisted(ui.lineEdit_OutputDataPath->text()))
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("影像图制图"));
		msgBox.setText(tr("影像图输出目录不存在，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}

	//-----------------------------------------//


#pragma region "设置绘制线样式"

	// 时间：2023-02-23
	// 修改内容：修改方里网颜色及线宽
	// 方里网刻度线的样式，方里网线改为黑色
	QVariantMap kmLineBorderMap;
	kmLineBorderMap["line_color"] = "0,0,0,255";
	kmLineBorderMap["line_style"] = "solid";
	kmLineBorderMap["line_width"] = "0.01";
	QgsLineSymbol* kmLineBorderLineSymbol = QgsLineSymbol::createSimple(kmLineBorderMap);

	// 邻带方里网刻度线的样式
	QVariantMap adjKmLineBorderMap;
	adjKmLineBorderMap["line_color"] = "0,0,0,255";
	adjKmLineBorderMap["line_style"] = "solid";
	adjKmLineBorderMap["line_width"] = "0.1";
	QgsLineSymbol* adjKmLineBorderLineSymbol = QgsLineSymbol::createSimple(adjKmLineBorderMap);


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
	exteriorMapLineMap["line_width"] = "0.5";		// 外图廓线宽0.5mm
	QgsLineSymbol* exteriorMapLineSymbol = QgsLineSymbol::createSimple(exteriorMapLineMap);

#pragma endregion


	ui.progressBar->setValue(0);

	/*以地图外图左上角为参考点，获取基于地图外图廓的所有布局要素的相对参考点位置，
	用于自动调整影像图各元素位置*/

	/*影像制图流程
	（1）根据制图方案、制图范围及制图比例尺自动计算制图范围内覆盖的图幅列表，
		 从矢量数据目录、影像数据目录、镶嵌线数据目录中自动查找对应图幅的矢量、影像、镶嵌线数据
	（2）从矢量数据元数据文件SMS中读取图名，在【整饰要素】-【主标题】中显示图名，
		 副标题需要界面设置
	（3）根据界面设置的整饰要素显示状态，设置布局元素item的显示状态
	（4）每个图幅输出工程文件、元数据文件、地图文件（pdf格式或tiff格式）
	*/

	int i = 0;
	int j = 0;
#pragma region "根据界面设置制图方案"
	/*----------------------------------------*/
	/*【1】根据制图方案、制图范围及制图比例尺
		   自动计算制图范围内覆盖的图幅列表，从矢量数据目录、影像数据目录、
		   镶嵌线数据目录中自动查找对应图幅的矢量、影像、镶嵌线数据*/
	/*----------------------------------------*/

	/*----------获取数据源--------*/
	// 矢量数据路径
	m_ImageMappingSchema.qstrVectorDataPath = GetVectorDataPath();

	// 影像数据路径
	m_ImageMappingSchema.qstrImageDataPath = GetImageDataPath();

	// 镶嵌线数据路径
	m_ImageMappingSchema.qstrMosaicDataPath = GetMosaicDataPath();

	/*---------获取输出设置---------*/
	// 获取输出路径
	m_ImageMappingSchema.qstrMapExportPath = GetDataSavePath();
	m_qstrImageMappingDataSavePath = GetDataSavePath();

	// 获取输出地图类型
	m_ImageMappingSchema.qstrMapExportType = GetMapExportType();

	/*--------获取制图范围-------*/

	// 获取制图范围覆盖的图幅列表
	// 图幅列表
	vector<QString> vMapSheetList;
	vMapSheetList.clear();

	// 经纬度范围选择标志
	bool bLonLatChecked = false;

	// 图幅列表选择标志
	bool bSheetList = false;

	/*获取制图范围选择标志*/
	GetRadioChecked(bLonLatChecked, bSheetList);

	/*如果选择输入经纬度范围*/
	if (bLonLatChecked)
	{
		// 获取制图范围
		GetImageMappingRange(m_ImageMappingSchema.dLeft,
			m_ImageMappingSchema.dTop,
			m_ImageMappingSchema.dRight,
			m_ImageMappingSchema.dBottom);

		GetSheetListByMappingRange(m_ImageMappingSchema.qstrLayoutTemplateName,
			m_ImageMappingSchema.dLeft,
			m_ImageMappingSchema.dTop,
			m_ImageMappingSchema.dRight,
			m_ImageMappingSchema.dBottom,
			vMapSheetList);
	}
	/*如果选择图幅列表*/
	else if (bSheetList)
	{
		GetSheetListFromPlainTextEdit(vMapSheetList);
	}



	/*--------获取要素选择状态-------*/
	// 获取要素选择状态
	m_ImageMappingSchema.vFeatureClass.clear();
	for (i = 0; i < m_vStrLayerName.size(); i++)
	{
		FeatureClassParam param;
		GetListWidgetStatus(m_vStrLayerName[i], param);
		m_ImageMappingSchema.vFeatureClass.push_back(param);
	}

	/*---------获取布局模板---------*/
	// 布局模板名称
	m_ImageMappingSchema.qstrLayoutTemplateName = GetLayoutTemplateName();

	// 布局模板全路径
	m_ImageMappingSchema.qstrLayoutTemplatePath = GetLayoutTemplateFullPath();

	/*---------获取整饰要素---------*/
	// 主标题名称和主标题图幅名自动计算，从SMS文件中获取


	// 密级
	m_ImageMappingSchema.layoutItemParam.qstrSecurityClassification = GetSecurityClassification();

	// 地图说明
	m_ImageMappingSchema.layoutItemParam.qstrMapDetails = GetMapDetail();

	// 制图单位
	m_ImageMappingSchema.layoutItemParam.qstrMapAgency = GetMapAgency();

	// 左上角图幅名称和右下角图幅名称自动获取

	// 比例尺绘制状态
	m_ImageMappingSchema.layoutItemParam.bScaleChecked = ui.checkBox_Scale->isChecked();

	// 比例尺
	m_ImageMappingSchema.layoutItemParam.qstrScale = GetScale();

	// 方里网
	m_ImageMappingSchema.layoutItemParam.bKmNetChecked = ui.checkBox_KmNet->isChecked();

	// 坡度尺
	//m_ImageMappingSchema.layoutItemParam.bSlopeRulerChecked = ui.checkBox_SlopeRuler->isChecked();

	// 是否绘制三北方向线
	m_ImageMappingSchema.layoutItemParam.bNorthDirectionLinesChecked = ui.checkBox_NorthDirectionLines->isChecked();

	// 磁偏角
	m_ImageMappingSchema.layoutItemParam.dMagenticAngle = GetMagenticAngle();

	// 子午线收敛角
	m_ImageMappingSchema.layoutItemParam.dMeridianConvergenceAngle = GetMeridianConvergenceAngle();

	// 是否绘制接图表
	m_ImageMappingSchema.layoutItemParam.bSheetListChecked = ui.checkBox_SheetList->isChecked();

	// 接图表文字和图幅号自动计算

	// 是否绘制镶嵌线缩略图
	m_ImageMappingSchema.layoutItemParam.bMosaicMapChecked = ui.checkBox_MosaicMap->isChecked();

	// 是否绘制主图图例
	m_ImageMappingSchema.layoutItemParam.bMainMapLegendChecked = ui.checkBox_MainMapLegend->isChecked();

	// 是否绘制镶嵌线地图图例
	m_ImageMappingSchema.layoutItemParam.bMosaicMapLegendChecked = ui.checkBox_MosaicMapLegend->isChecked();

	// 是否绘制附注
	m_ImageMappingSchema.layoutItemParam.bAnnotationsChecked = ui.checkBox_Annotations->isChecked();

	// 地图附注
	m_ImageMappingSchema.layoutItemParam.qstrMapAnnotations = GetMapAnnotation();

	// 制图比例尺
	double dScale = 0;
	if (m_ImageMappingSchema.qstrLayoutTemplateName == "1w_layout.qpt")
	{
		dScale = SCALE_10K;
	}
	else if (m_ImageMappingSchema.qstrLayoutTemplateName == "5w_layout.qpt")
	{
		dScale = SCALE_50K;
	}
#pragma endregion

#pragma region "获取布局元素位置配置信息"

	/*获取布局元素位置配置信息*/
	vector<LayoutItemPositions> vLayoutItemPositions;
	vLayoutItemPositions.clear();

	bool bLoadXml = LoadLayoutItemPositionsConfig(dScale, vLayoutItemPositions);
	if (!bLoadXml)
	{
		QgsMessageLog::logMessage(tr("打开布局元素配置文件失败"), tr("影像图制图"), Qgis::Critical);
	}

#pragma endregion


#pragma region "对每个图幅循环进行制图输出"

	// 对每个图幅循环进行制图输出
	for (i = 0; i < vMapSheetList.size(); i++)
	{

		/*----------------------------------------*/
		/*【2】从矢量数据元数据文件SMS中读取图名，
		在【整饰要素】-【主标题】中显示图名，副标题需要界面设置*/
		/*----------------------------------------*/
		// 当前图幅名称
		QString qstrSheet = vMapSheetList[i];

		/*计算高斯范围*/
		/*根据图幅号获取地理范围*/
		CSE_MapSheet mapSheet;
		string strSheet = qstrSheet.toLocal8Bit();

		double dSheetBoxLeft = 0;       // 图幅左边界经度
		double dSheetBoxTop = 0;        // 图幅上边界纬度
		double dSheetBoxRight = 0;      // 图幅右边界经度
		double dSheetBoxBottom = 0;     // 图幅下边界纬度

		mapSheet.get_box(strSheet, dSheetBoxLeft, dSheetBoxTop, dSheetBoxRight, dSheetBoxBottom);

		/*根据要素选择参数自动筛选出需要输出的矢量地图图层*/
		vector<QString> vVectorLayerPathList;
		vVectorLayerPathList.clear();

		GetVectorLayerPathList(m_ImageMappingSchema,
			qstrSheet,
			vVectorLayerPathList);

#pragma region "从元数据SMS文件中获取“图名”、“政区说明”、“磁偏角”、“坐标纵线偏角”、“磁坐偏角”"

		/*获取当前图幅元数据信息*/
		QString qstrSMSFilePath = m_ImageMappingSchema.qstrVectorDataPath + "/" + qstrSheet + "/" + qstrSheet + ".SMS";

		// 从SMS文件中读取“图名”、“政区说明”、“磁偏角”、“坐标纵线偏角”、“磁坐偏角”
		// 图名
		string strMapName = "";

		// 政区说明
		string strMapSubTitleProvince = "";


		// 磁偏角：是磁子午线与真子午线间的夹角，前两位为度，后两位为分
		string strCiPianJiao;

		// 坐标纵线偏角：子午线收敛角即坐标纵线偏角，以真子午线为准，真子午线与坐标纵线之间的夹角。
		// 坐标纵线东偏为正，西偏为负。其角值可用近似计算公式∆L*sinB计算。
		// 前两位为度，后两位为分
		string strZuoBiaoZongXianPianJiao;

		// 磁坐偏角：是磁子午线与坐标纵线间的夹角，常以δm表示，并规定以坐标纵线北方向为准，
		// 磁子午线位于以东时称东偏、其角值为正，位于以西时称西偏、其角值为负
		// 前两位为度，后两位为分
		string strCiZuoPianJiao;

		string strSMSPath = qstrSMSFilePath.toLocal8Bit();



		string strNorthWestName;		// 西北图幅名称
		string strNorthName;			// 北图幅名称
		string strNorthEastName;		// 东北图幅名称

		string strWestName;				// 西图幅名称
		string strEastName;				// 东图幅名称

		string strSouthWestName;		// 西南图幅名称
		string strSouthName;			// 南图幅名称
		string strSouthEastName;		// 东南图幅名称

		bool bReadSMS = false;
		FILE* fp = fopen(strSMSPath.c_str(), "r");
		if (!fp) {
			QgsMessageLog::logMessage(tr("读取元数据文件失败！:%1").arg(qstrSMSFilePath), tr("影像图制图"), Qgis::Warning);
		}
		else
		{
			// 读取图名
			ReadSMSFile(strSMSPath, 6, strMapName);

			// 读取政区名称
			ReadSMSFile(strSMSPath, 93, strMapSubTitleProvince);

			// 读取磁偏角
			ReadSMSFile(strSMSPath, 34, strCiPianJiao);

			// 读取坐标纵线偏角
			ReadSMSFile(strSMSPath, 36, strZuoBiaoZongXianPianJiao);

			// 读取磁坐偏角
			ReadSMSFile(strSMSPath, 35, strCiZuoPianJiao);

			/*获取上、下、左、右四个图幅名*/
			/*
			修改时间：2023-03-21
			修改内容：从SMS数据中85-92行依次读取上下左右图幅的图层名
			*/

			// 85       图幅接图表中西北图幅名称
			ReadSMSFile(strSMSPath, 85, strNorthWestName);

			// 86       图幅接图表中北图幅名称
			ReadSMSFile(strSMSPath, 86, strNorthName);

			// 87       图幅接图表中东北图幅名称
			ReadSMSFile(strSMSPath, 87, strNorthEastName);

			// 88       图幅接图表中西图幅名称
			ReadSMSFile(strSMSPath, 88, strWestName);

			// 89       图幅接图表中东图幅名称
			ReadSMSFile(strSMSPath, 89, strEastName);

			// 90       图幅接图表中西南图幅名称
			ReadSMSFile(strSMSPath, 90, strSouthWestName);

			// 91       图幅接图表中南图幅名称
			ReadSMSFile(strSMSPath, 91, strSouthName);

			// 92       图幅接图表中东南图幅名称
			ReadSMSFile(strSMSPath, 92, strSouthEastName);

		}

#pragma region "计算上下左右邻接图幅"

		string strLeftSheet;        // 左图幅名
		string strTopSheet;         // 上图幅名
		string strRightSheet;       // 右图幅名
		string strBottomSheet;      // 下图幅名
		mapSheet.get_adjsheet_by_scale_and_sheet(dScale, strSheet, strLeftSheet, strTopSheet, strRightSheet, strBottomSheet, "D");

		// 左图幅
		QString qstrLeftSheet = QString::fromLocal8Bit(strLeftSheet.c_str());
		// 上图幅
		QString qstrTopSheet = QString::fromLocal8Bit(strTopSheet.c_str());
		// 右图幅
		QString qstrRightSheet = QString::fromLocal8Bit(strRightSheet.c_str());
		// 下图幅
		QString qstrBottomSheet = QString::fromLocal8Bit(strBottomSheet.c_str());

#pragma region "接图角中各图幅对应地图名称赋值"

		// 左图幅图名（西图幅）
		QString qstrLeftSheetMapName = QString::fromLocal8Bit(strWestName.c_str());

		// 右图幅图名（东图幅）
		QString qstrRightSheetMapName = QString::fromLocal8Bit(strEastName.c_str());

		// 上图幅图名（北图幅）
		QString qstrTopSheetMapName = QString::fromLocal8Bit(strNorthName.c_str());

		// 下图幅图名（南图幅）
		QString qstrBottomSheetMapName = QString::fromLocal8Bit(strSouthName.c_str());

		// 左上图幅图名（西北图幅）
		QString qstrLeftTopSheetMapName = QString::fromLocal8Bit(strNorthWestName.c_str());

		// 右上图幅图名（东北图幅）
		QString qstrRightTopSheetMapName = QString::fromLocal8Bit(strNorthEastName.c_str());

		// 左下图幅图名（西南图幅）
		QString qstrLeftBottomSheetMapName = QString::fromLocal8Bit(strSouthWestName.c_str());

		// 右下图幅图名（东南图幅）
		QString qstrRightBottomSheetMapName = QString::fromLocal8Bit(strSouthEastName.c_str());

#pragma endregion


#pragma endregion


#pragma endregion

#pragma region "图层列表定义"

		/*获取对应图幅的影像图层*/
		// 原始tif影像数据
		QString qstrTifImageFilePath = m_ImageMappingSchema.qstrImageDataPath;

		// 将tif影像转换为jpg
		if (!TranslateFromTif2Jpg(qstrTifImageFilePath, qstrSheet))
		{
			// 记录日志
			QgsMessageLog::logMessage((QString(tr("影像处理失败:")) + qstrTifImageFilePath), tr("影像图制图"), Qgis::Warning);
			continue;
		}

		QString qstrImageFilePath;
		qstrImageFilePath = m_ImageMappingSchema.qstrImageDataPath + "/" + qstrSheet + ".jpg";


		/*获取对应图幅的镶嵌线图层*/
		QString qstrMosaicFilePath = m_ImageMappingSchema.qstrMosaicDataPath + "/" + qstrSheet + ".shp";

		/*获取工程实例*/
		bool bResult = false;
		QgsProject* pProject = QgsProject::instance();
		if (!pProject)
		{
			// 记录日志
			QgsMessageLog::logMessage(tr("工程实例获取异常"), tr("影像图制图"), Qgis::Critical);
			continue;
		}

		/*清空当前工程的所有图层*/
		pProject->removeAllMapLayers();

		QList<QgsMapLayer*> qMapLayerList;          // 添加到工程中的地图图层列表
		qMapLayerList.clear();

		QList<QgsMapLayer*> qMainMapLayerList;      // 主地图加载图层列表
		qMainMapLayerList.clear();

		QList<QgsMapLayer*> qMosaicMapLayerList;    // 镶嵌线地图加载图层列表，只加载镶嵌线地图
		qMosaicMapLayerList.clear();
#pragma endregion

#pragma region "加载影像图层"
		/*-------------------------------*/
		/*----------加载影像图层---------*/
		/*-------------------------------*/
		QString fileNameRaster = qstrImageFilePath;
		QFileInfo fileInfoRaster(fileNameRaster);

		// 获取影像数据名称
		QString baseNameRaster = fileInfoRaster.completeBaseName();

		// 根据文件存放路径，栅格的名称创建一个QgsRasterLayer类
		QgsRasterLayer* rasterLayer = new QgsRasterLayer(fileNameRaster, baseNameRaster);
		if (!rasterLayer || !rasterLayer->isValid())
		{
			// 记录日志
			QgsMessageLog::logMessage((QString(tr("栅格图层不合法:")) + fileNameRaster), tr("影像图制图"), Qgis::Warning);
			continue;
		}
		// 获取影像数据分辨率
		double dImageResolution = rasterLayer->extent().width() / rasterLayer->width();

		QgsMapLayer* pLayerRaster = pProject->addMapLayer(rasterLayer);
		if (!pLayerRaster)
		{
			delete rasterLayer;
			// 记录日志
			QgsMessageLog::logMessage((QString(tr("增加栅格图层失败:")) + fileNameRaster), tr("影像图制图"), Qgis::Warning);
			continue;
		}

		// 影像图层添加到地图图层列表
		qMapLayerList.append(pLayerRaster);
#pragma endregion

#pragma region "加载地形图矢量图层"
		/*-------------------------------*/
		/*---------加载矢量图层----------*/
		/*-------------------------------*/

		/*循环读取矢量图层，按照要素选择列表进行符号化，如：编码为铁路的要素采用黑色/白色相间显示*/
		for (j = 0; j < vVectorLayerPathList.size(); j++)
		{
			QString qstrVectorLayerFilePath = vVectorLayerPathList[j];
			// 判断矢量图层是否存在
			// 如果文件存在
			if (QFileInfo::exists(qstrVectorLayerFilePath))
			{
				/*将矢量图层加载到地图图层列表中*/
				QString fileNameVector = qstrVectorLayerFilePath;
				QFileInfo fileInfoVector(fileNameVector);

				// 获取数据名称
				QString baseNameVector = fileInfoVector.completeBaseName();

				//-------------------------------------------------------------//
				// 2022-09-01 增加矢量图层样式搭配
				// ------------------------------------------------------------//
				// 通过图层类型获取对应的样式文件全路径
				QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称

				/*获取qml名称*/
				QString qstrQmlFilePath = GetQmlFilePathFromLayerName(dScale, qstrBaseName);

				QgsVectorLayer* pVecotrLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
				if (!pVecotrLayer->isValid())
				{
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				/*图层已有的样式*/
				QgsMapLayerStyle oldStyle = pVecotrLayer->styleManager()->style(pVecotrLayer->styleManager()->currentStyle());
				QString message;
				bool defaultLoadedFlag = false;
				message = pVecotrLayer->loadNamedStyle(qstrQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

				if (!defaultLoadedFlag)
				{
					// QgsMessageLog::logMessage((QString(tr("样式文件不存在:")) + qstrQmlFilePath), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				QgsMapLayer* pLayer = pProject->addMapLayer(pVecotrLayer);
				if (!pLayer)
				{
					delete pVecotrLayer;
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				qMapLayerList.append(pLayer);
				qMainMapLayerList.append(pLayer);
			}
		}

		/*
		修改时间：2023-04-16
		修改内容：判断输出路径是否存在当前图幅文件夹，并且存在对应的图幅命名的外图廓内线图层和方里网刻度值点图层，
				线图层名称:图幅号_LayoutLine：线要素类型包括外图廓、内图廓、东西方向方里网、南北方向方里网、邻带方里网
				点图层名称：图幅号_LayoutPoint:点要素类型包括
				（上边界代号、上边界百公里数、上边界十公里数、
				下边界代号、下边界百公里数、下边界十公里数、
				左边界千百公里数、左边界十公里数、
				右边界千百公里数、右边界十公里数、
				邻带方里网上边界代号、上边界百公里数、上边界十公里数、
				邻带方里网下边界代号、下边界百公里数、下边界十公里数、
				邻带方里网左边界千百公里数、左边界十公里数、
				邻带方里网右边界千百公里数、右边界十公里数）

				（1）如果存在，则加载对应的布局点、线图层及样式；
				（2）如果不存在，则后续生成布局点、线图层

		*/

		//------------------------【开始】--------------------------------//
#pragma region "加载方里网点、线图层"

		// 布局点图层默认不存在
		bool bLayoutLayerPointExisted = false;

		// 布局线图层默认不存在
		bool bLayoutLayerLineExisted = false;

		// 布局点图层路径
		QString qstrLayoutLayerPointPath = m_ImageMappingSchema.qstrMapExportPath + "/" + qstrSheet + "/" + qstrSheet + "_LayoutPoint.shp";

		// 布局线图层路径
		QString qstrLayoutLayerLinePath = m_ImageMappingSchema.qstrMapExportPath + "/" + qstrSheet + "/" + qstrSheet + "_LayoutLine.shp";

		// 获取布局点样式文件路径
		QString qstrLayoutPointQmlFilePath;
		QString qstrLayoutLineQmlFilePath;

		GetLayoutQmlFilePath(dScale, qstrLayoutPointQmlFilePath, qstrLayoutLineQmlFilePath);


		// 获取布局线样式文件路径

		// 判断图层是否存在
		// 如果布局点图层文件存在
		if (QFileInfo::exists(qstrLayoutLayerPointPath))
		{
			bLayoutLayerPointExisted = true;

			/*将矢量图层加载到地图图层列表中*/
			QString fileNameVector = qstrLayoutLayerPointPath;
			QFileInfo fileInfoVector(fileNameVector);

			// 获取数据名称
			QString baseNameVector = fileInfoVector.completeBaseName();

			// 通过图层类型获取对应的样式文件全路径
			QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称

			/*获取qml名称*/


			QgsVectorLayer* pVecotrLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
			if (!pVecotrLayer->isValid())
			{
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
				continue;
			}

			/*图层已有的样式*/
			QgsMapLayerStyle oldStyle = pVecotrLayer->styleManager()->style(pVecotrLayer->styleManager()->currentStyle());
			QString message;
			bool defaultLoadedFlag = false;
			message = pVecotrLayer->loadNamedStyle(qstrLayoutPointQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

			if (!defaultLoadedFlag)
			{
				continue;
			}

			QgsMapLayer* pLayer = pProject->addMapLayer(pVecotrLayer);
			if (!pLayer)
			{
				delete pVecotrLayer;
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
				continue;
			}

			qMapLayerList.append(pLayer);
			qMainMapLayerList.append(pLayer);
		}
		else
		{
			bLayoutLayerPointExisted = false;
		}


		// 判断图层是否存在
		// 如果布局线图层文件存在
		if (QFileInfo::exists(qstrLayoutLayerLinePath))
		{
			bLayoutLayerLineExisted = true;

			/*将矢量图层加载到地图图层列表中*/
			QString fileNameVector = qstrLayoutLayerLinePath;
			QFileInfo fileInfoVector(fileNameVector);

			// 获取数据名称
			QString baseNameVector = fileInfoVector.completeBaseName();

			// 通过图层类型获取对应的样式文件全路径
			QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称


			QgsVectorLayer* pVecotrLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
			if (!pVecotrLayer->isValid())
			{
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
				continue;
			}

			/*图层已有的样式*/
			QgsMapLayerStyle oldStyle = pVecotrLayer->styleManager()->style(pVecotrLayer->styleManager()->currentStyle());
			QString message;
			bool defaultLoadedFlag = false;
			message = pVecotrLayer->loadNamedStyle(qstrLayoutLineQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

			if (!defaultLoadedFlag)
			{
				continue;
			}

			QgsMapLayer* pLayer = pProject->addMapLayer(pVecotrLayer);
			if (!pLayer)
			{
				delete pVecotrLayer;
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
				continue;
			}

			qMapLayerList.append(pLayer);
			qMainMapLayerList.append(pLayer);
		}
		else
		{
			bLayoutLayerLineExisted = false;
		}


#pragma endregion

#pragma region "加载图外整饰点、整饰面图层"

		// 图外整饰点图层默认不存在
		bool bLayoutDecPointExisted = false;

		// 图外整饰面图层默认不存在
		bool bLayoutDecPolygonExisted = false;

		// 图外整饰点图层路径
		QString qstrLayoutDecPointPath = m_ImageMappingSchema.qstrMapExportPath + "/" + qstrSheet + "/" + qstrSheet + "_DecPoint.shp";

		// 图外整饰面图层路径
		QString qstrLayoutDecPolygonPath = m_ImageMappingSchema.qstrMapExportPath + "/" + qstrSheet + "/" + qstrSheet + "_DecPolygon.shp";

		// 获取图外整饰点样式文件路径
		QString qstrLayoutDecPointQmlFilePath;

		// 获取图外整饰面样式文件路径
		QString qstrLayoutDecPolygonQmlFilePath;

		GetLayoutDecPointQmlFilePath(dScale, qstrLayoutDecPointQmlFilePath);
		GetLayoutDecPolygonQmlFilePath(dScale, qstrLayoutDecPolygonQmlFilePath);

		// 判断整饰点图层是否存在
		// 如果整饰点图层文件存在
		if (QFileInfo::exists(qstrLayoutDecPointPath))
		{
			bLayoutDecPointExisted = true;

			/*将矢量图层加载到地图图层列表中*/
			QString fileNameVector = qstrLayoutDecPointPath;
			QFileInfo fileInfoVector(fileNameVector);

			// 获取数据名称
			QString baseNameVector = fileInfoVector.completeBaseName();

			// 通过图层类型获取对应的样式文件全路径
			QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称

			QgsVectorLayer* pVecotrLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
			if (!pVecotrLayer->isValid())
			{
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
				continue;
			}

			/*图层已有的样式*/
			QgsMapLayerStyle oldStyle = pVecotrLayer->styleManager()->style(pVecotrLayer->styleManager()->currentStyle());
			QString message;
			bool defaultLoadedFlag = false;
			message = pVecotrLayer->loadNamedStyle(qstrLayoutDecPointQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

			if (!defaultLoadedFlag)
			{
				continue;
			}

			QgsMapLayer* pLayer = pProject->addMapLayer(pVecotrLayer);
			if (!pLayer)
			{
				delete pVecotrLayer;
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
				continue;
			}

			qMapLayerList.append(pLayer);
			qMainMapLayerList.append(pLayer);
		}
		else
		{
			bLayoutDecPointExisted = false;
		}


		// 判断整饰面图层是否存在
		// 如果整饰面图层文件存在
		if (QFileInfo::exists(qstrLayoutDecPolygonPath))
		{
			bLayoutDecPolygonExisted = true;

			/*将矢量图层加载到地图图层列表中*/
			QString fileNameVector = qstrLayoutDecPolygonPath;
			QFileInfo fileInfoVector(fileNameVector);

			// 获取数据名称
			QString baseNameVector = fileInfoVector.completeBaseName();

			// 通过图层类型获取对应的样式文件全路径
			QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称


			QgsVectorLayer* pVecotrLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
			if (!pVecotrLayer->isValid())
			{
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
				continue;
			}

			/*图层已有的样式*/
			QgsMapLayerStyle oldStyle = pVecotrLayer->styleManager()->style(pVecotrLayer->styleManager()->currentStyle());
			QString message;
			bool defaultLoadedFlag = false;
			message = pVecotrLayer->loadNamedStyle(qstrLayoutDecPolygonQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

			if (!defaultLoadedFlag)
			{
				continue;
			}

			QgsMapLayer* pLayer = pProject->addMapLayer(pVecotrLayer);
			if (!pLayer)
			{
				delete pVecotrLayer;
				// 记录日志
				QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
				continue;
			}

			qMapLayerList.append(pLayer);
			qMainMapLayerList.append(pLayer);
		}
		else
		{
			bLayoutDecPolygonExisted = false;
		}


#pragma endregion
		//------------------------【结束】---------------------------------//

#pragma endregion

#pragma region "加载镶嵌线图层"
		/*-------------------------------*/
		/*---------加载镶嵌线图层--------*/
		/*-------------------------------*/
		QString fileNameMosaic = qstrMosaicFilePath;
		QFileInfo fileInfoMosaic(fileNameMosaic);

		// 获取数据名称
		QString basenameMosaic = fileInfoMosaic.completeBaseName();
		QgsVectorLayer* vecLayerMosaic = new QgsVectorLayer(fileNameMosaic, basenameMosaic, "ogr");

		if (!vecLayerMosaic->isValid())
		{
			// 记录日志
			QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameMosaic), tr("影像图制图"), Qgis::Warning);
			continue;
		}


#pragma region "使用唯一值渲染镶嵌类别"

		// 渲染字段
		QString qstrRenderField = ui.lineEdit_MosaicDataRenderField->text();

		// 摄影时间字段
		QString qstrAcqDataField = ui.lineEdit_MosaicDataAcqDataField->text();

		// 传感器字段
		QString qstrSensorField = ui.lineEdit_MosaicDataSensorField->text();

		int currentFieldIndexOf = vecLayerMosaic->fields().indexOf(qstrRenderField);
		// 得到字段值对应的所有属性
		QSet<QVariant> unique = vecLayerMosaic->uniqueValues(currentFieldIndexOf);
		QVariantList uniqueValues = unique.toList();
		//获取所有属性对应的类别
		QgsCategoryList cats = QgsCategorizedSymbolRenderer::createCategories(uniqueValues, QgsSymbol::defaultSymbol(vecLayerMosaic->geometryType()), vecLayerMosaic, qstrRenderField);
		QColor color;
		int num = 0;


		//设置不同的符号颜色
		for (auto iter = cats.begin(); iter != cats.end(); ++iter)
		{
			QColor clr(rand() % 256, rand() % 256, rand() % 256);
			// 设置面要素颜色
			iter->symbol()->setColor(clr);

			// 2022-10-24
			// 设置面要素渲染边框为透明

		}

		//加载进QgsCategorizedSymbolRenderer渲染器中
		QgsCategorizedSymbolRenderer* pRender = new QgsCategorizedSymbolRenderer(qstrRenderField, cats);

		//渲染图层
		vecLayerMosaic->setRenderer(pRender);

#pragma region "获取镶嵌线属性集合"

		// 存储要素集合
		QList<QMap<QString, QString>> qAttributeList;
		qAttributeList.clear();

		// 获取镶嵌线图层的属性集合
		qAttributeList = GetAttributeValue(vecLayerMosaic);

		// 记录类型的颜色值
		vector<QColor> vCateColor;
		vCateColor.clear();

		// 记录摄影时间
		vector<QString> vCateAcqDate;
		vCateAcqDate.clear();

		// 记录传感器类型
		vector<QString> vCateSensor;
		vCateSensor.clear();


		// 对每个类型进行查询"摄影时间:acq_data"、"传感器:sensor"
		for (int iCatIndex = 0; iCatIndex < cats.size(); iCatIndex++)
		{
			// 获取类型
			QgsRendererCategory qgsRenderCate = cats.at(iCatIndex);

			// 类型名称
			QString qstrCateValue = qgsRenderCate.value().toString();

			// 如果类型不为空
			if (qstrCateValue.length() > 0)
			{
				// 根据类型名称查询摄影时间和传感器类型
				QString qstrAcqDateValue;
				QString qstrSensorValue;
				GetAcqDataAndSensorByCateName(qAttributeList,
					qstrCateValue,
					qstrRenderField,
					qstrAcqDataField,
					qstrSensorField,
					qstrAcqDateValue,
					qstrSensorValue);

				vCateAcqDate.push_back(qstrAcqDateValue);
				vCateSensor.push_back(qstrSensorValue);

				// 获取类型的符号颜色
				QColor qSymColor = qgsRenderCate.symbol()->color();
				vCateColor.push_back(qSymColor);
			}

		}



#pragma endregion



#pragma endregion


		QgsMapLayer* pLayerMosaic = pProject->addMapLayer(vecLayerMosaic);
		if (!pLayerMosaic)
		{
			delete vecLayerMosaic;
			QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameMosaic), tr("影像图制图"), Qgis::Warning);
			continue;
		}

    /*鹰眼图加到地图后默认不显示*/
		QgsLayerTree* root = pProject->layerTreeRoot();
		if (!root)
		{
			delete vecLayerMosaic;
			QgsMessageLog::logMessage(QString(tr("获取图层控制器失败:")), tr("影像图制图"), Qgis::Warning);
			continue;
		}

		QgsLayerTreeLayer* layerNode = root->findLayer(pLayerMosaic->id());
		if (!layerNode)
		{
			delete vecLayerMosaic;
			QgsMessageLog::logMessage(QString(tr("获取鹰眼图图层ID失败:")), tr("影像图制图"), Qgis::Warning);
			continue;
		}

		// 设置图层默认不可见
		layerNode->setItemVisibilityChecked(false);


		qMapLayerList.append(pLayerMosaic);
		qMosaicMapLayerList.append(pLayerMosaic);
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
			QgsMessageLog::logMessage(tr("创建打印布局失败！"), tr("影像图制图"), Qgis::Critical);
			continue;
		}

		/*设置布局名称 - 图幅号*/
		pPrintLayout->setName(qstrSheet);

		/*初始化默认设置*/
		pPrintLayout->initializeDefaults();

		/*从制图方案对应的模板中加载布局项*/
		QString qstrLayoutTemplatePath = m_ImageMappingSchema.qstrLayoutTemplatePath;
		QFileInfo templateFileInfo(qstrLayoutTemplatePath);
		QFile templateFile(qstrLayoutTemplatePath);
		if (!templateFile.open(QIODevice::ReadOnly))
		{
			QgsMessageLog::logMessage(tr("读取布局模板文件失败！:%1").arg(qstrLayoutTemplatePath), tr("影像图制图"), Qgis::Critical);
			continue;
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
				QgsMessageLog::logMessage(tr("读取布局模板文件失败！:%1").arg(qstrLayoutTemplatePath), tr("影像图制图"), Qgis::Critical);
				continue;
			}
			else
			{
				// 默认全部设置可选及可见
				for (int j = 0; j < qLayoutItemList.size(); j++)
				{
					qLayoutItemList[j]->setSelected(true);
				}
			}
		}

#pragma endregion

#pragma region "设置主地图范围及空间参考"

		/*获取主地图位置配置信息*/
		LayoutItemPositions mainMapItemPosition;        // 主地图窗口位置配置信息

		GetLayoutItemPositionByName("MapItem_MainMap",
			vLayoutItemPositions,
			mainMapItemPosition);

		// 图层的范围
		QgsRectangle rect;



		/*对图幅四个角点进行高斯投影正算*/
		// 图幅四个角点坐标
		double dSheetPoints[8];
		// 左上角点
		dSheetPoints[0] = dSheetBoxLeft;
		dSheetPoints[1] = dSheetBoxTop;

		// 右上角点
		dSheetPoints[2] = dSheetBoxRight;
		dSheetPoints[3] = dSheetBoxTop;

		// 右下角点
		dSheetPoints[4] = dSheetBoxRight;
		dSheetPoints[5] = dSheetBoxBottom;

		// 左下角点
		dSheetPoints[6] = dSheetBoxLeft;
		dSheetPoints[7] = dSheetBoxBottom;

		// 当前图幅中央经线
		double dCenterMeridian = CalCenterMeridian((dSheetBoxLeft + dSheetBoxRight) / 2, dScale);

		int iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
			GaussKruger,
			dCenterMeridian,
			0,
			0,
			0,
			0,
			500000,
			0,
			4,
			dSheetPoints);

		if (iResult != 1)
		{
			// 记录日志
			QgsMessageLog::logMessage(tr("图幅角点坐标高斯投影变换计算失败！"), tr("影像图制图"), Qgis::Warning);
			continue;
		}

		/*获取最小外接矩形*/
		double dMBRLeft = 0;
		double dMBRTop = 0;
		double dMBRRight = 0;
		double dMBRBottom = 0;
		GetMBR(4, dSheetPoints, dMBRLeft, dMBRTop, dMBRRight, dMBRBottom);
		// 如果比例尺是5万
		if (dScale == SCALE_50K)
		{
			dMBRLeft -= 1000;
			dMBRRight += 1000;
			dMBRBottom -= 2000;
			dMBRTop += 1000;
		}
		// 如果比例尺是1万
		else if (dScale == SCALE_10K)
		{
			dMBRLeft -= 200;
			dMBRRight += 200;
			dMBRBottom -= 400;
			dMBRTop += 200;
		}

		rect.setXMinimum(dMBRLeft);
		rect.setXMaximum(dMBRRight);
		rect.setYMinimum(dMBRBottom);
		rect.setYMaximum(dMBRTop);
		rect.scale(1.0);

		/*主地图设置*/
		QgsLayoutItemMap* pMainMapItem = (QgsLayoutItemMap*)GetLayoutItemByName(qLayoutItemList, "MapItem_MainMap");
		if (!pMainMapItem)
		{
			// 记录日志
			QgsMessageLog::logMessage(tr("主地图控件获取失败！"), tr("影像图制图"), Qgis::Critical);
			continue;
		}

		/*根据中央经线和比例尺类型获取空间参考系编码（预定义中国区域）*/
		// 主地图和镶嵌线地图的空间参考系编码
		QString qstrEPSGCode = GetEPSGCodeByCenterMeridianAndScale(dCenterMeridian, dScale);

		/*设置主地图空间参考系*/
		pMainMapItem->setCrs(QgsCoordinateReferenceSystem(qstrEPSGCode));

		pMainMapItem->setBackgroundColor(QColor(255, 255, 255, 0));
		pMainMapItem->setScale(dScale);
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
			(dMBRTop - dMBRBottom) * dMapUnit2LayoutUnit));

		pMainMapItem->setExtent(rect);

#pragma endregion

		// 高斯坐标分布示意图
		/*
		A-------------B
		|			  |
		|			  |
		|		      |
		D-------------C
		*/


		// 地图内图廓多边形四个角点的高斯坐标
		// 地图内图廓多边形左上角点高斯坐标-A
		SE_DPoint interiorMapGaussPoint_A;
		interiorMapGaussPoint_A.dx = dSheetPoints[0];
		interiorMapGaussPoint_A.dy = dSheetPoints[1];

		// 地图内图廓多边形右上角点高斯坐标-B
		SE_DPoint interiorMapGaussPoint_B;
		interiorMapGaussPoint_B.dx = dSheetPoints[2];
		interiorMapGaussPoint_B.dy = dSheetPoints[3];

		// 地图内图廓多边形右下角点高斯坐标-C
		SE_DPoint interiorMapGaussPoint_C;
		interiorMapGaussPoint_C.dx = dSheetPoints[4];
		interiorMapGaussPoint_C.dy = dSheetPoints[5];

		// 地图内图廓多边形左下角点高斯坐标-D
		SE_DPoint interiorMapGaussPoint_D;
		interiorMapGaussPoint_D.dx = dSheetPoints[6];
		interiorMapGaussPoint_D.dy = dSheetPoints[7];

		// 地图内图廓多边形角点高斯坐标多边形
		// 制作图之前线清空数组，包括高斯坐标、布局坐标
		m_InteriorMapGaussPolygon.vExteriorPoints.clear();
		m_InteriorMapLayoutPolygon.vExteriorPoints.clear();


		m_InteriorMapGaussPolygon.vExteriorPoints.push_back(interiorMapGaussPoint_A);
		m_InteriorMapGaussPolygon.vExteriorPoints.push_back(interiorMapGaussPoint_B);
		m_InteriorMapGaussPolygon.vExteriorPoints.push_back(interiorMapGaussPoint_C);
		m_InteriorMapGaussPolygon.vExteriorPoints.push_back(interiorMapGaussPoint_D);

		// 地图内图廓矩形左上角高斯坐标
		m_InteriorMapLeftTopGaussPoint.dx = pMainMapItem->extent().xMinimum();
		m_InteriorMapLeftTopGaussPoint.dy = pMainMapItem->extent().yMaximum();

		// 四个高斯投影角点A、B、C、D转换为布局坐标
		for (int iIndex = 0; iIndex < m_InteriorMapGaussPolygon.vExteriorPoints.size(); iIndex++)
		{
			SE_DPoint dLayoutPoint;
			TransformGaussToLayout(m_InteriorMapLeftTopGaussPoint,
				m_InteriorMapLeftTopLayoutPoint,
				m_InteriorMapGaussPolygon.vExteriorPoints[iIndex],
				dMapUnit2LayoutUnit,
				dLayoutPoint);

			// 地图内图廓多边形角点布局坐标多边形
			m_InteriorMapLayoutPolygon.vExteriorPoints.push_back(dLayoutPoint);
		}


#pragma region "地图内图廓多边形四个布局点坐标A、B、C、D"

		// 布局坐标分布示意图
		/*
		A-------------B
		|			  |
		|			  |
		|		      |
		D-------------C
		*/

		// 内图廓左上角A布局坐标值
		SE_DPoint interiorMapLayoutPoint_A;
		interiorMapLayoutPoint_A.dx = m_InteriorMapLayoutPolygon.vExteriorPoints[0].dx;
		interiorMapLayoutPoint_A.dy = m_InteriorMapLayoutPolygon.vExteriorPoints[0].dy;

		// 内图廓右上角B布局坐标值
		SE_DPoint interiorMapLayoutPoint_B;
		interiorMapLayoutPoint_B.dx = m_InteriorMapLayoutPolygon.vExteriorPoints[1].dx;
		interiorMapLayoutPoint_B.dy = m_InteriorMapLayoutPolygon.vExteriorPoints[1].dy;

		// 内图廓右下角C布局坐标值
		SE_DPoint interiorMapLayoutPoint_C;
		interiorMapLayoutPoint_C.dx = m_InteriorMapLayoutPolygon.vExteriorPoints[2].dx;
		interiorMapLayoutPoint_C.dy = m_InteriorMapLayoutPolygon.vExteriorPoints[2].dy;

		// 内图廓左下角D布局坐标值
		SE_DPoint interiorMapLayoutPoint_D;
		interiorMapLayoutPoint_D.dx = m_InteriorMapLayoutPolygon.vExteriorPoints[3].dx;
		interiorMapLayoutPoint_D.dy = m_InteriorMapLayoutPolygon.vExteriorPoints[3].dy;


#pragma endregion



#pragma endregion

#pragma region "计算方里网整公里刻度所在布局点坐标"


#pragma region "内图廓A-B整公里刻度点布局坐标"

		// 上边界A-B高斯整公里坐标值集合
		vector<SE_DPoint> vBorderGaussCoords_A_B;
		vBorderGaussCoords_A_B.clear();
		CalGaussBorderCoords_A_B(interiorMapGaussPoint_A, interiorMapGaussPoint_B, vBorderGaussCoords_A_B);

#pragma endregion

#pragma region "内图廓B-C整公里刻度点布局坐标"

		// 右边界B-C高斯整公里坐标值集合
		vector<SE_DPoint> vBorderGaussCoords_C_B;
		vBorderGaussCoords_C_B.clear();
		CalGaussBorderCoords_C_B(interiorMapGaussPoint_C, interiorMapGaussPoint_B, vBorderGaussCoords_C_B);

#pragma endregion

#pragma region "内图廓C-D整公里刻度点布局坐标"

		// 下边界C-D高斯整公里坐标值集合
		vector<SE_DPoint> vBorderGaussCoords_D_C;
		vBorderGaussCoords_D_C.clear();
		CalGaussBorderCoords_D_C(interiorMapGaussPoint_D, interiorMapGaussPoint_C, vBorderGaussCoords_D_C);

#pragma endregion

#pragma region "内图廓A-D整公里刻度点布局坐标"

		// 左边界A-D高斯整公里坐标值集合
		vector<SE_DPoint> vBorderGaussCoords_D_A;
		vBorderGaussCoords_D_A.clear();
		CalGaussBorderCoords_D_A(interiorMapGaussPoint_D, interiorMapGaussPoint_A, vBorderGaussCoords_D_A);

#pragma endregion

#pragma region "计算内图廓各边界整公里数对应的布局点坐标"

#pragma region "上边界A-B布局整公里坐标值集合"

		// 上边界A-B布局整公里坐标值集合
		vector<SE_DPoint> vBorderLayoutCoords_A_B;
		vBorderLayoutCoords_A_B.clear();

		for (int iIndex = 0; iIndex < vBorderGaussCoords_A_B.size(); iIndex++)
		{
			SE_DPoint dLayoutPoint;
			// 高斯坐标到布局坐标的转换
			TransformGaussToLayout(m_InteriorMapLeftTopGaussPoint,
				m_InteriorMapLeftTopLayoutPoint,
				vBorderGaussCoords_A_B[iIndex],
				dMapUnit2LayoutUnit,
				dLayoutPoint);

			vBorderLayoutCoords_A_B.push_back(dLayoutPoint);
		}

#pragma endregion


#pragma region "右边界B-C布局整公里坐标值集合"

		// 右边界B-C布局整公里坐标值集合
		vector<SE_DPoint> vBorderLayoutCoords_C_B;
		vBorderLayoutCoords_C_B.clear();

		for (int iIndex = 0; iIndex < vBorderGaussCoords_C_B.size(); iIndex++)
		{
			SE_DPoint dLayoutPoint;
			// 高斯坐标到布局坐标的转换
			TransformGaussToLayout(m_InteriorMapLeftTopGaussPoint,
				m_InteriorMapLeftTopLayoutPoint,
				vBorderGaussCoords_C_B[iIndex],
				dMapUnit2LayoutUnit,
				dLayoutPoint);

			vBorderLayoutCoords_C_B.push_back(dLayoutPoint);
		}

#pragma endregion

#pragma region "下边界C-D布局整公里坐标值集合"

		// 下边界C-D布局整公里坐标值集合
		vector<SE_DPoint> vBorderLayoutCoords_D_C;
		vBorderLayoutCoords_D_C.clear();

		for (int iIndex = 0; iIndex < vBorderGaussCoords_D_C.size(); iIndex++)
		{
			SE_DPoint dLayoutPoint;
			// 高斯坐标到布局坐标的转换
			TransformGaussToLayout(m_InteriorMapLeftTopGaussPoint,
				m_InteriorMapLeftTopLayoutPoint,
				vBorderGaussCoords_D_C[iIndex],
				dMapUnit2LayoutUnit,
				dLayoutPoint);

			vBorderLayoutCoords_D_C.push_back(dLayoutPoint);
		}

#pragma endregion

#pragma region "左边界A-D布局整公里坐标值集合"

		// 左边界A-D布局整公里坐标值集合
		vector<SE_DPoint> vBorderLayoutCoords_D_A;
		vBorderLayoutCoords_D_A.clear();

		for (int iIndex = 0; iIndex < vBorderGaussCoords_D_A.size(); iIndex++)
		{
			SE_DPoint dLayoutPoint;
			// 高斯坐标到布局坐标的转换
			TransformGaussToLayout(m_InteriorMapLeftTopGaussPoint,
				m_InteriorMapLeftTopLayoutPoint,
				vBorderGaussCoords_D_A[iIndex],
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

		// 构建内图廓多边形
		OGRPolygon* oInteriorPolygon = new OGRPolygon();
		OGRLinearRing oLine;

		oLine.addPoint(interiorMapLayoutPoint_A.dx, interiorMapLayoutPoint_A.dy);
		oLine.addPoint(interiorMapLayoutPoint_B.dx, interiorMapLayoutPoint_B.dy);
		oLine.addPoint(interiorMapLayoutPoint_C.dx, interiorMapLayoutPoint_C.dy);
		oLine.addPoint(interiorMapLayoutPoint_D.dx, interiorMapLayoutPoint_D.dy);
		oLine.addPoint(interiorMapLayoutPoint_A.dx, interiorMapLayoutPoint_A.dy);
		oInteriorPolygon->addRingDirectly(&oLine);

		OGRPolygon* pExteriorPolygon = (OGRPolygon*)oInteriorPolygon->Buffer(6, 0);
		if (!pExteriorPolygon)
		{
			// 记录日志
			continue;
		}

		// 缓冲区多边形为9个结点，
		// 第1-2点构成外图廓左边界直线
		// 第3-4点构成外图廓下边界直线
		// 第5-6点构成外图廓右边界直线
		// 第7-8点构成外图廓上边界直线
		int iExteriorRingPointCount = pExteriorPolygon->getExteriorRing()->getNumPoints();

		// 缓冲区多边形点坐标
		OGRPoint oBufferPoints[9];
		SE_DPoint dBufferPoints[9];
		for (int iIndex = 0; iIndex < 9; iIndex++)
		{
			pExteriorPolygon->getExteriorRing()->getPoint(iIndex, &oBufferPoints[iIndex]);
			dBufferPoints[iIndex].dx = oBufferPoints[iIndex].getX();
			dBufferPoints[iIndex].dy = oBufferPoints[iIndex].getY();
		}


		// 外图廓a点的布局坐标（1-2与7-8交点）
		SE_DPoint exteriorMapLayoutPoint_a = LineIntersect(
			dBufferPoints[0],
			dBufferPoints[1],
			dBufferPoints[6],
			dBufferPoints[7]);

		// 外图廓d点的布局坐标（1-2与3-4交点）
		SE_DPoint exteriorMapLayoutPoint_d = LineIntersect(
			dBufferPoints[0],
			dBufferPoints[1],
			dBufferPoints[2],
			dBufferPoints[3]);

		// 外图廓c点的布局坐标（3-4与5-6交点）
		SE_DPoint exteriorMapLayoutPoint_c = LineIntersect(
			dBufferPoints[2],
			dBufferPoints[3],
			dBufferPoints[4],
			dBufferPoints[5]);

		// 外图廓b点的布局坐标（5-6与7-8交点）
		SE_DPoint exteriorMapLayoutPoint_b = LineIntersect(
			dBufferPoints[4],
			dBufferPoints[5],
			dBufferPoints[6],
			dBufferPoints[7]);

		// 制作图之前线清空外图廓数组（布局坐标）
		m_ExteriorMapLayoutPolygon.vExteriorPoints.clear();

		m_ExteriorMapLayoutPolygon.vExteriorPoints.push_back(exteriorMapLayoutPoint_a);
		m_ExteriorMapLayoutPolygon.vExteriorPoints.push_back(exteriorMapLayoutPoint_b);
		m_ExteriorMapLayoutPolygon.vExteriorPoints.push_back(exteriorMapLayoutPoint_c);
		m_ExteriorMapLayoutPolygon.vExteriorPoints.push_back(exteriorMapLayoutPoint_d);

#pragma endregion

#pragma region "计算外图廓整公里刻度布局点坐标"

#pragma region "计算上边界a-b整公里刻度布局点坐标"

		vector<SE_DPoint> vBorderLayoutCoords_a_b;
		vBorderLayoutCoords_a_b.clear();

		for (int iIndex = 0; iIndex < vBorderLayoutCoords_A_B.size(); iIndex++)
		{
			double dX = vBorderLayoutCoords_A_B[iIndex].dx;
			double dY = 0;
			// 根据计算内图廓上边界整公里横坐标计算与外图廓的交点坐标
			CalCoordYByTwoPoint(exteriorMapLayoutPoint_a,
				exteriorMapLayoutPoint_b,
				dX,
				dY);

			vBorderLayoutCoords_a_b.push_back(SE_DPoint(dX, dY));
		}

#pragma endregion


#pragma region "计算下边界c-d整公里刻度布局点坐标"

		vector<SE_DPoint> vBorderLayoutCoords_d_c;
		vBorderLayoutCoords_d_c.clear();

		for (int iIndex = 0; iIndex < vBorderLayoutCoords_D_C.size(); iIndex++)
		{
			double dX = vBorderLayoutCoords_D_C[iIndex].dx;
			double dY = 0;
			// 根据计算内图廓下边界整公里横坐标计算与外图廓的交点坐标
			CalCoordYByTwoPoint(exteriorMapLayoutPoint_c,
				exteriorMapLayoutPoint_d,
				dX,
				dY);

			vBorderLayoutCoords_d_c.push_back(SE_DPoint(dX, dY));
		}

#pragma endregion

#pragma region "计算左边界a-d整公里刻度布局点坐标"

		vector<SE_DPoint> vBorderLayoutCoords_d_a;
		vBorderLayoutCoords_d_a.clear();

		for (int iIndex = 0; iIndex < vBorderLayoutCoords_D_A.size(); iIndex++)
		{
			double dX = 0;
			double dY = vBorderLayoutCoords_D_A[iIndex].dy;
			// 根据计算内图廓左边界整公里纵坐标计算与外图廓的交点坐标
			CalCoordXByTwoPoint(exteriorMapLayoutPoint_d,
				exteriorMapLayoutPoint_a,
				dY,
				dX);

			vBorderLayoutCoords_d_a.push_back(SE_DPoint(dX, dY));
		}

#pragma endregion


#pragma region "计算右边界b-c整公里刻度布局点坐标"

		vector<SE_DPoint> vBorderLayoutCoords_c_b;
		vBorderLayoutCoords_c_b.clear();

		for (int iIndex = 0; iIndex < vBorderLayoutCoords_C_B.size(); iIndex++)
		{
			double dX = 0;
			double dY = vBorderLayoutCoords_C_B[iIndex].dy;
			// 根据计算内图廓左边界整公里纵坐标计算与外图廓的交点坐标
			CalCoordXByTwoPoint(exteriorMapLayoutPoint_b,
				exteriorMapLayoutPoint_c,
				dY,
				dX);

			vBorderLayoutCoords_c_b.push_back(SE_DPoint(dX, dY));
		}

#pragma endregion


#pragma endregion



#pragma endregion






#pragma endregion


#pragma region "如果布局点图层第一次生成，需要创建布局点图层"

		// 如果布局点图层第一次生成，需要创建布局点图层

		// 布局点图层
		OGRLayer* poLayoutPointLayer = NULL;
		//创建结果数据源
		GDALDataset* poLayoutPointDS = NULL;
		if (!bLayoutLayerPointExisted)
		{
			CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
			CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
			GDALAllRegister();

			// 获取shapefile驱动
			const char* pszShpDriverName = "ESRI Shapefile";
			GDALDriver* poShpDriver;
			poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
			if (poShpDriver == NULL)
			{
				continue;
			}

			string strExportPath = m_ImageMappingSchema.qstrMapExportPath.toLocal8Bit();

			// 结果保存路径
			string strShpFilePath = strExportPath;
			strShpFilePath += "/";
			strShpFilePath += strSheet;
#ifdef OS_FAMILY_WINDOWS
			_mkdir(strShpFilePath.c_str());
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

			// 点要素字段：
			// TYPE
			vFieldsName.push_back("TYPE");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(50);

			// 绘制标志，5万比例尺方里网刻度值在偶数时才进行绘制
			// FLAG
			vFieldsName.push_back("FLAG");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(10);

			// 数值
			vFieldsName.push_back("VALUE");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(10);


			// 创建结果图层
			OGRSpatialReference pResultSR;		// 结果图层的空间参考，默认CGCS2000坐标系

			// 地理基准CGCS2000
			pResultSR.SetWellKnownGeogCS("EPSG:4490");

			// 创建对应图幅的布局点图层，如:图幅_LayoutPoint.shp
			// 点要素图层全路径
			string strPointShpFilePath = strShpFilePath + "/" + strSheet + "_LayoutPoint.shp";
			string strCPGFilePath = strShpFilePath + "/" + strSheet + "_LayoutPoint.cpg";


			poLayoutPointDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);

			if (poLayoutPointDS == NULL)
			{
				continue;
			}



			// shp中存储属性信息和几何信息
			poLayoutPointLayer = poLayoutPointDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
			if (!poLayoutPointLayer)
			{
				continue;
			}

			// 创建结果图层属性字段
			int iResult = SetFieldDefn(poLayoutPointLayer, vFieldsName, vFieldType, vFieldWidth);
			if (iResult != 0) {
				continue;
			}

			if (!CreateShapefileCPG(strCPGFilePath, "UTF-8"))
			{
				continue;
			}

		}

#pragma endregion

#pragma region "如果布局线图层第一次生成，需要创建布局线图层"

		// 如果布局线图层第一次生成，需要创建布局线图层

		// 布局线图层
		OGRLayer* poLayoutLineLayer = NULL;
		//创建结果数据源
		GDALDataset* poLayoutLineDS = NULL;
		if (!bLayoutLayerLineExisted)
		{
			CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
			CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
			GDALAllRegister();

			// 获取shapefile驱动
			const char* pszShpDriverName = "ESRI Shapefile";
			GDALDriver* poShpDriver;
			poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
			if (poShpDriver == NULL)
			{
				continue;
			}

			string strExportPath = m_ImageMappingSchema.qstrMapExportPath.toLocal8Bit();

			// 结果保存路径
			string strShpFilePath = strExportPath;
			strShpFilePath += "/";
			strShpFilePath += strSheet;
#ifdef OS_FAMILY_WINDOWS
			_mkdir(strShpFilePath.c_str());
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

			// 线要素字段：
			// TYPE
			vFieldsName.push_back("TYPE");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(50);

			// 绘制标志
			// FLAG
			vFieldsName.push_back("FLAG");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(10);



			// 创建结果图层
			OGRSpatialReference pResultSR;		// 结果图层的空间参考，默认CGCS2000坐标系

			// 地理基准CGCS2000
			pResultSR.SetWellKnownGeogCS("EPSG:4490");

			// 创建对应图幅的布局线图层，如:图幅_LayoutLine.shp
			// 点要素图层全路径
			string strLineShpFilePath = strShpFilePath + "/" + strSheet + "_LayoutLine.shp";
			string strCPGFilePath = strShpFilePath + "/" + strSheet + "_LayoutLine.cpg";


			poLayoutLineDS = poShpDriver->Create(strLineShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);

			if (poLayoutLineDS == NULL)
			{
				continue;
			}



			// shp中存储属性信息和几何信息
			poLayoutLineLayer = poLayoutLineDS->CreateLayer(strLineShpFilePath.c_str(), &pResultSR, wkbLineString, NULL);
			if (!poLayoutLineLayer)
			{
				continue;
			}

			// 创建结果图层属性字段
			int iResult = SetFieldDefn(poLayoutLineLayer, vFieldsName, vFieldType, vFieldWidth);
			if (iResult != 0) {
				continue;
			}

			if (!CreateShapefileCPG(strCPGFilePath, "UTF-8"))
			{
				continue;
			}
		}

#pragma endregion

#pragma region "外图廓整饰点要素图层【生成地图标题（图幅号、地图名、副标题）、地图制图单位矢量点要素】"

		// add by yangzhilong @2023-05-13
		// 外图廓整饰点要素图层
		OGRLayer* poDecPointLayer = NULL;

		//创建结果数据源
		GDALDataset* poLayoutDecorationPointDS = NULL;

		// 如果不存在整饰点要素图层，则在此处创建
		if (!bLayoutDecPointExisted)
		{
			CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
			CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
			GDALAllRegister();

			// 获取shapefile驱动
			const char* pszShpDriverName = "ESRI Shapefile";
			GDALDriver* poShpDriver;
			poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
			if (poShpDriver == NULL)
			{
				continue;
			}

			string strExportPath = m_ImageMappingSchema.qstrMapExportPath.toLocal8Bit();

			// 结果保存路径
			string strShpFilePath = strExportPath;
			strShpFilePath += "/";
			strShpFilePath += strSheet;
#ifdef OS_FAMILY_WINDOWS
			_mkdir(strShpFilePath.c_str());
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

			// 点要素字段：
			// TYPE
			vFieldsName.push_back("TYPE");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(50);

			// 绘制标志，
			// FLAG
			vFieldsName.push_back("FLAG");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(10);

			// 整饰要素字符串
			vFieldsName.push_back("VALUE");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(100);


			// 创建结果图层
			OGRSpatialReference pResultSR;		// 结果图层的空间参考，默认CGCS2000坐标系

			// 地理基准CGCS2000
			pResultSR.SetWellKnownGeogCS("EPSG:4490");

			// 创建对应图幅的布局点图层，如:图幅_LayoutPoint.shp
			// 点要素图层全路径
			string strPointShpFilePath = strShpFilePath + "/" + strSheet + "_DecPoint.shp";
			string strCPGFilePath = strShpFilePath + "/" + strSheet + "_DecPoint.cpg";


			poLayoutDecorationPointDS = poShpDriver->Create(strPointShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);

			if (poLayoutDecorationPointDS == NULL)
			{
				continue;
			}

			// shp中存储属性信息和几何信息
			poDecPointLayer = poLayoutDecorationPointDS->CreateLayer(strPointShpFilePath.c_str(), &pResultSR, wkbPoint, NULL);
			if (!poDecPointLayer)
			{
				continue;
			}

			// 创建结果图层属性字段
			int iResult = SetFieldDefn(poDecPointLayer, vFieldsName, vFieldType, vFieldWidth);
			if (iResult != 0) {
				continue;
			}

			if (!CreateShapefileCPG(strCPGFilePath, "GBK"))
			{
				continue;
			}
		}

#pragma endregion

#pragma region "外图廓整饰面要素图层【生成地图说明、接图表、附注】"

		// add by yangzhilong @2023-07-26
		// 外图廓整饰面要素图层
		OGRLayer* poDecPolygonLayer = NULL;

		//创建结果数据源
		GDALDataset* poLayoutDecorationPolygonDS = NULL;

		// 如果不存在整饰面要素图层，则在此处创建
		if (!bLayoutDecPolygonExisted)
		{
			CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");	// 支持中文路径
			CPLSetConfigOption("SHAPE_ENCODING", "");			// 属性表支持中文字段
			GDALAllRegister();

			// 获取shapefile驱动
			const char* pszShpDriverName = "ESRI Shapefile";
			GDALDriver* poShpDriver;
			poShpDriver = GetGDALDriverManager()->GetDriverByName(pszShpDriverName);
			if (poShpDriver == NULL)
			{
				continue;
			}

			string strExportPath = m_ImageMappingSchema.qstrMapExportPath.toLocal8Bit();

			// 结果保存路径
			string strShpFilePath = strExportPath;
			strShpFilePath += "/";
			strShpFilePath += strSheet;
#ifdef OS_FAMILY_WINDOWS
			_mkdir(strShpFilePath.c_str());
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

			// 点要素字段：
			// TYPE
			vFieldsName.push_back("TYPE");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(50);

			// 绘制标志，
			// FLAG
			vFieldsName.push_back("FLAG");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(10);

			// 整饰要素字符串
			vFieldsName.push_back("VALUE");
			vFieldType.push_back(OFTString);
			vFieldWidth.push_back(100);


			// 创建结果图层
			OGRSpatialReference pResultSR;		// 结果图层的空间参考，默认CGCS2000坐标系

			// 地理基准CGCS2000
			pResultSR.SetWellKnownGeogCS("EPSG:4490");

			// 创建对应图幅的布局面图层，如:图幅_LayoutPolygon.shp
			// 面要素图层全路径
			string strPolygonShpFilePath = strShpFilePath + "/" + strSheet + "_DecPolygon.shp";
			string strCPGFilePath = strShpFilePath + "/" + strSheet + "_DecPolygon.cpg";


			poLayoutDecorationPolygonDS = poShpDriver->Create(strPolygonShpFilePath.c_str(), 0, 0, 0, GDT_Unknown, NULL);

			if (poLayoutDecorationPolygonDS == NULL)
			{
				continue;
			}

			// shp中存储属性信息和几何信息
			poDecPolygonLayer = poLayoutDecorationPolygonDS->CreateLayer(strPolygonShpFilePath.c_str(), &pResultSR, wkbPolygon, NULL);
			if (!poDecPolygonLayer)
			{
				continue;
			}

			// 创建结果图层属性字段
			int iResult = SetFieldDefn(poDecPolygonLayer, vFieldsName, vFieldType, vFieldWidth);
			if (iResult != 0) {
				continue;
			}

			if (!CreateShapefileCPG(strCPGFilePath, "GBK"))
			{
				continue;
			}
		}

#pragma endregion


#pragma region "绘制内图廓边框"

#pragma region "内图廓A-B"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_A.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_A.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = interiorMapLayoutPoint_B.dx;
			dLayoutPointTo.dy = interiorMapLayoutPoint_B.dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "top_interior_border";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion

#pragma region "内图廓B-C"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_B.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_B.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = interiorMapLayoutPoint_C.dx;
			dLayoutPointTo.dy = interiorMapLayoutPoint_C.dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "right_interior_border";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion

#pragma region "内图廓C-D"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_C.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_C.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = interiorMapLayoutPoint_D.dx;
			dLayoutPointTo.dy = interiorMapLayoutPoint_D.dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "bottom_interior_border";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion

#pragma region "内图廓A-D"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_A.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_A.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = interiorMapLayoutPoint_D.dx;
			dLayoutPointTo.dy = interiorMapLayoutPoint_D.dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "left_interior_border";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion

#pragma endregion

#pragma region "绘制外图廓边框"

#pragma region "外图廓a-b"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = exteriorMapLayoutPoint_a.dx;
			dLayoutPointFrom.dy = exteriorMapLayoutPoint_a.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = exteriorMapLayoutPoint_b.dx;
			dLayoutPointTo.dy = exteriorMapLayoutPoint_b.dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "top_exterior_border";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion

#pragma region "外图廓b-c"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = exteriorMapLayoutPoint_b.dx;
			dLayoutPointFrom.dy = exteriorMapLayoutPoint_b.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = exteriorMapLayoutPoint_c.dx;
			dLayoutPointTo.dy = exteriorMapLayoutPoint_c.dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "right_exterior_border";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion

#pragma region "外图廓c-d"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = exteriorMapLayoutPoint_c.dx;
			dLayoutPointFrom.dy = exteriorMapLayoutPoint_c.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = exteriorMapLayoutPoint_d.dx;
			dLayoutPointTo.dy = exteriorMapLayoutPoint_d.dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "bottom_exterior_border";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion

#pragma region "外图廓a-d"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = exteriorMapLayoutPoint_a.dx;
			dLayoutPointFrom.dy = exteriorMapLayoutPoint_a.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = exteriorMapLayoutPoint_d.dx;
			dLayoutPointTo.dy = exteriorMapLayoutPoint_d.dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "left_exterior_border";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion

#pragma endregion



		/*绘制方里网原则
		* 如果上、下边界整公里X坐标存在不同，则暂不绘制对应X坐标的方里网；
		* 如果左、右边界整公里Y坐标存在不同，则暂不绘制对应Y坐标的方里网；
		* 上、下、左、右边界方里网刻度与上、下、左、右边界线垂直
		* 对于5万比例尺，方里网刻度、邻带方里网刻度只需绘制偶数刻度值
		*/


#pragma region "绘制方里网"

		// 上边界方里网绘制索引
		int iBeginMarkIndex_X_Top = 0;
		int iEndMarkIndex_X_Top = 0;

		// 下边界方里网绘制索引
		int iBeginMarkIndex_X_Bottom = 0;
		int iEndMarkIndex_X_Bottom = 0;

		// 左边界方里网绘制索引
		int iBeginMarkIndex_Y_Left = 0;
		int iEndMarkIndex_Y_Left = 0;

		// 右边界方里网绘制索引
		int iBeginMarkIndex_Y_Right = 0;
		int iEndMarkIndex_Y_Right = 0;





		// 如果绘制方里网，并且不存在已生成的布局点、线图层
		if (m_ImageMappingSchema.layoutItemParam.bKmNetChecked
			&& !bLayoutLayerLineExisted
			&& !bLayoutLayerPointExisted)
		{
#pragma region "绘制上边界到下边界方里网及刻度线"

			/*-------------------------------------*/
			/*   绘制上边界到下边界方里网及刻度线  */
			/*-------------------------------------*/

			// X方向方里网
			// 从左往右依次绘制上边界刻度

			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_A_B.size(); iPointIndex++)
			{
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				// 内图廓上边界整公里刻度点
				SE_DPoint layoutTopBorderPoint = vBorderLayoutCoords_A_B[iPointIndex];

				// 查找当前点x值是否在内图廓下边界整刻度点中x值有相同的
				int iIndexOfX = CalIndexInVector_XCoord(layoutTopBorderPoint.dx, vBorderLayoutCoords_D_C);

				// 如果索引值不为-1，说明有相同x值的点
				if (iIndexOfX != -1)
				{
					// 类型
					FieldNameAndValue_Imp fieldImp;
					fieldImp.strFieldName = "TYPE";
					fieldImp.strFieldValue = "top_bottom_km";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "FLAG";
					fieldImp.strFieldValue = "1";
					vFieldNameAndValue.push_back(fieldImp);

					// 布局坐标转投影坐标
					// 上边界高斯坐标
					SE_DPoint dTopGaussPoint;
					double dTopValue[2] = { 0 };

					// 下边界高斯坐标
					SE_DPoint dBottomGaussPoint;
					double dBottomValue[2] = { 0 };

					TransformLayoutToGauss(interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						vBorderLayoutCoords_a_b[iPointIndex],
						dMapUnit2LayoutUnit,
						dTopGaussPoint);

					dTopValue[0] = dTopGaussPoint.dx;
					dTopValue[1] = dTopGaussPoint.dy;

					TransformLayoutToGauss(interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						vBorderLayoutCoords_d_c[iIndexOfX],
						dMapUnit2LayoutUnit,
						dBottomGaussPoint);

					dBottomValue[0] = dBottomGaussPoint.dx;
					dBottomValue[1] = dBottomGaussPoint.dy;

					// 高斯投影转地理坐标
					int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dTopValue);

					if (iRet != 1)
					{
						continue;
					}

					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dBottomValue);

					if (iRet != 1)
					{
						continue;
					}

					SE_DPoint dTopGeoPoint;
					dTopGeoPoint.dx = dTopValue[0];
					dTopGeoPoint.dy = dTopValue[1];

					SE_DPoint dBottomGeoPoint;
					dBottomGeoPoint.dx = dBottomValue[0];
					dBottomGeoPoint.dy = dBottomValue[1];

					vector<SE_DPoint> vLine;
					vLine.clear();
					vLine.push_back(dTopGeoPoint);
					vLine.push_back(dBottomGeoPoint);
					iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
					if (iRet != 0)
					{
						continue;
					}

				}
			}

			// 从左往右依次绘制下边界刻度
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



#pragma endregion

#pragma region "绘制左边界到右边界方里网及刻度线"

			/*-------------------------------------*/
			/*   绘制左边界到右边界方里网及刻度线  */
			/*-------------------------------------*/
			// 从下往上绘制左边界Y方向方里网

			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_D_A.size(); iPointIndex++)
			{
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				// 内图廓左边界整公里刻度点
				SE_DPoint layoutLeftBorderPoint = vBorderLayoutCoords_D_A[iPointIndex];

				// 查找当前点y值是否在内图廓右边界整刻度点中y值有相同的
				int iIndexOfY = CalIndexInVector_YCoord(layoutLeftBorderPoint.dy, vBorderLayoutCoords_C_B);

				// 如果索引值不为-1，说明有相同y值的点
				if (iIndexOfY != -1)
				{
					// 内图廓右边界整公里刻度点
					SE_DPoint layoutRightBorderPoint = vBorderLayoutCoords_C_B[iIndexOfY];
					QPolygonF polygon;
					// 整公里刻度线与外图廓左边界交叉点
					polygon.append(QPointF(vBorderLayoutCoords_d_a[iPointIndex].dx, vBorderLayoutCoords_d_a[iPointIndex].dy));

					// 整公里刻度线与外图廓右边界交叉点
					polygon.append(QPointF(vBorderLayoutCoords_c_b[iIndexOfY].dx, vBorderLayoutCoords_c_b[iIndexOfY].dy));


					// 类型
					FieldNameAndValue_Imp fieldImp;
					fieldImp.strFieldName = "TYPE";
					fieldImp.strFieldValue = "left_right_km";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "FLAG";
					fieldImp.strFieldValue = "1";
					vFieldNameAndValue.push_back(fieldImp);

					// 布局坐标转投影坐标
					// 左边界高斯坐标
					SE_DPoint dLeftGaussPoint;
					double dLeftValue[2] = { 0 };

					// 右边界高斯坐标
					SE_DPoint dRightGaussPoint;
					double dRightValue[2] = { 0 };

					TransformLayoutToGauss(interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						vBorderLayoutCoords_d_a[iPointIndex],
						dMapUnit2LayoutUnit,
						dLeftGaussPoint);

					dLeftValue[0] = dLeftGaussPoint.dx;
					dLeftValue[1] = dLeftGaussPoint.dy;

					TransformLayoutToGauss(interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						vBorderLayoutCoords_c_b[iIndexOfY],
						dMapUnit2LayoutUnit,
						dRightGaussPoint);

					dRightValue[0] = dRightGaussPoint.dx;
					dRightValue[1] = dRightGaussPoint.dy;

					// 高斯投影转地理坐标
					int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dLeftValue);

					if (iRet != 1)
					{
						continue;
					}

					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dRightValue);

					if (iRet != 1)
					{
						continue;
					}

					SE_DPoint dLeftGeoPoint;
					dLeftGeoPoint.dx = dLeftValue[0];
					dLeftGeoPoint.dy = dLeftValue[1];

					SE_DPoint dRightGeoPoint;
					dRightGeoPoint.dx = dRightValue[0];
					dRightGeoPoint.dy = dRightValue[1];

					vector<SE_DPoint> vLine;
					vLine.clear();
					vLine.push_back(dLeftGeoPoint);
					vLine.push_back(dRightGeoPoint);
					iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
					if (iRet != 0)
					{
						continue;
					}

				}

			}


			// 从下往上绘制右边界Y方向方里网
			QgsLayoutItemPolyline** pKmLine_Y_Right = new QgsLayoutItemPolyline * [vBorderLayoutCoords_C_B.size()];
			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_C_B.size(); iPointIndex++)
			{
				// 内图廓右边界整公里刻度点
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

#pragma region "绘制方里网刻度值"


			/*-----------------------------------------------------*/
			/*   绘制上边界、下边界、左边界、右边界方里网刻度值    */
			/*-----------------------------------------------------*/
			/*绘制刻度值规则：
			（1）内图廓最左侧方里网纵线和最右侧方里网纵线需要写带号+百公里数+十位公里数值；
			  如果纵坐标十位公里数为00，需要增加千位＋百位数值
			（2）其它方里网纵线只需写十位公里数值；
			（3）上边界参考点位置为左下角，坐标值为对应上边界整公里刻度点；
			（4）下边界参考点位置为左上角，坐标值为对应下边界整公里刻度点；
			（5）内图廓最上侧方里网横线和最下侧方里网纵线需要写千位百位+十位公里数值；
			（6）左边界参考点位置为右下角，坐标值为对应左边界整公里刻度点；
			（7）右边界参考点位置为左下角，坐标值为对应右边界整公里刻度点；
			*/


			/*上边界带号、上边界百公里数、上边界十公里数、
			下边界带号、下边界百公里数、下边界十公里数、
			左边界千百公里数、左边界十公里数、
			右边界千百公里数、右边界十公里数、
			邻带方里网上边界带号、上边界百公里数、上边界十公里数、
			邻带方里网下边界带号、下边界百公里数、下边界十公里数、
			邻带方里网左边界千百公里数、左边界十公里数、
			邻带方里网右边界千百公里数、右边界十公里数*/
			// 创建要素
#pragma region "【内图廓上边界】"

			/*	上边界方里网绘制索引
			/*记录需要绘制百公里方里网数值的第一个点和最后一个点索引值*/
			// 第一个需要绘制百公里及带号的上、下边界整公里方里网点索引
			iBeginMarkIndex_X_Top = 0;                                      // 默认索引为0
			iEndMarkIndex_X_Top = vBorderLayoutCoords_A_B.size() - 1;        // 默认索引为最后一个点
			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_A_B.size(); iPointIndex++)
			{
				SE_DPoint layoutTopBorderPoint = vBorderLayoutCoords_A_B[iPointIndex];
				if (fabs(layoutTopBorderPoint.dx - interiorMapLayoutPoint_A.dx) >= KM_LINE_TOLERANCE_X)
				{
					iBeginMarkIndex_X_Top = iPointIndex;
					break;
				}
			}

			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_A_B.size(); iPointIndex++)
			{
				SE_DPoint layoutTopBorderPoint = vBorderLayoutCoords_A_B[iPointIndex];
				if (fabs(layoutTopBorderPoint.dx - interiorMapLayoutPoint_B.dx) < KM_LINE_TOLERANCE_X)
				{
					iEndMarkIndex_X_Top = iPointIndex - 1;
					break;
				}
			}

			// 上边界高斯坐标整公里数值：vBorderGaussCoords_A_B
			for (int iPointIndex = iBeginMarkIndex_X_Top; iPointIndex <= iEndMarkIndex_X_Top; ++iPointIndex)
			{
				// 每个要素的属性信息
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				double dValues[2];
				dValues[0] = vBorderGaussCoords_A_B[iPointIndex].dx;
				dValues[1] = vBorderGaussCoords_A_B[iPointIndex].dy;

				SE_DPoint layoutTopBorderGaussPoint = vBorderGaussCoords_A_B[iPointIndex];

				// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
				int i10kmValue = int(vBorderGaussCoords_A_B[iPointIndex].dx / 1000.0);
				if (dScale == SCALE_50K
					&& iPointIndex != iBeginMarkIndex_X_Top
					&& iPointIndex != iEndMarkIndex_X_Top
					&& i10kmValue % 2 != 0)
				{
					continue;
				}

				QString qstr10km = tr("%1").arg(int(layoutTopBorderGaussPoint.dx / 1000.0));
				qstr10km = qstr10km.right(2);

				// 生成“十公里刻度值”点要素
				// 属性信息
				// 类型
				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "top_ten_km";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstr10km.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 高斯投影转地理坐标
				int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
					GaussKruger,
					dCenterMeridian,
					0,
					0,
					0,
					0,
					500000,
					0,
					1,
					dValues);

				if (iRet != 1)
				{
					continue;
				}

				iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
				if (iRet != 0)
				{
					continue;
				}

				// 绘制百公里和带号
				if (iPointIndex == iBeginMarkIndex_X_Top ||
					iPointIndex == iEndMarkIndex_X_Top)
				{
					QString qstr100km = tr("%1").arg(int(layoutTopBorderGaussPoint.dx / 100000.0));

					vFieldNameAndValue.clear();

					fieldImp.strFieldName = "TYPE";
					fieldImp.strFieldValue = "top_hundred_km";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "FLAG";
					fieldImp.strFieldValue = "1";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "VALUE";
					fieldImp.strFieldValue = qstr100km.toLocal8Bit();
					vFieldNameAndValue.push_back(fieldImp);

					iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
					if (iRet != 0)
					{
						continue;
					}

					// 带号
					int iBandNumber = CalBandNumber(dCenterMeridian, dScale);
					QString qstrBandNumber = tr("%1").arg(iBandNumber);

					vFieldNameAndValue.clear();

					fieldImp.strFieldName = "TYPE";
					fieldImp.strFieldValue = "top_band_number";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "FLAG";
					fieldImp.strFieldValue = "1";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "VALUE";
					fieldImp.strFieldValue = qstrBandNumber.toLocal8Bit();
					vFieldNameAndValue.push_back(fieldImp);

					iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
					if (iRet != 0)
					{
						continue;
					}

				}

			}

#pragma endregion

#pragma region "【内图廓下边界】"

			// 下边界方里网绘制索引
			/*记录需要绘制百公里方里网数值的第一个点和最后一个点索引值*/
			// 第一个需要绘制百公里及带号的上、下边界整公里方里网点索引
			iBeginMarkIndex_X_Bottom = 0;                                      // 默认索引为0
			iEndMarkIndex_X_Bottom = vBorderLayoutCoords_D_C.size() - 1;        // 默认索引为最后一个点
			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_D_C.size(); iPointIndex++)
			{
				SE_DPoint layoutBottomBorderPoint = vBorderLayoutCoords_D_C[iPointIndex];
				if (fabs(layoutBottomBorderPoint.dx - interiorMapLayoutPoint_D.dx) >= KM_LINE_TOLERANCE_X)
				{
					iBeginMarkIndex_X_Bottom = iPointIndex;
					break;
				}
			}

			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_D_C.size(); iPointIndex++)
			{
				SE_DPoint layoutBottomBorderPoint = vBorderLayoutCoords_D_C[iPointIndex];
				if (fabs(layoutBottomBorderPoint.dx - interiorMapLayoutPoint_C.dx) < KM_LINE_TOLERANCE_X)
				{
					iEndMarkIndex_X_Bottom = iPointIndex - 1;
					break;
				}
			}

			// 下边界高斯坐标整公里数值：vBorderGaussCoords_D_C
			for (int iPointIndex = iBeginMarkIndex_X_Bottom; iPointIndex <= iEndMarkIndex_X_Bottom; ++iPointIndex)
			{
				// 每个要素的属性信息
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				double dValues[2];
				dValues[0] = vBorderGaussCoords_D_C[iPointIndex].dx;
				dValues[1] = vBorderGaussCoords_D_C[iPointIndex].dy;

				SE_DPoint layoutBottomBorderGaussPoint = vBorderGaussCoords_D_C[iPointIndex];

				// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
				int i10kmValue = int(vBorderGaussCoords_D_C[iPointIndex].dx / 1000.0);
				if (dScale == SCALE_50K
					&& iPointIndex != iBeginMarkIndex_X_Bottom
					&& iPointIndex != iEndMarkIndex_X_Bottom
					&& i10kmValue % 2 != 0)
				{
					continue;
				}

				QString qstr10km = tr("%1").arg(int(layoutBottomBorderGaussPoint.dx / 1000.0));
				qstr10km = qstr10km.right(2);

				// 生成“十公里刻度值”点要素
				// 属性信息
				// 类型
				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "bottom_ten_km";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstr10km.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 高斯投影转地理坐标
				int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
					GaussKruger,
					dCenterMeridian,
					0,
					0,
					0,
					0,
					500000,
					0,
					1,
					dValues);

				if (iRet != 1)
				{
					continue;
				}

				iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
				if (iRet != 0)
				{
					continue;
				}

				// 绘制百公里和带号
				if (iPointIndex == iBeginMarkIndex_X_Bottom ||
					iPointIndex == iEndMarkIndex_X_Bottom)
				{
					QString qstr100km = tr("%1").arg(int(layoutBottomBorderGaussPoint.dx / 100000.0));

					vFieldNameAndValue.clear();

					fieldImp.strFieldName = "TYPE";
					fieldImp.strFieldValue = "bottom_hundred_km";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "FLAG";
					fieldImp.strFieldValue = "1";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "VALUE";
					fieldImp.strFieldValue = qstr100km.toLocal8Bit();
					vFieldNameAndValue.push_back(fieldImp);

					iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
					if (iRet != 0)
					{
						continue;
					}

					// 带号
					int iBandNumber = CalBandNumber(dCenterMeridian, dScale);
					QString qstrBandNumber = tr("%1").arg(iBandNumber);

					vFieldNameAndValue.clear();

					fieldImp.strFieldName = "TYPE";
					fieldImp.strFieldValue = "bottom_band_number";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "FLAG";
					fieldImp.strFieldValue = "1";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "VALUE";
					fieldImp.strFieldValue = qstrBandNumber.toLocal8Bit();
					vFieldNameAndValue.push_back(fieldImp);

					iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
					if (iRet != 0)
					{
						continue;
					}

				}

			}

#pragma endregion

#pragma region "【内图廓左边界】"

			/*记录需要绘制千公里+百公里方里网数值的第一个点和最后一个点索引值*/
			// 第一个需要绘制千公里+百公里的左边界整公里方里网点索引
			iBeginMarkIndex_Y_Left = 0;                                       // 默认索引为0
			iEndMarkIndex_Y_Left = vBorderLayoutCoords_D_A.size() - 1;        // 默认索引为最后一个点
			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_D_A.size(); iPointIndex++)
			{
				SE_DPoint layoutLeftBorderPoint = vBorderLayoutCoords_D_A[iPointIndex];
				if (fabs(layoutLeftBorderPoint.dy - interiorMapLayoutPoint_D.dy) >= KM_LINE_TOLERANCE_Y)
				{
					iBeginMarkIndex_Y_Left = iPointIndex;
					break;
				}
			}

			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_D_A.size(); iPointIndex++)
			{
				SE_DPoint layoutLeftBorderPoint = vBorderLayoutCoords_D_A[iPointIndex];
				if (fabs(layoutLeftBorderPoint.dy - interiorMapLayoutPoint_A.dy) < KM_LINE_TOLERANCE_Y)
				{
					iEndMarkIndex_Y_Left = iPointIndex - 1;
					break;
				}
			}

			/*绘制内图廓左边界整公里刻度值*/
			for (int iPointIndex = iBeginMarkIndex_Y_Left; iPointIndex <= iEndMarkIndex_Y_Left; iPointIndex++)
			{
				// 每个要素的属性信息
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				double dValues[2];
				dValues[0] = vBorderGaussCoords_D_A[iPointIndex].dx;
				dValues[1] = vBorderGaussCoords_D_A[iPointIndex].dy;

				SE_DPoint layoutLeftBorderGaussPoint = vBorderGaussCoords_D_A[iPointIndex];

				// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
				int i10kmValue = int(vBorderGaussCoords_D_A[iPointIndex].dy / 1000.0);
				if (dScale == SCALE_50K
					&& iPointIndex != iBeginMarkIndex_Y_Left
					&& iPointIndex != iEndMarkIndex_Y_Left
					&& i10kmValue % 2 != 0)
				{
					continue;
				}

				QString qstr10km = tr("%1").arg(int(layoutLeftBorderGaussPoint.dy / 1000.0));
				qstr10km = qstr10km.right(2);

				// 生成“十公里刻度值”点要素
				// 属性信息
				// 类型
				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "left_ten_km";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstr10km.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 高斯投影转地理坐标
				int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
					GaussKruger,
					dCenterMeridian,
					0,
					0,
					0,
					0,
					500000,
					0,
					1,
					dValues);

				if (iRet != 1)
				{
					continue;
				}

				iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
				if (iRet != 0)
				{
					continue;
				}

				// 绘制百公里
				if (iPointIndex == iBeginMarkIndex_Y_Left ||
					iPointIndex == iEndMarkIndex_Y_Left ||
					qstr10km == "00")
				{
					int i100km = abs(int(layoutLeftBorderGaussPoint.dy / 100000.0));
					// 千位补0
					QString qstr100km;
					if (i100km < 10 && i100km > 0)
					{
						qstr100km = tr("0%1").arg(int(layoutLeftBorderGaussPoint.dy / 100000.0));
					}
					else if (i100km == 0)
					{
						qstr100km = tr("00");
					}
					else if (i100km >= 10)
					{
						qstr100km = tr("%1").arg(int(layoutLeftBorderGaussPoint.dy / 100000.0));
					}

					vFieldNameAndValue.clear();

					fieldImp.strFieldName = "TYPE";
					fieldImp.strFieldValue = "left_hundred_km";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "FLAG";
					fieldImp.strFieldValue = "1";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "VALUE";
					fieldImp.strFieldValue = qstr100km.toLocal8Bit();
					vFieldNameAndValue.push_back(fieldImp);

					iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
					if (iRet != 0)
					{
						continue;
					}

				}
			}

#pragma endregion

#pragma region "【内图廓右边界】"

			// 第一个需要绘制千公里+百公里的右边界整公里方里网点索引
			iBeginMarkIndex_Y_Right = 0;                                       // 默认索引为0
			iEndMarkIndex_Y_Right = vBorderLayoutCoords_C_B.size() - 1;        // 默认索引为最后一个点
			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_C_B.size(); iPointIndex++)
			{
				SE_DPoint layoutRightBorderPoint = vBorderLayoutCoords_C_B[iPointIndex];
				if (fabs(layoutRightBorderPoint.dy - interiorMapLayoutPoint_C.dy) >= KM_LINE_TOLERANCE_Y)
				{
					iBeginMarkIndex_Y_Right = iPointIndex;
					break;
				}
			}

			for (int iPointIndex = 0; iPointIndex < vBorderLayoutCoords_C_B.size(); iPointIndex++)
			{
				SE_DPoint layoutRightBorderPoint = vBorderLayoutCoords_C_B[iPointIndex];
				if (fabs(layoutRightBorderPoint.dy - interiorMapLayoutPoint_B.dy) < KM_LINE_TOLERANCE_Y)
				{
					iEndMarkIndex_Y_Right = iPointIndex - 1;
					break;
				}
			}


			/*绘制内图廓右边界整公里刻度值*/
			for (int iPointIndex = iBeginMarkIndex_Y_Right; iPointIndex <= iEndMarkIndex_Y_Right; iPointIndex++)
			{
				// 每个要素的属性信息
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				double dValues[2];
				dValues[0] = vBorderGaussCoords_C_B[iPointIndex].dx;
				dValues[1] = vBorderGaussCoords_C_B[iPointIndex].dy;

				SE_DPoint layoutRightBorderGaussPoint = vBorderGaussCoords_C_B[iPointIndex];

				// 修改内容：如果是5万比例尺，并且公里刻度值是奇数、非首尾数值时不需要标刻度值
				int i10kmValue = int(vBorderGaussCoords_C_B[iPointIndex].dy / 1000.0);
				if (dScale == SCALE_50K
					&& iPointIndex != iBeginMarkIndex_Y_Right
					&& iPointIndex != iEndMarkIndex_Y_Right
					&& i10kmValue % 2 != 0)
				{
					continue;
				}

				QString qstr10km = tr("%1").arg(int(layoutRightBorderGaussPoint.dy / 1000.0));
				qstr10km = qstr10km.right(2);

				// 生成“十公里刻度值”点要素
				// 属性信息
				// 类型
				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "right_ten_km";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstr10km.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 高斯投影转地理坐标
				int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
					GaussKruger,
					dCenterMeridian,
					0,
					0,
					0,
					0,
					500000,
					0,
					1,
					dValues);

				if (iRet != 1)
				{
					continue;
				}

				iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
				if (iRet != 0)
				{
					continue;
				}

				// 绘制百公里
				if (iPointIndex == iBeginMarkIndex_Y_Right ||
					iPointIndex == iEndMarkIndex_Y_Right ||
					qstr10km == "00")
				{
					int i100km = abs(int(layoutRightBorderGaussPoint.dy / 100000.0));
					// 千位补0
					QString qstr100km;
					if (i100km < 10 && i100km > 0)
					{
						qstr100km = tr("0%1").arg(int(layoutRightBorderGaussPoint.dy / 100000.0));
					}
					else if (i100km == 0)
					{
						qstr100km = tr("00");
					}
					else if (i100km >= 10)
					{
						qstr100km = tr("%1").arg(int(layoutRightBorderGaussPoint.dy / 100000.0));
					}

					vFieldNameAndValue.clear();

					fieldImp.strFieldName = "TYPE";
					fieldImp.strFieldValue = "right_hundred_km";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "FLAG";
					fieldImp.strFieldValue = "1";
					vFieldNameAndValue.push_back(fieldImp);

					fieldImp.strFieldName = "VALUE";
					fieldImp.strFieldValue = qstr100km.toLocal8Bit();
					vFieldNameAndValue.push_back(fieldImp);

					iRet = Set_Point(poLayoutPointLayer, dValues[0], dValues[1], 0, vFieldNameAndValue);
					if (iRet != 0)
					{
						continue;
					}

				}
			}

#pragma endregion

#pragma endregion

#pragma region "绘制邻带方里网"

			// 判断当前图幅是否为邻带图幅，如果是邻带图幅，需要计算绘制邻带方里网
			int iAdjSheetType = bIsAdjProjectBandSheet(dScale, qstrSheet, dCenterMeridian);

			// 如果是5万比例尺
			// 投影带宽，单位：度
			double dProjectBandWidth = 0;
			// 5万比例尺，带宽6度
			if (dScale == SCALE_50K)
			{
				dProjectBandWidth = 6;
			}
			// 1万比例尺，带宽3度
			else if (dScale == SCALE_10K)
			{
				dProjectBandWidth = 3;
			}

#pragma region "计算外图廓a、b、c、d四个点的高斯坐标值"

			// 外图廓点a的高斯坐标
			SE_DPoint exteriorMapGaussPoint_a;
			TransformLayoutToGauss(interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				exteriorMapLayoutPoint_a,
				dMapUnit2LayoutUnit,
				exteriorMapGaussPoint_a);

			// 外图廓点b的高斯坐标
			SE_DPoint exteriorMapGaussPoint_b;
			TransformLayoutToGauss(interiorMapGaussPoint_B,
				interiorMapLayoutPoint_B,
				exteriorMapLayoutPoint_b,
				dMapUnit2LayoutUnit,
				exteriorMapGaussPoint_b);

			// 外图廓点c的高斯坐标
			SE_DPoint exteriorMapGaussPoint_c;
			TransformLayoutToGauss(interiorMapGaussPoint_C,
				interiorMapLayoutPoint_C,
				exteriorMapLayoutPoint_c,
				dMapUnit2LayoutUnit,
				exteriorMapGaussPoint_c);

			// 外图廓点d的高斯坐标
			SE_DPoint exteriorMapGaussPoint_d;
			TransformLayoutToGauss(interiorMapGaussPoint_D,
				interiorMapLayoutPoint_D,
				exteriorMapLayoutPoint_d,
				dMapUnit2LayoutUnit,
				exteriorMapGaussPoint_d);

#pragma endregion



			// 如果当前图幅为东侧投影边缘图幅，使用当前投影带东侧的投影带中央经线进行投影，计算邻带投影坐标
			if (iAdjSheetType == 2)
			{
				// 计算当前图幅在相邻东侧投影带的坐标
				// 图幅四个角点坐标
				double dSheetBox[8];
				// 左上角点
				dSheetBox[0] = dSheetBoxLeft;
				dSheetBox[1] = dSheetBoxTop;

				// 右上角点
				dSheetBox[2] = dSheetBoxRight;
				dSheetBox[3] = dSheetBoxTop;

				// 右下角点
				dSheetBox[4] = dSheetBoxRight;
				dSheetBox[5] = dSheetBoxBottom;

				// 左下角点
				dSheetBox[6] = dSheetBoxLeft;
				dSheetBox[7] = dSheetBoxBottom;
				iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
					GaussKruger,
					dCenterMeridian + dProjectBandWidth,
					0,
					0,
					0,
					0,
					500000,
					0,
					4,
					dSheetBox);

				if (iResult != 1)
				{
					// 记录日志
					QgsMessageLog::logMessage(tr("图幅角点坐标高斯投影变换计算失败！"), tr("影像图制图"), Qgis::Critical);
				}

				/*获取最小外接矩形*/
				// 使用邻带投影后的最小外接矩形左边界，单位：米
				double dMBR_SheetLeft = 0;

				// 使用邻带投影后的最小外接矩形右边界，单位：米
				double dMBR_SheetRight = 0;

				// 使用邻带投影后的最小外接矩形上边界，单位：米
				double dMBR_SheetTop = 0;

				// 使用邻带投影后的最小外接矩形下边界，单位：米
				double dMBR_SheetBottom = 0;

				GetMBR(4, dSheetBox, dMBR_SheetLeft, dMBR_SheetTop, dMBR_SheetRight, dMBR_SheetBottom);

				/*根据投影的最小外接矩形计算上、下、左、右边界整公里数格网坐标集合*/
				SE_DRect dSheetRange;
				dSheetRange.dleft = dMBR_SheetLeft;
				dSheetRange.dtop = dMBR_SheetTop;
				dSheetRange.dbottom = dMBR_SheetBottom;
				dSheetRange.dright = dMBR_SheetRight;

				// 邻带投影坐标值左边界整公里数坐标集合
				vector<SE_DPoint> vLeftBorderAdjGaussCoords;
				vLeftBorderAdjGaussCoords.clear();

				// 邻带投影坐标值上边界整公里数坐标集合
				vector<SE_DPoint> vTopBorderAdjGaussCoords;
				vTopBorderAdjGaussCoords.clear();

				// 邻带投影坐标值右边界整公里数坐标集合
				vector<SE_DPoint> vRightBorderAdjGaussCoords;
				vRightBorderAdjGaussCoords.clear();

				// 邻带投影坐标值下边界整公里数坐标集合
				vector<SE_DPoint> vBottomBorderAdjGaussCoords;
				vBottomBorderAdjGaussCoords.clear();

				CalGaussBorderCoords(dSheetRange,
					vLeftBorderAdjGaussCoords,
					vTopBorderAdjGaussCoords,
					vRightBorderAdjGaussCoords,
					vBottomBorderAdjGaussCoords);

				/*反算上、下、左、右邻带整公里坐标集合在右侧投影带中的地理坐标集合*/
				// 邻带投影坐标值左边界整公里数坐标对应的地理坐标集合
				vector<SE_DPoint> vLeftBorderAdjGeoCoords;
				vLeftBorderAdjGeoCoords.clear();

				// 邻带投影坐标值上边界整公里数坐标对应的地理坐标集合
				vector<SE_DPoint> vTopBorderAdjGeoCoords;
				vTopBorderAdjGeoCoords.clear();

				// 邻带投影坐标值右边界整公里数坐标对应的地理坐标集合
				vector<SE_DPoint> vRightBorderAdjGeoCoords;
				vRightBorderAdjGeoCoords.clear();

				// 邻带投影坐标值下边界整公里数坐标对应的地理坐标集合
				vector<SE_DPoint> vBottomBorderAdjGeoCoords;
				vBottomBorderAdjGeoCoords.clear();

				/*计算上边界地理坐标*/
				for (int iPointIndex = 0; iPointIndex < vTopBorderAdjGaussCoords.size(); iPointIndex++)
				{
					double dGaussPoint[2];
					dGaussPoint[0] = vTopBorderAdjGaussCoords[iPointIndex].dx;
					dGaussPoint[1] = vTopBorderAdjGaussCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian + dProjectBandWidth,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGaussPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带高斯坐标反算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGaussPoint[0];
					geoPoint.dy = dGaussPoint[1];
					vTopBorderAdjGeoCoords.push_back(geoPoint);
				}

				/*计算下边界地理坐标*/
				for (int iPointIndex = 0; iPointIndex < vBottomBorderAdjGaussCoords.size(); iPointIndex++)
				{
					double dGaussPoint[2];
					dGaussPoint[0] = vBottomBorderAdjGaussCoords[iPointIndex].dx;
					dGaussPoint[1] = vBottomBorderAdjGaussCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian + dProjectBandWidth,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGaussPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带高斯坐标反算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGaussPoint[0];
					geoPoint.dy = dGaussPoint[1];
					vBottomBorderAdjGeoCoords.push_back(geoPoint);
				}

				/*计算左边界地理坐标*/
				for (int iPointIndex = 0; iPointIndex < vLeftBorderAdjGaussCoords.size(); iPointIndex++)
				{
					double dGaussPoint[2];
					dGaussPoint[0] = vLeftBorderAdjGaussCoords[iPointIndex].dx;
					dGaussPoint[1] = vLeftBorderAdjGaussCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian + dProjectBandWidth,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGaussPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带高斯坐标反算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGaussPoint[0];
					geoPoint.dy = dGaussPoint[1];
					vLeftBorderAdjGeoCoords.push_back(geoPoint);
				}

				/*计算右边界地理坐标*/
				for (int iPointIndex = 0; iPointIndex < vRightBorderAdjGaussCoords.size(); iPointIndex++)
				{
					double dGaussPoint[2];
					dGaussPoint[0] = vRightBorderAdjGaussCoords[iPointIndex].dx;
					dGaussPoint[1] = vRightBorderAdjGaussCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian + dProjectBandWidth,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGaussPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带高斯坐标反算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGaussPoint[0];
					geoPoint.dy = dGaussPoint[1];
					vRightBorderAdjGeoCoords.push_back(geoPoint);
				}

				/*将上、下、左、右边界邻带经纬度坐标值计算在当前投影带的高斯投影坐标集合*/

				// 邻带投影坐标值左边界整公里数坐标对应的地理坐标在当前投影带的坐标集合
				vector<SE_DPoint> vLeftBorderAdjCurrentBandGaussCoords;
				vLeftBorderAdjCurrentBandGaussCoords.clear();

				// 邻带投影坐标值上边界整公里数坐标对应的地理坐标在当前投影带的坐标集合
				vector<SE_DPoint> vTopBorderAdjCurrentBandGaussCoords;
				vTopBorderAdjCurrentBandGaussCoords.clear();

				// 邻带投影坐标值右边界整公里数坐标对应的地理坐标在当前投影带的坐标集合
				vector<SE_DPoint> vRightBorderAdjCurrentBandGaussCoords;
				vRightBorderAdjCurrentBandGaussCoords.clear();

				// 邻带投影坐标值下边界整公里数坐标对应的地理坐标在当前投影带的坐标集合
				vector<SE_DPoint> vBottomBorderAdjCurrentBandGaussCoords;
				vBottomBorderAdjCurrentBandGaussCoords.clear();

				/*计算上边界在当前投影带的高斯坐标*/
				for (int iPointIndex = 0; iPointIndex < vTopBorderAdjGeoCoords.size(); iPointIndex++)
				{
					double dGeoPoint[2];
					dGeoPoint[0] = vTopBorderAdjGeoCoords[iPointIndex].dx;
					dGeoPoint[1] = vTopBorderAdjGeoCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGeoPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带地理坐标在当前投影带投影正算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGeoPoint[0];
					geoPoint.dy = dGeoPoint[1];
					vTopBorderAdjCurrentBandGaussCoords.push_back(geoPoint);
				}

				/*计算下边界在当前投影带的高斯坐标*/
				for (int iPointIndex = 0; iPointIndex < vBottomBorderAdjGeoCoords.size(); iPointIndex++)
				{
					double dGeoPoint[2];
					dGeoPoint[0] = vBottomBorderAdjGeoCoords[iPointIndex].dx;
					dGeoPoint[1] = vBottomBorderAdjGeoCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGeoPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带地理坐标在当前投影带投影正算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGeoPoint[0];
					geoPoint.dy = dGeoPoint[1];
					vBottomBorderAdjCurrentBandGaussCoords.push_back(geoPoint);
				}

				/*计算左边界在当前投影带的高斯坐标*/
				for (int iPointIndex = 0; iPointIndex < vLeftBorderAdjGeoCoords.size(); iPointIndex++)
				{
					double dGeoPoint[2];
					dGeoPoint[0] = vLeftBorderAdjGeoCoords[iPointIndex].dx;
					dGeoPoint[1] = vLeftBorderAdjGeoCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGeoPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带地理坐标在当前投影带投影正算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGeoPoint[0];
					geoPoint.dy = dGeoPoint[1];
					vLeftBorderAdjCurrentBandGaussCoords.push_back(geoPoint);
				}

				/*计算右边界在当前投影带的高斯坐标*/
				for (int iPointIndex = 0; iPointIndex < vRightBorderAdjGeoCoords.size(); iPointIndex++)
				{
					double dGeoPoint[2];
					dGeoPoint[0] = vRightBorderAdjGeoCoords[iPointIndex].dx;
					dGeoPoint[1] = vRightBorderAdjGeoCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGeoPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带地理坐标在当前投影带投影正算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGeoPoint[0];
					geoPoint.dy = dGeoPoint[1];
					vRightBorderAdjCurrentBandGaussCoords.push_back(geoPoint);
				}

				/*计算上边界邻带投影点Y坐标*/
				for (int iPointIndex = 0; iPointIndex < vTopBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					// 根据上边界邻带投影点X坐标计算与外图廓上边界a-b的交点Y坐标
					double dY = 0;
					CalCoordYByTwoPoint(exteriorMapGaussPoint_a,
						exteriorMapGaussPoint_b,
						vTopBorderAdjCurrentBandGaussCoords[iPointIndex].dx,
						dY);

					vTopBorderAdjCurrentBandGaussCoords[iPointIndex].dy = dY;
				}

				/*计算下边界邻带投影点Y坐标*/
				for (int iPointIndex = 0; iPointIndex < vBottomBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					// 根据下边界邻带投影点X坐标计算与外图廓上边界c-d的交点Y坐标
					double dY = 0;
					CalCoordYByTwoPoint(exteriorMapGaussPoint_d,
						exteriorMapGaussPoint_c,
						vBottomBorderAdjCurrentBandGaussCoords[iPointIndex].dx,
						dY);

					vBottomBorderAdjCurrentBandGaussCoords[iPointIndex].dy = dY;
				}

				/*计算左边界邻带投影点X坐标*/
				for (int iPointIndex = 0; iPointIndex < vLeftBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					double dX = 0;
					CalCoordXByTwoPoint(exteriorMapGaussPoint_d,
						exteriorMapGaussPoint_a,
						vLeftBorderAdjCurrentBandGaussCoords[iPointIndex].dy,
						dX);

					vLeftBorderAdjCurrentBandGaussCoords[iPointIndex].dx = dX;
				}

				/*将右边界邻带投影点X坐标赋值为地图投影右边界X坐标+内外图廓宽度*/
				for (int iPointIndex = 0; iPointIndex < vRightBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					double dX = 0;
					CalCoordXByTwoPoint(exteriorMapGaussPoint_c,
						exteriorMapGaussPoint_b,
						vRightBorderAdjCurrentBandGaussCoords[iPointIndex].dy,
						dX);

					vRightBorderAdjCurrentBandGaussCoords[iPointIndex].dx = dX;
				}

#pragma region "将邻带方里网上、下、左、右边界刻度值转化为布局坐标"

				// 邻带上边界整公里布局坐标
				vector<SE_DPoint> vAdjExteriorTopBorderLayoutCoords;
				vAdjExteriorTopBorderLayoutCoords.clear();
				for (int iPointIndex = 0; iPointIndex < vTopBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					SE_DPoint dPoint;
					TransformGaussToLayout(exteriorMapGaussPoint_a,
						exteriorMapLayoutPoint_a,
						vTopBorderAdjCurrentBandGaussCoords[iPointIndex],
						dMapUnit2LayoutUnit,
						dPoint);
					vAdjExteriorTopBorderLayoutCoords.push_back(dPoint);
				}


				// 邻带下边界整公里布局坐标
				vector<SE_DPoint> vAdjExteriorBottomBorderLayoutCoords;
				vAdjExteriorBottomBorderLayoutCoords.clear();
				for (int iPointIndex = 0; iPointIndex < vBottomBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					SE_DPoint dPoint;
					TransformGaussToLayout(exteriorMapGaussPoint_a,
						exteriorMapLayoutPoint_a,
						vBottomBorderAdjCurrentBandGaussCoords[iPointIndex],
						dMapUnit2LayoutUnit,
						dPoint);
					vAdjExteriorBottomBorderLayoutCoords.push_back(dPoint);
				}

				// 邻带左边界整公里布局坐标
				vector<SE_DPoint> vAdjExteriorLeftBorderLayoutCoords;
				vAdjExteriorLeftBorderLayoutCoords.clear();
				for (int iPointIndex = 0; iPointIndex < vLeftBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					SE_DPoint dPoint;
					TransformGaussToLayout(exteriorMapGaussPoint_a,
						exteriorMapLayoutPoint_a,
						vLeftBorderAdjCurrentBandGaussCoords[iPointIndex],
						dMapUnit2LayoutUnit,
						dPoint);
					vAdjExteriorLeftBorderLayoutCoords.push_back(dPoint);
				}

				// 邻带右边界整公里布局坐标
				vector<SE_DPoint> vAdjExteriorRightBorderLayoutCoords;
				vAdjExteriorRightBorderLayoutCoords.clear();
				for (int iPointIndex = 0; iPointIndex < vRightBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					SE_DPoint dPoint;
					TransformGaussToLayout(exteriorMapGaussPoint_a,
						exteriorMapLayoutPoint_a,
						vRightBorderAdjCurrentBandGaussCoords[iPointIndex],
						dMapUnit2LayoutUnit,
						dPoint);
					vAdjExteriorRightBorderLayoutCoords.push_back(dPoint);
				}

				m_dInteriorMapLayoutPoint_A = interiorMapLayoutPoint_A;
				m_dInteriorMapLayoutPoint_B = interiorMapLayoutPoint_B;
				m_dInteriorMapLayoutPoint_C = interiorMapLayoutPoint_C;
				m_dInteriorMapLayoutPoint_D = interiorMapLayoutPoint_D;

				m_dExteriorMapLayoutPoint_a = exteriorMapLayoutPoint_a;
				m_dExteriorMapLayoutPoint_b = exteriorMapLayoutPoint_b;
				m_dExteriorMapLayoutPoint_c = exteriorMapLayoutPoint_c;
				m_dExteriorMapLayoutPoint_d = exteriorMapLayoutPoint_d;

				/*绘制邻带方里网级刻度*/

				DrawAdjKmNet_CreateLayer(poLayoutPointLayer,
					poLayoutLineLayer,
					exteriorMapGaussPoint_a,
					exteriorMapLayoutPoint_a,
					vAdjExteriorLeftBorderLayoutCoords,
					vAdjExteriorTopBorderLayoutCoords,
					vAdjExteriorRightBorderLayoutCoords,
					vAdjExteriorBottomBorderLayoutCoords,
					vLeftBorderAdjGaussCoords,
					vTopBorderAdjGaussCoords,
					vRightBorderAdjGaussCoords,
					vBottomBorderAdjGaussCoords,
					dCenterMeridian,
					dScale,
					dMapUnit2LayoutUnit);

#pragma endregion
			}

			// 如果当前图幅为西侧投影边缘图幅，使用当前投影带西侧的投影带中央经线进行投影，计算邻带投影坐标
			else if (iAdjSheetType == 1)
			{
				// 计算当前图幅在相邻西侧投影带的坐标
				// 图幅四个角点坐标
				double dSheetBox[8];
				// 左上角点
				dSheetBox[0] = dSheetBoxLeft;
				dSheetBox[1] = dSheetBoxTop;

				// 右上角点
				dSheetBox[2] = dSheetBoxRight;
				dSheetBox[3] = dSheetBoxTop;

				// 右下角点
				dSheetBox[4] = dSheetBoxRight;
				dSheetBox[5] = dSheetBoxBottom;

				// 左下角点
				dSheetBox[6] = dSheetBoxLeft;
				dSheetBox[7] = dSheetBoxBottom;
				iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
					GaussKruger,
					dCenterMeridian - dProjectBandWidth,
					0,
					0,
					0,
					0,
					500000,
					0,
					4,
					dSheetBox);

				if (iResult != 1)
				{
					// 记录日志
					QgsMessageLog::logMessage(tr("图幅角点坐标高斯投影变换计算失败！"), tr("影像图制图"), Qgis::Critical);
				}

				/*获取最小外接矩形*/
				// 使用邻带投影后的最小外接矩形左边界，单位：米
				double dMBR_SheetLeft = 0;

				// 使用邻带投影后的最小外接矩形右边界，单位：米
				double dMBR_SheetRight = 0;

				// 使用邻带投影后的最小外接矩形上边界，单位：米
				double dMBR_SheetTop = 0;

				// 使用邻带投影后的最小外接矩形下边界，单位：米
				double dMBR_SheetBottom = 0;

				GetMBR(4, dSheetBox, dMBR_SheetLeft, dMBR_SheetTop, dMBR_SheetRight, dMBR_SheetBottom);

				/*根据投影的最小外接矩形计算上、下、左、右边界整公里数格网坐标集合*/
				SE_DRect dSheetRange;
				dSheetRange.dleft = dMBR_SheetLeft;
				dSheetRange.dtop = dMBR_SheetTop;
				dSheetRange.dbottom = dMBR_SheetBottom;
				dSheetRange.dright = dMBR_SheetRight;

				// 邻带投影坐标值左边界整公里数坐标集合
				vector<SE_DPoint> vLeftBorderAdjGaussCoords;

				// 邻带投影坐标值上边界整公里数坐标集合
				vector<SE_DPoint> vTopBorderAdjGaussCoords;

				// 邻带投影坐标值右边界整公里数坐标集合
				vector<SE_DPoint> vRightBorderAdjGaussCoords;

				// 邻带投影坐标值下边界整公里数坐标集合
				vector<SE_DPoint> vBottomBorderAdjGaussCoords;

				CalGaussBorderCoords(dSheetRange,
					vLeftBorderAdjGaussCoords,
					vTopBorderAdjGaussCoords,
					vRightBorderAdjGaussCoords,
					vBottomBorderAdjGaussCoords);

				/*反算上、下、左、右邻带整公里坐标集合在右侧投影带中的地理坐标集合*/
				// 邻带投影坐标值左边界整公里数坐标对应的地理坐标集合
				vector<SE_DPoint> vLeftBorderAdjGeoCoords;
				vLeftBorderAdjGeoCoords.clear();

				// 邻带投影坐标值上边界整公里数坐标对应的地理坐标集合
				vector<SE_DPoint> vTopBorderAdjGeoCoords;
				vTopBorderAdjGeoCoords.clear();

				// 邻带投影坐标值右边界整公里数坐标对应的地理坐标集合
				vector<SE_DPoint> vRightBorderAdjGeoCoords;
				vRightBorderAdjGeoCoords.clear();

				// 邻带投影坐标值下边界整公里数坐标对应的地理坐标集合
				vector<SE_DPoint> vBottomBorderAdjGeoCoords;
				vBottomBorderAdjGeoCoords.clear();

				/*计算上边界地理坐标*/
				for (int iPointIndex = 0; iPointIndex < vTopBorderAdjGaussCoords.size(); iPointIndex++)
				{
					double dGaussPoint[2];
					dGaussPoint[0] = vTopBorderAdjGaussCoords[iPointIndex].dx;
					dGaussPoint[1] = vTopBorderAdjGaussCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian - dProjectBandWidth,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGaussPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带高斯坐标反算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGaussPoint[0];
					geoPoint.dy = dGaussPoint[1];
					vTopBorderAdjGeoCoords.push_back(geoPoint);
				}

				/*计算下边界地理坐标*/
				for (int iPointIndex = 0; iPointIndex < vBottomBorderAdjGaussCoords.size(); iPointIndex++)
				{
					double dGaussPoint[2];
					dGaussPoint[0] = vBottomBorderAdjGaussCoords[iPointIndex].dx;
					dGaussPoint[1] = vBottomBorderAdjGaussCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian - dProjectBandWidth,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGaussPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带高斯坐标反算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGaussPoint[0];
					geoPoint.dy = dGaussPoint[1];
					vBottomBorderAdjGeoCoords.push_back(geoPoint);
				}

				/*计算左边界地理坐标*/
				for (int iPointIndex = 0; iPointIndex < vLeftBorderAdjGaussCoords.size(); iPointIndex++)
				{
					double dGaussPoint[2];
					dGaussPoint[0] = vLeftBorderAdjGaussCoords[iPointIndex].dx;
					dGaussPoint[1] = vLeftBorderAdjGaussCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian - dProjectBandWidth,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGaussPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带高斯坐标反算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGaussPoint[0];
					geoPoint.dy = dGaussPoint[1];
					vLeftBorderAdjGeoCoords.push_back(geoPoint);
				}

				/*计算右边界地理坐标*/
				for (int iPointIndex = 0; iPointIndex < vRightBorderAdjGaussCoords.size(); iPointIndex++)
				{
					double dGaussPoint[2];
					dGaussPoint[0] = vRightBorderAdjGaussCoords[iPointIndex].dx;
					dGaussPoint[1] = vRightBorderAdjGaussCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian - dProjectBandWidth,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGaussPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带高斯坐标反算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGaussPoint[0];
					geoPoint.dy = dGaussPoint[1];
					vRightBorderAdjGeoCoords.push_back(geoPoint);
				}

				/*将上、下、左、右边界邻带经纬度坐标值计算在当前投影带的高斯投影坐标集合*/

				// 邻带投影坐标值左边界整公里数坐标对应的地理坐标在当前投影带的坐标集合
				vector<SE_DPoint> vLeftBorderAdjCurrentBandGaussCoords;
				vLeftBorderAdjCurrentBandGaussCoords.clear();

				// 邻带投影坐标值上边界整公里数坐标对应的地理坐标在当前投影带的坐标集合
				vector<SE_DPoint> vTopBorderAdjCurrentBandGaussCoords;
				vTopBorderAdjCurrentBandGaussCoords.clear();

				// 邻带投影坐标值右边界整公里数坐标对应的地理坐标在当前投影带的坐标集合
				vector<SE_DPoint> vRightBorderAdjCurrentBandGaussCoords;
				vRightBorderAdjCurrentBandGaussCoords.clear();

				// 邻带投影坐标值下边界整公里数坐标对应的地理坐标在当前投影带的坐标集合
				vector<SE_DPoint> vBottomBorderAdjCurrentBandGaussCoords;
				vBottomBorderAdjCurrentBandGaussCoords.clear();

				/*计算上边界在当前投影带的高斯坐标*/
				for (int iPointIndex = 0; iPointIndex < vTopBorderAdjGeoCoords.size(); iPointIndex++)
				{
					double dGeoPoint[2];
					dGeoPoint[0] = vTopBorderAdjGeoCoords[iPointIndex].dx;
					dGeoPoint[1] = vTopBorderAdjGeoCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGeoPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带地理坐标在当前投影带投影正算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGeoPoint[0];
					geoPoint.dy = dGeoPoint[1];
					vTopBorderAdjCurrentBandGaussCoords.push_back(geoPoint);
				}

				/*计算下边界在当前投影带的高斯坐标*/
				for (int iPointIndex = 0; iPointIndex < vBottomBorderAdjGeoCoords.size(); iPointIndex++)
				{
					double dGeoPoint[2];
					dGeoPoint[0] = vBottomBorderAdjGeoCoords[iPointIndex].dx;
					dGeoPoint[1] = vBottomBorderAdjGeoCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGeoPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带地理坐标在当前投影带投影正算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGeoPoint[0];
					geoPoint.dy = dGeoPoint[1];
					vBottomBorderAdjCurrentBandGaussCoords.push_back(geoPoint);
				}

				/*计算左边界在当前投影带的高斯坐标*/
				for (int iPointIndex = 0; iPointIndex < vLeftBorderAdjGeoCoords.size(); iPointIndex++)
				{
					double dGeoPoint[2];
					dGeoPoint[0] = vLeftBorderAdjGeoCoords[iPointIndex].dx;
					dGeoPoint[1] = vLeftBorderAdjGeoCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGeoPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带地理坐标在当前投影带投影正算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGeoPoint[0];
					geoPoint.dy = dGeoPoint[1];
					vLeftBorderAdjCurrentBandGaussCoords.push_back(geoPoint);
				}

				/*计算右边界在当前投影带的高斯坐标*/
				for (int iPointIndex = 0; iPointIndex < vRightBorderAdjGeoCoords.size(); iPointIndex++)
				{
					double dGeoPoint[2];
					dGeoPoint[0] = vRightBorderAdjGeoCoords[iPointIndex].dx;
					dGeoPoint[1] = vRightBorderAdjGeoCoords[iPointIndex].dy;

					iResult = CSE_GeoTransformation::Geo2Proj_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
						0,
						1,
						dGeoPoint);

					if (iResult != 1)
					{
						// 记录日志
						QgsMessageLog::logMessage(tr("邻带地理坐标在当前投影带投影正算计算失败！"), tr("影像图制图"), Qgis::Critical);
					}
					SE_DPoint geoPoint;
					geoPoint.dx = dGeoPoint[0];
					geoPoint.dy = dGeoPoint[1];
					vRightBorderAdjCurrentBandGaussCoords.push_back(geoPoint);
				}

				/*计算上边界邻带投影点Y坐标*/
				for (int iPointIndex = 0; iPointIndex < vTopBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					// 根据上边界邻带投影点X坐标计算与外图廓上边界a-b的交点Y坐标
					double dY = 0;
					CalCoordYByTwoPoint(exteriorMapGaussPoint_a,
						exteriorMapGaussPoint_b,
						vTopBorderAdjCurrentBandGaussCoords[iPointIndex].dx,
						dY);

					vTopBorderAdjCurrentBandGaussCoords[iPointIndex].dy = dY;
				}

				/*计算下边界邻带投影点Y坐标*/
				for (int iPointIndex = 0; iPointIndex < vBottomBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					// 根据下边界邻带投影点X坐标计算与外图廓上边界c-d的交点Y坐标
					double dY = 0;
					CalCoordYByTwoPoint(exteriorMapGaussPoint_d,
						exteriorMapGaussPoint_c,
						vBottomBorderAdjCurrentBandGaussCoords[iPointIndex].dx,
						dY);

					vBottomBorderAdjCurrentBandGaussCoords[iPointIndex].dy = dY;
				}

				/*计算左边界邻带投影点X坐标*/
				for (int iPointIndex = 0; iPointIndex < vLeftBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					double dX = 0;
					CalCoordXByTwoPoint(exteriorMapGaussPoint_d,
						exteriorMapGaussPoint_a,
						vLeftBorderAdjCurrentBandGaussCoords[iPointIndex].dy,
						dX);

					vLeftBorderAdjCurrentBandGaussCoords[iPointIndex].dx = dX;
				}

				/*将右边界邻带投影点X坐标赋值为地图投影右边界X坐标+内外图廓宽度*/
				for (int iPointIndex = 0; iPointIndex < vRightBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					double dX = 0;
					CalCoordXByTwoPoint(exteriorMapGaussPoint_c,
						exteriorMapGaussPoint_b,
						vRightBorderAdjCurrentBandGaussCoords[iPointIndex].dy,
						dX);

					vRightBorderAdjCurrentBandGaussCoords[iPointIndex].dx = dX;
				}

#pragma region "将邻带方里网上、下、左、右边界刻度值转化为布局坐标"

				// 邻带上边界整公里布局坐标
				vector<SE_DPoint> vAdjExteriorTopBorderLayoutCoords;
				vAdjExteriorTopBorderLayoutCoords.clear();
				for (int iPointIndex = 0; iPointIndex < vTopBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					SE_DPoint dPoint;
					TransformGaussToLayout(exteriorMapGaussPoint_a,
						exteriorMapLayoutPoint_a,
						vTopBorderAdjCurrentBandGaussCoords[iPointIndex],
						dMapUnit2LayoutUnit,
						dPoint);
					vAdjExteriorTopBorderLayoutCoords.push_back(dPoint);
				}


				// 邻带下边界整公里布局坐标
				vector<SE_DPoint> vAdjExteriorBottomBorderLayoutCoords;
				vAdjExteriorBottomBorderLayoutCoords.clear();
				for (int iPointIndex = 0; iPointIndex < vBottomBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					SE_DPoint dPoint;
					TransformGaussToLayout(exteriorMapGaussPoint_a,
						exteriorMapLayoutPoint_a,
						vBottomBorderAdjCurrentBandGaussCoords[iPointIndex],
						dMapUnit2LayoutUnit,
						dPoint);
					vAdjExteriorBottomBorderLayoutCoords.push_back(dPoint);
				}

				// 邻带左边界整公里布局坐标
				vector<SE_DPoint> vAdjExteriorLeftBorderLayoutCoords;
				vAdjExteriorLeftBorderLayoutCoords.clear();
				for (int iPointIndex = 0; iPointIndex < vLeftBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					SE_DPoint dPoint;
					TransformGaussToLayout(exteriorMapGaussPoint_a,
						exteriorMapLayoutPoint_a,
						vLeftBorderAdjCurrentBandGaussCoords[iPointIndex],
						dMapUnit2LayoutUnit,
						dPoint);
					vAdjExteriorLeftBorderLayoutCoords.push_back(dPoint);
				}

				// 邻带右边界整公里布局坐标
				vector<SE_DPoint> vAdjExteriorRightBorderLayoutCoords;
				vAdjExteriorRightBorderLayoutCoords.clear();
				for (int iPointIndex = 0; iPointIndex < vRightBorderAdjCurrentBandGaussCoords.size(); iPointIndex++)
				{
					SE_DPoint dPoint;
					TransformGaussToLayout(exteriorMapGaussPoint_a,
						exteriorMapLayoutPoint_a,
						vRightBorderAdjCurrentBandGaussCoords[iPointIndex],
						dMapUnit2LayoutUnit,
						dPoint);
					vAdjExteriorRightBorderLayoutCoords.push_back(dPoint);
				}

				m_dInteriorMapLayoutPoint_A = interiorMapLayoutPoint_A;
				m_dInteriorMapLayoutPoint_B = interiorMapLayoutPoint_B;
				m_dInteriorMapLayoutPoint_C = interiorMapLayoutPoint_C;
				m_dInteriorMapLayoutPoint_D = interiorMapLayoutPoint_D;

				m_dExteriorMapLayoutPoint_a = exteriorMapLayoutPoint_a;
				m_dExteriorMapLayoutPoint_b = exteriorMapLayoutPoint_b;
				m_dExteriorMapLayoutPoint_c = exteriorMapLayoutPoint_c;
				m_dExteriorMapLayoutPoint_d = exteriorMapLayoutPoint_d;

				/*绘制邻带方里网及刻度*/

				DrawAdjKmNet_CreateLayer(poLayoutPointLayer,
					poLayoutLineLayer,
					exteriorMapGaussPoint_a,
					exteriorMapLayoutPoint_a,
					vAdjExteriorLeftBorderLayoutCoords,
					vAdjExteriorTopBorderLayoutCoords,
					vAdjExteriorRightBorderLayoutCoords,
					vAdjExteriorBottomBorderLayoutCoords,
					vLeftBorderAdjGaussCoords,
					vTopBorderAdjGaussCoords,
					vRightBorderAdjGaussCoords,
					vBottomBorderAdjGaussCoords,
					dCenterMeridian,
					dScale,
					dMapUnit2LayoutUnit);

#pragma endregion
			}

#pragma endregion

		}

#pragma endregion


#pragma region "图外整饰要素调整位置"

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

#pragma region "获取内图廓（矩形区域）最小外接矩形"

		// 获取内图廓（矩形区域）最小外接矩形
		SE_DRect dInteriorMapLayoutMBR;
		double dInteriorMapLayoutMBRPoints[8] = { 0 };
		// 依次存储内图廓A、B、C、D布局坐标
		dInteriorMapLayoutMBRPoints[0] = interiorMapLayoutPoint_A.dx;
		dInteriorMapLayoutMBRPoints[1] = interiorMapLayoutPoint_A.dy;

		dInteriorMapLayoutMBRPoints[2] = interiorMapLayoutPoint_B.dx;
		dInteriorMapLayoutMBRPoints[3] = interiorMapLayoutPoint_B.dy;

		dInteriorMapLayoutMBRPoints[4] = interiorMapLayoutPoint_C.dx;
		dInteriorMapLayoutMBRPoints[5] = interiorMapLayoutPoint_C.dy;

		dInteriorMapLayoutMBRPoints[6] = interiorMapLayoutPoint_D.dx;
		dInteriorMapLayoutMBRPoints[7] = interiorMapLayoutPoint_D.dy;

		GetMBR(4,
			dInteriorMapLayoutMBRPoints,
			dInteriorMapLayoutMBR.dleft,
			dInteriorMapLayoutMBR.dtop,
			dInteriorMapLayoutMBR.dright,
			dInteriorMapLayoutMBR.dbottom);

		m_dInteriorMapLayoutMBR = dInteriorMapLayoutMBR;

		// 外图廓左上角布局坐标
		m_dInteriorLeftTopLayoutPoint.dx = dInteriorMapLayoutMBR.dleft;
		m_dInteriorLeftTopLayoutPoint.dy = dInteriorMapLayoutMBR.dbottom;

		// 外图廓左下角布局坐标
		m_dInteriorLeftBottomLayoutPoint.dx = dInteriorMapLayoutMBR.dleft;
		m_dInteriorLeftBottomLayoutPoint.dy = dInteriorMapLayoutMBR.dtop;

		// 外图廓右上角布局坐标
		m_dInteriorRightTopLayoutPoint.dx = dInteriorMapLayoutMBR.dright;
		m_dInteriorRightTopLayoutPoint.dy = dInteriorMapLayoutMBR.dbottom;

		// 外图廓右下角布局坐标
		m_dInteriorRightBottomLayoutPoint.dx = dInteriorMapLayoutMBR.dright;
		m_dInteriorRightBottomLayoutPoint.dy = dInteriorMapLayoutMBR.dtop;

#pragma endregion



#pragma region "【1】更新地图标题-图名位置"

		// 获取地图标题-图幅名称
		QgsLayoutItemLabel* pTextItem_MapTitleName = (QgsLayoutItemLabel*)GetLayoutItemByName(
			qLayoutItemList,
			"TextItem_MapTitleName");

		// 从SMS文件中读取“图名”
		QString qstrMapTitleName = QString::fromLocal8Bit(strMapName.c_str());
		((QgsLayoutItemLabel*)pTextItem_MapTitleName)->setText(qstrMapTitleName);

		// 获取位置配置信息
		LayoutItemPositions textItem_MapTitleName;
		GetLayoutItemPositionByName("TextItem_MapTitleName", vLayoutItemPositions, textItem_MapTitleName);

		// 设置图幅名称参考点位置
		QgsLayoutItem::ReferencePoint mapTitleNameRefPoint = GetReferencePoint(textItem_MapTitleName.qstrReferencePoint);
		pTextItem_MapTitleName->setReferencePoint(mapTitleNameRefPoint);

		// 设置参考点坐标
		pTextItem_MapTitleName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2,
			m_dExteriorLeftTopLayoutPoint.dy + textItem_MapTitleName.dTop_offset));

		// 自动调整尺寸
		pTextItem_MapTitleName->adjustSizeToText();



#pragma region "创建地图标题-图名要素"

		// 创建图名要素，同时设置模板中图名要素不显示
		pTextItem_MapTitleName->setVisibility(false);

		// 如果不存在，需要创建要素
		if (!bLayoutDecPointExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPoint;
			dLayoutPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2;
			dLayoutPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + textItem_MapTitleName.dTop_offset;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "map_name";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = qstrMapTitleName.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dGaussPoint;
			double dValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPoint,
				dMapUnit2LayoutUnit,
				dGaussPoint);

			dValue[0] = dGaussPoint.dx;
			dValue[1] = dGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dGeoPoint;
			dGeoPoint.dx = dValue[0];
			dGeoPoint.dy = dValue[1];

			iRet = Set_Point(poDecPointLayer, dGeoPoint.dx, dGeoPoint.dy, 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion


#pragma endregion

#pragma region "【2】更新地图标题-图幅号位置"

		// 获取地图标题-图幅号元素
		QgsLayoutItemLabel* pTextItem_MapTitleSheet = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
			"TextItem_MapTitleSheet");

		// 设置图幅号
		pTextItem_MapTitleSheet->setText(qstrSheet);

		// 获取位置配置信息
		LayoutItemPositions textItem_MapTitleSheet;
		GetLayoutItemPositionByName("TextItem_MapTitleSheet", vLayoutItemPositions, textItem_MapTitleSheet);

		// 设置图幅号参考点位置
		QgsLayoutItem::ReferencePoint mapTitleSheetRefPoint = GetReferencePoint(textItem_MapTitleSheet.qstrReferencePoint);
		pTextItem_MapTitleSheet->setReferencePoint(mapTitleSheetRefPoint);

		// 设置参考点坐标
		pTextItem_MapTitleSheet->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2,
			m_dExteriorLeftTopLayoutPoint.dy + textItem_MapTitleSheet.dTop_offset));

		// 自动调整尺寸
		pTextItem_MapTitleSheet->adjustSizeToText();

#pragma region "创建地图标题-图幅号"

		// 创建图幅号要素，同时设置模板中图幅号要素不显示
		pTextItem_MapTitleSheet->setVisibility(false);

		// 如果不存在，需要创建要素
		if (!bLayoutDecPointExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPoint;
			dLayoutPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2;
			dLayoutPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + textItem_MapTitleSheet.dTop_offset;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "map_sheet";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = qstrSheet.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dGaussPoint;
			double dValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPoint,
				dMapUnit2LayoutUnit,
				dGaussPoint);

			dValue[0] = dGaussPoint.dx;
			dValue[1] = dGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dGeoPoint;
			dGeoPoint.dx = dValue[0];
			dGeoPoint.dy = dValue[1];

			iRet = Set_Point(poDecPointLayer, dGeoPoint.dx, dGeoPoint.dy, 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion


#pragma endregion

#pragma region "【3】更新地图副标题-政区说明"

		// 获取地图标题-政区说明
		QgsLayoutItemLabel* pTextItem_MapSubTileProvince = (QgsLayoutItemLabel*)GetLayoutItemByName(
			qLayoutItemList,
			"TextItem_MapSubTileProvince");

		// 从SMS文件中读取“政区说明”
		QString qstrMapSubTitleProvince = QString::fromLocal8Bit(strMapSubTitleProvince.c_str());
		pTextItem_MapSubTileProvince->setText(qstrMapSubTitleProvince);
		LayoutItemPositions textItem_MapSubTileProvince;
		// 时间：2023-02-23
		// 修改内容：调整1:1万模板，政区说明与地图名称位于同一行，并置于地图名称后
		if (dScale == SCALE_10K)
		{
			// 左下角点，左下角点（横坐标：地图图名元素的右边界+5毫米，纵坐标与地图名称纵坐标保持一致）
			pTextItem_MapSubTileProvince->setReferencePoint(QgsLayoutItem::LowerLeft);

			// 设置参考点坐标
			pTextItem_MapSubTileProvince->attemptMove(
				QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2 + pTextItem_MapTitleName->rect().width() + 5,
					m_dExteriorLeftTopLayoutPoint.dy + textItem_MapTitleName.dTop_offset));

			// 自动调整尺寸
			pTextItem_MapSubTileProvince->adjustSizeToText();
		}
		// 5万比例尺
		else if (dScale == SCALE_50K)
		{
			// 获取位置配置信息

			GetLayoutItemPositionByName("TextItem_MapSubTileProvince", vLayoutItemPositions, textItem_MapSubTileProvince);

			// 设置参考点位置
			QgsLayoutItem::ReferencePoint mapSubTileProvinceRefPoint = GetReferencePoint(textItem_MapSubTileProvince.qstrReferencePoint);
			pTextItem_MapSubTileProvince->setReferencePoint(mapSubTileProvinceRefPoint);

			// 设置参考点坐标
			pTextItem_MapSubTileProvince->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2,
				m_dExteriorLeftTopLayoutPoint.dy + textItem_MapSubTileProvince.dTop_offset));

			// 自动调整尺寸
			pTextItem_MapSubTileProvince->adjustSizeToText();
		}
		// 其它比例尺暂不支持
		else
		{
			// 记录日志
			QgsMessageLog::logMessage(tr("系统暂不支持1比1万和1比5万之外的比例尺！"), tr("影像图制图"), Qgis::Critical);
			return;
		}


#pragma region "创建地图副标题-政区说明"

		// 创建政区说明要素，同时设置模板中副标题要素不显示
		pTextItem_MapSubTileProvince->setVisibility(false);

		if (!bLayoutDecPointExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPoint;

			// 1万比例尺
			if (dScale == SCALE_10K)
			{
				dLayoutPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2 + pTextItem_MapTitleName->rect().width() + 5;
				dLayoutPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + textItem_MapTitleName.dTop_offset;

			}
			// 5万比例尺
			else if (dScale == SCALE_50K)
			{
				dLayoutPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2;
				dLayoutPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + textItem_MapSubTileProvince.dTop_offset;

			}

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "map_subtitle";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = qstrMapSubTitleProvince.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dGaussPoint;
			double dValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPoint,
				dMapUnit2LayoutUnit,
				dGaussPoint);

			dValue[0] = dGaussPoint.dx;
			dValue[1] = dGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dGeoPoint;
			dGeoPoint.dx = dValue[0];
			dGeoPoint.dy = dValue[1];

			iRet = Set_Point(poDecPointLayer, dGeoPoint.dx, dGeoPoint.dy, 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion


#pragma endregion

#pragma region "【4】更新左上角、右下角图幅名位置"

		m_ImageMappingSchema.layoutItemParam.qstrLeftTopCornerSheet = qstrSheet;
		m_ImageMappingSchema.layoutItemParam.qstrRightBottomCornerSheet = qstrSheet;

#pragma region "左上角图幅名"

		QgsLayoutItemLabel* pTextItem_LeftTopSheet = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopSheet");

		// 如果是绘制状态
		if (m_ImageMappingSchema.layoutItemParam.bLeftTopCornerSheetChecked)
		{
			pTextItem_LeftTopSheet->setText(qstrSheet);
			pTextItem_LeftTopSheet->setVisibility(true);

			// 调整尺寸
			pTextItem_LeftTopSheet->attemptSetSceneRect(QRectF(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + 50,
				m_dExteriorLeftTopLayoutPoint.dy - 5,
				50,
				10));
		}
		else
		{
			pTextItem_LeftTopSheet->setVisibility(false);
		}

#pragma endregion

#pragma region "右下角图幅名"

		QgsLayoutItemLabel* pTextItem_RightBottomSheet = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomSheet");

		// 如果是绘制状态
		if (m_ImageMappingSchema.layoutItemParam.bRightBottomCornerSheetChecked)
		{
			pTextItem_RightBottomSheet->setText(qstrSheet);
			pTextItem_RightBottomSheet->setVisibility(true);
			// 设置位置
			pTextItem_RightBottomSheet->setReferencePoint(QgsLayoutItem::LowerRight);

			// 设置参考点坐标
			pTextItem_RightBottomSheet->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dInteriorMapLayoutMBR.GetWidth() + OUTER_MAP_INTERIOR_MAP_DISTANCE + 49,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 30));

			// 自动调整尺寸
			pTextItem_RightBottomSheet->adjustSizeToText();

			// 设置水平对齐方式-右对齐
			pTextItem_RightBottomSheet->setHAlign(Qt::AlignLeft);

		}
		else
		{
			pTextItem_RightBottomSheet->setVisibility(false);
		}

#pragma endregion


#pragma endregion

#pragma region "【5】更新密级位置"

		// 密级
		QgsLayoutItemLabel* pTextItem_SecurityClassification = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
			"TextItem_SecurityClassification");

		pTextItem_SecurityClassification->setText(m_ImageMappingSchema.layoutItemParam.qstrSecurityClassification);

		// 设置参考点位置
		pTextItem_SecurityClassification->setReferencePoint(QgsLayoutItem::LowerRight);

		// 设置参考点坐标
		pTextItem_SecurityClassification->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth(),
			m_dExteriorLeftTopLayoutPoint.dy - 3));

		// 自动调整尺寸
		pTextItem_SecurityClassification->adjustSizeToText();
#pragma endregion

#pragma region "【6】图例标题"

		if (m_ImageMappingSchema.layoutItemParam.bMainMapLegendChecked)
		{
			// 图例标题
			QgsLayoutItemLabel* pTextItem_MainMapLegend = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_MainMapLegend");

			// 设置参考点位置
			pTextItem_MainMapLegend->setReferencePoint(QgsLayoutItem::UpperLeft);

			// 设置参考点坐标：规范是3毫米，为了避免与右侧邻带方里网绘制冲突，往右加3毫米
			pTextItem_MainMapLegend->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() + 6,
				m_dExteriorLeftTopLayoutPoint.dy));

			// 自动调整尺寸
			pTextItem_MainMapLegend->adjustSizeToText();
		}


#pragma endregion

#pragma region "【7】三北方向线"

		// 根据磁偏角、坐标纵线偏角、磁坐偏角数值选取不同的三北方向线样式
		// 共4种样式

		// 如果绘制三北方向线
		if (m_ImageMappingSchema.layoutItemParam.bNorthDirectionLinesChecked)
		{
#pragma region "计算坐标纵线偏角数值"

			// 将整数的坐标纵线偏角换算成浮点数
			QString qstrMeridianCoordAngle = QString::fromLocal8Bit(strZuoBiaoZongXianPianJiao.c_str());
			int iMeridianCoordDegreeAngle = 0;		// 度
			int iMeridianCoordMinuteAngle = 0;		// 分
			// 浮点数度
			double dMeridianCoordAngle = 0;
			if (qstrMeridianCoordAngle.toInt() < 0)
			{
				iMeridianCoordDegreeAngle = qstrMeridianCoordAngle.left(3).toInt();
				iMeridianCoordMinuteAngle = qstrMeridianCoordAngle.right(2).toInt();
				// 浮点数度
				dMeridianCoordAngle = -(fabs(iMeridianCoordDegreeAngle) + (double)iMeridianCoordMinuteAngle / 60.0);
			}
			else
			{
				iMeridianCoordDegreeAngle = qstrMeridianCoordAngle.left(2).toInt();
				iMeridianCoordMinuteAngle = qstrMeridianCoordAngle.right(2).toInt();
				dMeridianCoordAngle = (double)iMeridianCoordDegreeAngle + (double)iMeridianCoordMinuteAngle / 60.0;
			}

			// 浮点数度转换为DMS格式
			QString qstrMeridianCoordDMS = "";
			if (iMeridianCoordMinuteAngle < 10 && iMeridianCoordMinuteAngle > 0)
			{
				qstrMeridianCoordDMS = tr("%1°0%2′").arg(abs(iMeridianCoordDegreeAngle)).arg(iMeridianCoordMinuteAngle);
			}
			else if (iMeridianCoordMinuteAngle == 0)
			{
				qstrMeridianCoordDMS = tr("%1°00′").arg(abs(iMeridianCoordDegreeAngle));
			}
			else
			{
				qstrMeridianCoordDMS = tr("%1°%2′").arg(abs(iMeridianCoordDegreeAngle)).arg(iMeridianCoordMinuteAngle);
			}

			// 计算坐标纵线偏角密位
			// 1密位≈0.06°
			int iMeridian_Coord_Mil = int(round(fabs(dMeridianCoordAngle) / 0.06));

			// 坐标纵线偏角密位字符串，如：（1-71）
			QString qstrMeridian_Coord_Mil;
			ConvertMilFromIntToString(iMeridian_Coord_Mil, qstrMeridian_Coord_Mil);

#pragma endregion

#pragma region "计算磁偏角数值"

			// 将整数的磁偏角换算成浮点数
			QString qstrMeridianMagAngle = QString::fromLocal8Bit(strCiPianJiao.c_str());
			int iMeridianMagDegreeAngle = 0;		// 度
			int iMeridianMagMinuteAngle = 0;		// 分
			// 浮点数度
			double dMeridianMagAngle = 0;
			if (qstrMeridianMagAngle.toInt() < 0)
			{
				iMeridianMagDegreeAngle = qstrMeridianMagAngle.left(3).toInt();
				iMeridianMagMinuteAngle = qstrMeridianMagAngle.right(2).toInt();
				// 浮点数度
				dMeridianMagAngle = -(fabs(iMeridianMagDegreeAngle) + (double)iMeridianMagMinuteAngle / 60.0);
			}
			else
			{
				iMeridianMagDegreeAngle = qstrMeridianMagAngle.left(2).toInt();
				iMeridianMagMinuteAngle = qstrMeridianMagAngle.right(2).toInt();
				dMeridianMagAngle = (double)iMeridianMagDegreeAngle + (double)iMeridianMagMinuteAngle / 60.0;
			}

			// 浮点数度转换为DMS格式
			QString qstrMeridianMagDMS = "";
			if (iMeridianMagMinuteAngle < 10 && iMeridianMagMinuteAngle > 0)
			{
				qstrMeridianMagDMS = tr("%1°0%2′").arg(abs(iMeridianMagDegreeAngle)).arg(iMeridianMagMinuteAngle);
			}
			else if (iMeridianMagMinuteAngle == 0)
			{
				qstrMeridianMagDMS = tr("%1°00′").arg(abs(iMeridianMagDegreeAngle));
			}
			else
			{
				qstrMeridianMagDMS = tr("%1°%2′").arg(abs(iMeridianMagDegreeAngle)).arg(iMeridianMagMinuteAngle);
			}

			// 计算磁偏角密位
			// 1密位≈0.06°
			int iMeridianMag_Mil = int(round(fabs(dMeridianMagAngle) / 0.06));

			// 磁偏角密位字符串，如：（1-71）
			QString qstrMeridianMag_Mil;
			ConvertMilFromIntToString(iMeridianMag_Mil, qstrMeridianMag_Mil);

#pragma endregion

#pragma region "计算磁坐偏角数值"

			// 将整数的磁坐偏角换算成浮点数
			QString qstrCoordMagAngle = QString::fromLocal8Bit(strCiZuoPianJiao.c_str());
			int iCoordMagDegreeAngle = 0;		// 度
			int iCoordMagMinuteAngle = 0;		// 分
			// 浮点数度
			double dCoordMagAngle = 0;
			if (qstrCoordMagAngle.toInt() < 0)
			{
				iCoordMagDegreeAngle = qstrCoordMagAngle.left(3).toInt();
				iCoordMagMinuteAngle = qstrCoordMagAngle.right(2).toInt();
				// 浮点数度
				dCoordMagAngle = -(fabs(iCoordMagDegreeAngle) + (double)iCoordMagMinuteAngle / 60.0);
			}
			else
			{
				iCoordMagDegreeAngle = qstrCoordMagAngle.left(2).toInt();
				iCoordMagMinuteAngle = qstrCoordMagAngle.right(2).toInt();
				dCoordMagAngle = (double)iCoordMagDegreeAngle + (double)iCoordMagMinuteAngle / 60.0;
			}

			// 浮点数度转换为DMS格式
			QString qstrCoordMagDMS = "";
			if (iCoordMagMinuteAngle < 10 && iCoordMagMinuteAngle > 0)
			{
				qstrCoordMagDMS = tr("%1°0%2′").arg(abs(iCoordMagDegreeAngle)).arg(iCoordMagMinuteAngle);
			}
			else if (iCoordMagMinuteAngle == 0)
			{
				qstrCoordMagDMS = tr("%1°00′").arg(abs(iCoordMagDegreeAngle));
			}
			else
			{
				qstrCoordMagDMS = tr("%1°%2′").arg(abs(iCoordMagDegreeAngle)).arg(iCoordMagMinuteAngle);
			}

			// 计算磁坐偏角密位
			// 1密位≈0.06°
			int iCoordMag_Mil = int(round(fabs(dCoordMagAngle) / 0.06));

			// 磁坐偏角密位字符串，如：（1-71）
			QString qstrCoordMag_Mil;
			ConvertMilFromIntToString(iCoordMag_Mil, qstrCoordMag_Mil);

#pragma endregion


#pragma region "设置三北方向图"

#pragma region "三北方向线样图1"

#pragma endregion

			// 坐标纵线偏角小于0并且磁偏角小于0，选取三北样图1
			if (dMeridianCoordAngle < 0 && dMeridianMagAngle < 0)
			{
				// 三北样图
				QgsLayoutItemPicture* pTND_1 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_TND_1");

				// 三北样图参考点坐标（横坐标：地图外图廓宽度*0.75，纵坐标：外图廓下边界+3）
				QgsLayoutPoint tndPoint;
				tndPoint.setX(m_dExteriorLeftBottomLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() * 0.75);
				tndPoint.setY(m_dExteriorLeftBottomLayoutPoint.dy + 3);
				pTND_1->attemptMove(tndPoint);
				pTND_1->setVisibility(true);

				/*坐标纵线偏角角度	LabelItem_TND_Meridian_Coord_DMS_1		-10.446		4.837
				坐标纵线偏角密位	LabelItem_TND_Meridian_Coord_Mil_1		-9.998		6.701
				磁偏角角度			LabelItem_TND_Meridian_Mag_DMS_1		-14.828		15.165
				磁偏角密位			LabelItem_TND_Meridian_Mag_Mil_1		-9.27		10.244
				磁坐偏角角度		LabelItem_TND_Mag_Coord_DMS_1			-19.119		11.486
				磁坐偏角密位		LabelItem_TND_Mag_Coord_Mil_1			-17.61		12.662
				*/

				//--------------【坐标纵线偏角】----------------//
				// 坐标纵线偏角角度
				QgsLayoutItemLabel* pMeridian_Coord_DMS_1 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Coord_DMS_1");
				pMeridian_Coord_DMS_1->setText(qstrMeridianCoordDMS);
				pMeridian_Coord_DMS_1->attemptMove(QgsLayoutPoint(tndPoint.x() - 10.446, tndPoint.y() + 4.837));
				pMeridian_Coord_DMS_1->setVisibility(true);

				// 坐标纵线偏角密位
				QgsLayoutItemLabel* pTND_Meridian_Coord_Mil_1 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Coord_Mil_1");
				pTND_Meridian_Coord_Mil_1->setText(qstrMeridian_Coord_Mil);
				pTND_Meridian_Coord_Mil_1->attemptMove(QgsLayoutPoint(tndPoint.x() - 9.998, tndPoint.y() + 6.701));
				pTND_Meridian_Coord_Mil_1->setVisibility(true);

				//----------------------------------------------//


				//--------------【磁偏角】----------------//
				// 磁偏角角度
				QgsLayoutItemLabel* pTND_Meridian_Mag_DMS_1 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Mag_DMS_1");
				pTND_Meridian_Mag_DMS_1->setText(qstrMeridianMagDMS);
				pTND_Meridian_Mag_DMS_1->attemptMove(QgsLayoutPoint(tndPoint.x() - 14.828, tndPoint.y() + 15.165));
				pTND_Meridian_Mag_DMS_1->setVisibility(true);

				// 磁偏角密位
				QgsLayoutItemLabel* pMeridian_Mag_Mil_1 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Mag_Mil_1");
				pMeridian_Mag_Mil_1->setText(qstrMeridianMag_Mil);
				pMeridian_Mag_Mil_1->attemptMove(QgsLayoutPoint(tndPoint.x() - 9.27, tndPoint.y() + 10.244));
				pMeridian_Mag_Mil_1->setVisibility(true);

				//----------------------------------------------//


				//--------------【磁坐偏角】----------------//
				// 磁坐偏角角度
				QgsLayoutItemLabel* pMag_Coord_DMS_1 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Mag_Coord_DMS_1");
				pMag_Coord_DMS_1->setText(qstrCoordMagDMS);
				pMag_Coord_DMS_1->attemptMove(QgsLayoutPoint(tndPoint.x() - 19.119, tndPoint.y() + 11.486));
				pMag_Coord_DMS_1->setVisibility(true);

				// 磁坐偏角密位
				QgsLayoutItemLabel* pMag_Coord_Mil_1 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Mag_Coord_Mil_1");
				pMag_Coord_Mil_1->setText(qstrCoordMag_Mil);
				pMag_Coord_Mil_1->attemptMove(QgsLayoutPoint(tndPoint.x() - 17.61, tndPoint.y() + 12.662));
				pMag_Coord_Mil_1->setVisibility(true);

				//----------------------------------------------//
			}

			// 坐标纵线偏角小于0并且磁偏角大于0，选取三北样图2
			else if (dMeridianCoordAngle < 0 && dMeridianMagAngle > 0)
			{
				// 三北样图
				QgsLayoutItemPicture* pTND_2 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_TND_2");

				// 三北样图参考点坐标（横坐标：地图外图廓宽度*0.75，纵坐标：外图廓下边界+3）
				QgsLayoutPoint tndPoint;
				tndPoint.setX(m_dExteriorLeftBottomLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() * 0.75);
				tndPoint.setY(m_dExteriorLeftBottomLayoutPoint.dy + 3);
				pTND_2->attemptMove(tndPoint);
				pTND_2->setVisibility(true);

				/*LabelItem_TND_Meridian_Coord_DMS_2		-18.617		8.918
				LabelItem_TND_Meridian_Coord_Mil_2			-18.103		10.607
				LabelItem_TND_Meridian_Mag_DMS_2			-10.351		7.377
				LabelItem_TND_Meridian_Mag_Mil_2			-11.032		9.109
				LabelItem_TND_Mag_Coord_DMS_2				-19.312		6.727
				LabelItem_TND_Mag_Coord_Mil_2				-9.984		4.99
				*/

				//--------------【坐标纵线偏角】----------------//
				// 坐标纵线偏角角度
				QgsLayoutItemLabel* pMeridian_Coord_DMS_2 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Coord_DMS_2");
				pMeridian_Coord_DMS_2->setText(qstrMeridianCoordDMS);
				pMeridian_Coord_DMS_2->attemptMove(QgsLayoutPoint(tndPoint.x() - 18.617, tndPoint.y() + 8.918));
				pMeridian_Coord_DMS_2->setVisibility(true);

				// 坐标纵线偏角密位
				QgsLayoutItemLabel* pTND_Meridian_Coord_Mil_2 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Coord_Mil_2");
				pTND_Meridian_Coord_Mil_2->setText(qstrMeridian_Coord_Mil);
				pTND_Meridian_Coord_Mil_2->attemptMove(QgsLayoutPoint(tndPoint.x() - 18.103, tndPoint.y() + 10.607));
				pTND_Meridian_Coord_Mil_2->setVisibility(true);

				//----------------------------------------------//


				//--------------【磁偏角】----------------//
				// 磁偏角角度
				QgsLayoutItemLabel* pTND_Meridian_Mag_DMS_2 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Mag_DMS_2");
				pTND_Meridian_Mag_DMS_2->setText(qstrMeridianMagDMS);
				pTND_Meridian_Mag_DMS_2->attemptMove(QgsLayoutPoint(tndPoint.x() - 10.351, tndPoint.y() + 7.377));
				pTND_Meridian_Mag_DMS_2->setVisibility(true);

				// 磁偏角密位
				QgsLayoutItemLabel* pMeridian_Mag_Mil_2 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Mag_Mil_2");
				pMeridian_Mag_Mil_2->setText(qstrMeridianMag_Mil);
				pMeridian_Mag_Mil_2->attemptMove(QgsLayoutPoint(tndPoint.x() - 11.032, tndPoint.y() + 9.109));
				pMeridian_Mag_Mil_2->setVisibility(true);

				//----------------------------------------------//


				//--------------【磁坐偏角】----------------//
				// 磁坐偏角角度
				QgsLayoutItemLabel* pMag_Coord_DMS_2 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Mag_Coord_DMS_2");
				pMag_Coord_DMS_2->setText(qstrCoordMagDMS);
				pMag_Coord_DMS_2->attemptMove(QgsLayoutPoint(tndPoint.x() - 19.312, tndPoint.y() + 6.727));
				pMag_Coord_DMS_2->setVisibility(true);

				// 磁坐偏角密位
				QgsLayoutItemLabel* pMag_Coord_Mil_2 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Mag_Coord_Mil_2");
				pMag_Coord_Mil_2->setText(qstrCoordMag_Mil);
				pMag_Coord_Mil_2->attemptMove(QgsLayoutPoint(tndPoint.x() - 9.984, tndPoint.y() + 4.99));
				pMag_Coord_Mil_2->setVisibility(true);

				//----------------------------------------------//
			}

			// 坐标纵线偏角大于0并且磁偏角小于0，选取三北样图3
			else if (dMeridianCoordAngle > 0 && dMeridianMagAngle < 0)
			{
				// 三北样图
				QgsLayoutItemPicture* pTND_3 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_TND_3");

				// 三北样图参考点坐标（横坐标：地图外图廓宽度*0.75，纵坐标：外图廓下边界+3）
				QgsLayoutPoint tndPoint;
				tndPoint.setX(m_dExteriorLeftBottomLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() * 0.75);
				tndPoint.setY(m_dExteriorLeftBottomLayoutPoint.dy + 3);
				pTND_3->attemptMove(tndPoint);
				pTND_3->setVisibility(true);

				/*LabelItem_TND_Meridian_Coord_DMS_3		-10.889		7.659
					LabelItem_TND_Meridian_Coord_Mil_3		-11.55		9.201
					LabelItem_TND_Meridian_Mag_DMS_3		-19.267		8.929
					LabelItem_TND_Meridian_Mag_Mil_3		-18.827		10.655
					LabelItem_TND_Mag_Coord_DMS_3			-19.851		6.789
					LabelItem_TND_Mag_Coord_Mil_3			-10.159		5.394

				*/

				//--------------【坐标纵线偏角】----------------//
				// 坐标纵线偏角角度
				QgsLayoutItemLabel* pMeridian_Coord_DMS_3 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Coord_DMS_3");
				pMeridian_Coord_DMS_3->setText(qstrMeridianCoordDMS);
				pMeridian_Coord_DMS_3->attemptMove(QgsLayoutPoint(tndPoint.x() - 10.889, tndPoint.y() + 7.659));
				pMeridian_Coord_DMS_3->setVisibility(true);

				// 坐标纵线偏角密位
				QgsLayoutItemLabel* pTND_Meridian_Coord_Mil_3 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Coord_Mil_3");
				pTND_Meridian_Coord_Mil_3->setText(qstrMeridian_Coord_Mil);
				pTND_Meridian_Coord_Mil_3->attemptMove(QgsLayoutPoint(tndPoint.x() - 11.55, tndPoint.y() + 9.201));
				pTND_Meridian_Coord_Mil_3->setVisibility(true);

				//----------------------------------------------//


				//--------------【磁偏角】----------------//
				// 磁偏角角度
				QgsLayoutItemLabel* pTND_Meridian_Mag_DMS_3 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Mag_DMS_3");
				pTND_Meridian_Mag_DMS_3->setText(qstrMeridianMagDMS);
				pTND_Meridian_Mag_DMS_3->attemptMove(QgsLayoutPoint(tndPoint.x() - 19.267, tndPoint.y() + 8.929));
				pTND_Meridian_Mag_DMS_3->setVisibility(true);

				// 磁偏角密位
				QgsLayoutItemLabel* pMeridian_Mag_Mil_3 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Mag_Mil_3");
				pMeridian_Mag_Mil_3->setText(qstrMeridianMag_Mil);
				pMeridian_Mag_Mil_3->attemptMove(QgsLayoutPoint(tndPoint.x() - 18.827, tndPoint.y() + 10.655));
				pMeridian_Mag_Mil_3->setVisibility(true);

				//----------------------------------------------//


				//--------------【磁坐偏角】----------------//
				// 磁坐偏角角度
				QgsLayoutItemLabel* pMag_Coord_DMS_3 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Mag_Coord_DMS_3");
				pMag_Coord_DMS_3->setText(qstrCoordMagDMS);
				pMag_Coord_DMS_3->attemptMove(QgsLayoutPoint(tndPoint.x() - 19.851, tndPoint.y() + 6.789));
				pMag_Coord_DMS_3->setVisibility(true);

				// 磁坐偏角密位
				QgsLayoutItemLabel* pMag_Coord_Mil_3 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Mag_Coord_Mil_3");
				pMag_Coord_Mil_3->setText(qstrCoordMag_Mil);
				pMag_Coord_Mil_3->attemptMove(QgsLayoutPoint(tndPoint.x() - 10.159, tndPoint.y() + 5.394));
				pMag_Coord_Mil_3->setVisibility(true);

				//----------------------------------------------//
			}

			// 坐标纵线偏角大于0并且磁偏角大于0，选取三北样图4
			else if (dMeridianCoordAngle > 0 && dMeridianMagAngle > 0)
			{
				// 三北样图
				QgsLayoutItemPicture* pTND_4 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_TND_4");

				// 三北样图参考点坐标（横坐标：地图外图廓宽度*0.75，纵坐标：外图廓下边界+3）
				QgsLayoutPoint tndPoint;
				tndPoint.setX(m_dExteriorLeftBottomLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() * 0.75);
				tndPoint.setY(m_dExteriorLeftBottomLayoutPoint.dy + 3);
				pTND_4->attemptMove(tndPoint);
				pTND_4->setVisibility(true);

				/*LabelItem_TND_Meridian_Coord_DMS_4	-19.085		7.376
				LabelItem_TND_Meridian_Coord_Mil_4		-19.599		9.065
				LabelItem_TND_Meridian_Mag_DMS_4		-18.347		4.942
				LabelItem_TND_Meridian_Mag_Mil_4		-9.559		8.335
				LabelItem_TND_Mag_Coord_DMS_4			-11.542		10.097
				LabelItem_TND_Mag_Coord_Mil_4			-12.656		11.278
				*/

				//--------------【坐标纵线偏角】----------------//
				// 坐标纵线偏角角度
				QgsLayoutItemLabel* pMeridian_Coord_DMS_4 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Coord_DMS_4");
				pMeridian_Coord_DMS_4->setText(qstrMeridianCoordDMS);
				pMeridian_Coord_DMS_4->attemptMove(QgsLayoutPoint(tndPoint.x() - 19.085, tndPoint.y() + 7.376));
				pMeridian_Coord_DMS_4->setVisibility(true);

				// 坐标纵线偏角密位
				QgsLayoutItemLabel* pTND_Meridian_Coord_Mil_4 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Coord_Mil_4");
				pTND_Meridian_Coord_Mil_4->setText(qstrMeridian_Coord_Mil);
				pTND_Meridian_Coord_Mil_4->attemptMove(QgsLayoutPoint(tndPoint.x() - 19.599, tndPoint.y() + 9.065));
				pTND_Meridian_Coord_Mil_4->setVisibility(true);

				//----------------------------------------------//


				//--------------【磁偏角】----------------//
				// 磁偏角角度
				QgsLayoutItemLabel* pTND_Meridian_Mag_DMS_4 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Mag_DMS_4");
				pTND_Meridian_Mag_DMS_4->setText(qstrMeridianMagDMS);
				pTND_Meridian_Mag_DMS_4->attemptMove(QgsLayoutPoint(tndPoint.x() - 18.347, tndPoint.y() + 4.942));
				pTND_Meridian_Mag_DMS_4->setVisibility(true);

				// 磁偏角密位
				QgsLayoutItemLabel* pMeridian_Mag_Mil_4 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Meridian_Mag_Mil_4");
				pMeridian_Mag_Mil_4->setText(qstrMeridianMag_Mil);
				pMeridian_Mag_Mil_4->attemptMove(QgsLayoutPoint(tndPoint.x() - 9.559, tndPoint.y() + 8.335));
				pMeridian_Mag_Mil_4->setVisibility(true);

				//----------------------------------------------//


				//--------------【磁坐偏角】----------------//
				// 磁坐偏角角度
				QgsLayoutItemLabel* pMag_Coord_DMS_4 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Mag_Coord_DMS_4");
				pMag_Coord_DMS_4->setText(qstrCoordMagDMS);
				pMag_Coord_DMS_4->attemptMove(QgsLayoutPoint(tndPoint.x() - 11.542, tndPoint.y() + 10.097));
				pMag_Coord_DMS_4->setVisibility(true);

				// 磁坐偏角密位
				QgsLayoutItemLabel* pMag_Coord_Mil_4 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_TND_Mag_Coord_Mil_4");
				pMag_Coord_Mil_4->setText(qstrCoordMag_Mil);
				pMag_Coord_Mil_4->attemptMove(QgsLayoutPoint(tndPoint.x() - 12.656, tndPoint.y() + 11.278));
				pMag_Coord_Mil_4->setVisibility(true);

				//----------------------------------------------//
			}


#pragma endregion
		}












#pragma endregion

#pragma region "【8】地图左下角说明"

		QgsLayoutItemLabel* pTextItem_MapDetail = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_MapDetail");

		// 如果是绘制状态
		if (m_ImageMappingSchema.layoutItemParam.bMapDetailsChecked)
		{
			pTextItem_MapDetail->setText(m_ImageMappingSchema.layoutItemParam.qstrMapDetails);
			pTextItem_MapDetail->setVisibility(true);

			/*----------------------------------------------------------------*/
			// 修改时间：2023-03-22
			// 修改内容：调整地图左下角说明左边界起始x位置为地图内图廓左下角D的x值
			/*----------------------------------------------------------------*/


			// 调整尺寸
			pTextItem_MapDetail->attemptSetSceneRect(QRectF(interiorMapLayoutPoint_D.dx,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3,
				55,
				27));
		}
		else
		{
			pTextItem_MapDetail->setVisibility(false);
		}

#pragma region "创建整饰要素-地图说明"

		// 创建地图说明要素，同时设置模板中图名要素不显示
		pTextItem_MapDetail->setVisibility(false);

		// 如果不存在，需要创建要素
		if (!bLayoutDecPolygonExisted)
		{
			// 地图说明四个角点，依次左上角A、右上角B、右下角C、左下角D
			SE_DPoint dLayoutPoints[4];

			// 左上角
			dLayoutPoints[0].dx = interiorMapLayoutPoint_D.dx;
			dLayoutPoints[0].dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3;

			// 右上角
			dLayoutPoints[1].dx = dLayoutPoints[0].dx + 55;
			dLayoutPoints[1].dy = dLayoutPoints[0].dy;

			// 右下角
			dLayoutPoints[2].dx = dLayoutPoints[0].dx + 55;
			dLayoutPoints[2].dy = dLayoutPoints[0].dy + 27;

			// 左下角
			dLayoutPoints[3].dx = dLayoutPoints[0].dx;
			dLayoutPoints[3].dy = dLayoutPoints[0].dy + 27;


			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "map_detail";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = m_ImageMappingSchema.layoutItemParam.qstrMapDetails.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dGaussPoint[4];		// 高斯坐标
			double dValue[8] = { 0 };
			SE_DPoint dGeoPoint[4];			// 地理坐标
			vector<SE_DPoint> vPoints;
			vPoints.clear();
			int iRet = 0;
			for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
			{
				TransformLayoutToGauss(
					interiorMapGaussPoint_A,
					interiorMapLayoutPoint_A,
					dLayoutPoints[iTempIndex],
					dMapUnit2LayoutUnit,
					dGaussPoint[iTempIndex]);

				dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
				dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

				// 高斯投影转地理坐标
				iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
					GaussKruger,
					dCenterMeridian,
					0,
					0,
					0,
					0,
					500000,
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
				continue;
			}
		}

#pragma endregion



#pragma endregion

#pragma region "【9】比例尺文字"

		QgsLayoutItemLabel* pTextItem_Scale = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_Scale");

		// 如果是绘制状态
		if (m_ImageMappingSchema.layoutItemParam.bScaleChecked)
		{
			pTextItem_Scale->setText(m_ImageMappingSchema.layoutItemParam.qstrScale);
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

#pragma region "【10】图形比例尺"

		QgsLayoutItemPicture* pPictureItem_Scale = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_Scale");
		// 如果是绘制状态
		if (m_ImageMappingSchema.layoutItemParam.bScaleChecked)
		{
			// 设置图片模式
			pPictureItem_Scale->setMode(QgsLayoutItemPicture::FormatSVG);

			QString qstrSvgPath = "";
			// 如果是1万比例尺
			if (dScale == SCALE_10K)
			{
				qstrSvgPath = QCoreApplication::applicationDirPath() + "/svg/scale/10000.svg";
			}
			// 如果是5万比例尺
			else if (dScale == SCALE_50K)
			{
				qstrSvgPath = QCoreApplication::applicationDirPath() + "/svg/scale/50000.svg";
			}

			// 设置图片路径
			pPictureItem_Scale->setPicturePath(qstrSvgPath);
			pPictureItem_Scale->setResizeMode(QgsLayoutItemPicture::Zoom);
			pPictureItem_Scale->setVisibility(true);

			// 设置位置
			pPictureItem_Scale->setReferencePoint(QgsLayoutItem::Middle);

			// 设置参考点坐标
			pPictureItem_Scale->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 15));
		}
		else
		{
			pPictureItem_Scale->setVisibility(false);
		}



#pragma endregion

#pragma region "【11】制图单位"

		QgsLayoutItemLabel* pTextItem_MappingAgency = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_MappingAgency");
		// 如果是绘制状态
		if (m_ImageMappingSchema.layoutItemParam.bMapAgencyChecked)
		{
			pTextItem_MappingAgency->setText(m_ImageMappingSchema.layoutItemParam.qstrMapAgency);
			pTextItem_MappingAgency->setVisibility(true);

			// 设置位置
			pTextItem_MappingAgency->setReferencePoint(QgsLayoutItem::Middle);

			// 设置参考点坐标
			pTextItem_MappingAgency->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 28));

			// 自动调整尺寸
			pTextItem_MappingAgency->adjustSizeToText();

			// 设置水平对齐方式
			//pTextItem_MappingAgency->setHAlign(Qt::AlignHCenter);

		}
		else
		{
			pTextItem_MappingAgency->setVisibility(false);
		}

#pragma region "创建制图单位要素"

		// 创建制图单位要素，同时设置模板中制图单位要素不显示
		pTextItem_MappingAgency->setVisibility(false);

		if (!bLayoutDecPointExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPoint;

			dLayoutPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0;
			dLayoutPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 28;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "map_agency";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "VALUE";
			fieldImp.strFieldValue = m_ImageMappingSchema.layoutItemParam.qstrMapAgency.toLocal8Bit();
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dGaussPoint;
			double dValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPoint,
				dMapUnit2LayoutUnit,
				dGaussPoint);

			dValue[0] = dGaussPoint.dx;
			dValue[1] = dGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dGeoPoint;
			dGeoPoint.dx = dValue[0];
			dGeoPoint.dy = dValue[1];

			iRet = Set_Point(poDecPointLayer, dGeoPoint.dx, dGeoPoint.dy, 0, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}


		GDALClose(poLayoutDecorationPointDS);

#pragma endregion



#pragma endregion

#pragma region "【12】镶嵌线地图"

		/*镶嵌地图设置*/
		QgsLayoutItemMap* pMosaicMapItem = (QgsLayoutItemMap*)GetLayoutItemByName(qLayoutItemList, "MapItem_Mosaic");
		if (!pMosaicMapItem)
		{
			// 记录日志
			QgsMessageLog::logMessage(tr("镶嵌线地图控件获取失败！"), tr("影像图制图"), Qgis::Critical);
			continue;
		}

		/*判断是否绘制镶嵌线缩略图*/
		if (m_ImageMappingSchema.layoutItemParam.bMosaicMapChecked)
		{
			/*镶嵌线地图只显示镶嵌线图层*/
			pMosaicMapItem->setLayers(qMosaicMapLayerList);
			pMosaicMapItem->setCrs(QgsCoordinateReferenceSystem(qstrEPSGCode));
			pMosaicMapItem->setVisibility(true);

			/*----------------------------------------------------------------*/
			// 修改时间：2023-03-22
			// 修改内容：调整镶嵌线地图左边界参考x位置为地图内图廓左下角D的x值
			/*----------------------------------------------------------------*/


			pMosaicMapItem->attemptSetSceneRect(QRectF(interiorMapLayoutPoint_D.dx + MOSAIC_MAP_LEFT_X,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3,
				MOSAIC_MAP_WIDTH,
				MOSAIC_MAP_HEIGHT));
			pMosaicMapItem->zoomToExtent(rect);
			pMosaicMapItem->setBackgroundColor(QColor(255, 255, 255, 0));
			// 默认显示边框
			pMosaicMapItem->setFrameEnabled(true);
		}
		else
		{
			pMosaicMapItem->setVisibility(false);
		}


#pragma endregion

#pragma region "【13】当前图幅经纬度范围计算"

		/*将角点经纬度转换为DMS格式*/
		// 左边界经度
		int iLeftLonDegree = 0;
		int iLeftLonMinute = 0;
		int iLeftLonSecond = 0;

		// 上边界纬度
		int iTopLatDegree = 0;
		int iTopLatMinute = 0;
		int iTopLatSecond = 0;

		// 右边界经度
		int iRightLonDegree = 0;
		int iRightLonMinute = 0;
		int iRightLonSecond = 0;

		// 下边界纬度
		int iBottomLatDegree = 0;
		int iBottomLatMinute = 0;
		int iBottomLatSecond = 0;

		Degree2DMS(dSheetBoxLeft, iLeftLonDegree, iLeftLonMinute, iLeftLonSecond);
		Degree2DMS(dSheetBoxTop, iTopLatDegree, iTopLatMinute, iTopLatSecond);
		Degree2DMS(dSheetBoxRight, iRightLonDegree, iRightLonMinute, iRightLonSecond);
		Degree2DMS(dSheetBoxBottom, iBottomLatDegree, iBottomLatMinute, iBottomLatSecond);

		QString qstrFormatMinute;       // 规范化分，0′用00′表示，5′用05′表示，两位数′不需要处理
		QString qstrFormatSecond;       // 规范化秒，0″用00″表示，5″用05″表示，两位数″不需要处理

#pragma endregion


#pragma region "1万比例尺模板，四个角点显示度、分、秒"

		// 如果是1万比例尺
		if (dScale == SCALE_10K)
		{
#pragma region "【14】左上角A经纬度值"
			//---------------------------------------------//
			// 地图左上角纬度——度
			QgsLayoutItemLabel* TextItem_LeftTopLatDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLatDegree");
			TextItem_LeftTopLatDegree->setText(tr("%1").arg(FormatDegree(iTopLatDegree)));

			// 设置参考点-右下角（地图内图廓左上角）
			TextItem_LeftTopLatDegree->setReferencePoint(QgsLayoutItem::LowerRight);
			// 设置参考点坐标
			TextItem_LeftTopLatDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx,
				interiorMapLayoutPoint_A.dy));


			//--------------------------------------------//

			// 地图左上角纬度——分
			QgsLayoutItemLabel* TextItem_LeftTopLatMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLatMinute");
			qstrFormatMinute = FormatMinute(iTopLatMinute);
			TextItem_LeftTopLatMinute->setText(qstrFormatMinute);

			// 设置参考点-右上角（地图内图廓左上角）
			TextItem_LeftTopLatMinute->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_LeftTopLatMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx,
				interiorMapLayoutPoint_A.dy));


			//--------------------------------------------//
			// 地图左上角纬度——秒
			QgsLayoutItemLabel* TextItem_LeftTopLatSecond = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLatSecond");
			qstrFormatSecond = FormatSecond(iTopLatSecond);
			TextItem_LeftTopLatSecond->setText(qstrFormatSecond);

			// 设置参考点-右上角（地图内图廓左上角）
			TextItem_LeftTopLatSecond->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_LeftTopLatSecond->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx,
				interiorMapLayoutPoint_A.dy + TextItem_LeftTopLatMinute->rect().height()));


			//---------------------------------------------//
			// 地图左上角经度——度
			QgsLayoutItemLabel* TextItem_LeftTopLonDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLonDegree");
			TextItem_LeftTopLonDegree->setText(tr("%1°").arg(iLeftLonDegree));

			// 设置参考点-右下角（地图内图廓左上角）
			TextItem_LeftTopLonDegree->setReferencePoint(QgsLayoutItem::LowerRight);

			// 设置参考点坐标
			TextItem_LeftTopLonDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx,
				interiorMapLayoutPoint_A.dy - TextItem_LeftTopLatDegree->rect().height()));


			//--------------------------------------------//
			// 地图左上角经度——分
			QgsLayoutItemLabel* TextItem_LeftTopLonMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLonMinute");
			qstrFormatMinute = FormatMinute(iLeftLonMinute);
			TextItem_LeftTopLonMinute->setText(qstrFormatMinute);

			// 设置参考点-左下角（地图内图廓左上角）
			TextItem_LeftTopLonMinute->setReferencePoint(QgsLayoutItem::LowerLeft);

			// 设置参考点坐标
			TextItem_LeftTopLonMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx,
				interiorMapLayoutPoint_A.dy - TextItem_LeftTopLatDegree->rect().height()));


			//--------------------------------------------//
			// 地图左上角经度——秒
			QgsLayoutItemLabel* TextItem_LeftTopLonSecond = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLonSecond");
			qstrFormatSecond = FormatSecond(iLeftLonSecond);
			TextItem_LeftTopLonSecond->setText(qstrFormatSecond);

			// 设置参考点-左下角（地图内图廓左上角）
			TextItem_LeftTopLonSecond->setReferencePoint(QgsLayoutItem::LowerLeft);

			// 设置参考点坐标
			TextItem_LeftTopLonSecond->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx + TextItem_LeftTopLonMinute->rect().width(),
				interiorMapLayoutPoint_A.dy - TextItem_LeftTopLatDegree->rect().height()));


#pragma endregion

#pragma region "【15】左下角D经纬度值"

			//--------------------------------------------//
			// 地图左下角纬度——秒
			QgsLayoutItemLabel* TextItem_LeftBottomLatSecond = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLatSecond");
			qstrFormatSecond = FormatSecond(iBottomLatSecond);
			TextItem_LeftBottomLatSecond->setText(qstrFormatSecond);

			// 设置参考点-右上角（地图内图廓左下角）
			TextItem_LeftBottomLatSecond->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_LeftBottomLatSecond->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx,
				interiorMapLayoutPoint_D.dy));



			//--------------------------------------------//
			// 地图左下角纬度——分
			QgsLayoutItemLabel* TextItem_LeftBottomLatMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLatMinute");
			qstrFormatMinute = FormatMinute(iBottomLatMinute);
			TextItem_LeftBottomLatMinute->setText(qstrFormatMinute);

			// 设置参考点-右下角（地图内图廓左下角）
			TextItem_LeftBottomLatMinute->setReferencePoint(QgsLayoutItem::LowerRight);
			// 设置参考点坐标
			TextItem_LeftBottomLatMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx,
				interiorMapLayoutPoint_D.dy));




			//---------------------------------------------//
			// 地图左下角纬度——度
			QgsLayoutItemLabel* TextItem_LeftBottomLatDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLatDegree");
			TextItem_LeftBottomLatDegree->setText(tr("%1").arg(FormatDegree(iBottomLatDegree)));

			// 设置参考点-右下角（地图内图廓左下角）
			TextItem_LeftBottomLatDegree->setReferencePoint(QgsLayoutItem::LowerRight);

			// 设置参考点坐标
			TextItem_LeftBottomLatDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx,
				interiorMapLayoutPoint_D.dy - TextItem_LeftBottomLatMinute->rect().height()));




			//---------------------------------------------//
			// 地图左下角经度——度
			QgsLayoutItemLabel* TextItem_LeftBottomLonDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLonDegree");
			TextItem_LeftBottomLonDegree->setText(tr("%1°").arg(iLeftLonDegree));

			// 设置参考点位置
			TextItem_LeftBottomLonDegree->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_LeftBottomLonDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx,
				interiorMapLayoutPoint_D.dy + TextItem_LeftBottomLatSecond->rect().height()));


			//--------------------------------------------//
			// 地图左下角经度——分
			QgsLayoutItemLabel* TextItem_LeftBottomLonMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLonMinute");
			qstrFormatMinute = FormatMinute(iLeftLonMinute);
			TextItem_LeftBottomLonMinute->setText(qstrFormatMinute);

			// 设置参考点位置
			TextItem_LeftBottomLonMinute->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_LeftBottomLonMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx,
				interiorMapLayoutPoint_D.dy + TextItem_LeftBottomLatSecond->rect().height()));


			//--------------------------------------------//
			// 地图左下角经度——秒
			QgsLayoutItemLabel* TextItem_LeftBottomLonSecond = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLonSecond");
			qstrFormatSecond = FormatSecond(iLeftLonSecond);
			TextItem_LeftBottomLonSecond->setText(qstrFormatSecond);

			// 设置参考点位置
			TextItem_LeftBottomLonSecond->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_LeftBottomLonSecond->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx + TextItem_LeftBottomLonMinute->rect().width(),
				interiorMapLayoutPoint_D.dy + TextItem_LeftBottomLatSecond->rect().height()));


#pragma endregion

#pragma region "【16】右上角B经纬度"

			// 地图右上角纬度 - 度
			QgsLayoutItemLabel* TextItem_RightTopLatDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLatDegree");
			TextItem_RightTopLatDegree->setText(tr("%1").arg(FormatDegree(iTopLatDegree)));

			// 设置参考点位置-左下角
			TextItem_RightTopLatDegree->setReferencePoint(QgsLayoutItem::LowerLeft);
			// 设置参考点坐标
			TextItem_RightTopLatDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx,
				interiorMapLayoutPoint_B.dy));



			//--------------------------------------------//
			// 地图右上角纬度 - 分
			QgsLayoutItemLabel* TextItem_RightTopLatMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLatMinute");
			qstrFormatMinute = FormatMinute(iTopLatMinute);
			TextItem_RightTopLatMinute->setText(qstrFormatMinute);

			// 设置参考点位置-左上角
			TextItem_RightTopLatMinute->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_RightTopLatMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx,
				interiorMapLayoutPoint_B.dy));


			//--------------------------------------------//
			// 地图右上角纬度 - 秒
			QgsLayoutItemLabel* TextItem_RightTopLatSecond = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLatSecond");
			qstrFormatSecond = FormatSecond(iTopLatSecond);
			TextItem_RightTopLatSecond->setText(qstrFormatSecond);

			// 设置参考点位置-左上角
			TextItem_RightTopLatSecond->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_RightTopLatSecond->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx,
				interiorMapLayoutPoint_B.dy + TextItem_RightTopLatMinute->rect().height()));



			//---------------------------------------------//

			// 地图右上角经度 - 分
			QgsLayoutItemLabel* TextItem_RightTopLonMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLonMinute");
			qstrFormatMinute = FormatMinute(iRightLonMinute);
			TextItem_RightTopLonMinute->setText(qstrFormatMinute);

			// 设置参考点位置-右下角
			TextItem_RightTopLonMinute->setReferencePoint(QgsLayoutItem::LowerRight);
			// 设置参考点坐标
			TextItem_RightTopLonMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx,
				interiorMapLayoutPoint_B.dy - TextItem_RightTopLatDegree->rect().height()));


			//--------------------------------------------//
			// 地图右上角经度 - 秒
			QgsLayoutItemLabel* TextItem_RightTopLonSecond = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLonSecond");
			qstrFormatSecond = FormatSecond(iRightLonSecond);
			TextItem_RightTopLonSecond->setText(qstrFormatSecond);

			// 设置参考点位置-左下角
			TextItem_RightTopLonSecond->setReferencePoint(QgsLayoutItem::LowerLeft);
			// 设置参考点坐标
			TextItem_RightTopLonSecond->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx,
				interiorMapLayoutPoint_B.dy - TextItem_RightTopLatDegree->rect().height()));


			//--------------------------------------------//
			// 地图右上角经度 - 度
			QgsLayoutItemLabel* TextItem_RightTopLonDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLonDegree");
			TextItem_RightTopLonDegree->setText(tr("%1°").arg(iRightLonDegree));

			// 设置参考点位置-右下角
			TextItem_RightTopLonDegree->setReferencePoint(QgsLayoutItem::LowerRight);
			// 设置参考点坐标
			TextItem_RightTopLonDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx - TextItem_RightTopLonMinute->rect().width(),
				interiorMapLayoutPoint_B.dy - TextItem_RightTopLatDegree->rect().height()));

			//--------------------------------------------//

#pragma endregion

#pragma region "【17】右下角C经纬度"

		// 地图右下角纬度 - 分
			QgsLayoutItemLabel* TextItem_RightBottomLatMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLatMinute");
			qstrFormatMinute = FormatMinute(iBottomLatMinute);
			TextItem_RightBottomLatMinute->setText(qstrFormatMinute);

			// 设置参考点位置-左下角
			TextItem_RightBottomLatMinute->setReferencePoint(QgsLayoutItem::LowerLeft);
			// 设置参考点坐标
			TextItem_RightBottomLatMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx,
				interiorMapLayoutPoint_C.dy));


			//--------------------------------------------//
			// 地图右下角纬度 - 秒
			QgsLayoutItemLabel* TextItem_RightBottomLatSecond = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLatSecond");
			qstrFormatSecond = FormatSecond(iBottomLatSecond);
			TextItem_RightBottomLatSecond->setText(qstrFormatSecond);

			// 设置参考点位置-左下角
			TextItem_RightBottomLatSecond->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_RightBottomLatSecond->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx,
				interiorMapLayoutPoint_C.dy));



			//--------------------------------------------//
			// 地图右下角纬度 - 度
			QgsLayoutItemLabel* TextItem_RightBottomLatDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLatDegree");
			TextItem_RightBottomLatDegree->setText(tr("%1").arg(FormatDegree(iBottomLatDegree)));

			// 设置参考点位置-左下角
			TextItem_RightBottomLatDegree->setReferencePoint(QgsLayoutItem::LowerLeft);
			// 设置参考点坐标
			TextItem_RightBottomLatDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx,
				interiorMapLayoutPoint_C.dy - TextItem_RightBottomLatMinute->rect().height()));



			//--------------------------------------------//
			// 地图右下角经度 - 分
			QgsLayoutItemLabel* TextItem_RightBottomLonMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLonMinute");
			qstrFormatMinute = FormatMinute(iRightLonMinute);
			TextItem_RightBottomLonMinute->setText(qstrFormatMinute);

			// 设置参考点位置-右上角
			TextItem_RightBottomLonMinute->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_RightBottomLonMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx,
				interiorMapLayoutPoint_C.dy + TextItem_RightBottomLatSecond->rect().height()));


			//--------------------------------------------//
			// 地图右下角经度 - 秒
			QgsLayoutItemLabel* TextItem_RightBottomLonSecond = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLonSecond");
			qstrFormatSecond = FormatSecond(iRightLonSecond);
			TextItem_RightBottomLonSecond->setText(qstrFormatSecond);

			// 设置参考点位置-右上角
			TextItem_RightBottomLonSecond->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_RightBottomLonSecond->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx,
				interiorMapLayoutPoint_C.dy + TextItem_RightBottomLatSecond->rect().height()));



			//---------------------------------------------//
			// 地图右下角经度 - 度
			QgsLayoutItemLabel* TextItem_RightBottomLonDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLonDegree");
			TextItem_RightBottomLonDegree->setText(tr("%1°").arg(iRightLonDegree));

			// 设置参考点位置-右上角
			TextItem_RightBottomLonDegree->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_RightBottomLonDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx - TextItem_RightBottomLonMinute->rect().width(),
				interiorMapLayoutPoint_C.dy + TextItem_RightBottomLatSecond->rect().height()));



#pragma endregion

		}
#pragma endregion

#pragma region "5万比例尺模板，四个角点显示度、分"

		else if (dScale == SCALE_50K)
		{
#pragma region "【14】左上角A经纬度值"
			//---------------------------------------------//
			// 地图左上角纬度——度
			QgsLayoutItemLabel* TextItem_LeftTopLatDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLatDegree");
			TextItem_LeftTopLatDegree->setText(tr("%1").arg(FormatDegree(iTopLatDegree)));

			// 设置参考点-右下角（地图内图廓左上角）
			TextItem_LeftTopLatDegree->setReferencePoint(QgsLayoutItem::LowerRight);
			// 设置参考点坐标
			TextItem_LeftTopLatDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx,
				interiorMapLayoutPoint_A.dy));

			//--------------------------------------------//

			// 地图左上角纬度——分
			QgsLayoutItemLabel* TextItem_LeftTopLatMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLatMinute");
			qstrFormatMinute = FormatMinute(iTopLatMinute);
			TextItem_LeftTopLatMinute->setText(qstrFormatMinute);

			// 设置参考点-右上角（地图内图廓左上角）
			TextItem_LeftTopLatMinute->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_LeftTopLatMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx,
				interiorMapLayoutPoint_A.dy));


			//---------------------------------------------//
			// 地图左上角经度——度
			QgsLayoutItemLabel* TextItem_LeftTopLonDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLonDegree");
			TextItem_LeftTopLonDegree->setText(tr("%1°").arg(iLeftLonDegree));

			// 设置参考点-右下角（地图内图廓左上角）
			TextItem_LeftTopLonDegree->setReferencePoint(QgsLayoutItem::LowerRight);

			// 设置参考点坐标
			TextItem_LeftTopLonDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx,
				interiorMapLayoutPoint_A.dy - TextItem_LeftTopLatDegree->rect().height()));

			//--------------------------------------------//
			// 地图左上角经度——分
			QgsLayoutItemLabel* TextItem_LeftTopLonMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftTopLonMinute");
			qstrFormatMinute = FormatMinute(iLeftLonMinute);
			TextItem_LeftTopLonMinute->setText(qstrFormatMinute);

			// 设置参考点-左下角（地图内图廓左上角）
			TextItem_LeftTopLonMinute->setReferencePoint(QgsLayoutItem::LowerLeft);

			// 设置参考点坐标
			TextItem_LeftTopLonMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_A.dx,
				interiorMapLayoutPoint_A.dy - TextItem_LeftTopLatDegree->rect().height()));


			//--------------------------------------------//

#pragma endregion

#pragma region "【15】左下角D经纬度值"

		//--------------------------------------------//
		// 地图左下角纬度——分
			QgsLayoutItemLabel* TextItem_LeftBottomLatMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLatMinute");
			qstrFormatMinute = FormatMinute(iBottomLatMinute);
			TextItem_LeftBottomLatMinute->setText(qstrFormatMinute);

			// 设置参考点-右上角（地图内图廓左下角）
			TextItem_LeftBottomLatMinute->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_LeftBottomLatMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx,
				interiorMapLayoutPoint_D.dy));


			//---------------------------------------------//
			// 地图左下角纬度——度
			QgsLayoutItemLabel* TextItem_LeftBottomLatDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLatDegree");
			TextItem_LeftBottomLatDegree->setText(tr("%1").arg(FormatDegree(iBottomLatDegree)));

			// 设置参考点-右下角（地图内图廓左下角）
			TextItem_LeftBottomLatDegree->setReferencePoint(QgsLayoutItem::LowerRight);

			// 设置参考点坐标
			TextItem_LeftBottomLatDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx,
				interiorMapLayoutPoint_D.dy));


			//---------------------------------------------//
			// 地图左下角经度——度
			QgsLayoutItemLabel* TextItem_LeftBottomLonDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLonDegree");
			TextItem_LeftBottomLonDegree->setText(tr("%1°").arg(iLeftLonDegree));

			// 设置参考点位置
			TextItem_LeftBottomLonDegree->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_LeftBottomLonDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx,
				interiorMapLayoutPoint_D.dy + TextItem_LeftBottomLatMinute->rect().height()));

			//--------------------------------------------//
			// 地图左下角经度——分
			QgsLayoutItemLabel* TextItem_LeftBottomLonMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_LeftBottomLonMinute");
			qstrFormatMinute = FormatMinute(iLeftLonMinute);
			TextItem_LeftBottomLonMinute->setText(qstrFormatMinute);

			// 设置参考点位置
			TextItem_LeftBottomLonMinute->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_LeftBottomLonMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_D.dx,
				interiorMapLayoutPoint_D.dy + TextItem_LeftBottomLatMinute->rect().height()));


#pragma endregion

#pragma region "【16】右上角B经纬度"

			// 地图右上角纬度 - 度
			QgsLayoutItemLabel* TextItem_RightTopLatDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLatDegree");
			TextItem_RightTopLatDegree->setText(tr("%1").arg(FormatDegree(iTopLatDegree)));

			// 设置参考点位置-左下角
			TextItem_RightTopLatDegree->setReferencePoint(QgsLayoutItem::LowerLeft);
			// 设置参考点坐标
			TextItem_RightTopLatDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx,
				interiorMapLayoutPoint_B.dy));

			//--------------------------------------------//
			// 地图右上角纬度 - 分
			QgsLayoutItemLabel* TextItem_RightTopLatMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLatMinute");
			qstrFormatMinute = FormatMinute(iTopLatMinute);
			TextItem_RightTopLatMinute->setText(qstrFormatMinute);

			// 设置参考点位置-左上角
			TextItem_RightTopLatMinute->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_RightTopLatMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx,
				interiorMapLayoutPoint_B.dy));

			//---------------------------------------------//

			// 地图右上角经度 - 分
			QgsLayoutItemLabel* TextItem_RightTopLonMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLonMinute");
			qstrFormatMinute = FormatMinute(iRightLonMinute);
			TextItem_RightTopLonMinute->setText(qstrFormatMinute);

			// 设置参考点位置-左下角
			TextItem_RightTopLonMinute->setReferencePoint(QgsLayoutItem::LowerLeft);
			// 设置参考点坐标
			TextItem_RightTopLonMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx,
				interiorMapLayoutPoint_B.dy - TextItem_RightTopLatDegree->rect().height()));

			//--------------------------------------------//
			// 地图右上角经度 - 度
			QgsLayoutItemLabel* TextItem_RightTopLonDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightTopLonDegree");
			TextItem_RightTopLonDegree->setText(tr("%1°").arg(iRightLonDegree));

			// 设置参考点位置-右下角
			TextItem_RightTopLonDegree->setReferencePoint(QgsLayoutItem::LowerRight);
			// 设置参考点坐标
			TextItem_RightTopLonDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_B.dx,
				interiorMapLayoutPoint_B.dy - TextItem_RightTopLatDegree->rect().height()));

			//--------------------------------------------//

#pragma endregion

#pragma region "【17】右下角C经纬度"

		// 地图右下角纬度 - 分
			QgsLayoutItemLabel* TextItem_RightBottomLatMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLatMinute");
			qstrFormatMinute = FormatMinute(iBottomLatMinute);
			TextItem_RightBottomLatMinute->setText(qstrFormatMinute);

			// 设置参考点位置-左上角
			TextItem_RightBottomLatMinute->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_RightBottomLatMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx,
				interiorMapLayoutPoint_C.dy));

			//--------------------------------------------//

			// 地图右下角纬度 - 度
			QgsLayoutItemLabel* TextItem_RightBottomLatDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLatDegree");
			TextItem_RightBottomLatDegree->setText(tr("%1").arg(FormatDegree(iBottomLatDegree)));

			// 设置参考点位置-左下角
			TextItem_RightBottomLatDegree->setReferencePoint(QgsLayoutItem::LowerLeft);
			// 设置参考点坐标
			TextItem_RightBottomLatDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx,
				interiorMapLayoutPoint_C.dy));

			//--------------------------------------------//
			// 地图右下角经度 - 分
			QgsLayoutItemLabel* TextItem_RightBottomLonMinute = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLonMinute");
			qstrFormatMinute = FormatMinute(iRightLonMinute);
			TextItem_RightBottomLonMinute->setText(qstrFormatMinute);

			// 设置参考点位置-左上角
			TextItem_RightBottomLonMinute->setReferencePoint(QgsLayoutItem::UpperLeft);
			// 设置参考点坐标
			TextItem_RightBottomLonMinute->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx,
				interiorMapLayoutPoint_C.dy + TextItem_RightBottomLatMinute->rect().height()));

			//---------------------------------------------//
			// 地图右下角经度 - 度
			QgsLayoutItemLabel* TextItem_RightBottomLonDegree = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_RightBottomLonDegree");
			TextItem_RightBottomLonDegree->setText(tr("%1°").arg(iRightLonDegree));

			// 设置参考点位置-右上角
			TextItem_RightBottomLonDegree->setReferencePoint(QgsLayoutItem::UpperRight);
			// 设置参考点坐标
			TextItem_RightBottomLonDegree->attemptMove(QgsLayoutPoint(
				interiorMapLayoutPoint_C.dx,
				interiorMapLayoutPoint_C.dy + TextItem_RightBottomLatMinute->rect().height()));


#pragma endregion
		}




#pragma endregion


#pragma region "【18】接图表中各文本内容位置更新"


		/*
		add by yangzhilong @2023-08-08
		增加接图表的矢量化
		*/

		// 如果需要绘制接图表
		if (m_ImageMappingSchema.layoutItemParam.bSheetListChecked)
		{
#pragma region "1-左上接图表"

			// 左上接图表
			QgsLayoutItemLabel* pTextItem_LeftTopName = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_LeftTopName");
			pTextItem_LeftTopName->setText(qstrLeftTopSheetMapName);

			// 设置参考点位置
			pTextItem_LeftTopName->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_LeftTopName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26 - 28,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3));

#pragma endregion


#pragma region "1-【创建左上角图框要素】"

#pragma region "（1）左上接图表矩形框"

			// 创建左上角图框要素，同时设置模板中图名要素不显示
			pTextItem_LeftTopName->setVisibility(false);

			// 如果不存在，需要创建要素
			if (!bLayoutDecPolygonExisted)
			{
				// 参考点：右上角
				SE_DPoint dRefPoint;
				dRefPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26 - 28;
				dRefPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3;


				// 左上接图表，依次左上角A、右上角B、右下角C、左下角D
				SE_DPoint dLayoutPoints[4];

				// 左上角
				dLayoutPoints[0].dx = dRefPoint.dx - 26;
				dLayoutPoints[0].dy = dRefPoint.dy;

				// 右上角
				dLayoutPoints[1].dx = dRefPoint.dx;
				dLayoutPoints[1].dy = dRefPoint.dy;

				// 右下角
				dLayoutPoints[2].dx = dRefPoint.dx;
				dLayoutPoints[2].dy = dRefPoint.dy + 9;

				// 左下角
				dLayoutPoints[3].dx = dRefPoint.dx - 26;
				dLayoutPoints[3].dy = dRefPoint.dy + 9;


				// 类型
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "adjsheet_lefttop_mapname";		// 左上图名
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstrLeftTopSheetMapName.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 布局坐标转投影坐标
				SE_DPoint dGaussPoint[4];		// 高斯坐标
				double dValue[8] = { 0 };
				SE_DPoint dGeoPoint[4];			// 地理坐标
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				int iRet = 0;
				for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
				{
					TransformLayoutToGauss(
						interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						dLayoutPoints[iTempIndex],
						dMapUnit2LayoutUnit,
						dGaussPoint[iTempIndex]);

					dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
					dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

					// 高斯投影转地理坐标
					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
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
					continue;
				}
			}




#pragma endregion

#pragma endregion



#pragma region "2-左中接图表"

#pragma region "左中边框"
			// 左中接图表
			QgsLayoutItemLabel* pTextItem_LeftRectangle = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_LeftRectangle");

			// 如果左中图幅图名为空，则在左中边框中显示图幅号
			if (qstrLeftSheetMapName == "")
			{
				pTextItem_LeftRectangle->setText(qstrLeftSheet);
			}

			pTextItem_LeftRectangle->setFrameEnabled(true);

			// 设置参考点位置
			pTextItem_LeftRectangle->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_LeftRectangle->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26 - 28,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9));


#pragma endregion


#pragma region "2-【创建左中图框要素】"

#pragma region "（2）左中接图表矩形框"

			// 创建左中接图表图框要素，同时设置模板中不显示
			pTextItem_LeftRectangle->setVisibility(false);

			// 如果不存在，需要创建要素
			if (!bLayoutDecPolygonExisted)
			{
				// 参考点：右上角
				SE_DPoint dRefPoint;
				dRefPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26 - 28;
				dRefPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9;


				// 左上接图表，依次左上角A、右上角B、右下角C、左下角D
				SE_DPoint dLayoutPoints[4];

				// 左上角
				dLayoutPoints[0].dx = dRefPoint.dx - 26;
				dLayoutPoints[0].dy = dRefPoint.dy;

				// 右上角
				dLayoutPoints[1].dx = dRefPoint.dx;
				dLayoutPoints[1].dy = dRefPoint.dy;

				// 右下角
				dLayoutPoints[2].dx = dRefPoint.dx;
				dLayoutPoints[2].dy = dRefPoint.dy + 9;

				// 左下角
				dLayoutPoints[3].dx = dRefPoint.dx - 26;
				dLayoutPoints[3].dy = dRefPoint.dy + 9;

				// 类型
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "adjsheet_left_sheet_mapname";		// 左中接图表
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				// 显示图幅号及图名
				QString qstrLeft = tr("%1\n%2").arg(qstrLeftSheet).arg(qstrLeftSheetMapName);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstrLeft.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 布局坐标转投影坐标
				SE_DPoint dGaussPoint[4];		// 高斯坐标
				double dValue[8] = { 0 };
				SE_DPoint dGeoPoint[4];			// 地理坐标
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				int iRet = 0;
				for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
				{
					TransformLayoutToGauss(
						interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						dLayoutPoints[iTempIndex],
						dMapUnit2LayoutUnit,
						dGaussPoint[iTempIndex]);

					dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
					dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

					// 高斯投影转地理坐标
					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
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
					continue;
				}
			}




#pragma endregion

#pragma endregion


#pragma region "左中图幅号"


			// 左中接图表
			QgsLayoutItemLabel* pTextItem_LeftSheet = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_LeftSheet");

			pTextItem_LeftSheet->setVisibility(false);
			/*pTextItem_LeftSheet->setText(qstrLeftSheet);
			// 设置参考点位置
			pTextItem_LeftSheet->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_LeftSheet->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26 - 28,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9));

			// 如果左图名为空，则此项不显示
			if (qstrLeftSheetMapName == "")
			{
				pTextItem_LeftSheet->setVisibility(false);
			}*/
#pragma endregion

#pragma region "左中图名"

			// 左中图名
			QgsLayoutItemLabel* pTextItem_LeftName = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_LeftName");
			pTextItem_LeftName->setVisibility(false);
			/*pTextItem_LeftName->setText(qstrLeftSheetMapName);
			// 设置参考点位置
			pTextItem_LeftName->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_LeftName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26 - 28,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 4.5));

			// 如果左中图名为空，则此项不显示
			if (qstrLeftSheetMapName == "")
			{
				pTextItem_LeftName->setVisibility(false);
			}*/
#pragma endregion

#pragma endregion

#pragma region "3-左下接图表"

			// 左下接图表
			QgsLayoutItemLabel* pTextItem_LeftBottomName = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_LeftBottomName");

			pTextItem_LeftBottomName->setText(qstrLeftBottomSheetMapName);

			// 设置参考点位置
			pTextItem_LeftBottomName->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_LeftBottomName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26 - 28,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 9));


#pragma endregion


#pragma region "【创建左下角图框要素】"

#pragma region "（3）左下角接图表矩形框"

			// 创建左下角图框要素，同时设置模板中图名要素不显示
			pTextItem_LeftBottomName->setVisibility(false);

			// 如果不存在，需要创建要素
			if (!bLayoutDecPolygonExisted)
			{
				// 参考点：右上角
				SE_DPoint dRefPoint;
				dRefPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26 - 28;
				dRefPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 9;


				// 左上接图表，依次左上角A、右上角B、右下角C、左下角D
				SE_DPoint dLayoutPoints[4];

				// 左上角
				dLayoutPoints[0].dx = dRefPoint.dx - 26;
				dLayoutPoints[0].dy = dRefPoint.dy;

				// 右上角
				dLayoutPoints[1].dx = dRefPoint.dx;
				dLayoutPoints[1].dy = dRefPoint.dy;

				// 右下角
				dLayoutPoints[2].dx = dRefPoint.dx;
				dLayoutPoints[2].dy = dRefPoint.dy + 9;

				// 左下角
				dLayoutPoints[3].dx = dRefPoint.dx - 26;
				dLayoutPoints[3].dy = dRefPoint.dy + 9;


				// 类型
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "adjsheet_leftbottom_mapname";		// 左下角图名
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstrLeftBottomSheetMapName.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 布局坐标转投影坐标
				SE_DPoint dGaussPoint[4];		// 高斯坐标
				double dValue[8] = { 0 };
				SE_DPoint dGeoPoint[4];			// 地理坐标
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				int iRet = 0;
				for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
				{
					TransformLayoutToGauss(
						interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						dLayoutPoints[iTempIndex],
						dMapUnit2LayoutUnit,
						dGaussPoint[iTempIndex]);

					dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
					dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

					// 高斯投影转地理坐标
					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
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
					continue;
				}
			}


#pragma endregion

#pragma endregion



#pragma region "4-中上接图表"

#pragma region "中上边框"
			// 中上接图表
			QgsLayoutItemLabel* pTextItem_TopRectangle = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_TopRectangle");

			// 如果中上图幅图名为空，则在中上边框中显示图幅号
			if (qstrTopSheetMapName == "")
			{
				pTextItem_TopRectangle->setText(qstrTopSheet);
			}

			// 设置参考点位置
			pTextItem_TopRectangle->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_TopRectangle->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3));


#pragma endregion

#pragma region "4-【创建中上图框要素】"

#pragma region "（4）中上接图表矩形框"

			// 创建中上接图表图框要素，同时设置模板中不显示
			pTextItem_TopRectangle->setVisibility(false);

			// 如果不存在，需要创建要素
			if (!bLayoutDecPolygonExisted)
			{
				// 参考点：右上角
				SE_DPoint dRefPoint;
				dRefPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26;
				dRefPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3;


				// 左上接图表，依次左上角A、右上角B、右下角C、左下角D
				SE_DPoint dLayoutPoints[4];

				// 左上角
				dLayoutPoints[0].dx = dRefPoint.dx - 28;
				dLayoutPoints[0].dy = dRefPoint.dy;

				// 右上角
				dLayoutPoints[1].dx = dRefPoint.dx;
				dLayoutPoints[1].dy = dRefPoint.dy;

				// 右下角
				dLayoutPoints[2].dx = dRefPoint.dx;
				dLayoutPoints[2].dy = dRefPoint.dy + 9;

				// 左下角
				dLayoutPoints[3].dx = dRefPoint.dx - 28;
				dLayoutPoints[3].dy = dRefPoint.dy + 9;

				// 类型
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "adjsheet_top_sheet_mapname";		// 中上接图表
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				// 显示图幅号及图名
				QString qstrLeft = tr("%1\n%2").arg(qstrTopSheet).arg(qstrTopSheetMapName);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstrLeft.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 布局坐标转投影坐标
				SE_DPoint dGaussPoint[4];		// 高斯坐标
				double dValue[8] = { 0 };
				SE_DPoint dGeoPoint[4];			// 地理坐标
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				int iRet = 0;
				for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
				{
					TransformLayoutToGauss(
						interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						dLayoutPoints[iTempIndex],
						dMapUnit2LayoutUnit,
						dGaussPoint[iTempIndex]);

					dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
					dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

					// 高斯投影转地理坐标
					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
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
					continue;
				}
			}




#pragma endregion

#pragma endregion


#pragma region "中上图幅号"

			// 中上图幅号
			QgsLayoutItemLabel* pTextItem_TopSheet = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_TopSheet");
			pTextItem_TopSheet->setVisibility(false);
			/*pTextItem_TopSheet->setText(qstrTopSheet);

			// 设置参考点位置
			pTextItem_TopSheet->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_TopSheet->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3));


			// 如果中上图幅图名为空，则不显示此项
			if (qstrTopSheetMapName == "")
			{
				pTextItem_TopSheet->setVisibility(false);
			}*/

#pragma endregion

#pragma region "中上图名"
			// 中上图名
			QgsLayoutItemLabel* pTextItem_TopName = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_TopName");
			pTextItem_TopName->setVisibility(false);

			/*pTextItem_TopName->setText(qstrTopSheetMapName);

			// 设置参考点位置
			pTextItem_TopName->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_TopName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 4.5));


			// 如果中上图幅图名为空，则不显示此项
			if (qstrTopSheetMapName == "")
			{
				pTextItem_TopName->setVisibility(false);
			}*/


#pragma endregion

#pragma endregion

#pragma region "5-正中接图表"

#pragma region "5-正中图框"

			// 正中图框
			QgsLayoutItemLabel* pTextItem_CenterRectangle = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_CenterRectangle");

			// 图名为空，显示图幅号
			if (qstrMapTitleName == "")
			{
				pTextItem_CenterRectangle->setText(qstrSheet);
			}

			// 设置参考点位置
			pTextItem_CenterRectangle->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_CenterRectangle->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9));

#pragma endregion



#pragma region "【创建正中图框要素】"

#pragma region "（5）正中接图表矩形框"

			// 创建正中接图表图框要素，同时设置模板中不显示
			pTextItem_CenterRectangle->setVisibility(false);

			// 如果不存在，需要创建要素
			if (!bLayoutDecPolygonExisted)
			{
				// 参考点：右上角
				SE_DPoint dRefPoint;
				dRefPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26;
				dRefPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9;


				// 左上接图表，依次左上角A、右上角B、右下角C、左下角D
				SE_DPoint dLayoutPoints[4];

				// 左上角
				dLayoutPoints[0].dx = dRefPoint.dx - 28;
				dLayoutPoints[0].dy = dRefPoint.dy;

				// 右上角
				dLayoutPoints[1].dx = dRefPoint.dx;
				dLayoutPoints[1].dy = dRefPoint.dy;

				// 右下角
				dLayoutPoints[2].dx = dRefPoint.dx;
				dLayoutPoints[2].dy = dRefPoint.dy + 9;

				// 左下角
				dLayoutPoints[3].dx = dRefPoint.dx - 28;
				dLayoutPoints[3].dy = dRefPoint.dy + 9;

				// 类型
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "adjsheet_center_sheet_mapname";		// 正中接图表
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				// 显示图幅号及图名
				QString qstrMapName = QString::fromLocal8Bit(strMapName.c_str());
				QString qstrCenter = tr("%1\n%2").arg(qstrSheet).arg(qstrMapName);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstrCenter.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 布局坐标转投影坐标
				SE_DPoint dGaussPoint[4];		// 高斯坐标
				double dValue[8] = { 0 };
				SE_DPoint dGeoPoint[4];			// 地理坐标
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				int iRet = 0;
				for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
				{
					TransformLayoutToGauss(
						interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						dLayoutPoints[iTempIndex],
						dMapUnit2LayoutUnit,
						dGaussPoint[iTempIndex]);

					dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
					dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

					// 高斯投影转地理坐标
					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
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
					continue;
				}
			}




#pragma endregion

#pragma endregion


#pragma region "正中图幅名"

			// 正中图幅名
			QgsLayoutItemLabel* pTextItem_CenterSheet = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_CenterSheet");
			pTextItem_CenterSheet->setVisibility(false);
			/*pTextItem_CenterSheet->setText(qstrSheet);

			// 设置参考点位置
			pTextItem_CenterSheet->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_CenterSheet->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9));

			if (qstrMapTitleName == "")
			{
				pTextItem_CenterSheet->setVisibility(false);
			}*/


#pragma endregion

#pragma region "正中图名"

			// 正中图名
			QgsLayoutItemLabel* pTextItem_CenterName = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_CenterName");
			pTextItem_CenterName->setVisibility(false);
			/*pTextItem_CenterName->setText(qstrMapTitleName);

			// 设置参考点位置
			pTextItem_CenterName->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_CenterName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 4.5));


			if (qstrMapTitleName == "")
			{
				pTextItem_CenterName->setVisibility(false);
			}*/

#pragma endregion
#pragma endregion

#pragma region "6-中下接图表"

#pragma region "中下图框"
			// 中下图框
			QgsLayoutItemLabel* pTextItem_BottomRectangle = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_BottomRectangle");

			if (qstrBottomSheetMapName == "")
			{
				pTextItem_BottomRectangle->setText(qstrBottomSheet);
			}

			// 设置参考点位置
			pTextItem_BottomRectangle->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_BottomRectangle->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 9));


#pragma endregion


#pragma region "6-【创建中下图框要素】"

#pragma region "（6）中下接图表矩形框"

			// 创建中下接图表图框要素，同时设置模板中不显示
			pTextItem_BottomRectangle->setVisibility(false);

			// 如果不存在，需要创建要素
			if (!bLayoutDecPolygonExisted)
			{
				// 参考点：右上角
				SE_DPoint dRefPoint;
				dRefPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26;
				dRefPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 9;


				// 中下接图表，依次左上角A、右上角B、右下角C、左下角D
				SE_DPoint dLayoutPoints[4];

				// 左上角
				dLayoutPoints[0].dx = dRefPoint.dx - 28;
				dLayoutPoints[0].dy = dRefPoint.dy;

				// 右上角
				dLayoutPoints[1].dx = dRefPoint.dx;
				dLayoutPoints[1].dy = dRefPoint.dy;

				// 右下角
				dLayoutPoints[2].dx = dRefPoint.dx;
				dLayoutPoints[2].dy = dRefPoint.dy + 9;

				// 左下角
				dLayoutPoints[3].dx = dRefPoint.dx - 28;
				dLayoutPoints[3].dy = dRefPoint.dy + 9;

				// 类型
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "adjsheet_bottom_sheet_mapname";		// 中下接图表
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				// 显示图幅号及图名
				QString qstrLeft = tr("%1\n%2").arg(qstrBottomSheet).arg(qstrBottomSheetMapName);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstrLeft.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 布局坐标转投影坐标
				SE_DPoint dGaussPoint[4];		// 高斯坐标
				double dValue[8] = { 0 };
				SE_DPoint dGeoPoint[4];			// 地理坐标
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				int iRet = 0;
				for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
				{
					TransformLayoutToGauss(
						interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						dLayoutPoints[iTempIndex],
						dMapUnit2LayoutUnit,
						dGaussPoint[iTempIndex]);

					dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
					dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

					// 高斯投影转地理坐标
					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
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
					continue;
				}
			}




#pragma endregion

#pragma endregion




#pragma region "中下图幅名"
			// 中下图幅名
			QgsLayoutItemLabel* pTextItem_BottomSheet = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_BottomSheet");
			pTextItem_BottomSheet->setVisibility(false);
			/*pTextItem_BottomSheet->setText(qstrBottomSheet);

			// 设置参考点位置
			pTextItem_BottomSheet->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_BottomSheet->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 9));


			if (qstrBottomSheetMapName == "")
			{
				pTextItem_BottomSheet->setVisibility(false);
			}*/

#pragma endregion

#pragma region "中下图名"
			// 中下图名
			QgsLayoutItemLabel* pTextItem_BottomName = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_BottomName");
			pTextItem_BottomName->setVisibility(false);
			/*pTextItem_BottomName->setText(qstrBottomSheetMapName);

			// 设置参考点位置
			pTextItem_BottomName->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_BottomName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth() - 26,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 9 + 4.5));


			if (qstrBottomSheetMapName == "")
			{
				pTextItem_BottomName->setVisibility(false);
			}*/
#pragma endregion


#pragma endregion


#pragma region "7-右上接图表"

			// 右上接图表
			QgsLayoutItemLabel* pTextItem_RightTopName = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_RightTopName");

			pTextItem_RightTopName->setText(qstrRightTopSheetMapName);

			// 设置参考点位置
			pTextItem_RightTopName->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_RightTopName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth(),
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3));

#pragma endregion

#pragma region "7-【创建右上角图框要素】"

#pragma region "（7）右上接图表矩形框"

			// 创建右上角图框要素，同时设置模板中图名要素不显示
			pTextItem_RightTopName->setVisibility(false);

			// 如果不存在，需要创建要素
			if (!bLayoutDecPolygonExisted)
			{
				// 参考点：右上角
				SE_DPoint dRefPoint;
				dRefPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth();
				dRefPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3;


				// 右上接图表，依次左上角A、右上角B、右下角C、左下角D
				SE_DPoint dLayoutPoints[4];

				// 左上角
				dLayoutPoints[0].dx = dRefPoint.dx - 26;
				dLayoutPoints[0].dy = dRefPoint.dy;

				// 右上角
				dLayoutPoints[1].dx = dRefPoint.dx;
				dLayoutPoints[1].dy = dRefPoint.dy;

				// 右下角
				dLayoutPoints[2].dx = dRefPoint.dx;
				dLayoutPoints[2].dy = dRefPoint.dy + 9;

				// 左下角
				dLayoutPoints[3].dx = dRefPoint.dx - 26;
				dLayoutPoints[3].dy = dRefPoint.dy + 9;


				// 类型
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "adjsheet_righttop_mapname";		// 右上角图名
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstrRightTopSheetMapName.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 布局坐标转投影坐标
				SE_DPoint dGaussPoint[4];		// 高斯坐标
				double dValue[8] = { 0 };
				SE_DPoint dGeoPoint[4];			// 地理坐标
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				int iRet = 0;
				for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
				{
					TransformLayoutToGauss(
						interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						dLayoutPoints[iTempIndex],
						dMapUnit2LayoutUnit,
						dGaussPoint[iTempIndex]);

					dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
					dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

					// 高斯投影转地理坐标
					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
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
					continue;
				}
			}




#pragma endregion

#pragma endregion





#pragma region "8-右中接图表"

#pragma region "右中图框"
			// 右中图框
			QgsLayoutItemLabel* pTextItem_RightRectangle = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_RightRectangle");

			if (qstrRightSheetMapName == "")
			{
				pTextItem_RightRectangle->setText(qstrRightSheet);
			}


			// 设置参考点位置
			pTextItem_RightRectangle->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_RightRectangle->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth(),
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9));


#pragma endregion

#pragma region "8-【创建右中图框要素】"

#pragma region "（8）右中接图表矩形框"

			// 创建右中接图表图框要素，同时设置模板中不显示
			pTextItem_RightRectangle->setVisibility(false);

			// 如果不存在，需要创建要素
			if (!bLayoutDecPolygonExisted)
			{
				// 参考点：右上角
				SE_DPoint dRefPoint;
				dRefPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth();
				dRefPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9;


				// 左上接图表，依次左上角A、右上角B、右下角C、左下角D
				SE_DPoint dLayoutPoints[4];

				// 左上角
				dLayoutPoints[0].dx = dRefPoint.dx - 26;
				dLayoutPoints[0].dy = dRefPoint.dy;

				// 右上角
				dLayoutPoints[1].dx = dRefPoint.dx;
				dLayoutPoints[1].dy = dRefPoint.dy;

				// 右下角
				dLayoutPoints[2].dx = dRefPoint.dx;
				dLayoutPoints[2].dy = dRefPoint.dy + 9;

				// 左下角
				dLayoutPoints[3].dx = dRefPoint.dx - 26;
				dLayoutPoints[3].dy = dRefPoint.dy + 9;

				// 类型
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "adjsheet_right_sheet_mapname";		// 右中接图表
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				// 显示图幅号及图名
				QString qstrRight = tr("%1\n%2").arg(qstrRightSheet).arg(qstrRightSheetMapName);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstrRight.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 布局坐标转投影坐标
				SE_DPoint dGaussPoint[4];		// 高斯坐标
				double dValue[8] = { 0 };
				SE_DPoint dGeoPoint[4];			// 地理坐标
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				int iRet = 0;
				for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
				{
					TransformLayoutToGauss(
						interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						dLayoutPoints[iTempIndex],
						dMapUnit2LayoutUnit,
						dGaussPoint[iTempIndex]);

					dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
					dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

					// 高斯投影转地理坐标
					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
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
					continue;
				}
			}




#pragma endregion

#pragma endregion












#pragma region "右中图幅名"
			// 右中图幅名
			QgsLayoutItemLabel* pTextItem_RightSheet = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_RightSheet");
			pTextItem_RightSheet->setVisibility(false);
			/*pTextItem_RightSheet->setText(qstrRightSheet);

			// 设置参考点位置
			pTextItem_RightSheet->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_RightSheet->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth(),
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9));

			if (qstrRightSheetMapName == "")
			{
				pTextItem_RightSheet->setVisibility(false);
			}*/

#pragma endregion

#pragma region "右中图名"
			// 右中图名
			QgsLayoutItemLabel* pTextItem_RightName = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_RightName");
			pTextItem_RightName->setVisibility(false);
			/*pTextItem_RightName->setText(qstrRightSheetMapName);

			// 设置参考点位置
			pTextItem_RightName->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_RightName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth(),
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 4.5));


			if (qstrRightSheetMapName == "")
			{
				pTextItem_RightName->setVisibility(false);
			}*/

#pragma endregion


#pragma endregion

#pragma region "9-右下接图表"

			// 左下接图表
			QgsLayoutItemLabel* pTextItem_RightBottomName = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList,
				"TextItem_RightBottomName");

			pTextItem_RightBottomName->setText(qstrRightBottomSheetMapName);

			// 设置参考点位置
			pTextItem_RightBottomName->setReferencePoint(QgsLayoutItem::UpperRight);

			// 设置参考点坐标
			pTextItem_RightBottomName->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth(),
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 9));

#pragma endregion


#pragma region "9-【创建右下角图框要素】"

#pragma region "（9）右下接图表矩形框"

			// 创建右下角图框要素，同时设置模板中图名要素不显示
			pTextItem_RightBottomName->setVisibility(false);

			// 如果不存在，需要创建要素
			if (!bLayoutDecPolygonExisted)
			{
				// 参考点：右上角
				SE_DPoint dRefPoint;
				dRefPoint.dx = m_dExteriorLeftTopLayoutPoint.dx + OUTER_MAP_INTERIOR_MAP_DISTANCE + m_dInteriorMapLayoutMBR.GetWidth();
				dRefPoint.dy = m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 3 + 9 + 9;


				// 右下接图表，依次左上角A、右上角B、右下角C、左下角D
				SE_DPoint dLayoutPoints[4];

				// 左上角
				dLayoutPoints[0].dx = dRefPoint.dx - 26;
				dLayoutPoints[0].dy = dRefPoint.dy;

				// 右上角
				dLayoutPoints[1].dx = dRefPoint.dx;
				dLayoutPoints[1].dy = dRefPoint.dy;

				// 右下角
				dLayoutPoints[2].dx = dRefPoint.dx;
				dLayoutPoints[2].dy = dRefPoint.dy + 9;

				// 左下角
				dLayoutPoints[3].dx = dRefPoint.dx - 26;
				dLayoutPoints[3].dy = dRefPoint.dy + 9;


				// 类型
				vector<FieldNameAndValue_Imp> vFieldNameAndValue;
				vFieldNameAndValue.clear();

				FieldNameAndValue_Imp fieldImp;
				fieldImp.strFieldName = "TYPE";
				fieldImp.strFieldValue = "adjsheet_rightbottom_mapname";		// 右下角图名
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "FLAG";
				fieldImp.strFieldValue = "1";
				vFieldNameAndValue.push_back(fieldImp);

				fieldImp.strFieldName = "VALUE";
				fieldImp.strFieldValue = qstrRightBottomSheetMapName.toLocal8Bit();
				vFieldNameAndValue.push_back(fieldImp);

				// 布局坐标转投影坐标
				SE_DPoint dGaussPoint[4];		// 高斯坐标
				double dValue[8] = { 0 };
				SE_DPoint dGeoPoint[4];			// 地理坐标
				vector<SE_DPoint> vPoints;
				vPoints.clear();
				int iRet = 0;
				for (int iTempIndex = 0; iTempIndex < 4; iTempIndex++)
				{
					TransformLayoutToGauss(
						interiorMapGaussPoint_A,
						interiorMapLayoutPoint_A,
						dLayoutPoints[iTempIndex],
						dMapUnit2LayoutUnit,
						dGaussPoint[iTempIndex]);

					dValue[2 * iTempIndex] = dGaussPoint[iTempIndex].dx;
					dValue[2 * iTempIndex + 1] = dGaussPoint[iTempIndex].dy;

					// 高斯投影转地理坐标
					iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
						GaussKruger,
						dCenterMeridian,
						0,
						0,
						0,
						0,
						500000,
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
					continue;
				}
			}




#pragma endregion

#pragma endregion










		}


		GDALClose(poLayoutDecorationPolygonDS);
#pragma endregion

#pragma region "【19】计算内外图廓之间四个角的8条线并绘制"

#pragma region "计算内外图廓之间的连接线与外图廓的8个交点布局坐标"

		SE_DPoint dEightCornerLayoutPoints[9];

		// 计算内图廓AB与外图廓ad的交点（1）
		dEightCornerLayoutPoints[1] = LineIntersect(
			interiorMapLayoutPoint_A,
			interiorMapLayoutPoint_B,
			exteriorMapLayoutPoint_a,
			exteriorMapLayoutPoint_d);

		// 计算内图廓AD与外图廓ab的交点（2）
		dEightCornerLayoutPoints[2] = LineIntersect(
			interiorMapLayoutPoint_A,
			interiorMapLayoutPoint_D,
			exteriorMapLayoutPoint_a,
			exteriorMapLayoutPoint_b);

		// 计算内图廓BC与外图廓ab的交点（3）
		dEightCornerLayoutPoints[3] = LineIntersect(
			interiorMapLayoutPoint_B,
			interiorMapLayoutPoint_C,
			exteriorMapLayoutPoint_a,
			exteriorMapLayoutPoint_b);

		// 计算内图廓AB与外图廓bc的交点（4）
		dEightCornerLayoutPoints[4] = LineIntersect(
			interiorMapLayoutPoint_A,
			interiorMapLayoutPoint_B,
			exteriorMapLayoutPoint_b,
			exteriorMapLayoutPoint_c);


		// 计算内图廓DC与外图廓bc的交点（5）
		dEightCornerLayoutPoints[5] = LineIntersect(
			interiorMapLayoutPoint_D,
			interiorMapLayoutPoint_C,
			exteriorMapLayoutPoint_b,
			exteriorMapLayoutPoint_c);

		// 计算内图廓BC与外图廓cd的交点（6）
		dEightCornerLayoutPoints[6] = LineIntersect(
			interiorMapLayoutPoint_B,
			interiorMapLayoutPoint_C,
			exteriorMapLayoutPoint_c,
			exteriorMapLayoutPoint_d);

		// 计算内图廓AD与外图廓cd的交点（7）
		dEightCornerLayoutPoints[7] = LineIntersect(
			interiorMapLayoutPoint_A,
			interiorMapLayoutPoint_D,
			exteriorMapLayoutPoint_c,
			exteriorMapLayoutPoint_d);

		// 计算内图廓DC与外图廓ad的交点（8）
		dEightCornerLayoutPoints[8] = LineIntersect(
			interiorMapLayoutPoint_D,
			interiorMapLayoutPoint_C,
			exteriorMapLayoutPoint_a,
			exteriorMapLayoutPoint_d);


#pragma endregion

#pragma region "依次绘制8条短线"

		// 8条短线与内图廓线样式保持一致

#pragma region "绘制A-(1)短线"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_A.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_A.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = dEightCornerLayoutPoints[1].dx;
			dLayoutPointTo.dy = dEightCornerLayoutPoints[1].dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "left_top_line_1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}

#pragma endregion

#pragma region "绘制A-(2)短线"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_A.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_A.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = dEightCornerLayoutPoints[2].dx;
			dLayoutPointTo.dy = dEightCornerLayoutPoints[2].dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "left_top_line_2";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}


#pragma endregion

#pragma region "绘制B-(3)短线"

		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_B.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_B.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = dEightCornerLayoutPoints[3].dx;
			dLayoutPointTo.dy = dEightCornerLayoutPoints[3].dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "right_top_line_1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}


#pragma endregion

#pragma region "绘制B-(4)短线"
		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_B.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_B.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = dEightCornerLayoutPoints[4].dx;
			dLayoutPointTo.dy = dEightCornerLayoutPoints[4].dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "right_top_line_2";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}
#pragma endregion

#pragma region "绘制C-(5)短线"
		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_C.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_C.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = dEightCornerLayoutPoints[5].dx;
			dLayoutPointTo.dy = dEightCornerLayoutPoints[5].dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "right_bottom_line_1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}
#pragma endregion

#pragma region "绘制C-(6)短线"
		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_C.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_C.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = dEightCornerLayoutPoints[6].dx;
			dLayoutPointTo.dy = dEightCornerLayoutPoints[6].dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "right_bottom_line_2";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}
#pragma endregion

#pragma region "绘制D-(7)短线"
		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_D.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_D.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = dEightCornerLayoutPoints[7].dx;
			dLayoutPointTo.dy = dEightCornerLayoutPoints[7].dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "left_bottom_line_1";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}
#pragma endregion

#pragma region "绘制D-(8)短线"
		if (!bLayoutLayerLineExisted)
		{
			// 起始布局点
			SE_DPoint dLayoutPointFrom;
			dLayoutPointFrom.dx = interiorMapLayoutPoint_D.dx;
			dLayoutPointFrom.dy = interiorMapLayoutPoint_D.dy;

			// 终止布局点
			SE_DPoint dLayoutPointTo;
			dLayoutPointTo.dx = dEightCornerLayoutPoints[8].dx;
			dLayoutPointTo.dy = dEightCornerLayoutPoints[8].dy;

			// 类型
			vector<FieldNameAndValue_Imp> vFieldNameAndValue;
			vFieldNameAndValue.clear();

			FieldNameAndValue_Imp fieldImp;
			fieldImp.strFieldName = "TYPE";
			fieldImp.strFieldValue = "left_bottom_line_2";
			vFieldNameAndValue.push_back(fieldImp);

			fieldImp.strFieldName = "FLAG";
			fieldImp.strFieldValue = "1";
			vFieldNameAndValue.push_back(fieldImp);

			// 布局坐标转投影坐标
			SE_DPoint dFromGaussPoint;
			double dFromValue[2] = { 0 };

			SE_DPoint dToGaussPoint;
			double dToValue[2] = { 0 };

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointFrom,
				dMapUnit2LayoutUnit,
				dFromGaussPoint);

			dFromValue[0] = dFromGaussPoint.dx;
			dFromValue[1] = dFromGaussPoint.dy;

			TransformLayoutToGauss(
				interiorMapGaussPoint_A,
				interiorMapLayoutPoint_A,
				dLayoutPointTo,
				dMapUnit2LayoutUnit,
				dToGaussPoint);

			dToValue[0] = dToGaussPoint.dx;
			dToValue[1] = dToGaussPoint.dy;

			// 高斯投影转地理坐标
			int iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dFromValue);

			if (iRet != 1)
			{
				continue;
			}

			iRet = CSE_GeoTransformation::Proj2Geo_2(CGCS2000,
				GaussKruger,
				dCenterMeridian,
				0,
				0,
				0,
				0,
				500000,
				0,
				1,
				dToValue);

			if (iRet != 1)
			{
				continue;
			}

			SE_DPoint dFromGeoPoint;
			dFromGeoPoint.dx = dFromValue[0];
			dFromGeoPoint.dy = dFromValue[1];

			SE_DPoint dToGeoPoint;
			dToGeoPoint.dx = dToValue[0];
			dToGeoPoint.dy = dToValue[1];

			vector<SE_DPoint> vLine;
			vLine.clear();
			vLine.push_back(dFromGeoPoint);
			vLine.push_back(dToGeoPoint);
			iRet = Set_LineString(poLayoutLineLayer, vLine, vFieldNameAndValue);
			if (iRet != 0)
			{
				continue;
			}
		}
#pragma endregion

#pragma endregion
		GDALClose(poLayoutLineDS);
		GDALClose(poLayoutPointDS);
#pragma endregion


#pragma region "将外图廓整饰点要素图层加入到主地图中"

		if (!bLayoutDecPointExisted)
		{
			// 布局点图层路径
			QString qstrLayoutLayerDecPointPath = m_ImageMappingSchema.qstrMapExportPath + "/" + qstrSheet + "/" + qstrSheet + "_DecPoint.shp";

			// 获取布局点样式文件路径
			QString qstrLayoutDecPointQmlFilePath;


			GetLayoutDecPointQmlFilePath(dScale, qstrLayoutDecPointQmlFilePath);


			// 判断图层是否存在
			// 如果布局点图层文件存在
			if (QFileInfo::exists(qstrLayoutLayerDecPointPath))
			{

				/*将矢量图层加载到地图图层列表中*/
				QString fileNameVector = qstrLayoutLayerDecPointPath;
				QFileInfo fileInfoVector(fileNameVector);

				// 获取数据名称
				QString baseNameVector = fileInfoVector.completeBaseName();

				// 通过图层类型获取对应的样式文件全路径
				QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称

				/*获取qml名称*/


				QgsVectorLayer* pVecotrLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
				if (!pVecotrLayer->isValid())
				{
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				/*图层已有的样式*/
				QgsMapLayerStyle oldStyle = pVecotrLayer->styleManager()->style(pVecotrLayer->styleManager()->currentStyle());
				QString message;
				bool defaultLoadedFlag = false;
				message = pVecotrLayer->loadNamedStyle(qstrLayoutDecPointQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

				/*if (!defaultLoadedFlag)
				{
					continue;
				}*/

				QgsMapLayer* pLayer = pProject->addMapLayer(pVecotrLayer);
				if (!pLayer)
				{
					delete pVecotrLayer;
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				qMapLayerList.append(pLayer);
				qMainMapLayerList.append(pLayer);
			}
		}

#pragma endregion

#pragma region "将外图廓整饰面要素图层加入到主地图中"

		if (!bLayoutDecPolygonExisted)
		{
			// 整饰面要素图层路径
			QString qstrLayoutLayerDecPolygonPath = m_ImageMappingSchema.qstrMapExportPath + "/" + qstrSheet + "/" + qstrSheet + "_DecPolygon.shp";

			// 获取布局点样式文件路径
			QString qstrLayoutDecPolygonQmlFilePath;


			GetLayoutDecPolygonQmlFilePath(dScale, qstrLayoutDecPolygonQmlFilePath);


			// 判断图层是否存在
			// 如果布局点图层文件存在
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


				QgsVectorLayer* pVecotrLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
				if (!pVecotrLayer->isValid())
				{
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				/*图层已有的样式*/
				QgsMapLayerStyle oldStyle = pVecotrLayer->styleManager()->style(pVecotrLayer->styleManager()->currentStyle());
				QString message;
				bool defaultLoadedFlag = false;
				message = pVecotrLayer->loadNamedStyle(qstrLayoutDecPolygonQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

				/*if (!defaultLoadedFlag)
				{
					continue;
				}*/

				QgsMapLayer* pLayer = pProject->addMapLayer(pVecotrLayer);
				if (!pLayer)
				{
					delete pVecotrLayer;
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				qMapLayerList.append(pLayer);
				qMainMapLayerList.append(pLayer);
			}
		}

#pragma endregion



#pragma region "将整饰方里网点、线图层加入到主地图中"
		// 方里网线
		if (!bLayoutLayerLineExisted)
		{
			// 布局线图层路径
			QString qstrLayoutLayerLinePath = m_ImageMappingSchema.qstrMapExportPath + "/" + qstrSheet + "/" + qstrSheet + "_LayoutLine.shp";

			// 获取布局点样式文件路径
			QString qstrLayoutPointQmlFilePath;
			QString qstrLayoutLineQmlFilePath;

			GetLayoutQmlFilePath(dScale, qstrLayoutPointQmlFilePath, qstrLayoutLineQmlFilePath);


			// 获取布局线样式文件路径
			// 判断图层是否存在
			// 如果布局线图层文件存在
			if (QFileInfo::exists(qstrLayoutLayerLinePath))
			{
				bLayoutLayerLineExisted = true;

				/*将矢量图层加载到地图图层列表中*/
				QString fileNameVector = qstrLayoutLayerLinePath;
				QFileInfo fileInfoVector(fileNameVector);

				// 获取数据名称
				QString baseNameVector = fileInfoVector.completeBaseName();

				// 通过图层类型获取对应的样式文件全路径
				QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称


				QgsVectorLayer* pVecotrLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
				if (!pVecotrLayer->isValid())
				{
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				/*图层已有的样式*/
				QgsMapLayerStyle oldStyle = pVecotrLayer->styleManager()->style(pVecotrLayer->styleManager()->currentStyle());
				QString message;
				bool defaultLoadedFlag = false;
				message = pVecotrLayer->loadNamedStyle(qstrLayoutLineQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

				if (!defaultLoadedFlag)
				{
					continue;
				}

				QgsMapLayer* pLayer = pProject->addMapLayer(pVecotrLayer);
				if (!pLayer)
				{
					delete pVecotrLayer;
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				qMapLayerList.append(pLayer);
				qMainMapLayerList.append(pLayer);
			}
		}

		// 方里网点
		if (!bLayoutLayerPointExisted)
		{
			// 布局点图层路径
			QString qstrLayoutLayerPointPath = m_ImageMappingSchema.qstrMapExportPath + "/" + qstrSheet + "/" + qstrSheet + "_LayoutPoint.shp";

			// 获取布局点样式文件路径
			QString qstrLayoutPointQmlFilePath;
			QString qstrLayoutLineQmlFilePath;

			GetLayoutQmlFilePath(dScale, qstrLayoutPointQmlFilePath, qstrLayoutLineQmlFilePath);


			// 判断图层是否存在
			// 如果布局点图层文件存在
			if (QFileInfo::exists(qstrLayoutLayerPointPath))
			{
				bLayoutLayerPointExisted = true;

				/*将矢量图层加载到地图图层列表中*/
				QString fileNameVector = qstrLayoutLayerPointPath;
				QFileInfo fileInfoVector(fileNameVector);

				// 获取数据名称
				QString baseNameVector = fileInfoVector.completeBaseName();

				// 通过图层类型获取对应的样式文件全路径
				QString qstrBaseName = fileInfoVector.baseName();       // 去除所有扩展名的图层名称

				/*获取qml名称*/


				QgsVectorLayer* pVecotrLayer = new QgsVectorLayer(fileNameVector, baseNameVector, "ogr");
				if (!pVecotrLayer->isValid())
				{
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("矢量图层不合法:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				/*图层已有的样式*/
				QgsMapLayerStyle oldStyle = pVecotrLayer->styleManager()->style(pVecotrLayer->styleManager()->currentStyle());
				QString message;
				bool defaultLoadedFlag = false;
				message = pVecotrLayer->loadNamedStyle(qstrLayoutPointQmlFilePath, defaultLoadedFlag, true, QgsMapLayer::Symbology | QgsMapLayer::Labeling | QgsMapLayer::Fields);

				if (!defaultLoadedFlag)
				{
					continue;
				}

				QgsMapLayer* pLayer = pProject->addMapLayer(pVecotrLayer);
				if (!pLayer)
				{
					delete pVecotrLayer;
					// 记录日志
					QgsMessageLog::logMessage((QString(tr("增加矢量图层失败:")) + fileNameVector), tr("影像图制图"), Qgis::Warning);
					continue;
				}

				qMapLayerList.append(pLayer);
				qMainMapLayerList.append(pLayer);
			}

		}

		/*主地图最后加载影像*/
		qMainMapLayerList.append(pLayerRaster);
		pMainMapItem->setLayers(qMainMapLayerList);

#pragma endregion


#pragma region "【20】根据要素选择状态绘制图例"

		// TODO:待完善，需要根据地图窗口中实际显示的要素类型，自动加载图例，目前先将所有图例显示
		vector<QString> vFeatureClassCodes;
		vFeatureClassCodes.clear();

		for (int iIndex = 0; iIndex < m_ImageMappingSchema.vFeatureClass.size(); iIndex++)
		{
			FeatureClassParam param = m_ImageMappingSchema.vFeatureClass[iIndex];
			for (int m = 0; m < param.vFeatureClassCode.size(); m++)
			{
				vFeatureClassCodes.push_back(param.vFeatureClassCode[m]);
			}
		}

#pragma region "主地图图例列表"

		// 图例名称列表
		vector<MainMapLegendInfo> vMainMapLegends;
		vMainMapLegends.clear();

#pragma region "注记层"

		// 【R】注记图层：首都：280111；省：280121；市：280131；县：280141；乡：280102
		// 首都：280111
		MainMapLegendInfo legend_280111;
		legend_280111.qstrSymbolCode = "280111";
		QgsLayoutItemPicture* pPictureItem_280111 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_280111");
		QgsLayoutItemLabel* pLabelItem_280111 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_280111");
		legend_280111.pSymbolPicture = pPictureItem_280111;
		legend_280111.pSymbolLabel = pLabelItem_280111;
		vMainMapLegends.push_back(legend_280111);

		// 省：280121
		MainMapLegendInfo legend_280121;
		legend_280121.qstrSymbolCode = "280121";
		QgsLayoutItemPicture* pPictureItem_280121 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_280121");
		QgsLayoutItemLabel* pLabelItem_280121 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_280121");
		legend_280121.pSymbolPicture = pPictureItem_280121;
		legend_280121.pSymbolLabel = pLabelItem_280121;
		vMainMapLegends.push_back(legend_280121);

		// 市：280131
		MainMapLegendInfo legend_280131;
		legend_280131.qstrSymbolCode = "280131";
		QgsLayoutItemPicture* pPictureItem_280131 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_280131");
		QgsLayoutItemLabel* pLabelItem_280131 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_280131");
		legend_280131.pSymbolPicture = pPictureItem_280131;
		legend_280131.pSymbolLabel = pLabelItem_280131;
		vMainMapLegends.push_back(legend_280131);

		// 县：280141
		MainMapLegendInfo legend_280141;
		legend_280141.qstrSymbolCode = "280141";
		QgsLayoutItemPicture* pPictureItem_280141 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_280141");
		QgsLayoutItemLabel* pLabelItem_280141 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_280141");
		legend_280141.pSymbolPicture = pPictureItem_280141;
		legend_280141.pSymbolLabel = pLabelItem_280141;
		vMainMapLegends.push_back(legend_280141);

		// 乡：280102
		MainMapLegendInfo legend_280102;
		legend_280102.qstrSymbolCode = "280102";
		QgsLayoutItemPicture* pPictureItem_280102 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_280102");
		QgsLayoutItemLabel* pLabelItem_280102 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_280102");
		legend_280102.pSymbolPicture = pPictureItem_280102;
		legend_280102.pSymbolLabel = pLabelItem_280102;
		vMainMapLegends.push_back(legend_280102);


#pragma endregion

#pragma region "境界图层"

		// 【K】境界图层：国界：210101；省界：210201；地级行政区界线：210203；县级行政区界线：210205；乡级行政区界线：210207
		// 国界：210101
		MainMapLegendInfo legend_210101;
		legend_210101.qstrSymbolCode = "210101";
		QgsLayoutItemPicture* pPictureItem_210101 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_210101");
		QgsLayoutItemLabel* pLabelItem_210101 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_210101");
		legend_210101.pSymbolPicture = pPictureItem_210101;
		legend_210101.pSymbolLabel = pLabelItem_210101;
		vMainMapLegends.push_back(legend_210101);

		// 省界：210201
		MainMapLegendInfo legend_210201;
		legend_210201.qstrSymbolCode = "210201";
		QgsLayoutItemPicture* pPictureItem_210201 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_210201");
		QgsLayoutItemLabel* pLabelItem_210201 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_210201");
		legend_210201.pSymbolPicture = pPictureItem_210201;
		legend_210201.pSymbolLabel = pLabelItem_210201;
		vMainMapLegends.push_back(legend_210201);

		// 地级行政区界线：210203
		MainMapLegendInfo legend_210203;
		legend_210203.qstrSymbolCode = "210203";
		QgsLayoutItemPicture* pPictureItem_210203 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_210203");
		QgsLayoutItemLabel* pLabelItem_210203 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_210203");
		legend_210203.pSymbolPicture = pPictureItem_210203;
		legend_210203.pSymbolLabel = pLabelItem_210203;
		vMainMapLegends.push_back(legend_210203);

		// 县级行政区界线：210205
		MainMapLegendInfo legend_210205;
		legend_210205.qstrSymbolCode = "210205";
		QgsLayoutItemPicture* pPictureItem_210205 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_210205");
		QgsLayoutItemLabel* pLabelItem_210205 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_210205");
		legend_210205.pSymbolPicture = pPictureItem_210205;
		legend_210205.pSymbolLabel = pLabelItem_210205;
		vMainMapLegends.push_back(legend_210205);

		// 乡级行政区界线：210207
		MainMapLegendInfo legend_210207;
		legend_210207.qstrSymbolCode = "210207";
		QgsLayoutItemPicture* pPictureItem_210207 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_210207");
		QgsLayoutItemLabel* pLabelItem_210207 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_210207");
		legend_210207.pSymbolPicture = pPictureItem_210207;
		legend_210207.pSymbolLabel = pLabelItem_210207;
		vMainMapLegends.push_back(legend_210207);

#pragma endregion

#pragma region "陆地交通图层"

		// 【D】陆地交通图层：单线铁路：140102；复线铁路：140101；城市轻轨：140108；国道：140305；
		//            省道：140307；县道：140309；乡道：140311；机耕路：140401；小路：140403

		// 单线铁路：140102
		MainMapLegendInfo legend_140102;
		legend_140102.qstrSymbolCode = "140102";
		QgsLayoutItemPicture* pPictureItem_140102 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_140102");
		QgsLayoutItemLabel* pLabelItem_140102 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_140102");
		legend_140102.pSymbolPicture = pPictureItem_140102;
		legend_140102.pSymbolLabel = pLabelItem_140102;
		vMainMapLegends.push_back(legend_140102);

		// 复线铁路：140101
		MainMapLegendInfo legend_140101;
		legend_140101.qstrSymbolCode = "140101";
		QgsLayoutItemPicture* pPictureItem_140101 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_140101");
		QgsLayoutItemLabel* pLabelItem_140101 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_140101");
		legend_140101.pSymbolPicture = pPictureItem_140101;
		legend_140101.pSymbolLabel = pLabelItem_140101;
		vMainMapLegends.push_back(legend_140101);

		// 城市轻轨：140108
		MainMapLegendInfo legend_140108;
		legend_140108.qstrSymbolCode = "140108";
		QgsLayoutItemPicture* pPictureItem_140108 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_140108");
		QgsLayoutItemLabel* pLabelItem_140108 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_140108");
		legend_140108.pSymbolPicture = pPictureItem_140108;
		legend_140108.pSymbolLabel = pLabelItem_140108;
		vMainMapLegends.push_back(legend_140108);

		// 国道：140305
		MainMapLegendInfo legend_140305;
		legend_140305.qstrSymbolCode = "140305";
		QgsLayoutItemPicture* pPictureItem_140305 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_140305");
		QgsLayoutItemLabel* pLabelItem_140305 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_140305");
		legend_140305.pSymbolPicture = pPictureItem_140305;
		legend_140305.pSymbolLabel = pLabelItem_140305;
		vMainMapLegends.push_back(legend_140305);


		// 省道：140307
		MainMapLegendInfo legend_140307;
		legend_140307.qstrSymbolCode = "140307";
		QgsLayoutItemPicture* pPictureItem_140307 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_140307");
		QgsLayoutItemLabel* pLabelItem_140307 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_140307");
		legend_140307.pSymbolPicture = pPictureItem_140307;
		legend_140307.pSymbolLabel = pLabelItem_140307;
		vMainMapLegends.push_back(legend_140307);


		// 县道：140309
		MainMapLegendInfo legend_140309;
		legend_140309.qstrSymbolCode = "140309";
		QgsLayoutItemPicture* pPictureItem_140309 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_140309");
		QgsLayoutItemLabel* pLabelItem_140309 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_140309");
		legend_140309.pSymbolPicture = pPictureItem_140309;
		legend_140309.pSymbolLabel = pLabelItem_140309;
		vMainMapLegends.push_back(legend_140309);


		// 乡道：140311
		MainMapLegendInfo legend_140311;
		legend_140311.qstrSymbolCode = "140311";
		QgsLayoutItemPicture* pPictureItem_140311 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_140311");
		QgsLayoutItemLabel* pLabelItem_140311 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_140311");
		legend_140311.pSymbolPicture = pPictureItem_140311;
		legend_140311.pSymbolLabel = pLabelItem_140311;
		vMainMapLegends.push_back(legend_140311);


		// 机耕路：140401
		MainMapLegendInfo legend_140401;
		legend_140401.qstrSymbolCode = "140401";
		QgsLayoutItemPicture* pPictureItem_140401 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_140401");
		QgsLayoutItemLabel* pLabelItem_140401 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_140401");
		legend_140401.pSymbolPicture = pPictureItem_140401;
		legend_140401.pSymbolLabel = pLabelItem_140401;
		vMainMapLegends.push_back(legend_140401);


		// 小路：140403
		MainMapLegendInfo legend_140403;
		legend_140403.qstrSymbolCode = "140403";
		QgsLayoutItemPicture* pPictureItem_140403 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_140403");
		QgsLayoutItemLabel* pLabelItem_140403 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_140403");
		legend_140403.pSymbolPicture = pPictureItem_140403;
		legend_140403.pSymbolLabel = pLabelItem_140403;
		vMainMapLegends.push_back(legend_140403);



#pragma endregion

#pragma region "陆地地貌图层"

		// 【J】陆地地貌图层：高程点：200201；计曲线：200102
		// 高程点：200201
		MainMapLegendInfo legend_200201;
		legend_200201.qstrSymbolCode = "200201";
		QgsLayoutItemPicture* pPictureItem_200201 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_200201");
		QgsLayoutItemLabel* pLabelItem_200201 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_200201");
		legend_200201.pSymbolPicture = pPictureItem_200201;
		legend_200201.pSymbolLabel = pLabelItem_200201;
		vMainMapLegends.push_back(legend_200201);


		// 计曲线：200102
		MainMapLegendInfo legend_200102;
		legend_200102.qstrSymbolCode = "200102";
		QgsLayoutItemPicture* pPictureItem_200102 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_200102");
		QgsLayoutItemLabel* pLabelItem_200102 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_200102");
		legend_200102.pSymbolPicture = pPictureItem_200102;
		legend_200102.pSymbolLabel = pLabelItem_200102;
		vMainMapLegends.push_back(legend_200102);

#pragma endregion

#pragma region "水域陆地图层"

		// 【F】水域陆地图层：面状要素：160201
		// 面状要素：160201
		MainMapLegendInfo legend_160201;
		legend_160201.qstrSymbolCode = "160201";
		QgsLayoutItemPicture* pPictureItem_160201 = (QgsLayoutItemPicture*)GetLayoutItemByName(qLayoutItemList, "PictureItem_160201");
		QgsLayoutItemLabel* pLabelItem_160201 = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_160201");
		legend_160201.pSymbolPicture = pPictureItem_160201;
		legend_160201.pSymbolLabel = pLabelItem_160201;
		vMainMapLegends.push_back(legend_160201);
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

		// 最后一个图例左下角点布局纵坐标
		double dLastLegendLeftBottomLayoutPointY = dFirstLegendLeftTopLayoutPoint.dy + vMainMapLegends.size() * 6 + 4;









#pragma endregion





		//

#pragma endregion

#pragma region "【21】绘制彩色圆形"

#pragma region "如果是1万模板"

		// 如果是1万比例尺，颜色分别为：黑、蓝、红、黄
		if (dScale == SCALE_10K)
		{
#pragma region "【11】黑色圆形"

			// 黑色圆点
			QgsLayoutItemLabel* pEllipse_Black = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "Ellipse_Black");
			pEllipse_Black->setVisibility(true);

			// 设置位置
			pEllipse_Black->setReferencePoint(QgsLayoutItem::Middle);

			// 设置参考点坐标
			pEllipse_Black->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0 - 12,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 21.5));

			// 设置水平对齐方式
			pEllipse_Black->setHAlign(Qt::AlignHCenter);

#pragma endregion

#pragma region "【12】蓝色圆形"

			QgsLayoutItemLabel* pEllipse_Blue = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "Ellipse_Blue");

			pEllipse_Blue->setVisibility(true);

			// 设置位置
			pEllipse_Blue->setReferencePoint(QgsLayoutItem::Middle);

			// 设置参考点坐标
			pEllipse_Blue->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0 - 4,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 21.5));

			// 设置水平对齐方式
			pEllipse_Blue->setHAlign(Qt::AlignHCenter);


#pragma endregion

#pragma region "【13】红色圆形"

			QgsLayoutItemLabel* pEllipse_Red = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "Ellipse_Red");
			pEllipse_Red->setVisibility(true);

			// 设置位置
			pEllipse_Red->setReferencePoint(QgsLayoutItem::Middle);

			// 设置参考点坐标
			pEllipse_Red->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0 + 4,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 21.5));

			// 设置水平对齐方式
			pEllipse_Red->setHAlign(Qt::AlignHCenter);

#pragma endregion

#pragma region "【14】黄色圆形"

			QgsLayoutItemLabel* pEllipse_Yellow = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "Ellipse_Yellow");
			pEllipse_Yellow->setVisibility(true);

			// 设置位置
			pEllipse_Yellow->setReferencePoint(QgsLayoutItem::Middle);

			// 设置参考点坐标
			pEllipse_Yellow->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() / 2.0 + 12,
				m_dExteriorLeftTopLayoutPoint.dy + m_dExteriorMapLayoutMBR.GetHeight() + 21.5));

			// 设置水平对齐方式
			pEllipse_Yellow->setHAlign(Qt::AlignHCenter);

#pragma endregion

		}

		// 如果是5万比例尺，颜色分别为：灰、蓝、桔、绿
		else if (dScale == SCALE_50K)
		{
#pragma region "【11】灰色圆形"

			// 灰色圆点
			QgsLayoutItemLabel* pEllipse_Gray = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "Ellipse_Gray");
			pEllipse_Gray->setVisibility(true);

			// 设置位置
			pEllipse_Gray->setReferencePoint(QgsLayoutItem::UpperLeft);

			// 设置参考点坐标
			pEllipse_Gray->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() + GRAY_CIRLE_OFFSET_X,
				dLastLegendLeftBottomLayoutPointY + 5));

			// 设置水平对齐方式
			pEllipse_Gray->setHAlign(Qt::AlignHCenter);

#pragma endregion

#pragma region "【12】蓝色圆形"

			QgsLayoutItemLabel* pEllipse_Blue = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "Ellipse_Blue");

			pEllipse_Blue->setVisibility(true);

			// 设置位置
			pEllipse_Blue->setReferencePoint(QgsLayoutItem::UpperLeft);

			// 半径4.5mm + 间距5.0mm
			// 设置参考点坐标
			pEllipse_Blue->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() + GRAY_CIRLE_OFFSET_X + 9.5,
				dLastLegendLeftBottomLayoutPointY + 5));

			// 设置水平对齐方式
			pEllipse_Blue->setHAlign(Qt::AlignHCenter);


#pragma endregion

#pragma region "【13】桔色圆形"

			QgsLayoutItemLabel* pEllipse_Orange = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "Ellipse_Orange");
			pEllipse_Orange->setVisibility(true);

			// 设置位置
			pEllipse_Orange->setReferencePoint(QgsLayoutItem::UpperLeft);

			// 设置参考点坐标
			pEllipse_Orange->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() + GRAY_CIRLE_OFFSET_X + 9.5 * 2,
				dLastLegendLeftBottomLayoutPointY + 5));

			// 设置水平对齐方式
			pEllipse_Orange->setHAlign(Qt::AlignHCenter);

#pragma endregion

#pragma region "【14】绿色圆形"

			QgsLayoutItemLabel* pEllipse_Green = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "Ellipse_Green");
			pEllipse_Green->setVisibility(true);

			// 设置位置
			pEllipse_Green->setReferencePoint(QgsLayoutItem::UpperLeft);

			// 设置参考点坐标
			pEllipse_Green->attemptMove(QgsLayoutPoint(
				m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() + GRAY_CIRLE_OFFSET_X + 9.5 * 3,
				dLastLegendLeftBottomLayoutPointY + 5));

			// 设置水平对齐方式
			pEllipse_Green->setHAlign(Qt::AlignHCenter);

#pragma endregion
		}


#pragma endregion





#pragma endregion


#pragma region "【22】附注"

		QgsLayoutItemLabel* pMapAnnotationItem = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "TextItem_Annotations");

		// 设置附注
		pMapAnnotationItem->setText(GetMapAnnotation());

		pMapAnnotationItem->attemptMove(QgsLayoutPoint(m_dExteriorLeftTopLayoutPoint.dx + m_dExteriorMapLayoutMBR.GetWidth() + MAP_ANNOTATION_OFFSET_X,
			dLastLegendLeftBottomLayoutPointY + 15));


#pragma endregion

#pragma region "【23】镶嵌线图例（图例、摄影时间、传感器）"

		// 设置字体及大小
		QFont fontMosaicLegend;
		fontMosaicLegend.setFamily(QStringLiteral("等线"));
		fontMosaicLegend.setPixelSize(35);
		fontMosaicLegend.setBold(false);

#pragma region "颜色"




		// 镶嵌线图例左上角布局点坐标
		SE_DPoint dMosaicLegendLeftTopCornerLayoutPoint;

		// 镶嵌线图例左上角横坐标为镶嵌线地图右侧5毫米
		dMosaicLegendLeftTopCornerLayoutPoint.dx = interiorMapLayoutPoint_D.dx + MOSAIC_MAP_LEFT_X + MOSAIC_MAP_WIDTH + 3;
		dMosaicLegendLeftTopCornerLayoutPoint.dy = m_dExteriorLeftBottomLayoutPoint.dy + 3;

		// 调整颜色标题位置
		QgsLayoutItemLabel* pLabelItem_MosaicColor = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_MosaicColor");
		pLabelItem_MosaicColor->attemptMove(QgsLayoutPoint(dMosaicLegendLeftTopCornerLayoutPoint.dx,
			dMosaicLegendLeftTopCornerLayoutPoint.dy));


		// 创建颜色图例
		for (int iCateIndex = 0; iCateIndex < vCateColor.size(); iCateIndex++)
		{
			QgsLayoutItemLabel* pColorLabel = new QgsLayoutItemLabel(pPrintLayout);
			pColorLabel->attemptSetSceneRect(QRectF(dMosaicLegendLeftTopCornerLayoutPoint.dx,
				dMosaicLegendLeftTopCornerLayoutPoint.dy + (iCateIndex + 1) * 6,
				9,
				4));

			pColorLabel->setFrameEnabled(true);
			pColorLabel->setBackgroundEnabled(true);
			pColorLabel->setBackgroundColor(vCateColor[iCateIndex]);
			pColorLabel->setVisibility(true);

			pPrintLayout->addLayoutItem(pColorLabel);
		}



#pragma endregion


#pragma region "摄影时间"

		// LabelItem_MosaicAcaDate
		// 调整摄影时间标题位置
		QgsLayoutItemLabel* pLabelItem_MosaicAcaDate = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_MosaicAcaDate");
		pLabelItem_MosaicAcaDate->attemptMove(QgsLayoutPoint(dMosaicLegendLeftTopCornerLayoutPoint.dx + 16,
			dMosaicLegendLeftTopCornerLayoutPoint.dy));

		// 创建摄影时间文本
		for (int iCateIndex = 0; iCateIndex < vCateAcqDate.size(); iCateIndex++)
		{
			QgsLayoutItemLabel* pAcqDateLabel = new QgsLayoutItemLabel(pPrintLayout);
			pAcqDateLabel->attemptSetSceneRect(QRectF(dMosaicLegendLeftTopCornerLayoutPoint.dx + 13,
				dMosaicLegendLeftTopCornerLayoutPoint.dy + (iCateIndex + 1) * 6,
				18,
				4));
			pAcqDateLabel->setText(vCateAcqDate[iCateIndex]);
			pAcqDateLabel->setVisibility(true);
			pAcqDateLabel->setFont(fontMosaicLegend);
			pPrintLayout->addLayoutItem(pAcqDateLabel);
		}



#pragma endregion

#pragma region "传感器"

		// LabelItem_MosaicSensor
		// 调整摄影时间标题位置
		QgsLayoutItemLabel* pLabelItem_MosaicSensor = (QgsLayoutItemLabel*)GetLayoutItemByName(qLayoutItemList, "LabelItem_MosaicSensor");
		pLabelItem_MosaicSensor->attemptMove(QgsLayoutPoint(dMosaicLegendLeftTopCornerLayoutPoint.dx + 33,
			dMosaicLegendLeftTopCornerLayoutPoint.dy));

		// 创建传感器文本
		for (int iCateIndex = 0; iCateIndex < vCateSensor.size(); iCateIndex++)
		{
			QgsLayoutItemLabel* pSensorLabel = new QgsLayoutItemLabel(pPrintLayout);
			pSensorLabel->attemptSetSceneRect(QRectF(dMosaicLegendLeftTopCornerLayoutPoint.dx + 35,
				dMosaicLegendLeftTopCornerLayoutPoint.dy + (iCateIndex + 1) * 6,
				13,
				4));
			pSensorLabel->setText(vCateSensor[iCateIndex]);
			pSensorLabel->setVisibility(true);
			pSensorLabel->setFont(fontMosaicLegend);
			pPrintLayout->addLayoutItem(pSensorLabel);
		}


#pragma endregion





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
			QgsMessageLog::logMessage(tr("获取布局页面失败！"), tr("影像图制图"), Qgis::Critical);
			continue;
		}

		// TODO:默认页面尺寸，可通过配置文件获取
		double width = 900;     // 单位：毫米
		double height = 750;    // 单位：毫米
		QgsUnitTypes::LayoutUnit units = QgsUnitTypes::LayoutMillimeters;
		QgsLayoutSize pageSize = QgsLayoutSize(width, height, units);
		pPage->setPageSize(pageSize);

		// 导出地图路径下为每个图幅创建一个导出数据文件夹，包括地图文件、工程名称、影像图元数据
		QString qstrMapSavePath = m_ImageMappingSchema.qstrMapExportPath + "/" + qstrSheet;
		string strMapSavePath = qstrMapSavePath.toLocal8Bit();
#ifdef OS_FAMILY_WINDOWS
		_mkdir(strMapSavePath.c_str());
#else
#define MODE (S_IRWXU | S_IRWXG | S_IRWXO)
		mkdir(strMapSavePath.c_str(), MODE);
#endif

		// 导出地图文件全路径
		QString qstrMapFileName;
		if (m_ImageMappingSchema.qstrMapExportType == "GeoPDF"
			|| m_ImageMappingSchema.qstrMapExportType == "PDF")
		{
			qstrMapFileName = qstrMapSavePath + "/" + qstrSheet + ".pdf";
		}
		else if (m_ImageMappingSchema.qstrMapExportType == "tiff")
		{
			qstrMapFileName = qstrMapSavePath + "/" + qstrSheet + ".tiff";
		}

		/*-------------------根据导出类型导出地图-----------*/

		bResult = ExportMapToFile(qstrMapFileName, m_ImageMappingSchema.qstrMapExportType, pPrintLayout);
		if (!bResult)
		{
			QgsMessageLog::logMessage(tr("导出地图文件失败！"), tr("影像图制图"), Qgis::Critical);
			continue;
		}

		/*--------------导出元数据文件--------------*/
		// 生成元数据xlsx文件
		QString qstrMetaDataFilePath = qstrMapSavePath + "/" + qstrSheet + ".xlsx";
		ExportImageMetaData(qstrMetaDataFilePath,
			m_ImageMappingSchema,
			dSheetBoxLeft,
			dSheetBoxRight,
			dSheetBoxTop,
			dSheetBoxBottom,
			dSheetPoints,
			dCenterMeridian,
			dImageResolution);

		/*---------------导出工程文件--------------*/
		pProject->setFilePathStorage(Qgis::FilePathType::Absolute);
		QString qstrProjectName = qstrMapSavePath + "/" + qstrSheet + ".qgz";

		QFileInfo info(qstrProjectName);
		pProject->setFileName(qstrProjectName);

		bResult = pProject->write();
		if (!bResult)
		{
			QgsMessageLog::logMessage(tr("%1工程文件保存失败！").arg(qstrProjectName), tr("影像图制图"), Qgis::Critical);
			continue;
		}

		// 设置进度条
		ui.progressBar->setValue(int(100.0 * (int(i) + 1) / vMapSheetList.size()));



	}

#pragma endregion

	ui.progressBar->setValue(100);



#pragma endregion

	// 设置当前保存路径
	QgsSettings settings;

	// 地图输出路径
	settings.setValue(QStringLiteral("QgsImageMappingDialog_LayoutVectorization/ImageMappingDataSavePath"), m_qstrImageMappingDataSavePath, QgsSettings::Section::Plugins);
	settings.setValue(QStringLiteral("QgsImageMappingDialog_LayoutVectorization/ImageMappingSchemaSavePath"), m_qstrImageMappingSchemaSavePath, QgsSettings::Section::Plugins);
}


int QgsImageMappingDialog_LayoutVectorization::SetFieldDefn(OGRLayer* poLayer,
	vector<string> fieldname,
	vector<OGRFieldType> fieldtype,
	vector<int> fieldwidth)
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

		pField->SetWidth(fieldwidth[i]);		//设置字段宽度，实际操作需要根据不同字段设置不同长度
		OGRErr err = poLayer->CreateField(pField);

		if (err != OGRERR_NONE)
		{
			printf("CreateField failed!\n");
			return -1;
		}
	}
	return 0;
}



bool QgsImageMappingDialog_LayoutVectorization::CreateShapefileCPG(string strCPGFilePath, string strEncoding /*= "System"*/)
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



int QgsImageMappingDialog_LayoutVectorization::Set_Point(OGRLayer* poLayer, double x, double y, double z, vector<FieldNameAndValue_Imp>& vFieldAndValue)
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


int QgsImageMappingDialog_LayoutVectorization::Set_LineString(OGRLayer* poLayer, vector<SE_DPoint> Line, vector<FieldNameAndValue_Imp>& vFieldAndValue)
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
	OGRLineString pLine;

	for (int i = 0; i < Line.size(); i++)
		pLine.addPoint(Line[i].dx, Line[i].dy, Line[i].dz);
	poFeature->SetGeometry((OGRGeometry*)(&pLine));
	if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
	{
		return -1;
	}
	OGRFeature::DestroyFeature(poFeature);
	return 0;
}

int QgsImageMappingDialog_LayoutVectorization::Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing, vector<FieldNameAndValue_Imp>& vFieldAndValue)
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
