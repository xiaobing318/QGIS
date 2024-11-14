#pragma region "包含头文件（减少重复）"
/*-----------------------QT-----------------------*/
#include <QComboBox>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QTableWidget>

/*-----------------------QT-----------------------*/

/*-----------------------GDAL-----------------------*/
#include "gdal_priv.h"
#include <ogrsf_frmts.h>
#include <ogr_feature.h>
#include "ogr_geometry.h"
/*-----------------------GDAL-----------------------*/

/*-----------------------STL-----------------------*/
#include <filesystem>
#include <fstream> // 包含对文件操作的库
/*-----------------------STL-----------------------*/

#include "cse_config_file_info_table.h"
#include "ui_cse_config_file_info_table.h"
#pragma endregion

#pragma region "1、配置文件信息类公开API接口"
cse_config_file_info_table::cse_config_file_info_table(QWidget* parent, Qt::WindowFlags fl, const QString& config_path)
  :QDialog(parent, fl), m_pocse_config_file_info_table_UI(new Ui::cse_config_file_info_table)
{
  m_pocse_config_file_info_table_UI->setupUi(this);
  //  用于自动保存和恢复窗口的几何布局
  //QgsGui::enableAutoGeometryRestore(this);
  //  加载配置文件信息并且展示
  if(config_path == "")
  {
    //  TODO:处理异常情况
  }
  parse_config_file_hard_encoding(config_path);

  //  设置表格相关内容
	// 表头居中
  this->m_pocse_config_file_info_table_UI->tableWidget_config_info->horizontalHeader()->setDefaultAlignment(Qt::AlignHCenter);
  this->m_pocse_config_file_info_table_UI->tableWidget_config_info->setAlternatingRowColors(true);
	// 设置表格内容双击可编辑
  this->m_pocse_config_file_info_table_UI->tableWidget_config_info->setEditTriggers(QAbstractItemView::DoubleClicked);
	// 最后一列铺满最后
  this->m_pocse_config_file_info_table_UI->tableWidget_config_info->horizontalHeader()->setStretchLastSection(true);

  //  根据m_config_info中的信息将tableWidget_config_info显示出来，给出一个函数
  updateTableWidget(this->m_pocse_config_file_info_table_UI->tableWidget_config_info);
}

cse_config_file_info_table::~cse_config_file_info_table()
{
    delete this->m_pocse_config_file_info_table_UI;
}
#pragma endregion

#pragma region "2、配置文件信息类私有API接口"
//  解析配置文件信息并且保存下来
void cse_config_file_info_table::parse_config_file(const QString& config_path)
{
  initialize_gdal();

	if (config_path == "")
	{
		return;
	}
	// 打开CSV文件
	GDALDataset* poCSVDS = static_cast<GDALDataset*>(GDALOpenEx(config_path.toStdString().c_str(), GDAL_OF_ALL, NULL, NULL, NULL));

	if (!poCSVDS)
	{
		return;
	}

	// 获取图层（CSV文件被视为一个图层）
	OGRLayer* poCSVLayer = poCSVDS->GetLayer(0);


	// 重置图层的读取指针到起始位置
	poCSVLayer->ResetReading();

	// 遍历图层中的所有要素（即CSV文件中的每一行）
	OGRFeature* poCSVFeature = nullptr;

	//  获取要素的所有属性（即CSV文件中的每一列）
  single_record_t single_record;

	while ((poCSVFeature = poCSVLayer->GetNextFeature()) != NULL)
	{

		//  图层类型
		std::string layer_type = poCSVFeature->GetFieldAsString(0);

		//  筛选条件
		std::string selection_condition = poCSVFeature->GetFieldAsString(1);

		//  级别
    std::string level = poCSVFeature->GetFieldAsString(2);

    single_record.layer_type = layer_type;
    single_record.selection_condition = selection_condition;
    single_record.level = level;

    this->m_config_info.vrecords.push_back(single_record);
		// 释放要素资源
		OGRFeature::DestroyFeature(poCSVFeature);
	}

	// 关闭并释放数据集资源
	GDALClose(poCSVDS);
}

//  配置文件中的内容硬编码
void cse_config_file_info_table::parse_config_file_hard_encoding(const QString& config_path)
{
  

  if (config_path == "")
  {
    return;
  }

  //  获取要素的所有属性（即CSV文件中的每一列）
  single_record_t single_record01;
  single_record01.layer_type = "L";
  single_record01.selection_condition = "\"TYPE\" = \'coniferous_forest\'";
  single_record01.level = "5";
  this->m_config_info.vrecords.push_back(single_record01);

  single_record_t single_record02;
  single_record02.layer_type = "L";
  single_record02.selection_condition = "\"TYPE\" = \'broad_leaf_forest\'";
  single_record02.level = "10";
  this->m_config_info.vrecords.push_back(single_record02);

  single_record_t single_record03;
  single_record03.layer_type = "L";
  single_record03.selection_condition = "\"TYPE\" = \'not_forest\'";
  single_record03.level = "1";
  this->m_config_info.vrecords.push_back(single_record03);

  single_record_t single_record04;
  single_record04.layer_type = "F";
  single_record04.selection_condition = "\"fclass\" = \'reservoir\'";
  single_record04.level = "10";
  this->m_config_info.vrecords.push_back(single_record04);

  single_record_t single_record05;
  single_record05.layer_type = "F";
  single_record05.selection_condition = "\"fclass\" = \'riverbank\'";
  single_record05.level = "8";
  this->m_config_info.vrecords.push_back(single_record05);

  single_record_t single_record06;
  single_record06.layer_type = "F";
  single_record06.selection_condition = "\"fclass\" = \'water\'";
  single_record06.level = "5";
  this->m_config_info.vrecords.push_back(single_record06);

  single_record_t single_record07;
  single_record07.layer_type = "F";
  single_record07.selection_condition = "\"fclass\" = \'wetland\'";
  single_record07.level = "8";
  this->m_config_info.vrecords.push_back(single_record07);

  single_record_t single_record08;
  single_record08.layer_type = "C";
  single_record08.selection_condition = "\"TYPE\" = \''with_forest_land\'";
  single_record08.level = "5";
  this->m_config_info.vrecords.push_back(single_record08);

  single_record_t single_record09;
  single_record09.layer_type = "LANDUSE";
  single_record09.selection_condition = "\"TYPE\" = \'water\'";
  single_record09.level = "10";
  this->m_config_info.vrecords.push_back(single_record09);

  single_record_t single_record10;
  single_record10.layer_type = "LANDUSE";
  single_record10.selection_condition = "\"TYPE\" = \'coniferous_forest\'";
  single_record10.level = "8";
  this->m_config_info.vrecords.push_back(single_record10);

  single_record_t single_record11;
  single_record11.layer_type = "LANDUSE";
  single_record11.selection_condition = "\"TYPE\" = \'sparse_forest_land\'";
  single_record11.level = "5";
  this->m_config_info.vrecords.push_back(single_record11);

  single_record_t single_record12;
  single_record12.layer_type = "LANDUSE";
  single_record12.selection_condition = "\"TYPE\" = \'grass\'";
  single_record12.level = "3";
  this->m_config_info.vrecords.push_back(single_record12);

  single_record_t single_record13;
  single_record13.layer_type = "LANDUSE";
  single_record13.selection_condition = "\"TYPE\" = \'coniferous_forest\'";
  single_record13.level = "1";
  this->m_config_info.vrecords.push_back(single_record13);

  single_record_t single_record14;
  single_record14.layer_type = "GEOLOGY";
  single_record14.selection_condition = "\"TYPE\" = \'dry_land\'";
  single_record14.level = "2";
  this->m_config_info.vrecords.push_back(single_record14);

  single_record_t single_record15;
  single_record15.layer_type = "D";
  single_record15.selection_condition = "";
  single_record15.level = "2";
  this->m_config_info.vrecords.push_back(single_record15);

}


//  初始化GDAL并设置相关选项
void cse_config_file_info_table::initialize_gdal()
{
  // 注册所有的驱动
  GDALAllRegister();

  // 设置文件名为UTF-8编码，以支持非ASCII字符的文件名
  CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");

  // 设置Shapefile的编码为UTF-8，以正确处理多语言文本字段
  CPLSetConfigOption("SHAPE_ENCODING", "UTF-8");
}

//  填充表格
void cse_config_file_info_table::updateTableWidget(QTableWidget* tableWidget)
{
    // 清空表格当前内容
    tableWidget->setRowCount(0);

    // 设置行数
    tableWidget->setRowCount(m_config_info.vrecords.size());

    // 遍历配置信息，并填充表格
    for (size_t i = 0; i < m_config_info.vrecords.size(); ++i)
    {
      const single_record_t& record = m_config_info.vrecords[i];
      // 设置图层类型
      QTableWidgetItem* itemLayerType = new QTableWidgetItem(QString::fromStdString(record.layer_type));
      tableWidget->setItem(i, 0, itemLayerType);
      // 设置筛选条件
      QTableWidgetItem* itemSelectionCondition = new QTableWidgetItem(QString::fromStdString(record.selection_condition));
      tableWidget->setItem(i, 1, itemSelectionCondition);
      // 设置级别
      QTableWidgetItem* itemLevel = new QTableWidgetItem(QString::fromStdString(record.level));
      tableWidget->setItem(i, 2, itemLevel);
    }
}

#pragma endregion
