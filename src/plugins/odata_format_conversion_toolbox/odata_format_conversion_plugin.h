#ifndef QGS_ODATA_FORMAT_CONVERSION_PLUGIN_H
#define QGS_ODATA_FORMAT_CONVERSION_PLUGIN_H

#pragma region "包含头文件（减少重复）"
/*--------------QGIS---------------*/
#include "qgisplugin.h"
/*--------------QGIS---------------*/

/*--------------QT---------------*/
#include <QObject>
/*--------------QT---------------*/

/*--------------STL---------------*/
#include <string>
#include <vector>
/*--------------STL---------------*/

using namespace std;
#pragma endregion

#pragma region "类的前向声明"
class QAction;
class QgisInterface;
#pragma endregion

#pragma region "odata格式转化插件类"
class QgsOdataFormatConversionPlugin : public QObject, public QgisPlugin
{
	Q_OBJECT

public:
	explicit QgsOdataFormatConversionPlugin(QgisInterface* qgisInterface);
	~QgsOdataFormatConversionPlugin() override;

public slots:

	//  初始化Gui
	void initGui() override;

	//  actions1:线划图数据格式转换
	void OdataFormatConversion();
	//  actions2:注记指针反向挂接
	void AnnoPointerReverseExtract();
	//	actions3:pbf文件向gpkg文件格式进行提取转化
	void pbf2gpkg_func();
  //  actions4:将GBK/GB2312字符集编码的文本文件转化成UTF-8字符集编码的文本文件
  void TextFilesGBK2UTF8_tool();

	// 卸载插件
	void unload() override;

private:
	
	//! Pointer to the QGIS interface object
	QgisInterface* mQGisIface = nullptr;


	//!pointer to the qaction for this plugin
	
	// qaction1:线划图数据格式转换
	QAction* mActionVectorConvert = nullptr;
  // qaction2:文本文件字符集编码转化
  QAction* mActionTextFilesGBK2UTF8_tool = nullptr;
	// qaction3:注记指针反向挂接
	QAction* mActionAnnoPointerReverseExtract = nullptr;
  // qaction4:pbf地理空间数据文件格式向GeoPackage地理空间数据库文件格式转化
	QAction* mAction_pbf2gpkg = nullptr;

private slots:
	void updateActions();

};
#pragma endregion

#endif // QGS_ODATA_FORMAT_CONVERSION_PLUGIN_H
