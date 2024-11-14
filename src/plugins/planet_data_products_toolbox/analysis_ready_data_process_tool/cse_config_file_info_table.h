#ifndef CSE_CONFIG_FILE_INFO_TABLE_H
#define CSE_CONFIG_FILE_INFO_TABLE_H

#pragma region "包含头文件（减少重复）"
#include <QWidget>
#include <QString>
#include <string>
#include <vector>
#include "ui_cse_config_file_info_table.h"
#pragma endregion

#pragma region "配置文件结构体"
typedef struct single_record
{
    //  图层类型
    std::string layer_type;
    //  筛选条件
    std::string selection_condition;
    //  级别
    std::string level;
    single_record()
    {
        layer_type = "";
        selection_condition = "";
        level = "";
    }
}single_record_t;

typedef struct config_info
{
    std::vector<single_record_t> vrecords;
    config_info()
    {
        vrecords.clear();
    }
}config_info_t;
#pragma endregion

class cse_config_file_info_table : public QDialog, private Ui::cse_config_file_info_table
{
    Q_OBJECT

public:
#pragma region "1、配置文件信息类公开API接口"
    cse_config_file_info_table(QWidget *parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags(), const QString& config_path = "");
    ~cse_config_file_info_table();
#pragma endregion

#pragma region "2、配置文件信息类私有API接口"
private:
    void parse_config_file(const QString& config_path);
    void parse_config_file_hard_encoding(const QString& config_path);
    void initialize_gdal();
    void updateTableWidget(QTableWidget* tableWidget);
#pragma endregion

#pragma region "3、配置文件信息类私有数据成员"
private:
    Ui::cse_config_file_info_table *m_pocse_config_file_info_table_UI = nullptr;

    config_info_t m_config_info = config_info_t();

#pragma endregion
};
#endif // CSE_CONFIG_FILE_INFO_TABLE_H
