#ifndef QGS_IMAGE_SPATIAL_ACCURACY_CHECK_H
#define QGS_IMAGE_SPATIAL_ACCURACY_CHECK_H

#include <QDialog>
#include <qlabel.h>

#include "ui_image_spatial_accuracy_check.h"
#include "qgsmapcanvas.h"
#include "qgsmaplayer.h"
#include "qgspoint.h"
#include "qgsmaptoolzoom.h"
#include "qgsmaptoolemitpoint.h"
#include "qgsmaptoolpan.h"
#include "se_commontypedef.h"
#include "qgsvertexmarker.h"

class QgsImageSpatialAccuracyCheckDlg : public QMainWindow
{
	Q_OBJECT

public:
	QgsImageSpatialAccuracyCheckDlg(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~QgsImageSpatialAccuracyCheckDlg() override;

public:

private:

	// UI界面
	Ui_MainWindow_ImageSpatialAccuracyCheck ui;

	// 待检查地图画布
	QgsMapCanvas* mapCanvas_Check;

	// 参考地图画布
	QgsMapCanvas* mapCanvas_Refer;

	// 待检查地图画布所用的图层集合
	QList<QgsMapLayer*> mapCanvasLayerSet_Check;

	// 参考地图画布所用的图层集合
	QList<QgsMapLayer*> mapCanvasLayerSet_Refer;

	// 待检查地图画布中点标记数组
	vector<QgsVertexMarker*> m_vCheckPointMarker;

	// 参考地图画布中点标记数组
	vector<QgsVertexMarker*> m_vReferPointMarker;

private slots:

	// 加载待检查影像
	void action_AddCheckImage_slot();

	// 加载参考影像数据
	void action_AddReferenceImage_slot();

	// 加载参考矢量数据
	void action_AddReferenceVector_slot();

	// 退出按钮
	void action_Exit_slot();

	// 添加检查点
	void action_AddCheckPoint_slot();

	// 删除检查点
	void action_DeleteCheckPoint_slot();

	// 删除全部检查点
	void action_DeleteAllCheckPoints_slot();

	// 添加参考点
	void action_AddReferPoint_slot();

	// 删除参考点
	void action_DeleteReferPoint_slot();

	// 删除全部参考点
	void action_DeleteAllReferPoints_slot();

	// 空间精度检查
	void action_CheckAccuracy_slot();

	// 剔除粗差重新计算
	void action_RejectCoarseDifferences_slot();

	// 放大检查图
	void ZoomInCheckImage();

	// 缩小检查图
	void ZoomOutCheckImage();

	// 漫游检查图
	void PanCheckImage();

	// 放大参考图
	void ZoomInReferImage();

	// 缩小参考图
	void ZoomOutReferImage();

	// 漫游参考图
	void PanReferImage();

	// 获取检查图上鼠标左键点击坐标
	void GetMouseClickedXY_Check(const QgsPointXY& xy, Qt::MouseButton button);

	// 获取参考图上鼠标左键点击坐标
	void GetMouseClickedXY_Refer(const QgsPointXY& xy, Qt::MouseButton button);

	// 生成空间精度检查报告
	void CreateReport();

private:

	// 待检查放大工具
	QgsMapToolZoom* m_ZoomInTool_Check;

	// 待检查缩小工具
	QgsMapToolZoom* m_ZoomOutTool_Check;

	// 待检查漫游工具
	QgsMapToolPan* m_PanTool_Check;

	// 参考图放大工具
	QgsMapToolZoom* m_ZoomInTool_Refer;

	// 参考图缩小工具
	QgsMapToolZoom* m_ZoomOutTool_Refer;

	// 参考图漫游工具
	QgsMapToolPan* m_PanTool_Refer;

	// 待检查左键单击工具
	QgsMapToolEmitPoint* m_EmitPoint_Check;

	// 参考图左键单击工具
	QgsMapToolEmitPoint* m_EmitPoint_Refer;

	// 检查点坐标
	vector<SE_DPoint> m_vCheckPoint;

	// 参考点坐标
	vector<SE_DPoint> m_vReferPoint;

	// 表格中检查点
	vector<SE_DPoint> m_vTableCheckPoint;

	// 表格中参考点
	vector<SE_DPoint> m_vTableReferPoint;

	// 中误差
	double m_dMeanSquareError;

private:

	double CalDistanceOfTwoPoints(SE_DPoint dPoint1, SE_DPoint dPoint2, double dSemiMajorAxis, double dSemiMinorAxis);
};

#endif // QGS_IMAGE_SPATIAL_ACCURACY_CHECK_H
