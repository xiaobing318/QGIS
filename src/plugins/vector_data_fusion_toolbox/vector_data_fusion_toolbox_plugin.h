
#ifndef VECTOR_DATA_FUSION_TOOLBOX_H
#define VECTOR_DATA_FUSION_TOOLBOX_H

#pragma region "1 包含头文件（减少重复）"
/*-------------QT-------------*/
#include <QObject>

/*-------------QGIS-------------*/
#include "../qgisplugin.h"
#pragma endregion

#pragma region "2 类的前向声明（声明）"
class QAction;
class QgisInterface;
class qgs_semantic_fusion;
#pragma endregion

#pragma region "3 QgsVectorDataFusionToolBox类"
class QgsVectorDataFusionToolBox : public QObject, public QgisPlugin
{
  Q_OBJECT

public:
  explicit QgsVectorDataFusionToolBox(QgisInterface* qgisInterface);
  ~QgsVectorDataFusionToolBox() override;

public slots:
  //! init the gui
  void initGui() override;

  //! actions

  //  1、JJBDX--->NJBDX语义融合处理工具
  void JJBDX_NJBDX_semantic_fusion();
  //  2、OSM--->NJBDX语义融合处理工具
  void OSM_NJBDX_semantic_fusion();
  //  3、DZHT--->NJBDX语义融合处理工具
  void DZHT_NJBDX_semantic_fusion();
  //  4、JBHT--->NJBDX语义融合处理工具
  void JBHT_NJBDX_semantic_fusion();
  //  5、SW--->NJBDX语义融合处理工具
  void SW_NJBDX_semantic_fusion();
  //  6、JYDH--->NJBDX语义融合处理工具
  void JYDH_NJBDX_semantic_fusion();
  //  7、QQCT--->NJBDX语义融合处理工具
  void QQCT_NJBDX_semantic_fusion();

  //  8、矢量数据要素提取工具
  void vector_data_feature_extraction();

  //! unload the plugin
  void unload() override;
  //! show the help document
  void help();

private:
  //! Pointer to the QGIS interface object
  QgisInterface* mQGisIface = nullptr;

  //  矢量数据语义融合处理工具集
  QAction* mAction_JJBDX_NJBDX_semantic_fusion = nullptr;
  QAction* mAction_OSM_NJBDX_semantic_fusion = nullptr;
  QAction* mAction_DZHT_NJBDX_semantic_fusion = nullptr;
  QAction* mAction_JBHT_NJBDX_semantic_fusion = nullptr;
  QAction* mAction_SW_NJBDX_semantic_fusion = nullptr;
  QAction* mAction_JYDH_NJBDX_semantic_fusion = nullptr;
  QAction* mAction_QQCT_NJBDX_semantic_fusion = nullptr;

  //  矢量数据要素提取工具
  QAction* mActionAttributeExtractTool = nullptr;
  

};
#pragma endregion

#endif // VECTOR_DATA_FUSION_TOOLBOX_H
