#ifndef CSE_QualityInspectionTools_H
#define CSE_QualityInspectionTools_H

/* 需要在工程属性【C/C++】【命令行】中添加“/D_HAS_STD_BYTE=0”解决byte冲突 */

#include "commontype/se_config.h"
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;



//	单个点要素
typedef struct geo_point
{
	int point_id;
	string point_x;
	string point_y;
	string point_direction_x;
	string point_direction_y;

} geo_point_t;


typedef struct positioning_point
{
	string positioning_points_x;
	string positioning_points_y;
}positioning_point_t;

//	单个线要素
typedef struct geo_line
{
	//	单个线要素元素组成：（线）要素编号、定位点数 、定位点横纵坐标（有N对定位点横纵坐标点对） 
	long int line_id;
	long int num_positioning_point;
	vector <positioning_point_t> positioning_point_string;

}geo_line_t;


typedef struct geo_polygon_interior_ring
{
	//	单个内环
	vector <geo_point_t> geo_polygon_single_interior_ring;

}geo_polygon_interior_ring_t;

typedef struct geo_polygon_outer_ring
{
	//	单个外环
	vector<geo_point_t> geo_polygon_single_outer_ring;

}geo_polygon_outer_ring_t;

//	单个面要素
typedef struct geo_polygon
{
	//	这个结构体是单个面要素
	//	单个面要素的元素组成有：面要素的编号、表示点横纵坐标、面数K、面 1 定位点数 n、一个外环、0个或者多个内环

	long int polygon_id;
	string mark_point_x;
	string mark_point_y;
	short int num_polygon;
	//	面要素外环的定位点数（内环的定位点数存放在后面，可以在后面读取）
	long int num_positioning_point;
	geo_polygon_outer_ring_t outer_ring;
	vector<geo_polygon_interior_ring_t> interior_ring;

}geo_polygon_t;

//	单个ZB文件中的点线面数据
typedef struct odata
{
/*
	每个要素层有一个几何数据文件，几何数据文件记录同一要素层中点、线、面状要素的空间坐标
数据。空间坐标信息包括平面坐标（横坐标、纵坐标或经度、纬度）和高程坐标，其中高程坐标可空
缺，只记录二维信息。
odata点的文本文件存储方式：
P(用来标识点数据)       4（点要素数量，长整型）
		 1（点数据的id，长整型）      4378.676666（X坐标，双精度型）     18437.189358（Y坐标，双精度型）         0.000000（方向点横坐标，双精度型）         0.000000（方向点纵坐标，双精度型）
		 2      4296.834854     18449.375060         0.000000         0.000000
		 3      4408.152893     18545.543307         0.000000         0.000000
		 4      4535.608754     18575.184205         0.000000         0.000000

odata线的文本文件存储方式：
L（用来标识线数据）       2881（线要素数量，长整型）
		 1（要素编号，长整型）          9（定位点数，长整型）
   11427.637631（定位点 1 横坐标，双精度型 ）      4155.367670（定位点 1 纵坐标，双精度型）     11415.709679      4155.367670     11406.763715      4151.888684     11398.811747      4134.742253     11398.811747      4060.938049
   11400.799739      3978.933377     11395.829759      3904.383676     11396.823755      3870.587812     11405.769719      3846.731907
		 2          4
   20988.027756      1191.486456     21031.710151      1173.057945     21085.630607      1145.415180     21109.860685      1133.129507
		 3          6
   20725.214514      1178.985281     20763.814215      1189.780112     20803.742654      1196.605486     20825.925120      1194.216605     20875.750351      1179.200782
   20877.405895      1178.114888
		 4          6
   22859.250466      1029.159744     22904.949073      1034.582062     22931.718437      1037.869528     22968.663290      1041.783178     23012.183074      1040.061172
   23057.686837      1039.371972

odata面的文本文件存储方式：
A（用来标识面数据）         76（面要素数量，长整型）
		 1（（面）要素编号，长整型）        0.000000（标识点横坐标，双精度型）        0.000000（标识点纵坐标，双精度型）          0（面数 k，短整型）
		 2     4423.651035    18633.362376          1
		57
	4245.786640     18710.791314      4240.928826     18718.860225      4239.611453     18722.977016      4240.270140     18727.587823      4247.351021     18732.198629
	4257.231320     18736.150749      4277.815277     18739.938197      4302.845369     18745.043018      4325.734729     18744.384332      4340.061163     18735.986077
	4352.905552     18723.965046      4362.538844     18717.954531      4374.477539     18715.402120      4382.381779     18715.402120      4388.968645     18710.379635
	4393.167772     18700.746343      4399.342959     18697.288238      4405.765154     18697.535246      4412.352020     18693.912469      4421.244289     18686.172901
	4431.783275     18682.550125      4444.627665     18683.867498      4462.412203     18690.783708      4472.786518     18691.277723      4486.783608     18685.678886
	4497.487266     18679.421364      4509.672969     18678.268662      4520.376626     18684.361513      4524.822761     18696.053201      4529.927582     18701.322694
	4535.855762     18700.664007      4540.466568     18696.876559      4544.748031     18689.631006      4547.547449     18680.574065      4547.547449     18662.954198
	4545.077375     18647.969077      4539.313867     18633.148628      4533.715030     18621.950956      4527.951522     18618.163508      4517.741880     18617.340149
	4506.214864     18620.633582      4483.325504     18621.127597      4467.681696     18612.564671      4451.708546     18610.423940      4428.325171     18610.094596
	4406.423840     18613.552701      4381.393749     18619.645552      4363.115195     18626.891105      4341.872551     18645.828346      4324.746699     18662.789526
	4309.432235     18670.693766      4294.941130     18673.163841      4278.803307     18673.822527      4266.617605     18677.939319      4259.701395     18685.678886
	4255.419932     18697.864589      4245.786640     18710.791314
		 3     9520.960986    18583.155591          1
		51
	9515.015445     18623.948604      9508.078981     18633.692684      9507.748674     18639.803378      9511.382059     18647.895919      9513.529060     18652.850536
	9512.868445     18657.144537      9510.391136     18661.438539      9505.766827     18662.924924      9498.500055     18662.594616      9486.608975     18652.189920
	9471.084508     18630.059298      9467.285968     18622.957680      9469.928431     18616.186370      9474.717894     18612.222677      9480.828588     18603.304366
	9483.140743     18594.716363      9486.774128     18589.101131      9494.206054     18581.008590      9506.922904     18570.273586      9518.979139     18565.153815
	9530.539912     18562.181045      9549.037149     18556.235505      9559.276690     18547.317194      9574.140541     18537.407960      9589.334700     18531.792728
	9599.904550     18527.829034      9610.474399     18525.847187      9622.695788     18525.847187      9628.641328     18526.507803      9634.586868     18536.747345
	9634.586868     18537.407960      9634.586868     18548.638426      9628.971636     18561.850737      9621.374556     18574.732742      9618.401786     18576.714588
	9612.456246     18576.384281      9606.841013     18569.778125      9602.216704     18561.190122      9595.940856     18550.620272      9589.995316     18545.995963
	9579.095158     18545.995963      9574.801157     18550.950580      9567.534385     18565.814431      9564.891923     18576.384281      9558.616075     18587.614746
	9547.055302     18594.881517      9538.136991     18597.523980      9527.897449     18606.442290      9525.254987     18613.709062      9522.942832     18619.324294
	9515.015445     18623.948604

*/
	vector<geo_point_t> single_zb_file_odata_point;
	vector<geo_line_t> single_zb_file_odata_line;
	vector<geo_polygon_t> single_zb_file_odata_polygon;
	string single_zb_file_odata_path;
}odata_t;

typedef struct single_point_feature
{
	long int number_fields;
	long int point_feature_position_id;
	std::string sx_file_data_path;
	//	这里将单个要素的字段值保存在vector中，并且都以string的方式进行存储
	vector<string> feature_fields;
	single_point_feature()
	{
		number_fields = 0;
		point_feature_position_id = 0;
		sx_file_data_path = "";
		feature_fields.clear();
	}
}single_point_feature_t;

typedef struct single_line_feature
{
	long int number_fields;
	long int line_feature_position_id;
	std::string sx_file_data_path;
	vector<string> feature_fields;
	single_line_feature()
	{
		number_fields = 0;
		line_feature_position_id = 0;
		sx_file_data_path = "";
		feature_fields.clear();
	}
}single_line_feature_t;

typedef struct single_polygon_feature
{
	long int number_fields;
	long int polygon_feature_position_id;
	std::string sx_file_data_path;
	vector<string> feature_fields;
	single_polygon_feature()
	{
		number_fields = 0;
		polygon_feature_position_id = 0;
		sx_file_data_path = "";
		feature_fields.clear();
	}
}single_polygon_feature_t;

typedef struct attributes
{
	//	对于同一种要素类型（例如A,B,C等等）其点数据、线数据、面数据的属性字段数量是一样的，但是也有可能是不一致的，因此将其分为三个不同的部分来进行存储其属性信息
	string layer_type;	//	例如A,B,C等等
	vector<single_point_feature_t> point_feature_attributes;
	vector<single_line_feature_t> line_feature_attributes;
	vector<single_polygon_feature_t> polygon_feature_attributes;

}single_sx_file_attributes_t;

typedef struct layer_type_paths
{
	//	这个结构体是用来存放某一种图层类型的所有相关文件，假设图层类型为A，则相关文件（文件路径+文件主干+文件后缀）
	//	SX文件，ZB文件，TP文件，MS文件四个与之相关的文件将会存放到paths中
	string layer_type;
	vector<string> paths;

}layer_type_paths_t;

typedef struct sx_file_rare_word
{
	string layer_type;
	string sx_file_path;
	vector<string> point_lines;
	vector<string> line_lines;
	vector<string> polygon_lines;
}sx_file_rare_word_t;

#pragma region "生僻字统计相关的自定义结构体"

typedef struct iconv_errno
{
	//	SX文件路径
	std::string single_sx_file_path;
	//	要素类别（点、线、面：P、L、A）
	std::string feature_type;
	//	要素的id
	std::string ID;
	//	要素字段id（第几个字段）
	std::string feature_field_ID;
	//	字段属性值的字符编码方式
	size_t err;
	//	被转化的字符编码类型
	std::string fromcode;
	//	转向的字符编码类型
	std::string tocode;
	//	存储字符字节数组（UTF-8字符最大长度为4字节）
	char bytes[5];
	//	不能转化的字节在原始字节数组中开始的地方
	int start_pos;
	//	不能转化的字节长度
	int length;
	iconv_errno()
	{
		single_sx_file_path = "";
		feature_type = "";
		ID = "";
		feature_field_ID = "";
		err = 0;
		fromcode = "";
		tocode = "";
		for (size_t i = 0; i < 5; i++)
		{
			bytes[i] = 0;
		}
		start_pos = 0;
		length = 0;
	}
}iconv_errno_t;

#pragma endregion

class CSE_QualityInspectionTools
{
public:
	CSE_QualityInspectionTools();
	~CSE_QualityInspectionTools();
	//	odata质量检查工具
	static void odataQualityCheckTool(const string& odata_framing_data_path, const string& output_data_path);
	//	数据文件编码规范性检查
	static int DataFileEncodingStandardizationCheck(const string& odata_framing_data_path, const string& log_data_path);
	//	图幅文件、数据文件命名规范性（图幅长度和比例尺对应）
	static int NamingStandardizationOfMapFrameFilesAndDataFiles(const string& odata_framing_data_path, const string& log_data_path);
	//	数据文件完整性检查（每个图层都应该包含4个文件，SX文件，ZB文件，TP文件，SM文件，每个图幅文件包含一个SMS文件）
	static int DataFileIntegrityCheck(const string& odata_framing_data_path, const string& log_data_path);
	//	生僻字统计
	static int RareWordStatistics(const string& odata_framing_data_path, const string& log_data_path);

	// 层类型的枚举（设置为私有的，因为在外部函数中使用不到，所以需要隐藏起来）
	enum LayerType {
		A = 12, B = 10, C = 11, D = 21, E = 12, F = 24, G = 17,
		H = 19, I = 14, J = 12, K = 12, L = 10, M = 9,
		N = 27, O = 14, P = 12, Q = 8, R = 8
	};


private:
#pragma region "公用的一些内部函数或者数据结构"
	
	//	layer_type_mapping将会在构造函数中进行初始化
	static unordered_map<string, LayerType> layer_type_mapping;
	static int getStandardFieldCount(const string& layer_type);

#pragma endregion

#pragma region "数据文件编码规范性检查"


	//	a)	SX文件是否乱码
	static int IsTheSXFileGarbled(const string& odata_framing_data_path, const string& log_data_path);
	//	b)	ZB文件是否存在乱码，是否存在异常值（超出正常精度要求，小数点后6位）
	static int IsTheZBFileGarbledAndOutliers(const string& odata_framing_data_path, const string& log_data_path);
	//	c)	字段缺失或多余
	static int IsFieldMissingOrRedundant(const string& odata_framing_data_path, const string& log_data_path);

	static void process_SX_single_file_FieldMissingOrRedundant(const string& single_sx_file_path, const string& log_data_path);

	static string get_layer_type(const string& single_sx_file_path);

	static void are_single_point_feature_fields_missing_or_redundant(
		const string& layer_type, 
		const single_point_feature_t& single_point_feature,
		const string& log_data_path);

	static void are_single_line_feature_fields_missing_or_redundant(
		const string& layer_type, 
		const single_line_feature_t& single_line_feature,
		const string& log_data_path);

	static void are_single_polygon_feature_fields_missing_or_redundant(
		const string& layer_type, 
		const single_polygon_feature_t& single_polygon_feature,
		const string& log_data_path);

	static int IsTheZBFileGarbled(const string& odata_framing_data_path, const string& log_data_path);

	static int IsTheZBFileOutliers(const string& odata_framing_data_path, const string& log_data_path);

	static void ReadSingleZBFileData(const string& zb_file_data_path, odata_t& single_zb_file_odata);

	static void ReadSingleSXFileData(
		const string& sx_file_data_path, 
		const string& log_data_path,
		single_sx_file_attributes_t& attributes_data);

	static void CheckPathExists(const std::string& path, const std::string& logFile);

	static void GetFilesInDirectory(const std::string& path, std::vector<std::string>& file_paths);

	static void replaceBackslashes(std::string& path);

	static void CheckFileReadability(const std::string& filePath, const std::string& logFile);

	static void CreateFileInPath(
		const std::string& log_data_path, 
		const std::string& file_name, 
		std::string& log_file_path);

	static void PreCheckForPath(
		const string& odata_framing_data_path,
		const string& output_data_path,
		string& log_file_path);

	static void GetZBFile(const vector<string>& file_paths, vector<string>& ZB_file_paths);

	static void GetSXFile(const vector<string>& file_paths, vector<string>& SX_file_paths);

	static int IsTheZBFileOutliers_single_file(
		const string& single_zb_file_path, 
		const string& log_data_path,
		bool& are_all_zb_files_good_flag);

	static void DetermineWhetherThereAreOutliers(
		const odata_t& single_zb_file_odata, 
		const string& log_data_path,
		bool& are_all_zb_files_good_flag);


	static void is_outlier_point_feature(
		const geo_point_t& single_geo_point_feature, 
		const string& log_data_path, 
		const string& single_zb_file_odata_path,
		bool& are_all_zb_files_good_flag);

	static void is_outlier_line_feature(
		const geo_line_t& single_geo_line_feature, 
		const string& log_data_path,
		const string& single_zb_file_odata_path,
		bool& are_all_zb_files_good_flag);

	static void is_outlier_polygon_feature(
		const geo_polygon_t& single_geo_polygon_feature, 
		const string& log_data_path,
		const string& single_zb_file_odata_path,
		bool& are_all_zb_files_good_flag);

	static int countDecimalPlaces(const std::string& number);

	static int log(
		const std::string& log_data_path, 
		const std::string& message);

	static int log_point(
		const std::string& log_data_path, 
		const std::string& single_zb_file_odata_path,
		const geo_point_t& single_geo_point_feature);

	static int log_line(
		const std::string& log_data_path,
		const std::string& single_zb_file_odata_path,
		const geo_line_t& single_geo_line_feature);

	static int log_polygon(
		const std::string& log_data_path, 
		const std::string& single_zb_file_odata_path,
		const geo_polygon_t& single_geo_polygon_feature);

	static void is_single_SXFile_Garbled(const string& single_sx_file_path, const string& log_data_path);
#pragma endregion

#pragma region "生僻字统计"

	static void RareWordStatisticsProcess(const string& single_sx_file_path, const string& log_data_path);

	static bool isUncommonChineseCharacter(const string& character);

	static std::vector<std::string> getGB2312Chars(const std::string& strTemp);

	static int handle_iconv_error(const std::string& log_path, const iconv_errno_t& iconv_errno);

	static void replaceCharacterInString(
		std::string str, 
		char toReplace, 
		char replacement, 
		const std::string& encoding = "GBK");

	static int convert2GB2312(
		const std::string& strTemp, 
		const std::string& fromcode, 
		const std::string& tocode,
		std::string& converted_string,
		iconv_errno_t& iconv_errno);

	static void read_sx_file_for_rare_word(
		const string& single_sx_file_path,
		sx_file_rare_word_t& sx_file_rare_word,
		const string& log_data_path);

	static void get_layer_type_and_number(const std::string& line, std::string& type, int& number);

	static void get_single_field_from_line(const string& line, vector<string>& single_feature_fields);

#pragma endregion


#pragma region "乱码统计"
	static void ZBFileGarbledStatisticsProcess(const string& single_sx_file_path, const string& log_data_path);

	static std::string determineEncoding(const std::string& filePath);

	static std::string readAndConvert(const std::string& filePath, const std::string& encoding);

	static std::string convertEncoding(const std::string& input, const std::string& fromEncoding, const std::string& toEncoding);

	static bool checkCodePoints(const std::string& utf8Str, int rangeStart, int rangeEnd);

	static std::string analyzeGarbledText(const std::string& filePath);

	static void handleErrorAndLog(const std::string& step, const std::string& message);

#pragma endregion

#pragma region "数据文件完整性检查"

	static void classification_file_paths(const vector<string>& file_paths, vector<layer_type_paths_t>& all_layer_type_paths);

	static bool IsTheFileComplete(const layer_type_paths_t& single_layer_type_paths);

#pragma endregion

#pragma region"图幅文件、数据文件命名规范性（图幅长度和比例尺对应）"

	static void IsTheOdataFrameNameLegal(const string& odata_frame_data_name, const string& log_data_path);

	static void IsTheOdataFrameNameLegalUsingScale(
		const string& odata_frame_data_name,
		const int& scale,
		const string& log_data_path);

	static int CountDigitsInString(const string& str);

	static void BasicVerification(const string& odata_frame_data_name, const string& log_data_path);

	static void CharacterCompositionVerification(const string& odata_frame_data_name, const string& log_data_path);

	static void LengthLimitValidation(const string& odata_frame_data_name, const string& log_data_path);

	static void FirstCharacterLimitValidation(const string& odata_frame_data_name, const string& log_data_path);

	static void LetterPartRuleValidation(const string& odata_frame_data_name, const string& log_data_path);

	static void NumberPartRuleValidation(const string& odata_frame_data_name, const string& log_data_path);

	static void AssociationRuleValidationForOtherLengthsandNumbers(const string& odata_frame_data_name, const string& log_data_path);

	static bool FileExtensionCheck(const string& odata_file_name);

	static void FileNameCompliance(
		const string& odata_framing_data_name, 
		const string& odata_file_path,
		const string& log_data_path);

	static void lower_string2upper_string(string& str);

	static void lower_letter2upper_letter(char& character);


#pragma endregion


};


#endif	//	CSE_QualityInspectionTools_H