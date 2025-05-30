#include "geoextractandprocess_plugin_setmergeparams.h"

#include "qgshelp.h"
#include "qgsmaplayer.h"
#include "qgsproject.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayer.h"
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsgui.h"

#include <QFileDialog>
#include <QMessageBox>

#include <QFile>
#include <QtXml\QtXml>
#include <QtXml\QDomDocument>
#include <QtGui\qvalidator.h>


using namespace std;

QgsSetMergeParams::QgsSetMergeParams(QWidget* parent, Qt::WindowFlags fl)
	: QDialog(parent, fl)
{
	ui.setupUi(this);
	QgsGui::enableAutoGeometryRestore(this);
	ui.tabWidget_LayerType->setCurrentIndex(0);
	connect(ui.pushButton_LoadMergeParams, &QPushButton::clicked, this, &QgsSetMergeParams::pushButton_LoadMergeParams_clicked);
	connect(ui.pushButton_SaveMergeParams, &QPushButton::clicked, this, &QgsSetMergeParams::pushButton_SaveMergeParams_clicked);
	connect(ui.Button_OK, &QPushButton::clicked, this, &QgsSetMergeParams::Button_OK_accepted);
	connect(ui.Button_Cancel, &QPushButton::clicked, this, &QgsSetMergeParams::Button_Cancel_rejected);
	connect(ui.Button_Help, &QPushButton::clicked, this, &QgsSetMergeParams::Button_Help_showHelp);

	// 默认30米
	QString strDefaultDistance = "30";
	ui.lineEdit_BufferDistance->setText(strDefaultDistance);

	InitLineEdit();
}

QgsSetMergeParams::~QgsSetMergeParams()
{
}

void QgsSetMergeParams::GetGeoTypeChecked(bool& bLineChecked, bool& bPolygonChecked)
{
	bLineChecked = ui.checkBox_Line->isChecked();
	bPolygonChecked = ui.checkBox_Polygon->isChecked();
}

int QgsSetMergeParams::GetScaleType()
{
	int iType = 0;
	// 1:5万
	if (ui.comboBox_Scale->currentIndex() == 0)
	{
		iType = 1;
	}
	// 1:25万
	else if (ui.comboBox_Scale->currentIndex() == 1)
	{
		iType = 2;
	}
	// 1:100万
	else if (ui.comboBox_Scale->currentIndex() == 2)
	{
		iType = 3;
	}
	return iType;
}

double QgsSetMergeParams::GetBufferDistance()
{
	QString qstrDistance = ui.lineEdit_BufferDistance->text();
	double dDistance = qstrDistance.toDouble();
	return dDistance;
}

void QgsSetMergeParams::GetLayerMatchParams(vector<LayerMatchParam>& vLayerMatchParam)
{
	vLayerMatchParam.clear();

	// 参数匹配图层列表，从B图层到Q图层
	vector<string> vLayerType;
	vLayerType.clear();

	vLayerType.push_back("B");
	vLayerType.push_back("C");
	vLayerType.push_back("D");
	vLayerType.push_back("E");
	vLayerType.push_back("F");

	vLayerType.push_back("G");
	vLayerType.push_back("H");
	vLayerType.push_back("I");
	vLayerType.push_back("J");
	vLayerType.push_back("K");

	vLayerType.push_back("L");
	vLayerType.push_back("M");
	vLayerType.push_back("N");
	vLayerType.push_back("O");
	vLayerType.push_back("P");
	vLayerType.push_back("Q");

	// 循环获取每个图层的字段选择状态
	for (int i = 0; i < vLayerType.size(); i++)
	{
		LayerMatchParam param;
		param.strLayerName = vLayerType[i];
		GetLayerFieldsChecked(vLayerType[i], param.vFields);
		vLayerMatchParam.push_back(param);
	}
}

void QgsSetMergeParams::GetLayerFieldsChecked(string strLayerType, vector<string>& vFields)
{
	vFields.clear();
	/*根据图层类型，获取字段选中状态，将选中的字段加入到列表中*/
	// 工农业社会文化设施
	if (strLayerType == "B")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_B->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_B->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_B->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_B->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiBie_B->isChecked())
		{
			qstrField = "类别";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoCheng_B->isChecked())
		{
			qstrField = "高程";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiGao_B->isChecked())
		{
			qstrField = "比高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XingZhengQuHua_B->isChecked())
		{
			qstrField = "行政区划";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_B->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_B->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_B->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 居民地及附属设施
	else if (strLayerType == "C")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_C->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_C->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_C->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_C->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_JuZhuYueFen_C->isChecked())
		{
			qstrField = "居住月份";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_RenKou_C->isChecked())
		{
			qstrField = "人口";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiGao_C->isChecked())
		{
			qstrField = "比高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XingZhengQuHua_C->isChecked())
		{
			qstrField = "行政区划";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_C->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_C->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_C->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 陆地交通
	else if (strLayerType == "D")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_D->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_D->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_D->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_D->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianHao_D->isChecked())
		{
			qstrField = "编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_DengJi_D->isChecked())
		{
			qstrField = "等级";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_KuanDu_D->isChecked())
		{
			qstrField = "宽度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_PuKuan_D->isChecked())
		{
			qstrField = "铺宽";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_QiaoChang_D->isChecked())
		{
			qstrField = "桥长";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_JingKongGao_D->isChecked())
		{
			qstrField = "净空高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZaiZhongDunShu_D->isChecked())
		{
			qstrField = "载重吨数";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LiCheng_D->isChecked())
		{
			qstrField = "里程";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiGao_D->isChecked())
		{
			qstrField = "比高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TongXingYueFen_D->isChecked())
		{
			qstrField = "通行月份";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ShuiShen_D->isChecked())
		{
			qstrField = "水深";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_DiZhi_D->isChecked())
		{
			qstrField = "底质";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_QuLvBanJing_D->isChecked())
		{
			qstrField = "曲率半径";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZuiDaZongPo_D->isChecked())
		{
			qstrField = "最大纵坡";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_D->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_D->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_D->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 管线
	else if (strLayerType == "E")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_E->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_E->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_E->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_E->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_JingKongGao_E->isChecked())
		{
			qstrField = "净空高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MaiCangShenDu_E->isChecked())
		{
			qstrField = "埋藏深度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CunZaiZhuangTai_E->isChecked())
		{
			qstrField = "存在状态";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZuoYongFangShi_E->isChecked())
		{
			qstrField = "作用方式";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XianZhiZhongLei_E->isChecked())
		{
			qstrField = "限制种类";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_E->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_E->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_E->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 水域/陆地
	else if (strLayerType == "F")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_F->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_F->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_F->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_F->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_KuanDu_F->isChecked())
		{
			qstrField = "宽度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ShuiShen_F->isChecked())
		{
			qstrField = "水深";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_NiShen_F->isChecked())
		{
			qstrField = "泥深";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ShiLingYueFen_F->isChecked())
		{
			qstrField = "时令月份";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ChangDu_F->isChecked())
		{
			qstrField = "长度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoCheng_F->isChecked())
		{
			qstrField = "高程";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiGao_F->isChecked())
		{
			qstrField = "比高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_KuRongLiang_F->isChecked())
		{
			qstrField = "库容量";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_DunWei_F->isChecked())
		{
			qstrField = "吨位";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_HeLiuDaiMa_F->isChecked())
		{
			qstrField = "河流代码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TongHangXingZhi_F->isChecked())
		{
			qstrField = "通航性质";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhiWuLeiXing_F->isChecked())
		{
			qstrField = "植物类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiaoMianWuZhi_F->isChecked())
		{
			qstrField = "表面物质";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CunZaiZhuangTai_F->isChecked())
		{
			qstrField = "存在状态";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WeiZhiZhiLiang_F->isChecked())
		{
			qstrField = "位置质量";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_YanSe_F->isChecked())
		{
			qstrField = "颜色";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_YuShuiMianGuanXi_F->isChecked())
		{
			qstrField = "与水面关系";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_F->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_F->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_F->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 海底地貌及底质
	else if (strLayerType == "G")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_G->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_G->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_G->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_G->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ShuiShenZhi_G->isChecked())
		{
			qstrField = "水深值";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ShuiShenZhi1_G->isChecked())
		{
			qstrField = "水深值1";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ShuiShenZhi2_G->isChecked())
		{
			qstrField = "水深值2";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CeShenJiShu_G->isChecked())
		{
			qstrField = "测深技术";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CeShenZhiLiang_G->isChecked())
		{
			qstrField = "测深质量";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WeiZhiZhiLiang_G->isChecked())
		{
			qstrField = "位置质量";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZuoYongFangShi_G->isChecked())
		{
			qstrField = "作用方式";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiaoMianWuZhi_G->isChecked())
		{
			qstrField = "表面物质";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WuZhiXingTai_G->isChecked())
		{
			qstrField = "物质形态";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WeiXianJi_G->isChecked())
		{
			qstrField = "危险级";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_G->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_G->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_G->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 礁石、沉船、障碍物
	else if (strLayerType == "H")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_H->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_H->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_H->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_H->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ShuiShenZhi_H->isChecked())
		{
			qstrField = "水深值";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoDu_H->isChecked())
		{
			qstrField = "高度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ChuiGao_H->isChecked())
		{
			qstrField = "垂高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CunZaiZhuangTai_H->isChecked())
		{
			qstrField = "存在状态";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CeShenJiShu_H->isChecked())
		{
			qstrField = "测深技术";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CeShenZhiLiang_H->isChecked())
		{
			qstrField = "测深质量";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WeiZhiZhiLiang_H->isChecked())
		{
			qstrField = "位置质量";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XianZhiZhongLei_H->isChecked())
		{
			qstrField = "限制种类";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZuoYongFangShi_H->isChecked())
		{
			qstrField = "作用方式";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiaoMianWuZhi_H->isChecked())
		{
			qstrField = "表面物质";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_YuShuiMianGuanXi_H->isChecked())
		{
			qstrField = "与水面关系";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WeiXianJi_H->isChecked())
		{
			qstrField = "危险级";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_H->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_H->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_H->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 水文
	else if (strLayerType == "I")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_I->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_I->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_I->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_KuanDu_I->isChecked())
		{
			qstrField = "宽度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ShuiShen_I->isChecked())
		{
			qstrField = "水深";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_DiZhi_I->isChecked())
		{
			qstrField = "底质";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LiuSu_I->isChecked())
		{
			qstrField = "流速";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_FangWei_I->isChecked())
		{
			qstrField = "方位";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoCheng_I->isChecked())
		{
			qstrField = "高程";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_YueFen_I->isChecked())
		{
			qstrField = "月份";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MiaoShu_I->isChecked())
		{
			qstrField = "描述";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_I->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_I->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_I->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 陆地地貌及土质
	else if (strLayerType == "J")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_J->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_J->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_J->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_J->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiBie_J->isChecked())
		{
			qstrField = "类别";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoCheng_J->isChecked())
		{
			qstrField = "高程";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiGao_J->isChecked())
		{
			qstrField = "比高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GouKuan_J->isChecked())
		{
			qstrField = "沟宽";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_FangXiang_J->isChecked())
		{
			qstrField = "方向";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_J->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_J->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_J->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 境界与政区
	else if (strLayerType == "K")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_K->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_K->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_K->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_K->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_DaiMa_K->isChecked())
		{
			qstrField = "代码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianHao_K->isChecked())
		{
			qstrField = "编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XuHao_K->isChecked())
		{
			qstrField = "序号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoCheng_K->isChecked())
		{
			qstrField = "高程";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiGao_K->isChecked())
		{
			qstrField = "比高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_K->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_K->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_K->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 植被
	else if (strLayerType == "L")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_L->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_L->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_L->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_L->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoCheng_L->isChecked())
		{
			qstrField = "高程";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoDu_L->isChecked())
		{
			qstrField = "高度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XiongJing_L->isChecked())
		{
			qstrField = "胸径";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_KuanDu_L->isChecked())
		{
			qstrField = "宽度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_L->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_L->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_L->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 地磁要素
	else if (strLayerType == "M")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_M->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_M->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_M->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CiChaZhi_M->isChecked())
		{
			qstrField = "磁差值";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CanKaoNian_M->isChecked())
		{
			qstrField = "参考年";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_NianBianZhi_M->isChecked())
		{
			qstrField = "年变值";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_M->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_M->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_M->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 助航设备及航道
	else if (strLayerType == "N")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_N->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_N->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_N->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_N->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XingZhi_N->isChecked())
		{
			qstrField = "性质";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianHao_N->isChecked())
		{
			qstrField = "编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_YanSe_N->isChecked())
		{
			qstrField = "颜色";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_SeCaiTuAn_N->isChecked())
		{
			qstrField = "色彩图案";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_DingBiaoYanSe_N->isChecked())
		{
			qstrField = "顶标颜色";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_FaGuangZhuangTai_N->isChecked())
		{
			qstrField = "发光状态";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoDu_N->isChecked())
		{
			qstrField = "高度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_DengGuangTeXing_N->isChecked())
		{
			qstrField = "灯光特性";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XinHaoZu_N->isChecked())
		{
			qstrField = "信号组";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XinHaoZhouQi_N->isChecked())
		{
			qstrField = "信号周期";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZuoYongJuLi_N->isChecked())
		{
			qstrField = "作用距离";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_DengGuangKeShi_N->isChecked())
		{
			qstrField = "灯光可视";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_FangWei_N->isChecked())
		{
			qstrField = "方位";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_HangXingZhiXiang_N->isChecked())
		{
			qstrField = "航行指向";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GuangHuJiaoDu1_N->isChecked())
		{
			qstrField = "光弧角度1";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GuangHuJiaoDu2_N->isChecked())
		{
			qstrField = "光弧角度2";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiDaKeShi_N->isChecked())
		{
			qstrField = "雷达可视";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ShuiShenZhi1_N->isChecked())
		{
			qstrField = "水深值1";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZuoYongFangShi_N->isChecked())
		{
			qstrField = "作用方式";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_FuBiaoXiTong_N->isChecked())
		{
			qstrField = "浮标系统";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_N->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_N->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_N->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 海上区域界线
	else if (strLayerType == "O")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_O->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_O->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_J->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_O->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GuoJiaDaiMa_O->isChecked())
		{
			qstrField = "国家代码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianHao_O->isChecked())
		{
			qstrField = "编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XianZhiZhongLei_O->isChecked())
		{
			qstrField = "限制种类";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_CunZaiZhuangTai_O->isChecked())
		{
			qstrField = "存在状态";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiDaKeShi_O->isChecked())
		{
			qstrField = "雷达可视";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BanJing_O->isChecked())
		{
			qstrField = "半径";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoDu_O->isChecked())
		{
			qstrField = "高度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_O->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_O->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_O->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 航空要素
	else if (strLayerType == "P")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_P->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_P->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_P->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_P->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianHao_P->isChecked())
		{
			qstrField = "编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_PaoDaoCiFangXiang_P->isChecked())
		{
			qstrField = "跑道磁方向";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_GaoCheng_P->isChecked())
		{
			qstrField = "高程";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BiGao_P->isChecked())
		{
			qstrField = "比高";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_KuanDu_P->isChecked())
		{
			qstrField = "宽度";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_P->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_P->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_P->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}

	// 军事区域
	else if (strLayerType == "Q")
	{
		QString qstrField;
		string strField;
		if (ui.checkBox_YaoSuBianHao_Q->isChecked())
		{
			qstrField = "要素编号";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_BianMa_Q->isChecked())
		{
			qstrField = "编码";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_MingCheng_Q->isChecked())
		{
			qstrField = "名称";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_LeiXing_Q->isChecked())
		{
			qstrField = "类型";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_XianZhiZhongLei_Q->isChecked())
		{
			qstrField = "限制种类";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_TuXingTeZheng_Q->isChecked())
		{
			qstrField = "图形特征";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_ZhuJiZhiZhen_Q->isChecked())
		{
			qstrField = "注记指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
		if (ui.checkBox_WaiGuaBiaoZhiZhen_Q->isChecked())
		{
			qstrField = "外挂表指针";
			strField = qstrField.toLocal8Bit();
			vFields.push_back(strField);
		}
	}
}

void QgsSetMergeParams::pushButton_LoadMergeParams_clicked()
{
	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = "选择配置文件";
	QString filter = "xml 文件(*.xml)";
	QString strFileName = QFileDialog::getOpenFileName(this,
		dlgTitle, curPath, filter);
	if (strFileName.isEmpty())
	{
		return;
	}

	QDomDocument doc;
	QFile file(strFileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		return;
	}

	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	// 读取根节点
	QDomElement root = doc.documentElement();
	// 读取第一个子节点   QDomNode 节点
	QDomNode node = root.firstChild();
	vector<LayerMatchParam> vLayerMatchParam;
	vLayerMatchParam.clear();
	while (!node.isNull())
	{
		// 循环遍历每个子节点
		LayerMatchParam param;

		// 节点元素名称
		QString tagName = node.toElement().tagName();
		// 节点标记查找
		if (tagName == "Layer")
		{
			QDomNodeList nodeList = node.childNodes();
			for (int i = 0; i < nodeList.size(); i++)
			{
				QDomNode nodeTemp = nodeList.at(i);
				QDomElement elementTemp = nodeTemp.toElement();
				if (elementTemp.tagName() == "LayerName")
				{
					param.strLayerName = elementTemp.text().toStdString();
				}
				else if (elementTemp.tagName() == "LineChecked")
				{
					if (elementTemp.text() == "true")
					{
						param.bLineStringChecked = true;
					}
					else
					{
						param.bLineStringChecked = false;
					}
				}
				else if (elementTemp.tagName() == "PolygonChecked")
				{
					if (elementTemp.text() == "true")
					{
						param.bPolygonChecked = true;
					}
					else
					{
						param.bPolygonChecked = false;
					}
				}
				else if (elementTemp.tagName() == "Fields")
				{
					// 获取字段列表
					QDomNodeList fieldList = nodeTemp.childNodes();
					for (int j = 0; j < fieldList.size(); j++)
					{
						QDomNode fieldNode = fieldList.at(j);
						QDomElement fieldElement = fieldNode.toElement();
						QString qstrField = fieldElement.text();
						string strField = qstrField.toLocal8Bit();
						param.vFields.push_back(strField);
					}
				}
			}
		}

		vLayerMatchParam.push_back(param);

		node = node.nextSibling();//读取下一个兄弟节点
	}

	// 根据配置文件设置界面
	for (int i = 0; i < vLayerMatchParam.size(); i++)
	{
		LayerMatchParam param = vLayerMatchParam[i];
		SetCheckBoxStatus(param);
	}
}

void QgsSetMergeParams::SetCheckBoxStatus(LayerMatchParam& param)
{
	// 按图层依次进行设置
	// 工农业社会文化设施
	if (param.strLayerName == "B")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_B->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_B->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_B->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_B->setChecked(true);
			}

			if (qstrField == "类别")
			{
				ui.checkBox_LeiBie_B->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_B->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_B->setChecked(true);
			}

			if (qstrField == "行政区划")
			{
				ui.checkBox_XingZhengQuHua_B->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_B->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_B->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_B->setChecked(true);
			}
		}
	}

	// 居民地及附属设施
	if (param.strLayerName == "C")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_C->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_C->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_C->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_C->setChecked(true);
			}

			if (qstrField == "居住月份")
			{
				ui.checkBox_JuZhuYueFen_C->setChecked(true);
			}

			if (qstrField == "人口")
			{
				ui.checkBox_RenKou_C->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_C->setChecked(true);
			}

			if (qstrField == "行政区划")
			{
				ui.checkBox_XingZhengQuHua_C->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_C->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_C->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_C->setChecked(true);
			}
		}
	}

	// 陆地交通
	if (param.strLayerName == "D")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_D->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_D->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_D->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_D->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_D->setChecked(true);
			}

			if (qstrField == "等级")
			{
				ui.checkBox_DengJi_D->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_D->setChecked(true);
			}

			if (qstrField == "铺宽")
			{
				ui.checkBox_PuKuan_D->setChecked(true);
			}

			if (qstrField == "桥长")
			{
				ui.checkBox_QiaoChang_D->setChecked(true);
			}

			if (qstrField == "净空高")
			{
				ui.checkBox_JingKongGao_D->setChecked(true);
			}

			if (qstrField == "载重吨数")
			{
				ui.checkBox_ZaiZhongDunShu_D->setChecked(true);
			}

			if (qstrField == "里程")
			{
				ui.checkBox_LiCheng_D->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_D->setChecked(true);
			}

			if (qstrField == "通行月份")
			{
				ui.checkBox_TongXingYueFen_D->setChecked(true);
			}

			if (qstrField == "水深")
			{
				ui.checkBox_ShuiShen_D->setChecked(true);
			}

			if (qstrField == "底质")
			{
				ui.checkBox_DiZhi_D->setChecked(true);
			}

			if (qstrField == "曲率半径")
			{
				ui.checkBox_QuLvBanJing_D->setChecked(true);
			}

			if (qstrField == "最大纵坡")
			{
				ui.checkBox_ZuiDaZongPo_D->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_D->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_D->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_D->setChecked(true);
			}
		}
	}

	// 管线
	if (param.strLayerName == "E")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_E->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_E->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_E->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_E->setChecked(true);
			}

			if (qstrField == "净空高")
			{
				ui.checkBox_JingKongGao_E->setChecked(true);
			}

			if (qstrField == "埋藏深度")
			{
				ui.checkBox_MaiCangShenDu_E->setChecked(true);
			}

			if (qstrField == "存在状态")
			{
				ui.checkBox_CunZaiZhuangTai_E->setChecked(true);
			}

			if (qstrField == "作用方式")
			{
				ui.checkBox_ZuoYongFangShi_E->setChecked(true);
			}

			if (qstrField == "限制种类")
			{
				ui.checkBox_XianZhiZhongLei_E->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_E->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_E->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_E->setChecked(true);
			}
		}
	}

	// 水域/陆地
	if (param.strLayerName == "F")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_F->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_F->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_F->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_F->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_F->setChecked(true);
			}

			if (qstrField == "水深")
			{
				ui.checkBox_ShuiShen_F->setChecked(true);
			}

			if (qstrField == "泥深")
			{
				ui.checkBox_NiShen_F->setChecked(true);
			}

			if (qstrField == "时令月份")
			{
				ui.checkBox_ShiLingYueFen_F->setChecked(true);
			}

			if (qstrField == "长度")
			{
				ui.checkBox_ChangDu_F->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_F->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_F->setChecked(true);
			}

			if (qstrField == "库容量")
			{
				ui.checkBox_KuRongLiang_F->setChecked(true);
			}

			if (qstrField == "吨位")
			{
				ui.checkBox_DunWei_F->setChecked(true);
			}

			if (qstrField == "河流代码")
			{
				ui.checkBox_HeLiuDaiMa_F->setChecked(true);
			}

			if (qstrField == "通航性质")
			{
				ui.checkBox_TongHangXingZhi_F->setChecked(true);
			}

			if (qstrField == "植物类型")
			{
				ui.checkBox_ZhiWuLeiXing_F->setChecked(true);
			}

			if (qstrField == "表面物质")
			{
				ui.checkBox_BiaoMianWuZhi_F->setChecked(true);
			}

			if (qstrField == "存在状态")
			{
				ui.checkBox_CunZaiZhuangTai_F->setChecked(true);
			}

			if (qstrField == "位置质量")
			{
				ui.checkBox_WeiZhiZhiLiang_F->setChecked(true);
			}

			if (qstrField == "颜色")
			{
				ui.checkBox_YanSe_F->setChecked(true);
			}

			if (qstrField == "与水面关系")
			{
				ui.checkBox_YuShuiMianGuanXi_F->setChecked(true);
			}
			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_F->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_F->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_F->setChecked(true);
			}
		}
	}

	// 海底地貌及底质
	if (param.strLayerName == "G")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_G->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_G->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_G->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_G->setChecked(true);
			}

			if (qstrField == "水深值")
			{
				ui.checkBox_ShuiShenZhi_G->setChecked(true);
			}

			if (qstrField == "水深值1")
			{
				ui.checkBox_ShuiShenZhi1_G->setChecked(true);
			}

			if (qstrField == "水深值2")
			{
				ui.checkBox_ShuiShenZhi2_G->setChecked(true);
			}

			if (qstrField == "测深技术")
			{
				ui.checkBox_CeShenJiShu_G->setChecked(true);
			}

			if (qstrField == "测深质量")
			{
				ui.checkBox_CeShenZhiLiang_G->setChecked(true);
			}

			if (qstrField == "位置质量")
			{
				ui.checkBox_WeiZhiZhiLiang_G->setChecked(true);
			}

			if (qstrField == "作用方式")
			{
				ui.checkBox_ZuoYongFangShi_G->setChecked(true);
			}

			if (qstrField == "表面物质")
			{
				ui.checkBox_BiaoMianWuZhi_G->setChecked(true);
			}

			if (qstrField == "物质形态")
			{
				ui.checkBox_WuZhiXingTai_G->setChecked(true);
			}

			if (qstrField == "危险级")
			{
				ui.checkBox_WeiXianJi_G->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_G->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_G->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_G->setChecked(true);
			}
		}
	}

	// 礁石、沉船、障碍物
	if (param.strLayerName == "H")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_H->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_H->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_H->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_H->setChecked(true);
			}

			if (qstrField == "水深值")
			{
				ui.checkBox_ShuiShenZhi_H->setChecked(true);
			}

			if (qstrField == "高度")
			{
				ui.checkBox_GaoDu_H->setChecked(true);
			}

			if (qstrField == "垂高")
			{
				ui.checkBox_ChuiGao_H->setChecked(true);
			}

			if (qstrField == "存在状态")
			{
				ui.checkBox_CunZaiZhuangTai_H->setChecked(true);
			}

			if (qstrField == "测深技术")
			{
				ui.checkBox_CeShenJiShu_H->setChecked(true);
			}

			if (qstrField == "测深质量")
			{
				ui.checkBox_CeShenZhiLiang_H->setChecked(true);
			}

			if (qstrField == "位置质量")
			{
				ui.checkBox_WeiZhiZhiLiang_H->setChecked(true);
			}

			if (qstrField == "限制种类")
			{
				ui.checkBox_XianZhiZhongLei_H->setChecked(true);
			}

			if (qstrField == "作用方式")
			{
				ui.checkBox_ZuoYongFangShi_H->setChecked(true);
			}

			if (qstrField == "表面物质")
			{
				ui.checkBox_BiaoMianWuZhi_H->setChecked(true);
			}

			if (qstrField == "与水面关系")
			{
				ui.checkBox_YuShuiMianGuanXi_H->setChecked(true);
			}

			if (qstrField == "危险级")
			{
				ui.checkBox_WeiXianJi_H->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_H->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_H->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_H->setChecked(true);
			}
		}
	}

	// 工水文
	if (param.strLayerName == "I")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_I->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_I->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_I->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_I->setChecked(true);
			}

			if (qstrField == "水深")
			{
				ui.checkBox_ShuiShen_I->setChecked(true);
			}

			if (qstrField == "底质")
			{
				ui.checkBox_DiZhi_I->setChecked(true);
			}

			if (qstrField == "流速")
			{
				ui.checkBox_LiuSu_I->setChecked(true);
			}

			if (qstrField == "方位")
			{
				ui.checkBox_FangWei_I->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_I->setChecked(true);
			}

			if (qstrField == "月份")
			{
				ui.checkBox_YueFen_I->setChecked(true);
			}

			if (qstrField == "描述")
			{
				ui.checkBox_MiaoShu_I->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_I->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_I->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_I->setChecked(true);
			}
		}
	}

	// 陆地地貌及土质
	if (param.strLayerName == "J")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_J->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_J->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_J->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_J->setChecked(true);
			}

			if (qstrField == "类别")
			{
				ui.checkBox_LeiBie_J->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_J->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_J->setChecked(true);
			}

			if (qstrField == "沟宽")
			{
				ui.checkBox_GouKuan_J->setChecked(true);
			}

			if (qstrField == "方向")
			{
				ui.checkBox_FangXiang_J->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_J->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_J->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_J->setChecked(true);
			}
		}
	}

	// 境界与政区
	if (param.strLayerName == "K")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_K->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_K->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_K->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_K->setChecked(true);
			}

			if (qstrField == "代码")
			{
				ui.checkBox_DaiMa_K->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_K->setChecked(true);
			}

			if (qstrField == "序号")
			{
				ui.checkBox_XuHao_K->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_K->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_K->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_K->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_K->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_K->setChecked(true);
			}
		}
	}

	// 植被
	if (param.strLayerName == "L")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_L->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_L->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_L->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_L->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_L->setChecked(true);
			}

			if (qstrField == "高度")
			{
				ui.checkBox_GaoDu_L->setChecked(true);
			}

			if (qstrField == "胸径")
			{
				ui.checkBox_XiongJing_L->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_L->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_L->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_L->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_L->setChecked(true);
			}
		}
	}

	// 地磁要素
	if (param.strLayerName == "M")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_M->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_M->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_M->setChecked(true);
			}

			if (qstrField == "磁差值")
			{
				ui.checkBox_CiChaZhi_M->setChecked(true);
			}

			if (qstrField == "参考年")
			{
				ui.checkBox_CanKaoNian_M->setChecked(true);
			}

			if (qstrField == "年变值")
			{
				ui.checkBox_NianBianZhi_M->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_M->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_M->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_M->setChecked(true);
			}
		}
	}

	// 助航设备及航道
	if (param.strLayerName == "N")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_N->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_N->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_N->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_N->setChecked(true);
			}

			if (qstrField == "性质")
			{
				ui.checkBox_XingZhi_N->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_N->setChecked(true);
			}

			if (qstrField == "颜色")
			{
				ui.checkBox_YanSe_N->setChecked(true);
			}

			if (qstrField == "色彩图案")
			{
				ui.checkBox_SeCaiTuAn_N->setChecked(true);
			}

			if (qstrField == "顶标颜色")
			{
				ui.checkBox_DingBiaoYanSe_N->setChecked(true);
			}

			if (qstrField == "发光状态")
			{
				ui.checkBox_FaGuangZhuangTai_N->setChecked(true);
			}

			if (qstrField == "高度")
			{
				ui.checkBox_GaoDu_N->setChecked(true);
			}

			if (qstrField == "灯光特性")
			{
				ui.checkBox_DengGuangTeXing_N->setChecked(true);
			}

			if (qstrField == "信号组")
			{
				ui.checkBox_XinHaoZu_N->setChecked(true);
			}

			if (qstrField == "信号周期")
			{
				ui.checkBox_XinHaoZhouQi_N->setChecked(true);
			}

			if (qstrField == "作用距离")
			{
				ui.checkBox_ZuoYongJuLi_N->setChecked(true);
			}

			if (qstrField == "灯光可视")
			{
				ui.checkBox_DengGuangKeShi_N->setChecked(true);
			}

			if (qstrField == "方位")
			{
				ui.checkBox_FangWei_N->setChecked(true);
			}

			if (qstrField == "航行指向")
			{
				ui.checkBox_HangXingZhiXiang_N->setChecked(true);
			}

			if (qstrField == "光弧角度1")
			{
				ui.checkBox_GuangHuJiaoDu1_N->setChecked(true);
			}

			if (qstrField == "光弧角度2")
			{
				ui.checkBox_GuangHuJiaoDu2_N->setChecked(true);
			}

			if (qstrField == "雷达可视")
			{
				ui.checkBox_LeiDaKeShi_N->setChecked(true);
			}

			if (qstrField == "水深值1")
			{
				ui.checkBox_ShuiShenZhi1_N->setChecked(true);
			}

			if (qstrField == "作用方式")
			{
				ui.checkBox_ZuoYongFangShi_N->setChecked(true);
			}

			if (qstrField == "浮标系统")
			{
				ui.checkBox_FuBiaoXiTong_N->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_N->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_N->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_N->setChecked(true);
			}
		}
	}

	// 海上区域界线
	if (param.strLayerName == "O")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_O->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_O->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_O->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_O->setChecked(true);
			}

			if (qstrField == "国家代码")
			{
				ui.checkBox_GuoJiaDaiMa_O->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_O->setChecked(true);
			}

			if (qstrField == "限制种类")
			{
				ui.checkBox_XianZhiZhongLei_O->setChecked(true);
			}

			if (qstrField == "存在状态")
			{
				ui.checkBox_CunZaiZhuangTai_O->setChecked(true);
			}

			if (qstrField == "雷达可视")
			{
				ui.checkBox_LeiDaKeShi_O->setChecked(true);
			}

			if (qstrField == "半径")
			{
				ui.checkBox_BanJing_O->setChecked(true);
			}

			if (qstrField == "高度")
			{
				ui.checkBox_GaoDu_O->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_O->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_O->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_O->setChecked(true);
			}
		}
	}

	// 航空要素
	if (param.strLayerName == "P")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_P->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_P->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_P->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_P->setChecked(true);
			}

			if (qstrField == "编号")
			{
				ui.checkBox_BianHao_P->setChecked(true);
			}

			if (qstrField == "跑道磁方向")
			{
				ui.checkBox_PaoDaoCiFangXiang_P->setChecked(true);
			}

			if (qstrField == "高程")
			{
				ui.checkBox_GaoCheng_P->setChecked(true);
			}

			if (qstrField == "比高")
			{
				ui.checkBox_BiGao_P->setChecked(true);
			}

			if (qstrField == "宽度")
			{
				ui.checkBox_KuanDu_P->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_P->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_P->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_P->setChecked(true);
			}
		}
	}

	// 军事区域
	if (param.strLayerName == "Q")
	{
		if (param.bLineStringChecked)
		{
			ui.checkBox_Line->setChecked(true);
		}

		if (param.bPolygonChecked)
		{
			ui.checkBox_Polygon->setChecked(true);
		}

		for (int i = 0; i < param.vFields.size(); i++)
		{
			QString qstrField = QString::fromLocal8Bit(param.vFields[i].c_str());
			if (qstrField == "要素编号")
			{
				ui.checkBox_YaoSuBianHao_Q->setChecked(true);
			}

			if (qstrField == "编码")
			{
				ui.checkBox_BianMa_Q->setChecked(true);
			}

			if (qstrField == "名称")
			{
				ui.checkBox_MingCheng_Q->setChecked(true);
			}

			if (qstrField == "类型")
			{
				ui.checkBox_LeiXing_Q->setChecked(true);
			}

			if (qstrField == "限制种类")
			{
				ui.checkBox_XianZhiZhongLei_Q->setChecked(true);
			}

			if (qstrField == "图形特征")
			{
				ui.checkBox_TuXingTeZheng_Q->setChecked(true);
			}

			if (qstrField == "注记指针")
			{
				ui.checkBox_ZhuJiZhiZhen_Q->setChecked(true);
			}

			if (qstrField == "外挂表指针")
			{
				ui.checkBox_WaiGuaBiaoZhiZhen_Q->setChecked(true);
			}
		}
	}
}

/*生成界面配置文件*/
void QgsSetMergeParams::pushButton_SaveMergeParams_clicked()
{
	// QDomDocument类
	QDomDocument doc;
	QDomProcessingInstruction instruction = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
	doc.appendChild(instruction);
	// 创建根节点  QDomElemet元素
	QDomElement root = doc.createElement("LayerMergeParams");
	// 添加根节点
	doc.appendChild(root);

	// 获取界面图层字段匹配列表
	vector<LayerMatchParam> vLayerMatchParam;
	vLayerMatchParam.clear();
	GetLayerMatchParams(vLayerMatchParam);

	bool bLineChecked = false;
	bool bPolygonChecked = false;
	GetGeoTypeChecked(bLineChecked, bPolygonChecked);

	for (int i = 0; i < vLayerMatchParam.size(); i++)
	{
		LayerMatchParam param = vLayerMatchParam[i];
		// 如果有选择的匹配字段，则进行接边匹配字段xml的生成
		if (param.vFields.size() > 0)
		{
			param.bLineStringChecked = bLineChecked;
			param.bPolygonChecked = bPolygonChecked;
			// 生成Layer节点
			QDomElement layerNode = doc.createElement("Layer");
			root.appendChild(layerNode);

			// 生成LayerName节点
			QDomElement layerName = doc.createElement("LayerName");
			layerNode.appendChild(layerName);
			QDomText strLayerNameText = doc.createTextNode(param.strLayerName.c_str());
			layerName.appendChild(strLayerNameText);

			// 生成LineChecked节点
			QDomElement lineChecked = doc.createElement("LineChecked");
			if (param.bLineStringChecked)
			{
				layerNode.appendChild(lineChecked);
				QDomText strLineCheckedText = doc.createTextNode("true");
				lineChecked.appendChild(strLineCheckedText);
			}
			else
			{
				layerNode.appendChild(lineChecked);
				QDomText strLineCheckedText = doc.createTextNode("false");
				lineChecked.appendChild(strLineCheckedText);
			}

			// 生成PolygonChecked节点
			QDomElement polygonChecked = doc.createElement("PolygonChecked");
			if (param.bPolygonChecked)
			{
				layerNode.appendChild(polygonChecked);
				QDomText strPolygonCheckedText = doc.createTextNode("true");
				polygonChecked.appendChild(strPolygonCheckedText);
			}
			else
			{
				layerNode.appendChild(polygonChecked);
				QDomText strPolygonCheckedText = doc.createTextNode("false");
				polygonChecked.appendChild(strPolygonCheckedText);
			}

			// 生成Fields节点
			QDomElement fields = doc.createElement("Fields");
			layerNode.appendChild(fields);
			for (int j = 0; j < param.vFields.size(); j++)
			{
				// 生成Field节点
				QDomElement field = doc.createElement("Field");
				fields.appendChild(field);
				QString qstr = QString::fromLocal8Bit(param.vFields[j].c_str());
				QDomText strFieldText = doc.createTextNode(qstr);
				field.appendChild(strFieldText);
			}
		}
	}

	QString curPath = QCoreApplication::applicationDirPath();
	QString dlgTitle = "保存配置文件";
	QString filter = "xml 文件(*.xml)";
	QString strFileName = QFileDialog::getSaveFileName(this,
		dlgTitle, curPath, filter);
	if (!strFileName.isEmpty())
	{
		QFile file(strFileName);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
			return;
		QTextStream out(&file);
		out.setCodec("GBK");
		doc.save(out, 4, QDomNode::EncodingFromTextStream);
		file.close();
	}
}

void QgsSetMergeParams::Button_OK_accepted()
{
	// 缓冲区距离大于0
	if (ui.lineEdit_BufferDistance->text().toDouble() < 1e-6)
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle(tr("地理提取与加工"));
		msgBox.setText(tr("缓冲距离应大于0，请重新输入！"));
		msgBox.setStandardButtons(QMessageBox::Yes);
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.exec();
		return;
	}


	accept();
}

void QgsSetMergeParams::Button_Cancel_rejected()
{
	reject();
}

void QgsSetMergeParams::Button_Help_showHelp()
{
	// TODO:增加帮助文档
	//QgsHelp::openHelp( QStringLiteral( "plugins/core_plugins/plugins_offline_editing.html" ) );
}

void QgsSetMergeParams::InitLineEdit()
{
	// 点匹配阈值设置输入限制，非负浮点数
	ui.lineEdit_BufferDistance->setValidator(new QRegExpValidator(QRegExp("^(([0-9]+\.[0-9]*[1-9][0-9]*)|([0-9]*[1-9][0-9]*\.[0-9]+)|([0-9]*[1-9][0-9]*))$")));

}