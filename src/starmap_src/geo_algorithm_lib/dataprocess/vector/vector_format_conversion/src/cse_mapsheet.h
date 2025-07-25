#pragma once
#ifndef STAREARTH_MAPSHEET_H_
#define STAREARTH_MAPSHEET_H_

/*比例尺定义*/
const double SCALE_2M = 2000000;
const double SCALE_1M = 1000000;
const double SCALE_500K = 500000;
const double SCALE_250K = 250000;
const double SCALE_100K = 100000;
const double SCALE_50K = 50000;
const double SCALE_25K = 25000;
const double SCALE_10K = 10000;
const double SCALE_5K = 5000;
const double SCALE_2K = 2000;
const double SCALE_1K = 1000;
const double SCALE_500 = 500;

/*判断小数相等用的值*/
const double EQUAL_VALUE = 0.0000001;

/*为避免使用小数, 采用最小经差的八倍作为单位1。
8秒是一个单位（每秒乘8），1度对应28800（60 * 60 * 8）*/
const double RAD8 = 28800;

#include <vector>
#include <string>

using namespace std;

/*图幅计算类*/
class CSE_MapSheet
{
public:
	CSE_MapSheet();

	~CSE_MapSheet();
	
public:
	/*@brief 根据经纬度范围获取某比例尺图幅列表
	*
	* 根据经纬度范围获取某比例尺图幅列表
	*
	* @param scale:					比例尺分母
	* @param map_type:				地图类型，地形图为'D'
	* @param lon_min:				最小经度，默认-180
	* @param lat_min:				最小纬度，默认-88
	* @param lon_max:				最大经度，默认180
	* @param lat_max:				最大纬度，默认88
	* @param vTileCodes:			返回图幅列表
	*
	*/
	static void get_name_from_bbox(double scale,
		string map_type, 
		double lon_min,
		double lat_min,
		double lon_max, 
		double lat_max,
		vector<string> &vTileCodes);

	/*@brief 根据图幅号获取图幅的经纬度范围
	*
	* 根据图幅号获取图幅的经纬度范围
	*
	* @param strCode:				图幅号
	* @param left:					左边界经度
	* @param top:					上边界纬度
	* @param right:					右边界经度
	* @param bottom:				下边界纬度
	*/
	static bool get_box(string strCode, double& left, double& top, double& right, double& bottom);


	/*@brief 国标图幅转军标图幅
	*
	* 2012版国标图幅名转2008版军标图幅名
	*
	* @param gb_name:				国标图幅名
	* @param map_type:				地图类型，默认为地形图
	* 
	* @return 军标图幅名
	*/
	static string gb_to_gjb(string gb_name, string map_type = "D");

	
	/*@brief 根据经纬度计算某比例尺下图幅
	*
	* 根据经纬度计算某比例尺下图幅
	*
	* @param scale:					比例尺分母
	* @param lon_input:				输入经度，单位为度
	* @param lat_input:				输入纬度，单位为度
	* @param map_type:				地图类型，地形图为'D'			
	* @return 军标图幅名
	*/
	static string get_name_from_latlon(double scale,
		double lon_input,
		double lat_input,
		string map_type = "D");

	/*@brief 计算相邻上、下、左、右四个图幅名
	*
	* 根据比例尺及当前图幅名计算相邻上、下、左、右四个图幅名
	*
	* @param scale:						比例尺分母
	* @param strSheet:					当前图幅名
	* @param strLeftSheet:				左边图幅名
	* @param strTopSheet:				上边图幅名
	* @param strRightSheet:				右边图幅名
	* @param strBottomSheet:			下边图幅名
	* @param map_type:					地图类型，地形图为'D'
	*/
	static void get_adjsheet_by_scale_and_sheet(double scale,
		string strSheet,
		string& strLeftSheet,
		string& strTopSheet,
		string& strRightSheet,
		string& strBottomSheet,
		string map_type = "D");

private:

	static string _gb2012_to_gjb2008_500k_250k_100k(string gb_name, string map_type);
	static string _gb2012_to_gjb2008_50k_25k_10k_5k(string gb_name, string map_type);
	static string _gb2012_to_gjb2008_1m(string gb_name, string map_type);
	static void _get_name_list(double scale, string map_type, double lon_min, double lat_min, double lon_max, double lat_max, vector<string>& vTileInnerCodes, vector<string>& vTileOuterCodes);
	static void _del_outer(vector<string> vTileCodes, double lon_min, double lat_min, double lon_max, double lat_max, vector<string>& vTileCodesResult);

	static double get_scale(string strCode);
	static bool get_box_1m(string strCode, double& left, double& top, double& right, double& bottom);
	static bool _lat_1m(string strCode, double& dLatMax, double& dLatMin);
	static bool _lon_1m(string strCode, double& dLonMax, double& dLonMin);
	static bool get_box_500k_250k_100k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom);
	static int _get_row_nums(double scale);
	static bool _calculate_row_column(int sequence_num, int row_sum, int& row, int& column);
	static void _calculate_bbox(double lon_left, double lat_bottom, double scale, int row, int column, int row_sum, double& lon_min, double& lat_min, double& lon_max, double& lat_max);
	static double get_lat_diff(double scale);
	static double get_lon_diff(double scale);
	static double get_lon_diff(double scale, int lat_region);
	static bool get_bbox_50k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom);
	static bool get_bbox_25k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom);
	static bool get_bbox_10k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom);
	static bool get_bbox_5k_2008(string strCode, double dScale, double& left, double& top, double& right, double& bottom);
	static int get_lat_region(double lat);
	static string get_name_1m(double lon_start, double lat_start, string map_type);
	static string _get_row_1m(double lat_start, double lat_diff);
	static string _get_column_1m(double lat_region, double lon_start, double lon_diff);
	static vector<string> get_large_scale_name_list(string name, double scale, double large_scale);
	static vector<string> _get_name_list_from_1m(string name_1m, double child_scale);
	static vector<string> _get_name_500k_from_1m(string name_1m);
	static vector<string> _get_namelist_from_small_scale(string name_upper);
	static vector<string> _get_name_250k_from_1m(string name_1m);
	static vector<string> _get_name_100k_from_1m(string name_1m);
	static vector<string> _get_name_50k_from_1m(string name_1m);
	static vector<string> _get_name_50k_from_100k(string name_100k);
	static vector<string> _get_name_25k_from_1m(string name_1m);
	static vector<string> _get_name_10k_from_1m(string name_1m);
	static vector<string> _get_name_10k_from_25k(string name_25k);
	static vector<string> _get_name_5k_from_1m(string name_1m);
	static vector<string> _get_name_5k_from_10k(string name_10k);

	static vector<string> _get_name_list_from_500k(string name_500k, double child_scale);
	static vector<string> _get_name_250k_from_500k(string name_500k);
	static vector<string> _get_name_100k_from_500k(string name_500k);
	static vector<string> _get_name_50k_from_500k(string name_500k);
	static vector<string> _get_name_25k_from_500k(string name_500k);
	static vector<string> _get_name_10k_from_500k(string name_500k);
	static vector<string> _get_name_list_from_250k(string name_250k, double child_scale);
	static vector<string> _get_name_100k_from_250k(string name_250k);
	static vector<string> _get_name_list_from_100k(string name_100k, double child_scale);
	static vector<string> _get_name_list_from_50k(string name_50k, double child_scale);
	static vector<string> _get_name_list_from_25k(string name_25k, double child_scale);
	static vector<string> _get_name_list_from_10k(string name_10k, double child_scale);
	static vector<string> _get_name_25k_from_50k(string name_50k);
	static vector<string> _get_name_25k_from_100k(string name_100k);
	static vector<string> _get_name_5k_from_500k(string name_500k);
	static vector<string> _get_name_50k_from_250k(string name_250k);
	static vector<string> _get_name_25k_from_250k(string name_250k);
	static vector<string> _get_name_10k_from_250k(string name_250k);
	static vector<string> _get_name_5k_from_250k(string name_250k);
	static vector<string> _get_name_10k_from_100k(string name_100k);
	static vector<string> _get_name_5k_from_100k(string name_100k);
	static vector<string> _get_name_10k_from_50k(string name_50k);
	static vector<string> _get_name_5k_from_50k(string name_50k);
	static vector<string> _get_name_5k_from_25k(string name_25k);
	static string _find_name_list(vector<string>& name_list, double lon, double lat);
};

#endif