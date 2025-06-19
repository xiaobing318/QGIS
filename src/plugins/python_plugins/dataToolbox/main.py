import argparse
from qgis.core import QgsApplication
from dataFieldOptimization_dialog import dataFieldOptimizationDialog
from dataEncodeConvert_dialog import dataEncodeConvertDialog
from dataInDatabase_dialog import dataInDatabaseDialog
from dataFormatConvert_dialog import dataFormatConvertDialog
from dataEdgeDetection_dialog import dataEdgeDetectionDialog
from dataEdgeProcess_dialog import dataEdgeProcessDialog
from dataScaleAlignment_dialog import dataScaleAlignmentDialog
from dataUnifiedLogic_dialog import dataUnifiedLogicDialog

# app = QgsApplication([], False)  # 第二个参数为False表示不启动GUI
# app.initQgis()
# dataEdgeDetection = dataEdgeDetectionDialog(None, False)
# # dataEdgeDetection.process("tab", 100000, 0, 2, "D:/测试数据/数据接边检测/tufushp", 1, "D:/测试数据/数据接边检测/图幅范围/tffw.shp", "D:/测试数据/数据接边检测/config.txt", "D:/测试数据/数据接边检测/结果")
# dataEdgeDetection.process("tab", 100000, 1, 0, "D:/gpkg测试数据/数据接边检测/gpkg", 1, "D:/gpkg测试数据/数据接边检测/图幅范围/tffw.shp", "D:/gpkg测试数据/数据接边检测/config.txt", "D:/gpkg测试数据/数据接边检测/result")

if __name__ == "__main__":

    app = QgsApplication([], False)  # 第二个参数为False表示不启动GUI
    app.initQgis()

    parser = argparse.ArgumentParser(description="")
    parser.add_argument('-t', '--type',  type=int, help='调用接口')
    #0数据接边检测 1接边处理 2字段优化 3编码转化 4数据入库 5格式转换 6尺度位置对齐 7高程检测 8高程校正

    parser.add_argument('-i', '--inputpath', type=str, help="输入路径")
    parser.add_argument('-it', '--inputtype', type=int, help="输入类型") #0 shp 1 gpkg
    parser.add_argument('-o', '--outputpath', type=str, help="输出路径")
    parser.add_argument('-ot', '--outputtype', type=int, help="输出类型") #当为要素类型时 0点1线2面  当为输出类型时0为gpkg 1gdb 格式转换时 0gdb->gpkg 1gpkg->gdb
    parser.add_argument('-a', '--affpath', type=str, help="附属文件路径") #接边处理时为检测结果目录 高程检测时为陆地面路径 高程校正时校正后shp路径
    parser.add_argument('-f', '--fieldname', type=str, help='附属字段名称')
    parser.add_argument('-c', '--configpath', type=str, help="配置文件路径")
    parser.add_argument('-l', '--logpath', type=str, help="日志文件路径")
    parser.add_argument('-j', '--detectiontype', type=int, help="接边类型") #0图幅接边 1任意接边
    parser.add_argument('-s', '--scale', type=int, help="比例尺")

    parser.add_argument('-m', '--merge', action='store_true', help='是否合并')
    parser.add_argument('-r', '--resultion', type=float, help='输出分辨率')
    parser.add_argument('-p', '--resample', type=int, help='重采样算法') #0最邻近 1双线性 2双三次 3平均采样法
    
    args = parser.parse_args()
    if args.type == 0:
        dataEdgeDetection = dataEdgeDetectionDialog(None, False)
        dataEdgeDetection.process(args.fieldname, args.scale, args.inputtype, args.outputtype, args.inputpath, args.detectiontype, args.affpath, args.configpath, args.outputpath)
    elif args.type == 1:
        dataEdgeProcess = dataEdgeProcessDialog(None, False)
        dataEdgeProcess.process(args.inputpath, args.affpath, args.outputpath)
    elif args.type == 2:
        dataFieldOptimization = dataFieldOptimizationDialog(None, False)
        dataFieldOptimization.process(args.inputpath, args.configpath, args.logpath, args.outputpath, args.inputtype)
    elif args.type == 3:
        dataEncodeConvert = dataEncodeConvertDialog(None, False)
        dataEncodeConvert.process(args.inputpath, args.outputpath, args.inputtype)
    elif args.type == 4:
        dataInDatabase = dataInDatabaseDialog(None, False)
        dataInDatabase.process(args.inputpath, args.inputtype, args.affpath, args.fieldname, args.outputpath, args.merge, args.outputtype)
    elif args.type == 5:
        dataFormatConvert = dataFormatConvertDialog(None, False)
        dataFormatConvert.process(args.inputpath, args.inputtype, args.outputpath)
    elif args.type == 6:
        dataScaleAlignment = dataScaleAlignmentDialog(None, False)
        dataScaleAlignment.process(args.inputpath, args.outputpath, args.merge, args.resultion, args.resample)
    elif args.type == 7:
        dataUnifiedLogic = dataUnifiedLogicDialog(None, False)
        dataUnifiedLogic.detection_proc(args.inputpath, args.affpath, args.outputpath)
    elif args.type == 8:
        dataUnifiedLogic = dataUnifiedLogicDialog(None, False)
        dataUnifiedLogic.correct_proc(args.inputpath, args.affpath, args.outputpath)
    else:
        print('调用接口错误')