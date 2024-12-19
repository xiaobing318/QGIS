#pragma once
#ifndef STAREARTH_MAPSHEET_H_
#define STAREARTH_MAPSHEET_H_

/*�����߶���*/
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

/*�ж�С������õ�ֵ*/
const double EQUAL_VALUE = 0.0000001;

/*Ϊ����ʹ��С��, ������С����İ˱���Ϊ��λ1��
8����һ����λ��ÿ���8����1�ȶ�Ӧ28800��60 * 60 * 8��*/
const double RAD8 = 28800;

#include <vector>
#include <string>

using namespace std;

/*ͼ��������*/
class CSE_MapSheet
{
public:
	CSE_MapSheet();

	~CSE_MapSheet();
	
public:
	/*@brief ���ݾ�γ�ȷ�Χ��ȡĳ������ͼ���б�
	*
	* ���ݾ�γ�ȷ�Χ��ȡĳ������ͼ���б�
	*
	* @param scale:					�����߷�ĸ
	* @param map_type:				��ͼ���ͣ�����ͼΪ'D'
	* @param lon_min:				��С���ȣ�Ĭ��-180
	* @param lat_min:				��Сγ�ȣ�Ĭ��-88
	* @param lon_max:				��󾭶ȣ�Ĭ��180
	* @param lat_max:				���γ�ȣ�Ĭ��88
	* @param vTileCodes:			����ͼ���б�
	*
	*/
	static void get_name_from_bbox(double scale,
		string map_type, 
		double lon_min,
		double lat_min,
		double lon_max, 
		double lat_max,
		vector<string> &vTileCodes);

	/*@brief ����ͼ���Ż�ȡͼ���ľ�γ�ȷ�Χ
	*
	* ����ͼ���Ż�ȡͼ���ľ�γ�ȷ�Χ
	*
	* @param strCode:				ͼ����
	* @param left:					��߽羭��
	* @param top:					�ϱ߽�γ��
	* @param right:					�ұ߽羭��
	* @param bottom:				�±߽�γ��
	*/
	static bool get_box(string strCode, double& left, double& top, double& right, double& bottom);


	/*@brief ����ͼ��ת����ͼ��
	*
	* 2012�����ͼ����ת2008�����ͼ����
	*
	* @param gb_name:				����ͼ����
	* @param map_type:				��ͼ���ͣ�Ĭ��Ϊ����ͼ
	* 
	* @return ����ͼ����
	*/
	static string gb_to_gjb(string gb_name, string map_type = "D");

	
	/*@brief ���ݾ�γ�ȼ���ĳ��������ͼ��
	*
	* ���ݾ�γ�ȼ���ĳ��������ͼ��
	*
	* @param scale:					�����߷�ĸ
	* @param lon_input:				���뾭�ȣ���λΪ��
	* @param lat_input:				����γ�ȣ���λΪ��
	* @param map_type:				��ͼ���ͣ�����ͼΪ'D'			
	* @return ����ͼ����
	*/
	static string get_name_from_latlon(double scale,
		double lon_input,
		double lat_input,
		string map_type = "D");

	/*@brief ���������ϡ��¡������ĸ�ͼ����
	*
	* ���ݱ����߼���ǰͼ�������������ϡ��¡������ĸ�ͼ����
	*
	* @param scale:						�����߷�ĸ
	* @param strSheet:					��ǰͼ����
	* @param strLeftSheet:				���ͼ����
	* @param strTopSheet:				�ϱ�ͼ����
	* @param strRightSheet:				�ұ�ͼ����
	* @param strBottomSheet:			�±�ͼ����
	* @param map_type:					��ͼ���ͣ�����ͼΪ'D'
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