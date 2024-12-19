
#ifndef VECTOR_DATA_FUSION_TOOLBOX_H
#define VECTOR_DATA_FUSION_TOOLBOX_H

/*-------------QGIS-------------*/
#include "../qgisplugin.h"

/*-------------语义融合工具相关头文件-------------*/
#include "semantic_fusion.h"

/*-------------QT-------------*/
#include <QObject>

class QAction;
class QgisInterface;
class qgs_semantic_fusion;

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
  //  1、语义融合处理工具
  void semantic_fusion();
  //  2、（矢量要素）数据提取工具
  void vector_data_feature_extraction();

  //! unload the plugin
  void unload() override;
  //! show the help document
  void help();

private:
  //! Pointer to the QGIS interface object
  QgisInterface* mQGisIface = nullptr;


  //  语义融合处理工具
  QAction* mAction_semantic_fusion = nullptr;

  // （矢量要素）数据提取工具
  QAction* mActionAttributeExtractTool = nullptr;
  

};

#endif // VECTOR_DATA_FUSION_TOOLBOX_H
