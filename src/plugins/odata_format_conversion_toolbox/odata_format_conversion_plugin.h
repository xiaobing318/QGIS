#ifndef QGS_ODATA_FORMAT_CONVERSION_PLUGIN_H
#define QGS_ODATA_FORMAT_CONVERSION_PLUGIN_H

#include "qgisplugin.h"
#include <QObject>

#include <string>
#include <vector>
using namespace std;

class QAction;
class QgisInterface;

class QgsOdataFormatConversionPlugin : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit QgsOdataFormatConversionPlugin(QgisInterface* qgisInterface);
	~QgsOdataFormatConversionPlugin() override;

public slots:

	//! 初始化Gui
	void initGui() override;
	//! actions
	
	// 线划图数据格式转换
	void OdataFormatConversion();

	// 注记指针反向挂接
	void AnnoPointerReverseExtract();

	//	pbf文件向gpkg文件格式进行提取转化
	void pbf2gpkg_func();

  //  将GBK/GB2312字符集编码的文本文件转化成UTF-8字符集编码的文本文件
  void TextFilesGBK2UTF8_tool();

	//! 卸载插件
	void unload() override;

private:
	
	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;

	//!pointer to the qaction for this plugin
	
	// “线划图数据格式转换”菜单
	QAction* mActionVectorConvert = nullptr;

  //  “文本文件字符集编码转化”工具
  QAction* mActionTextFilesGBK2UTF8_tool = nullptr;

	// “注记指针反向挂接”菜单
	QAction* mActionAnnoPointerReverseExtract = nullptr;

	/***********pointer:pbf地理空间数据文件格式向GeoPackage地理空间数据库文件格式转化**********/
	QAction* mAction_pbf2gpkg = nullptr;

private slots:

	void updateActions();

private:



};

#endif // QGS_ODATA_FORMAT_CONVERSION_PLUGIN_H
