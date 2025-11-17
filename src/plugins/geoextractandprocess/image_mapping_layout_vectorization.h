#pragma once
#ifndef QGS_IMAGE_MAPPING_DIALOG_LAYOUT_VECTORIZATION_H
#define QGS_IMAGE_MAPPING_DIALOG_LAYOUT_VECTORIZATION_H

#include <QDialog>
#include <map>
#include <vector>
#include <string>
#include "image_mapping.h"
#include "ui_image_mapping_layout_vectorization.h"
#include "qgslayoutitemmap.h"
#include "qgslayoutexporter.h"
#include "qgsprintlayout.h"
#include "se_commontypedef.h"
#include "qgslayoutitempolyline.h"
#include "qgslayoutitem.h"
#include "qgslayoutitempicture.h"
#include "qgslayoutitemlabel.h"
#include <QtGui/qcolor.h>


#include <gdal.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>
#include <gdal_version.h>
#include <cpl_error.h>
#include <cpl_string.h>
#include "qgsogrutils.h"
#include <tchar.h>
#include <ogrsf_frmts.h>
#include <ogr_feature.h>
#include <gdal_priv.h>

using namespace std;

/*要素类名称-编码映射*/
typedef map<QString, QString> MAP_FeatureClassName_Code;

/*要素类编码-名称映射*/
typedef map<QString, QString> MAP_FeatureClassCode_Name;

/*布局整饰要素结构体*/
// struct LayoutItemParam
// {
// 	QString     qstrMapTitleName;                   // 主标题图名
// 	QString     qstrMapSubTitleProvince;            // 地图副标题省名
// 	QString     qstrMapSubTitleCounty;              // 地图副标题县名
// 	bool        bMapSubTitleChecked;                // 是否绘制地图副标题

// 	QString     qstrSecurityClassification;         // 密级
// 	QString     qstrMapDetails;                     // 地图说明
// 	bool        bMapDetailsChecked;                 // 是否绘制地图说明
// 	QString     qstrMapAgency;                      // 制图单位
// 	bool        bMapAgencyChecked;                  // 是否绘制制图单位

// 	QString     qstrLeftTopCornerSheet;             // 左上角图幅号
// 	bool        bLeftTopCornerSheetChecked;         // 是否绘制左上角图幅号
// 	QString     qstrRightBottomCornerSheet;         // 右下角图幅号
// 	bool        bRightBottomCornerSheetChecked;     // 是否绘制右下角图幅号

// 	QString     qstrScale;                          // 地图比例尺
// 	bool        bScaleChecked;                      // 是否绘制地图比例尺

// 	bool        bKmNetChecked;                      // 是否绘制方里网
// 	bool        bSlopeRulerChecked;                 // 是否绘制坡度尺
// 	bool        bNorthDirectionLinesChecked;        // 是否绘制三北方向线

// 	double      dMagenticAngle;                     // 磁偏角，单位：度
// 	double      dMeridianConvergenceAngle;          // 子午线收敛角

// 	bool        bSheetListChecked;                  // 是否绘制接图表
// 	bool        bMosaicMapChecked;                  // 是否绘制镶嵌线缩略图

// 	bool        bMainMapLegendChecked;              // 是否绘制主地图图例
// 	bool        bMosaicMapLegendChecked;            // 是否绘制镶嵌线地图图例

// 	QString     qstrMapAnnotations;                 // 附注
// 	bool        bAnnotationsChecked;                // 是否绘制附注

// 	LayoutItemParam()
// 	{
// 		qstrMapTitleName = "";
// 		qstrMapSubTitleProvince = "";
// 		qstrMapSubTitleCounty = "";
// 		bMapSubTitleChecked = true;

// 		qstrSecurityClassification = "";
// 		qstrMapDetails = "";
// 		bMapDetailsChecked = true;
// 		qstrMapAgency = "";
// 		bMapAgencyChecked = true;

// 		qstrLeftTopCornerSheet = "";
// 		bLeftTopCornerSheetChecked = true;
// 		qstrRightBottomCornerSheet = "";
// 		bRightBottomCornerSheetChecked = true;

// 		qstrScale = "";
// 		bScaleChecked = true;

// 		bKmNetChecked = true;
// 		bSlopeRulerChecked = false;
// 		bNorthDirectionLinesChecked = false;

// 		dMagenticAngle = 0;
// 		dMeridianConvergenceAngle = 0;

// 		bSheetListChecked = true;
// 		bMosaicMapChecked = true;
// 		bMainMapLegendChecked = true;
// 		bMosaicMapLegendChecked = true;

// 		qstrMapAnnotations = "";
// 		bAnnotationsChecked = false;
// 	}
// };

/*要素选择参数结构体*/
// struct FeatureClassParam
// {
// 	QString qstrLayerName;				// 图层名称：GJB标准图层名称，如"A"
// 	vector<QString> vFeatureClassCode;	// 当前图层内要素类选择列表，通过编码区分，例如：测量控制点中的三角点（110101）

// 	FeatureClassParam()
// 	{
// 		qstrLayerName = "";
// 		vFeatureClassCode.clear();
// 	}
// };

/*制图方案结构体*/
// struct ImageMappingSchema
// {
// 	QString     qstrName;                       // 制图方案名称

// 	/*数据源*/
// 	QString     qstrVectorDataPath;             // 矢量数据路径
// 	QString     qstrImageDataPath;              // 影像数据路径
// 	QString     qstrMosaicDataPath;             // 镶嵌线数据路径

// 	/*制图范围*/
// 	double      dLeft;                          // 左边界经度，单位：度
// 	double      dTop;                           // 上边界纬度，单位：度
// 	double      dRight;                         // 右边界经度，单位：度
// 	double      dBottom;                        // 下边界纬度，单位：度

// 	/*矢量要素选择*/
// 	vector<FeatureClassParam>        vFeatureClass;     // 图层要素类列表

// 	/*布局模板*/
// 	QString     qstrLayoutTemplatePath;         // 布局模板路径
// 	QString     qstrLayoutTemplateName;         // 布局模板名称

// 	/*布局整饰要素*/
// 	LayoutItemParam     layoutItemParam;        // 布局整饰要素

// 	/*地图输出*/
// 	QString     qstrMapExportType;                 // 地图输出类型：包括GeoPDF、PDF、tiff
// 	QString     qstrMapExportPath;                 // 地图输出路径，包括工程文件、元数据文件、地图文件

// 	ImageMappingSchema()
// 	{
// 		qstrVectorDataPath = "";
// 		qstrImageDataPath = "";
// 		qstrMosaicDataPath = "";

// 		dLeft = 0;
// 		dTop = 0;
// 		dRight = 0;
// 		dBottom = 0;

// 		vFeatureClass.clear();
// 		qstrLayoutTemplatePath = "";
// 		qstrLayoutTemplateName = "";
// 		layoutItemParam = LayoutItemParam();

// 		qstrMapExportType = "";
// 		qstrMapExportPath = "";
// 	}
// };

/*制图布局元素位置结构体*/
// struct LayoutItemPositions
// {
// 	QString             qstrName;               // 布局元素名称
// 	QString             qstrAlias;              // 布局元素别名
// 	double              dLeft_offset;           // 相对地图外图廓左上角点的横坐标方向偏移量，单位：毫米
// 	double              dTop_offset;            // 相对地图外图廓左上角点的纵坐标方向偏移量，单位：毫米
// 	double              dWidth;                 // 布局元素宽度，单位：毫米
// 	double              dHeight;                // 布局元素高度，单位：毫米
// 	QString             qstrReferencePoint;     // 布局元素参考点位置，单位：毫米

// 	LayoutItemPositions()
// 	{
// 		qstrName = "";
// 		qstrAlias = "";
// 		dLeft_offset = 0;
// 		dTop_offset = 0;
// 		dWidth = 0;
// 		dHeight = 0;
// 		qstrReferencePoint = "";
// 	}
// };

/*影像图元数据项结构体*/
// struct ImageMetaDataItem
// {
// 	string         strCategory;       // 元数据类别，包括：产品信息、安全信息、空间坐标信息、数据质量信息、其他信息、分发信息
// 	string         strName;           // 元数据项名称
// 	string         strType;           // 元数据类型，包括：字符型、数值型、整型
// 	int            iLength;            // 长度
// 	string         strValue;          // 元数据项属性值

// 	ImageMetaDataItem()
// 	{
// 		strCategory = "";
// 		strName = "";
// 		strType = "";
// 		iLength = 0;
// 		strValue = "";
// 	}
// };

/*影像图元数据结构体*/
/*struct ImageMetaData
{
	vector<ImageMetaDataItem> vItems;       // 元数据项集合

	ImageMetaData()
	{
		vItems.clear();
	}
};*/

/*主图图例信息结构体*/
// struct MainMapLegendInfo
// {
// 	QString						qstrSymbolCode;			// 符号编码：如首都：280111
// 	QgsLayoutItemPicture*		pSymbolPicture;			// 符号示意图
// 	QgsLayoutItemLabel*			pSymbolLabel;			// 符号名称

// 	MainMapLegendInfo()
// 	{
// 		qstrSymbolCode = "";
// 		pSymbolPicture = nullptr;
// 		pSymbolLabel = nullptr;
// 	}

// };

/*镶嵌线图例信息结构体*/
// struct MosaicMapLegendInfo
// {
// 	QColor					qColor;					// 颜色
// 	QString					qstrData;				// 摄影时间
// 	QString					qstrSensor;				// 传感器

// 	MosaicMapLegendInfo()
// 	{
// 		qColor = QColor(0,0,0);
// 		qstrData = "";
// 		qstrSensor = "";
// 	}

// };


/*属性字段属性值对照结构体*/
// struct FieldNameAndValue_Imp
// {
// 	string strFieldName;
// 	string strFieldValue;
// 	FieldNameAndValue_Imp()
// 	{
// 		strFieldName = "";
// 		strFieldValue = "";
// 	}

// };


class QgsImageMappingDialog_LayoutVectorization : public QDialog
{
	Q_OBJECT

public:
	QgsImageMappingDialog_LayoutVectorization(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsImageMappingDialog_LayoutVectorization() override;

private:

	Ui_QgsImageMappingDlg_LayoutVectorization ui;

	// 矢量数据存储路径
	QString m_qstrVectorDataPath;

	// 影像数据存储路径
	QString m_qstrImageDataPath;

	// 镶嵌线数据存储路径
	QString m_qstrMosaicDataPath;

	// 影像图制图数据保存路径，包括：元数据、影像图、工程文件等
	QString m_qstrImageMappingDataSavePath;

	// 影像图制图方案保存路径
	QString m_qstrImageMappingSchemaSavePath;

	// 图层D、F、J、K、R
	vector<QString> m_vStrLayerName;

	// 编码—名称映射表
	MAP_FeatureClassCode_Name m_mCode_Names;

	// 名称—编码映射表
	MAP_FeatureClassName_Code m_mName_Codes;

	// 制图方案列表
	vector<ImageMappingSchema> m_vImageMappingSchema;

	// 当前所选制图方案
	ImageMappingSchema m_ImageMappingSchema;

	// 地图外图廓左上角点布局坐标，单位：毫米
	SE_DPoint m_MapOutlineLeftTopCornerLayoutPoint;

	// 地图内图廓尺寸（布局单位）
	QRectF m_qMainMapItemLayoutRect;

	// 地图外图廓尺寸（布局单位）
	QRectF m_qMapOutLineItemLayoutRect;

	//---------------------------------------//
	// 地图内图廓四个角点构成的多边形布局坐标（实际多边形内图廓），依次存储：左上角点-A、右上角点-B、右下角点-C、左下角点-D
	SE_Polygon		m_InteriorMapLayoutPolygon;

	// 地图内图廓四个角点构成的多边形高斯坐标（实际多边形内图廓），依次存储：左上角点-A、右上角点-B、右下角点-C、左下角点-D
	SE_Polygon		m_InteriorMapGaussPolygon;

	// 地图内图廓尺寸左上角的布局坐标（矩形内图廓），作为其它布局要素的起算原点，单位：毫米
	SE_DPoint		m_InteriorMapLeftTopLayoutPoint;

	// 地图内图廓尺寸左上角的高斯坐标（矩形内图廓），作为其它布局要素的起算原点，单位：毫米
	SE_DPoint		m_InteriorMapLeftTopGaussPoint;

	//------------------------------------------//
	// 地图外图廓四个角点构成的多边形布局坐标（实际多边形外图廓），依次存储：左上角点-a、右上角点-b、右下角点-c、左下角点-d
	SE_Polygon		m_ExteriorMapLayoutPolygon;

	// 地图外图廓四个角点构成的多边形高斯坐标（实际多边形外图廓），依次存储：左上角点-a、右上角点-b、右下角点-c、左下角点-d
	SE_Polygon		m_ExteriorMapGaussPolygon;

	// 内图廓A点布局坐标
	SE_DPoint		m_dInteriorMapLayoutPoint_A;

	// 内图廓B点布局坐标
	SE_DPoint		m_dInteriorMapLayoutPoint_B;

	// 内图廓C点布局坐标
	SE_DPoint		m_dInteriorMapLayoutPoint_C;

	// 内图廓D点布局坐标
	SE_DPoint		m_dInteriorMapLayoutPoint_D;


	// 外图廓a点布局坐标
	SE_DPoint		m_dExteriorMapLayoutPoint_a;

	// 外图廓b点布局坐标
	SE_DPoint		m_dExteriorMapLayoutPoint_b;

	// 外图廓c点布局坐标
	SE_DPoint		m_dExteriorMapLayoutPoint_c;

	// 外图廓d点布局坐标
	SE_DPoint		m_dExteriorMapLayoutPoint_d;

	// 外图廓布局坐标最小外接矩形（矩形）
	SE_DRect		m_dExteriorMapLayoutMBR;

	// 内图廓布局坐标最小外接矩形（矩形）
	SE_DRect		m_dInteriorMapLayoutMBR;

	// 外图廓最小外接矩形左上角点布局坐标
	SE_DPoint		m_dExteriorLeftTopLayoutPoint;

	// 外图廓最小外接矩形左下角点布局坐标
	SE_DPoint		m_dExteriorLeftBottomLayoutPoint;

	// 外图廓最小外接矩形右上角点布局坐标
	SE_DPoint		m_dExteriorRightTopLayoutPoint;

	// 外图廓最小外接矩形右下角点布局坐标
	SE_DPoint		m_dExteriorRightBottomLayoutPoint;


	// 内图廓最小外接矩形左上角点布局坐标
	SE_DPoint		m_dInteriorLeftTopLayoutPoint;

	// 内图廓最小外接矩形左下角点布局坐标
	SE_DPoint		m_dInteriorLeftBottomLayoutPoint;

	// 内图廓最小外接矩形右上角点布局坐标
	SE_DPoint		m_dInteriorRightTopLayoutPoint;

	// 内图廓最小外接矩形右下角点布局坐标
	SE_DPoint		m_dInteriorRightBottomLayoutPoint;

	//-----------------------------------------//



signals:

private slots:

	// 关闭对话框
	void Close();

	// 浏览矢量数据按钮
	void OpenVectorData();

	// 浏览影像图数据按钮
	void OpenImageData();

	// 浏览镶嵌线数据按钮
	void OpenMosaicData();

	// 保存路径按钮
	void ImageMappingDataSave();

	// 根据制图方案更新对话框界面
	void UpdateUIByImageMappingSchema(const QModelIndex& index);

	// 经纬度范围单选按钮
	void LonLatRadioButton_Click();

	// 图幅列表单选按钮
	void SheetListRadioButton_Click();

	// 从文本文件加载图幅列表
	void LoadSheetListFile();

	// 加载制图方案
	void LoadMappingSchema();

	// 保存制图方案
	void SaveMappingSchema();

	// 影像图制图
	void ImageMapping_LayoutVectorization();

private:

	// 根据名称获取布局元素
	QgsLayoutItem* GetLayoutItemByName(QList<QgsLayoutItem*>& ItemList, const QString qstrName);

	// 获取矢量数据存储路径
	QString GetVectorDataPath();

	// 获取影像数据存储路径
	QString GetImageDataPath();

	// 获取镶嵌线数据存储路径
	QString GetMosaicDataPath();

	// 获取数据保存路径
	QString GetDataSavePath();

	// 获取地图输出格式
	QString GetMapExportType();

	// 获取主标题-图幅名
	QString GetMainTitleSheet();

	// 获取主标题-图名
	QString GetMainTitleName();

	// 获取副标题-省名
	QString GetSubTitleProvinceName();

	// 获取副标题-县名
	QString GetSubTitleCountyName();

	// 获取密级
	QString GetSecurityClassification();

	// 获取地图说明
	QString GetMapDetail();

	// 获取制图单位
	QString GetMapAgency();

	// 获取左上角图幅号
	QString GetLeftTopCornerSheet();

	// 获取右下角图幅号
	QString GetRightBottomCornerSheet();

	// 获取比例尺
	QString GetScale();

	// 获取磁偏角
	double GetMagenticAngle();

	// 获取子午线收敛角
	double GetMeridianConvergenceAngle();

	// 获取地图附注
	QString GetMapAnnotation();

	// 获取布局模板名称
	QString GetLayoutTemplateName();

	// 获取布局模板全路径
	QString GetLayoutTemplateFullPath();

	// 获取制图范围
	void GetImageMappingRange(double& dLeft, double& dTop, double& dRight, double& dBottom);

	// 设置要素类列表框的全部不选择状态
	void SetListWidgetAllUnSelectedStatus(const QString& strLayerName);

	// 初始化要素类名称和编码的映射表
	void InitFeatureClassNameCodeTable();

	// 获取某一图层字段选择状态
	void GetLayerFeatureClassChecked(string strLayerType, vector<string>& vFeatureClassCode);

	// 通过配置文件初始化制图方案（配置文件为xml文件）
	void InitImageMappingSchema();

	// 获取目录下指定扩展名的文件
	QStringList GetFileNamesByExtName(const QString& path, const QString& extName);

	// 加载制图方案xml文件
	bool LoadImageMappingSchemaXml(const QString& xmlPath, vector<ImageMappingSchema>& vSchema);

	// 根据制图方案xml文件设置界面状态
	void SetUIByImageMappingSchema(const ImageMappingSchema& schema);

	// 设置要素选择列表框的状态
	void SetListWidgetStatus(FeatureClassParam& param);

	// 获取要素选择列表框的状态
	void GetListWidgetStatus(const QString& qstrLayerType, FeatureClassParam& param);

	// 判断列表中元素是否在要素类列表中
	bool bIsInFeatureClasses(QString qstrText, vector<QString>& vFeatureClassCode);

	// 地图文件导出到文件：GeoPDF、PDF、tiff格式
	bool ExportMapToFile(const QString& filePath,
		const QString& qstrExportType,
		QgsPrintLayout* layout);

	// 毫米转像素
	void ConvertMilliMeterToPixel(double w, double h, int dpi, int& x, int& y);

	// 像素转毫米
	void ConvertPixeltoMilliMeter(int x, int y, int dpi, double& w, double& h);

	// 恢复保存参数
	void restoreState();

	// 根据制图范围及制图模板计算制图范围内对应比例尺图幅列表
	void GetSheetListByMappingRange(const QString& qstrTemplateName,
		double dLeft,
		double dTop,
		double dRight,
		double dBottom,
		vector<QString>& qstrSheetList);

	// 根据制图方案获取矢量图层全路径列表
	void GetVectorLayerPathList(const ImageMappingSchema& schema,
		const QString& qstrSheet,
		vector<QString>& vPath);

	// 根据经度值及分带方式计算中央经线
	double CalCenterMeridian(double dLong, double dScale);

	// 获取最小外接矩形
	void GetMBR(int iCount, double* pPoints, double& dLeft, double& dTop, double& dRight, double& dBottom);

	// 根据中央经线和比例尺类型获取空间参考系编码（预定义中国区域）
	// 1w比例尺使用3度分带，5w比例尺使用6度分带
	QString GetEPSGCodeByCenterMeridianAndScale(double dLong, double dScale);

	// 度转度分秒
	void Degree2DMS(double degree, int& iDegree, int& iMinute, int& iSecond);

	// 导出影像图元数据信息
	bool ExportImageMetaData(const QString& filePath,
		ImageMappingSchema& schema,
		double dLeft,
		double dRight,
		double dTop,
		double dBottom,
		double dValue[],
		double dCenterMeridian,
		double dImageResolution);

	// 规范化度°显示
	QString FormatDegree(int iDegree);

	// 规范化度°显示
	QString FormatDegree2(int iDegree);

	// 规范化分′显示
	QString FormatMinute(int iMinute);

	// 规范化分
	QString FormatMinute2(int iMinute);

	// 规范化秒″显示
	QString FormatSecond(int iSecond);

	// 规范化秒
	QString FormatSecond2(int iSecond);

	// 读取影像图元数据项配置文件
	bool LoadImageMappingMetaDataConfigFile(string& strCategory,
		string& strName,
		string& strType,
		string& strLength,
		string& strValue,
		ImageMetaData& data);

	// 增加邻带方里网绘制
	void DrawGridOfNeighboringZone(QString qstrSheet,
		QgsPrintLayout* pLayout,
		QList< QgsLayoutItem* > layoutItemList,
		QList<QgsMapLayer*> mapLayerList,
		QString qstrEPSGCode,
		double dScale);

	// 通过图层名称获取样式文件全路径
	QString GetQmlFilePathFromLayerName(double dScale, const QString& qstrLayerName);

	// 获取镶嵌线图层的样式文件路径
	QString GetMosaicQmlFilePath(double dScale);

	// 获取布局点、线样式配置文件
	void GetLayoutQmlFilePath(double dScale, QString& qstrLayoutPointQmlPath, QString& qstrLayoutLineQmlPath);

	// 获取图外整饰点样式文件
	void GetLayoutDecPointQmlFilePath(double dScale, QString& qstrLayoutDecPointQmlPath);

	// 获取图外整饰面样式文件
	void GetLayoutDecPolygonQmlFilePath(double dScale, QString& qstrLayoutDecPolygonQmlPath);

	/* 根据地图投影范围计算内图廓上、下、左、右边界整公里坐标点集合
	* @param dRange:                     内图廓高斯投影范围
	* @param vLeftBorderCoords：         左边界高斯投影点坐标集合
	* @param vTopBorderCoords：          上边界高斯投影点坐标集合
	* @param vRightBorderCoords：        右边界高斯投影点坐标集合
	* @param vBottomBorderCoords：       下边界高斯投影点坐标集合
	*/
	void CalGaussBorderCoords(const SE_DRect& dRange,
		vector<SE_DPoint>& vLeftBorderCoords,
		vector<SE_DPoint>& vTopBorderCoords,
		vector<SE_DPoint>& vRightBorderCoords,
		vector<SE_DPoint>& vBottomBorderCoords);

	/*计算上、下、左、右边界点在布局中的位置，mm为单位,页面左上角为（0,0）
	*
	* @param dLeftTopCornerGaussPoint:          地图内图廓左上角点高斯坐标值，单位为米
	* @param dLeftTopCornerLayoutPoint:         地图内图廓左上角点在布局中的位置，单位为毫米
	* @param dLeftBottomCornerGaussPoint:       地图内图廓左下角点高斯坐标值，单位为米
	* @param dLeftBottomCornerLayoutPoint:      地图内图廓左下角点在布局中的位置，单位为毫米
	* @param dRightTopCornerGaussPoint:         地图内图廓右上角点高斯坐标值，单位为米
	* @param dRightTopCornerLayoutPoint:        地图内图廓右上角点在布局中的位置，单位为毫米
	* @param dRightBottomCornerGaussPoint:      地图内图廓右下角点高斯坐标值，单位为米
	* @param dRightBottomCornerLayoutPoint:     地图内图廓右下角点在布局中的位置，单位为毫米
	* @param dMapUnitToLayoutUnit:              地图到布局单位的变换系数
	* @param vLeftBorderCoords：                左边界高斯投影点坐标集合
	* @param vTopBorderCoords：                 上边界高斯投影点坐标集合
	* @param vRightBorderCoords：               右边界高斯投影点坐标集合
	* @param vBottomBorderCoords：              下边界高斯投影点坐标集合
	* @param vLeftBorderLayoutCoords：          左边界布局位置点坐标集合
	* @param vTopBorderLayoutCoords：           上边界布局位置点坐标集合
	* @param vRightBorderLayoutCoords：         右边界布局位置点坐标集合
	* @param vBottomBorderLayoutCoords：        下边界布局位置点坐标集合
	*
	*/
	void CalLayoutBorderPoints(
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
		vector<SE_DPoint>& vBottomBorderLayoutCoords);

	// 根据中央经线及比例尺计算带号
	int CalBandNumber(double dCenterMeridian, double dScale);

	// 根据比例尺及图幅号判断当前图幅是邻带图幅，
	// 邻带图幅定义：1:5万东侧边缘1个图幅宽度，西侧边缘4个图幅宽度的所有图幅绘制邻带方里网
	//               1:1万东侧边缘1个图幅宽度，西侧边缘4个图幅宽度的所有图幅绘制邻带方里网
	// 返回值0表示当前图幅位于为非邻带图幅；
	// 返回值1表示当前图幅位于投影带西侧边缘；
	// 返回值2表示当前图幅位于投影带东侧边缘；
	int bIsAdjProjectBandSheet(double dScale, const QString& qstrSheet, double dCenterMeridian);

	// 绘制邻带方里网刻度线
	void DrawAdjKmNet(QgsPrintLayout* pPrintLayout,
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
		QgsLineSymbol* kmLineBorderLine);

	// 绘制邻带方里网要素
	void DrawAdjKmNet_CreateLayer(OGRLayer* pLayoutPointLayer,
		OGRLayer* pLayoutLineLayer,
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
		double dMapUnit2LayoutUnit);

	// 获取单选按钮状态
	void GetRadioChecked(bool& bLonLatRange, bool& bSheetList);

	// 获取图幅列表中所有图幅
	void GetSheetListFromPlainTextEdit(vector<QString>& qstrSheetList);

	// 加载布局元素位置配置文件
	bool LoadLayoutItemPositionsConfig(double dScale, vector<LayoutItemPositions>& vLayoutItemPositions);

	// 根据布局元素名称获取布局元素配置信息
	void GetLayoutItemPositionByName(const QString& qstrName,
		vector<LayoutItemPositions>& vItem,
		LayoutItemPositions& item);

	// 根据参考点配置信息返回参考点类型
	QgsLayoutItem::ReferencePoint GetReferencePoint(const QString& qstrReferencePoint);

	// 从SMS元数据中读取某一项属性值
	bool ReadSMSFile(string strSMSPath, int iKey, string& strValue);

	// 高斯坐标到布局坐标的转换
	void TransformGaussToLayout(SE_DPoint dRefGaussPoint,
		SE_DPoint dRefLayoutPoint,
		SE_DPoint dGaussPoint,
		double dMapUnitToLayoutUnit,
		SE_DPoint& dLayoutPoint);

	// 根据两点确定的直线计算点坐标Y
	void CalCoordYByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputX, double& dOutputY);

	// 根据两点确定的直线计算点坐标X
	void CalCoordXByTwoPoint(SE_DPoint dPoint1, SE_DPoint dPoint2, double dInputY, double& dOutputX);

	// 计算A-B边界高斯整公里点（起点为A，终点为B，自左往右）
	void CalGaussBorderCoords_A_B(SE_DPoint dBeginPoint, SE_DPoint dEndPoint, vector<SE_DPoint>& vGaussBorderCoords);

	// 计算B-C边界高斯整公里点（起点为C，终点为B，自下而上）
	void CalGaussBorderCoords_C_B(SE_DPoint dBeginPoint, SE_DPoint dEndPoint, vector<SE_DPoint>& vGaussBorderCoords);

	// 计算C-D边界高斯整公里点（起点为D，终点为C，自左往右）
	void CalGaussBorderCoords_D_C(SE_DPoint dBeginPoint, SE_DPoint dEndPoint, vector<SE_DPoint>& vGaussBorderCoords);

	// 计算A-D边界高斯整公里点（起点为D，终点为A，自下而上）
	void CalGaussBorderCoords_D_A(SE_DPoint dBeginPoint, SE_DPoint dEndPoint, vector<SE_DPoint>& vGaussBorderCoords);

	// 判断多边形是顺时针或逆时针
	int ClockWise(vector<SE_DPoint>& vPoints);

	// 求两条直线角点坐标
	SE_DPoint LineIntersect(SE_DPoint p0, SE_DPoint p1, SE_DPoint p2, SE_DPoint p3);

	// 根据某一点的横坐标计算在数组中的索引
	int CalIndexInVector_XCoord(double dx, vector<SE_DPoint>& vPoints);

	// 根据某一点的纵坐标计算在数组中的索引
	int CalIndexInVector_YCoord(double dy, vector<SE_DPoint>& vPoints);


	// 布局坐标到高斯坐标的转换
	void TransformLayoutToGauss(SE_DPoint dRefGaussPoint,
		SE_DPoint dRefLayoutPoint,
		SE_DPoint dLayoutPoint,
		double dMapUnitToLayoutUnit,
		SE_DPoint& dGaussPoint);

	// 根据已知点坐标、坐标纵线偏角或磁偏角计算坐标纵线终点或磁子午线终点坐标
	void CalEndPointByStartPointAndAngle(
		SE_DPoint dStartPoint,
		double dLength,
		double dAngle,
		SE_DPoint& dEndPoint);

	// 密位整数转字符（1-71、0-34）形式
	void ConvertMilFromIntToString(int iMil, QString& qstrMil);

	// 获取vector图层的属性值
	QList<QgsStringMap> GetAttributeValue(QgsVectorLayer* layer);

	// 根据类别查找摄影时间和传感器
	void GetAcqDataAndSensorByCateName(QList<QgsStringMap>& qAttributeList,
		const QString& qstrCateName,
		const QString& qstrCateField,
		const QString& qstrAcaDataField,
		const QString& qstrSensorField,
		QString& qstrAcqDataValue,
		QString& qstrSensorValue);

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	// 限制文本框输入
	void InitLineEdit();

	// 将tif文件转换为jpg
	bool TranslateFromTif2Jpg(QString qstrFilePath, QString qstrSheetNumber);

	// 根据图层类型获取查询要素类编码
	void GetFeatureClassesByLayerType(vector<FeatureClassParam>& vParams, QString qstrLayerType, vector<QString>& vCodes);

	// 设置图层字段
	int SetFieldDefn(OGRLayer* poLayer, vector<string> fieldname, vector<OGRFieldType> fieldtype, vector<int> fieldwidth);

	// 创建cpg文件
	bool CreateShapefileCPG(string strCPGFilePath, string strEncoding);

	// 设置点要素
	int Set_Point(OGRLayer* poLayer, double x, double y, double z, vector<FieldNameAndValue_Imp>& vFieldAndValue);

	// 设置线要素
	int Set_LineString(OGRLayer* poLayer, vector<SE_DPoint> Line, vector<FieldNameAndValue_Imp>& vFieldAndValue);

	// 设置面要素
	int Set_Polygon(OGRLayer* poLayer, vector<SE_DPoint> OuterRing, vector<FieldNameAndValue_Imp>& vFieldAndValue);

};

#endif // QGS_IMAGE_MAPPING_DIALOG_LAYOUT_VECTORIZATION_H
