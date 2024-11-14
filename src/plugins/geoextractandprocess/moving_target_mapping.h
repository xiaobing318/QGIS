#ifndef QGS_MOVING_TARGET_MAPPING_DIALOG_H
#define QGS_MOVING_TARGET_MAPPING_DIALOG_H

#include <QDialog>
#include <map>
#include <vector>
#include <string>

#include "ui_moving_target_mapping.h"
#include "qgslayoutitemmap.h"
#include "qgslayoutexporter.h"
#include "qgsprintlayout.h"
#include "se_commontypedef.h"
#include "qgslayoutitempolyline.h"
#include "qgslayoutitem.h"
#include "qgslayoutitempicture.h"
#include "qgslayoutitemlabel.h"
#include <QtGui/qcolor.h>
#include <QtGui/qfont.h>


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

#pragma region "条件编译预处理器指令：用来在不同的操作系统平台上包含不同的头文件：这些头文件都是与文件系统操作相关的"
#ifdef OS_FAMILY_WINDOWS
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <direct.h>

#else
#include <sys/io.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#endif
#pragma endregion

using namespace std; 

/*属性字段属性值对照结构体*/
struct FieldNameAndValue
{
	string strFieldName;
	string strFieldValue;
	FieldNameAndValue()
	{
		strFieldName = "";
		strFieldValue = "";
	}

};

class QgsMovingTargetMappingDialog : public QDialog
{
	Q_OBJECT

public:
	QgsMovingTargetMappingDialog(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsMovingTargetMappingDialog() override;

private:

	Ui_QgsMovingTargetMappingDlg ui;

	// 轨迹点数据文件路径
	QString m_qstrTracePointDataPath;

	// 轨迹线数据文件路径
	QString m_qstrTraceLineDataPath;

	// 影像图数据文件路径
	QString m_qstrImageDataPath;

	// 专题图保存路径，包括：专题图、工程文件等
	QString m_qstrMappingDataSavePath;

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

	//-------------------------------------------//
	// 外图廓a点布局坐标
	SE_DPoint		m_dExteriorMapLayoutPoint_a;

	// 外图廓b点布局坐标
	SE_DPoint		m_dExteriorMapLayoutPoint_b;

	// 外图廓c点布局坐标
	SE_DPoint		m_dExteriorMapLayoutPoint_c;

	// 外图廓d点布局坐标
	SE_DPoint		m_dExteriorMapLayoutPoint_d;

	//------------------------------------------------//
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

	//-----------------------------------------------//
	// 内图廓最小外接矩形左上角点布局坐标
	SE_DPoint		m_dInteriorLeftTopLayoutPoint;

	// 内图廓最小外接矩形左下角点布局坐标
	SE_DPoint		m_dInteriorLeftBottomLayoutPoint;

	// 内图廓最小外接矩形右上角点布局坐标
	SE_DPoint		m_dInteriorRightTopLayoutPoint;

	// 内图廓最小外接矩形右下角点布局坐标
	SE_DPoint		m_dInteriorRightBottomLayoutPoint;

	//-----------------------------------------//

	// 地图内图廓尺寸左上角的地理坐标（矩形内图廓），作为其它布局要素的起算原点，单位：毫米
	SE_DPoint		m_InteriorMapLeftTopGeoPoint;

	// 地图内图廓尺寸左上角的投影坐标（矩形内图廓）
	SE_DPoint		m_InteriorMapLeftTopProjectPoint;

	//------------------------------------------//
	// 纸张方向：1——横向；2——纵向
	int				m_iPageDirection;


signals:

private slots:

	// 关闭对话框
	void Close();

	// 浏览轨迹点数据按钮
	void OpenTracePointData();

	// 浏览轨迹线数据按钮
	void OpenTraceLineData();

	// 浏览影像图数据按钮
	void OpenImageData();

	// 保存路径按钮
	void MovingTargetMappingDataSave();

	// 判断目录是否存在
	bool FilePathIsExisted(QString qstrPath);

	// 判断文件是否存在
	bool FileIsExisted(QString qstrFilePath);

	// 输入框限制
	void InitLineEdit();

	// web墨卡托投影正算
	void WebMercator_BL_TO_XY(double B, double L, double& X, double& Y);

	// web墨卡托投影反算
	void WebMercator_XY_TO_BL(double X, double Y, double& B, double& L);

	QString FormatLonDegree(int iDegree);

	QString FormatLonDegree_Double(double dDegree);

	QString FormatLatDegree_Double(double dDegree);

	QString FormatLatDegree(int iDegree);

	void CalGeoBorderCoords_A_B(SE_DPoint dBeginPoint, SE_DPoint dEndPoint, double dLonInterval, vector<SE_DPoint>& vGeoBorderCoords);

	void CalGeoBorderCoords_C_B(SE_DPoint dBeginPoint, SE_DPoint dEndPoint, double dLatInterval, vector<SE_DPoint>& vGeoBorderCoords);

	void CalGeoBorderCoords_D_C(SE_DPoint dBeginPoint, SE_DPoint dEndPoint, double dLonInterval, vector<SE_DPoint>& vGeoBorderCoords);

	void CalGeoBorderCoords_D_A(SE_DPoint dBeginPoint, SE_DPoint dEndPoint, double dLatInterval, vector<SE_DPoint>& vGeoBorderCoords);

	void TransformProjectToLayout(SE_DPoint dRefProjectPoint, SE_DPoint dRefLayoutPoint, SE_DPoint dProjPoint, double dMapUnitToLayoutUnit, SE_DPoint& dLayoutPoint);

	int Get_Point(OGRFeature* poFeature, SE_DPoint& coordinate, vector<FieldNameAndValue>& vFieldvalue);

	void GetLayerFields(OGRLayer* pLayer, 
		vector<string>& vFieldname, 
		vector<OGRFieldType>& vFieldtype, 
		vector<int>& vFieldwidth,
		vector<int>& vFieldPrecision);

	int SetFieldDefn(OGRLayer* poLayer, 
		vector<string> fieldname, 
		vector<OGRFieldType> fieldtype, 
		vector<int> fieldwidth,
		vector<int> fieldPrecision);

	int Set_Point(OGRLayer* poLayer, double x, double y, double z, vector<FieldNameAndValue>& vFieldAndValue);

	bool CreateShapefileCPG(string strCPGFilePath, string strEncoding);

	// 专题图制图
	void MovingTargetMapping();

	// 地图尺寸下拉选择框事件
	void ComboPageSizeSelectedChanged();

	// 纸张方向选择
	void ComboPageDirectionSelectedChanged();

	// 地图名称字体设置按钮
	void SetMapNameFontSize();

	// 地图名称字体颜色设置按钮
	void SetMapNameFontColor();

	// 制图单位字体设置按钮
	void SetMapAgencyFontSize();

	// 制图单位字体颜色设置按钮
	void SetMapAgencyFontColor();

	// 地图说明字体设置按钮
	void SetMapDetailFontSize();

	// 地图说明字体颜色设置按钮
	void SetMapDetailFontColor();

	// 动目标说明字体设置按钮
	void SetMapMovingTargetFontSize();

	// 动目标说明字体颜色设置按钮
	void SetMapMovingTargetFontColor();


	// 设置地图尺寸按钮
	void SetMapSizeRadioButton();

	// 设置地图比例尺按钮
	void SetMapScaleRadioButton();



private:

	// 根据名称获取布局元素
	QgsLayoutItem* GetLayoutItemByName(QList<QgsLayoutItem*>& ItemList, const QString qstrName);

	// 获取轨迹点数据路径
	QString GetTracePointDataPath();

	// 获取轨迹线数据路径
	QString GetTraceLineDataPath();

	// 获取影像数据存储路径
	QString GetImageDataPath();

	// 获取数据保存路径
	QString GetDataSavePath();

	// 获取地图输出格式
	QString GetMapExportType();

	// 获取地图名称
	QString GetMapName();

	// 获取地图说明
	QString GetMapDetail();

	// 获取动目标说明
	QString GetMovingTargetDetail();

	// 获取制图单位
	QString GetMapAgency();

	// 获取制图单位绘制状态
	bool GetDrawMapAgencyState();

	// 获取比例尺绘制状态
	bool GetDrawScaleState();

	// 获取指北针绘制状态
	bool GetDrawNorthArrowState();

	// 获取方里网绘制状态
	bool GetDrawKmGridState();

	// 获取经纬网绘制状态
	bool GetDrawGeoGridState();

	// 获取方里网间隔
	void GetKmGridInterval(double& dKmInterval);

	// 获取经纬网间隔
	void GetGeoGridInterval(double& dLonInterval, double& dLatInterval);

	// 获取地图尺寸
	void GetMapSize(double& dWidth, double& dHeight);
	
	// 获取纸张方向
	



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

	// 根据经度值及分带方式计算中央经线
	double CalCenterMeridian(double dLong, double dScale);

	// 获取最小外接矩形
	void GetMBR(int iCount, double* pPoints, double& dLeft, double& dTop, double& dRight, double& dBottom);

	// 根据中央经线和比例尺类型获取空间参考系编码（预定义中国区域）
	// 1w比例尺使用3度分带，5w比例尺使用6度分带
	QString GetEPSGCodeByCenterMeridianAndScale(double dLong, double dScale);

	// 度转度分秒
	void Degree2DMS(double degree, int& iDegree, int& iMinute, int& iSecond);

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

	/* 根据地图投影范围计算内图廓上、下、左、右边界整公里坐标点集合
	* @param dRange:                     内图廓高斯投影范围
	* @param dKmInternal:				 方里网间隔
	* @param vLeftBorderCoords：         左边界高斯投影点坐标集合
	* @param vTopBorderCoords：          上边界高斯投影点坐标集合
	* @param vRightBorderCoords：        右边界高斯投影点坐标集合
	* @param vBottomBorderCoords：       下边界高斯投影点坐标集合
	*/
	void CalGaussBorderCoords(const SE_DRect& dRange,
		double dKmInternal,
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

	void CalEndPointByStartPointAndAngle(SE_DPoint dStartPoint, double dLength, double dAngle, SE_DPoint& dEndPoint);



private:

	QFont		m_qMapNameFont;			// 地图名称字体
	QColor		m_qMapNameColor;		// 地图名称颜色

	QFont		m_qMapAgencyFont;		// 地图制图单位字体
	QColor		m_qMapAgencyColor;		// 地图制图单位颜色


	bool		m_bSetMapNameFont;		// 是否设置地图名称字体
	bool		m_bSetMapNameColor;		// 是否设置地图名称颜色

	bool		m_bSetMapAgencyFont;	// 是否设置制图单位字体
	bool		m_bSetMapAgencyColor;	// 是否设置制图单位颜色

	/*地图说明字体*/
	QFont		m_qMapDetailFont;			// 地图说明字体
	QColor		m_qMapDetailColor;			// 地图说明颜色
	bool		m_bSetMapDetailFont;		// 是否设置地图说明字体
	bool		m_bSetMapDetailColor;		// 是否设置地图说明颜色

	/*动目标说明字体*/
	QFont		m_qMapMovingTargetFont;			// 动目标说明字体
	QColor		m_qMapMovingTargetColor;		// 动目标说明颜色
	bool		m_bSetMapMovingTargetFont;		// 是否设置动目标说明字体
	bool		m_bSetMapMovingTargetColor;		// 是否设置动目标说明颜色

};

#endif // QGS_MOVING_TARGET_MAPPING_DIALOG_H
