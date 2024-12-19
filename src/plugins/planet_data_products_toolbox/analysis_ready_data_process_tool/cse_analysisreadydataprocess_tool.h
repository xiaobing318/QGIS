#ifndef CSE_ANALYSISREADYDATAPROCESS_H
#define CSE_ANALYSISREADYDATAPROCESS_H

#pragma region "包含头文件（减少重复）"
#include <QDialog>
#include <QWidget>
#include <QString>
#include "ui_cse_analysisreadydataprocess_tool.h"
#pragma endregion


class CSE_AnalysisReadyDataProcess : public QDialog, private Ui::CSE_AnalysisReadyDataProcess
{
    Q_OBJECT

public:
#pragma region "1、分析就绪数据工具公开API接口"
    CSE_AnalysisReadyDataProcess(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
    ~CSE_AnalysisReadyDataProcess();
#pragma endregion

#pragma region "2、分析就绪数据工具私有API接口"
private slots:
    void pushButton_vector_data_input_path_browse();
    void pushButton_DEM_data_input_path_browse();
    void pushButton_vector_config_input_path_browse();
    void pushButton_vector_config_input_path_edit();
    void pushButton_data_output_path_browse();
    void pushButton_run();
    void pushButton_cancel();
#pragma endregion

#pragma region "3、分析就绪数据工具私有数据成员"
private:
    Ui::CSE_AnalysisReadyDataProcess *m_poCSE_AnalysisReadyDataProcess_UI = nullptr;

    QString m_lineEdit_vector_data_input_path = "";
    QString m_lineEdit_DEM_data_input_path = "";
    QString m_lineEdit_vector_config_input_path = "";
    QString m_lineEdit_data_output_path = "";
    

#pragma endregion
};
#endif // CSE_ANALYSISREADYDATAPROCESS_H
