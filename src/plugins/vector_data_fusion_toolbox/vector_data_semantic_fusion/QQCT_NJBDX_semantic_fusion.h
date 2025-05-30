#ifndef QQCT_NJBDX_SEMANTIC_FUSION_H
#define QQCT_NJBDX_SEMANTIC_FUSION_H

#pragma region "1 包含头文件（减少重复）"
/*-------------STL-------------*/
#include <vector>
#include <string>

/*-------------QT-------------*/
#include <QDialog>
#include <QString>

/*-------------QGIS-------------*/
#include "qgsexpressioncontext.h"

/*-------------UI-------------*/
#include "ui_QQCT_NJBDX_semantic_fusion.h"

/*-------------自定义-------------*/
#include "QQCT_NJBDX_semantic_fusion_common.h"
#include "QQCT_NJBDX_Config_Reader.h"
#pragma endregion

#pragma region "2 qgs_QQCT_NJBDX_semantic_fusion类"
class qgs_QQCT_NJBDX_semantic_fusion : public QDialog
{
	Q_OBJECT

public:
	qgs_QQCT_NJBDX_semantic_fusion(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
	~qgs_QQCT_NJBDX_semantic_fusion() override;

private:
/**************************************************类成员对象set\get函数******************************************************/
	//  获取输入数据目录成员函数
	QString get_m_input_data_path();

	//  获取输出数据目录成员函数
	QString get_m_output_data_path();

    //  设置输入数据目录成员函数
    void set_m_input_data_path(const std::string& input_data_path);

    //  设置输出数据目录成员函数
    void set_m_output_data_path(const std::string& output_data_path);

    //  恢复保存参数
    void restore_states();

    //  保存参数
    void save_states();
/**************************************************类成员对象set\get函数******************************************************/




/**************************************************类辅助成员函数******************************************************/
    //  获取指定目录中的多个子目录
    QStringList get_sub_directories(const QString& file_directory_path);

	//  判断文件目录是否存在
	bool is_exist_directory(const QString& qstr_file_path);

	//  判断文件是否存在
	bool is_exist_file(const QString& qstr_file_path);

    //  自定义信息提示框
    void showMessageBox(const QString& title, const QString& text);

    //  辅助函数：转换路径为统一格式
    QString normalizePath(const QString& path);
/**************************************************类辅助成员函数******************************************************/


/**************************************************QQCT--->NJBDX内部函数******************************************************/
    //  QQCT--->NJBDX转化：总入口
    void QQCT2NJBDX(
      const std::vector<std::string>& vstr_input_frame_data_path,
      const std::string& str_output_data_path,
      const QQCT_Layers_Fields_Info_Json_t& QQCT_Layers_Fields_Info_Json_entity,
      const QQCT_Layers_Mapping_NJBDX_Layers_Json_t& QQCT_Layers_Mapping_NJBDX_Layers_Json_entity,
      const QQCT_Classification_Code_Conditional_Mapping_Lists_Json_t& QQCT_Classification_Code_Conditional_Mapping_Lists_Json_entity,
      const QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t& QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity,
      const QQCT_NJBDX_Layers_Fields_Info_Json_t& NJBDX_Layers_Fields_Info_Json_entity);
    //  QQCT--->NJBDX转化：单图幅
    int QQCT2NJBDX_Single_Frame_Data(
      const std::string& single_frame_data_path,
      const std::string& str_output_data_path,
      const QQCT_Layers_Fields_Info_Json_t& QQCT_Layers_Fields_Info_Json_entity,
      const QQCT_Layers_Mapping_NJBDX_Layers_Json_t& QQCT_Layers_Mapping_NJBDX_Layers_Json_entity,
      const QQCT_Classification_Code_Conditional_Mapping_Lists_Json_t& QQCT_Classification_Code_Conditional_Mapping_Lists_Json_entity,
      const QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t& QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity,
      const QQCT_NJBDX_Layers_Fields_Info_Json_t& NJBDX_Layers_Fields_Info_Json_entity);

    //  根据QQCT数据集信息和NJBDX数据集信息、分类编码配置信息进行字段属性映射
    void QQCT_Mapping_NJBDX_Fields(
      const QQCT_single_frame_data_all_layers_info_t& QQCT_single_frame_data_all_layers_info_entity,
      const QQCT_single_frame_data_all_layers_info_t& QQCT_single_frame_data_all_layers_info_empty_entity,
      const QQCT_NJBDX_single_frame_data_all_layers_info_t& NJBDX_single_frame_data_all_layers_info_entity,
      const QQCT_Layers_Fields_Info_Json_t& QQCT_Layers_Fields_Info_Json_entity,
      const QQCT_NJBDX_Layers_Fields_Info_Json_t& NJBDX_Layers_Fields_Info_Json_entity,
      const QQCT_Layers_Mapping_NJBDX_Layers_Json_t& QQCT_Layers_Mapping_NJBDX_Layers_Json_entity,
      const QQCT_Classification_Code_Conditional_Mapping_Lists_Json_t& QQCT_Classification_Code_Conditional_Mapping_Lists_Json_entity,
      const QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_t& QQCT_Other_Fields_Mapping_NJBDX_Other_Fields_Json_entity,
      std::shared_ptr<spdlog::logger> single_frame_data_successful_logger,
      std::shared_ptr<spdlog::logger> single_frame_data_failed_logger);



    //  从QQCT_Classification_Code_Conditional_Mapping_Lists.json表达式中提取NJBDX'分类编码'
    std::string QQCT_extractClassificationCode(const std::string& input);
    
    //  对单字段计算QGIS表达式
    std::string QQCT_execute_single_qgis_expression(
      const std::string& expression,
      OGRFeature* poSourceFeature,
      OGRFeature* poTargetFeature,
      std::shared_ptr<spdlog::logger> logger);

    //  对多字段计算QGIS表达式
    std::vector<std::string> QQCT_execute_multiple_qgis_expressions(
      const std::vector<std::string>& expressions,
      OGRFeature* poSourceFeature,
      OGRFeature* poTargetFeature,
      std::shared_ptr<spdlog::logger> logger);

    //  将OGRFeature转化成QgsFeature
    QgsFeature convertOGRFeatureToQgsFeature(OGRFeature* poSourceFeature);

    //  将一个源要素的几何信息拷贝到目的要素中
    void QQCT_copy_geometry_from_source_to_target(
      OGRFeature* poSourceFeature,
      OGRFeature* poTargetFeature,
      std::shared_ptr<spdlog::logger> single_frame_data_logger);

    //  设置'分类编码'字段属性值
    void QQCT_mapping_classification_code_field(
      OGRFeature* poTargetFeature,
      const std::string& targetFieldName,
      const std::string& classificationCodeExpr);

    //  映射QQCT其他字段属性值
    void QQCT_mapping_other_fields(
      OGRFeature* poSourceFeature,
      OGRFeature* poTargetFeature,
      const QQCT_Layers_Other_Fields_Mapping_t& QQCT_Layers_Other_Fields_Mapping_entity,
      std::shared_ptr<spdlog::logger> single_frame_data_logger);

    //  根据要素来填充表达式上下文
    void QQCT_fillExpressionContextFromOGRFeature(
      OGRFeature* poSourceFeature,
      QgsExpressionContext& context);

    //  将原图层中的要素拷贝到目标图层中
    void CopyFeaturesToEmptyLayer(
        OGRLayer* pSrcLayer,
        OGRLayer* pDestLayer,
        std::shared_ptr<spdlog::logger> logger);

    //  将原图层中的指定要素要素拷贝到目标图层中
    void CopyOneFeatureToEmptyLayer(
        OGRFeature* poSrcFeature,
        OGRLayer* pDestLayer,
        std::shared_ptr<spdlog::logger> logger);



    //  获取QQCT单个分幅数据集的信息
    void QQCT_Create_single_frame_data_all_layers_info(
      GDALDataset* poSingleFrameQQCTDataSet,
      QQCT_single_frame_data_all_layers_info_t& QQCT_single_frame_data_all_layers_info_entity,
      std::shared_ptr<spdlog::logger> single_frame_data_logger);
    //  根据QQCT单个分幅数据中所有图层的信息创建一个空的输出数据集
    void CreateEmptyDatasetFromQQCTInfo(
        const QQCT_single_frame_data_all_layers_info_t& QQCT_single_frame_data_all_layers_info_entity,
        QQCT_single_frame_data_all_layers_info_t& QQCT_single_frame_data_all_layers_info_empty_entity,
        const std::string& newDatasetPath,
        GDALDriver* pDriver,
        std::shared_ptr<spdlog::logger> logger);
    //  检查QQCT数据集中的有效图层是否为空
    bool QQCT_Check_single_frame_data_all_layers_info(
      const QQCT_single_frame_data_all_layers_info_t& QQCT_single_frame_data_all_layers_info_entity,
      std::shared_ptr<spdlog::logger> single_frame_data_logger);
    //  释放QQCT单个分幅数据集资源
    void QQCT_Close_single_frame_data_all_layers_info(
      QQCT_single_frame_data_all_layers_info_t& QQCT_single_frame_data_all_layers_info_entity);
    //  删除空图层并且关闭非空图层
    void QQCT_Delete_And_Close_single_frame_data_all_layers_info(
        QQCT_single_frame_data_all_layers_info_t& QQCT_single_frame_data_all_layers_info_entity);


    //  根据 QQCT 图层、图层映射关系和字段信息，创建对应的 NJBDX 图层
    bool QQCT_Create_NJBDX_all_layers_info(
      const QQCT_single_frame_data_all_layers_info_t& QQCT_single_frame_data_all_layers_info_entity,
      const QQCT_Layers_Mapping_NJBDX_Layers_Json_t& QQCT_Layers_Mapping_NJBDX_Layers_Json_entity,
      const QQCT_Layers_Fields_Info_Json_t& QQCT_Layers_Fields_Info_Json_entity,
      const QQCT_NJBDX_Layers_Fields_Info_Json_t& NJBDX_Layers_Fields_Info_Json_entity,
      const std::string& NJBDX_str_output_data_path,
      QQCT_NJBDX_single_frame_data_all_layers_info_t& NJBDX_single_frame_data_all_layers_info_entity,
      std::shared_ptr<spdlog::logger> single_frame_data_logger);
    //  检查NJBDX数据集中的有效图层是否为空
    bool QQCT_Check_NJBDX_all_layers_info(
      const QQCT_NJBDX_single_frame_data_all_layers_info_t& NJBDX_single_frame_data_all_layers_info_entity);
    //  释放NJBDX单个分幅数据集资源
    void QQCT_Close_NJBDX_all_layers_info(
      QQCT_NJBDX_single_frame_data_all_layers_info_t& NJBDX_single_frame_data_all_layers_info_entity);



    //  提取图层名称
    std::string QQCT_Extract_LayerName(
      const std::string& layerName,
      const QQCT_Layers_Fields_Info_Json_t& QQCT_Layers_Fields_Info_Json_entity,
      std::shared_ptr<spdlog::logger> logger);

    //  辅助函数：将OGRwkbGeometryType映射到字符串
    std::string QQCT_OGRGeometryType2String(const OGRwkbGeometryType& GeoType);
    //  辅助函数：从过滤条件中提取所有字段名称,该函数使用正则表达式，匹配所有形如 "字段名" 后面紧跟比较运算符的模式
    std::vector<std::string> extractFieldNames(const std::string& filter);
    //  辅助函数：1. 提取|符号前的子字符串并自动除去前后空格
    std::string extractBeforeDelimiter(const std::string& str);
    //  辅助函数：2. 提取|符号后的子字符串并自动除去前后空格
    std::string extractAfterDelimiter(const std::string& str);
/**************************************************QQCT--->NJBDX内部函数******************************************************/
private slots:
    void Button_input_data_view();
    void Button_output_data_view();
    void Button_OK();
    void Button_Cancel();


private:

	Ui_qgs_QQCT_NJBDX_semantic_fusion ui;

	//  输入数据路径
	QString m_input_data_path;
	//  输出数据路径
	QString m_output_data_path;

    //  用来对处理结果进行统计的内容变量，总的待处理的图幅数量、处理成功的图幅数量、处理失败的图幅数量
    int m_counter_total;                  //  记录总的分幅数据数量
    int m_counter_succeeded;              //  记录处理成功的分幅数据数量

};
#pragma endregion

#endif // QQCT_NJBDX_SEMANTIC_FUSION_H
