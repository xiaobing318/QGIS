#[[
杨小兵-2024-03-04
一、解释
1. `add_subdirectory`命令在CMake中被用来向当前的构建树中添加子目录。这个命令的主要目的不仅仅是添加头文件搜索路径，也不是仅仅添加cpp文件路径，
而是更全面的。当你使用`add_subdirectory`命令时，CMake会在指定的子目录中寻找`CMakeLists.txt`文件，并执行它。这样做的结果是，子目录中定义的目
标（如库或可执行文件）、变量等将被添加到整个项目构建中。通过这种方式，你可以组织项目的结构，将项目分解成多个逻辑上独立的部分，每部分有自己的
`CMakeLists.txt`文件。在你提供的例子中，`add_subdirectory(../../external/libdxfrw dwg/libdxfrw)`命令将`../../external/libdxfrw`目录添
加到构建树中，并指定这个目录在构建输出中的基本路径为`dwg/libdxfrw`。

2. `SUBDIRS`命令是旧版CMake的命令，用于添加子目录到构建过程中。与`add_subdirectory`类似，`SUBDIRS`也指示CMake处理子目录中的`CMakeLists.txt`
文件。但是，建议使用`add_subdirectory`代替`SUBDIRS`，因为`add_subdirectory`提供了更多的灵活性和控制。`SUBDIRS`命令现在已经被视为过时，可能在
未来的CMake版本中被完全移除。在你的例子中，`SUBDIRS(${CMAKE_SOURCE_DIR}/external/qt-unix-signals)`会在非Windows系统上添加
`${CMAKE_SOURCE_DIR}/external/qt-unix-signals`目录到构建过程中。这种方式通常用于包含特定平台相关代码的情况，在这个例子中，可能是因为
`qt-unix-signals`模块仅在非Windows平台上需要。

二、总结
1、add_subdirectory的作用不仅仅是添加.h和.cpp文件，最为重要的是找到指定目录下的cmakelists.txt并且执行其内容，这样可以使得模块功能分开
2、其中`qt-unix-signals`模块仅在不是Windows平台上需要（例如unix、mac os两种平台上）
]]
add_subdirectory(../../external/libdxfrw dwg/libdxfrw)
if(NOT WIN32)
SUBDIRS(${CMAKE_SOURCE_DIR}/external/qt-unix-signals)
endif()





#[[
杨小兵-2024-03-04
一、解释
  将*.cpp文件存放在QGIS_APP_SRCS变量中，QGIS_APP_SRCS这个变量将会在后面的构建build中使用到
]]
set(QGIS_APP_SRCS
  ${CMAKE_SOURCE_DIR}/external/nmea/gmath.c
  ${CMAKE_SOURCE_DIR}/external/nmea/time.c

  qgisapp.cpp
  qgisappinterface.cpp
  qgisappstylesheet.cpp
  qgsabout.cpp
  qgsalignrasterdialog.cpp
  qgsappauthrequesthandler.cpp
  qgsappbrowserproviders.cpp
  qgsappcoordinateoperationhandlers.cpp
  qgsapplayertreeviewmenuprovider.cpp
  qgsappwindowmanager.cpp
  qgsappscreenshots.cpp
  qgsanimationexportdialog.cpp
  qgsannotationwidget.cpp
  qgsappsslerrorhandler.cpp
  qgsattributetabledialog.cpp
  qgsbookmarks.cpp
  qgsbookmarkeditordialog.cpp
  qgsclipboard.cpp
  qgscustomization.cpp
  qgsdatumtransformtablewidget.cpp
  qgsdevtoolspanelwidget.cpp
  qgsdiscoverrelationsdialog.cpp
  qgsdxfexportdialog.cpp
  qgsformannotationdialog.cpp
  qgsguivectorlayertools.cpp
  qgshtmlannotationdialog.cpp
  qgsdelattrdialog.cpp
  qgsdisplayangle.cpp
  qgsfirstrundialog.cpp
  qgsfixattributedialog.cpp
  qgsgeometryvalidationservice.cpp
  qgsgeometryvalidationdock.cpp
  qgsgeometryvalidationmodel.cpp
  qgsidentifyresultsdialog.cpp
  qgsfeatureaction.cpp
  qgslayercapabilitiesmodel.cpp
  qgslayernotesmanager.cpp
  qgslayertreegrouppropertieswidget.cpp
  qgslayertreeviewindicatorprovider.cpp
  qgslayertreeviewembeddedindicator.cpp
  qgslayertreeviewfilterindicator.cpp
  qgslayertreeviewlowaccuracyindicator.cpp
  qgslayertreeviewmemoryindicator.cpp
  qgslayertreeviewnocrsindicator.cpp
  qgslayertreeviewnonremovableindicator.cpp
  qgslayertreeviewnotesindicator.cpp
  qgslayertreeviewbadlayerindicator.cpp
  qgslayertreeviewtemporalindicator.cpp
  qgslayertreeviewofflineindicator.cpp
  qgsmapcanvasdockwidget.cpp
  qgsmapsavedialog.cpp
  qgsprojectlistitemdelegate.cpp
  qgsprovidersublayersdialog.cpp
  qgspuzzlewidget.cpp
  qgsversionmigration.cpp
  qgssnappinglayertreemodel.cpp
  qgssnappingwidget.cpp
  qgsstatusbarcoordinateswidget.cpp
  qgsstatusbarmagnifierwidget.cpp
  qgsstatusbarscalewidget.cpp
  qgstemplateprojectsmodel.cpp
  qgstemporalcontrollerdockwidget.cpp
  qgsversioninfo.cpp
  qgsrecentprojectsitemsmodel.cpp
  qgsvectorlayerdigitizingproperties.cpp
  qgswelcomepage.cpp
  qgsdockablewidgethelper.cpp

  qgsmaptooladdfeature.cpp
  qgsmaptooladdpart.cpp
  qgsmaptooladdring.cpp
  qgsmaptoolfillring.cpp
  qgsmaptoolannotation.cpp
  qgsmaptooldeletering.cpp
  qgsmaptooldeletepart.cpp
  qgsmaptooltrimextendfeature.cpp
  qgsmaptoolfeatureaction.cpp
  qgsmaptoolformannotation.cpp
  qgsmaptoolhtmlannotation.cpp
  qgsmaptoolidentifyaction.cpp
  qgsmaptoolmeasureangle.cpp
  qgsmaptoolmeasurebearing.cpp
  qgsmaptoolmovefeature.cpp
  qgsmaptooloffsetcurve.cpp
  qgsmaptooloffsetpointsymbol.cpp
  qgsmaptoolpointsymbol.cpp
  qgsmaptoolreshape.cpp
  qgsmaptoolreverseline.cpp
  qgsmaptoolrotatefeature.cpp
  qgsmaptoolrotatepointsymbols.cpp
  qgsmaptoolscalefeature.cpp
  qgsmaptoolselect.cpp
  qgsmaptoolselectionhandler.cpp
  qgsmaptoolselectutils.cpp
  qgsmaptoolsimplify.cpp
  qgsmaptoolsplitfeatures.cpp
  qgsmaptoolsplitparts.cpp
  qgsmaptoolsvgannotation.cpp
  qgsmaptooltextannotation.cpp

  annotations/qgsannotationitempropertieswidget.cpp
  annotations/qgsannotationlayerproperties.cpp

  canvas/qgscanvasrefreshblocker.cpp

  decorations/qgsdecorationitem.cpp
  decorations/qgsdecorationtitle.cpp
  decorations/qgsdecorationtitledialog.cpp
  decorations/qgsdecorationcopyright.cpp
  decorations/qgsdecorationcopyrightdialog.cpp
  decorations/qgsdecorationimage.cpp
  decorations/qgsdecorationimagedialog.cpp
  decorations/qgsdecorationlayoutextent.cpp
  decorations/qgsdecorationlayoutextentdialog.cpp
  decorations/qgsdecorationnortharrow.cpp
  decorations/qgsdecorationnortharrowdialog.cpp
  decorations/qgsdecorationscalebar.cpp
  decorations/qgsdecorationscalebardialog.cpp
  decorations/qgsdecorationgrid.cpp
  decorations/qgsdecorationgriddialog.cpp

  pointcloud/qgspointcloudelevationpropertieswidget.cpp
  pointcloud/qgspointcloudlayerproperties.cpp
  pointcloud/qgspointcloudlayerstylewidget.cpp

  raster/qgsrasterelevationpropertieswidget.cpp

  vertextool/qgslockedfeature.cpp
  vertextool/qgsvertexeditor.cpp
  vertextool/qgsvertextool.cpp

  qgslayerstylingwidget.cpp
  qgsmeasuredialog.cpp
  qgsmeasuretool.cpp
  qgsmergeattributesdialog.cpp
  qgspointmarkeritem.cpp
  qgspointrotationitem.cpp
  qgspluginmetadata.cpp
  qgspluginregistry.cpp
  qgsprojectlayergroupdialog.cpp
  qgsprojectproperties.cpp
  qgsrastercalcdialog.cpp
  qgsrelationmanagerdialog.cpp
  qgsrelationadddlg.cpp
  qgsrelationaddpolymorphicdialog.cpp
  qgsselectbyformdialog.cpp
  qgsstatisticalsummarydockwidget.cpp
  qgstextannotationdialog.cpp
  qgssvgannotationdialog.cpp
  qgsundowidget.cpp
  qgsmapthemes.cpp
  qgshandlebadlayers.cpp

  browser/qgsinbuiltdataitemproviders.cpp

  devtools/qgsappdevtoolutils.cpp
  devtools/qgsdevtoolsmodelnode.cpp
  devtools/networklogger/qgsnetworklogger.cpp
  devtools/networklogger/qgsnetworkloggernode.cpp
  devtools/networklogger/qgsnetworkloggerpanelwidget.cpp
  devtools/networklogger/qgsnetworkloggerwidgetfactory.cpp
  devtools/profiler/qgsprofilerpanelwidget.cpp
  devtools/profiler/qgsprofilerwidgetfactory.cpp
  devtools/querylogger/qgsappquerylogger.cpp
  devtools/querylogger/qgsdatabasequeryloggernode.cpp
  devtools/querylogger/qgsqueryloggerpanelwidget.cpp
  devtools/querylogger/qgsqueryloggerwidgetfactory.cpp

  dwg/qgsdwgimportdialog.cpp
  dwg/qgsdwgimporter.cpp

  elevation/qgselevationprofileexportsettingswidget.cpp
  elevation/qgselevationprofileimageexportdialog.cpp
  elevation/qgselevationprofilelayertreemodel.cpp
  elevation/qgselevationprofilelayertreeview.cpp
  elevation/qgselevationprofilepdfexportdialog.cpp
  elevation/qgselevationprofiletoolidentify.cpp
  elevation/qgselevationprofiletoolmeasure.cpp
  elevation/qgselevationprofilewidget.cpp
  elevation/qgsmaptoolprofilecurve.cpp
  elevation/qgsmaptoolprofilecurvefromfeature.cpp

  georeferencer/qgsvalidateddoublespinbox.cpp

  labeling/qgslabelpropertydialog.cpp
  labeling/qgsmaptoolchangelabelproperties.cpp
  labeling/qgsmaptoolpinlabels.cpp
  labeling/qgsmaptoolshowhidelabels.cpp
  labeling/qgsmaptoollabel.cpp
  labeling/qgsmaptoolmovelabel.cpp
  labeling/qgsmaptoolrotatelabel.cpp

  layers/qgsapplayerhandling.cpp

  layout/qgslayoutappmenuprovider.cpp
  layout/qgslayoutdesignerdialog.cpp
  layout/qgslayoutimagedrophandler.cpp
  layout/qgslayoutmanagerdialog.cpp
  layout/qgsreportfieldgroupsectionwidget.cpp
  layout/qgsreportlayoutsectionwidget.cpp
  layout/qgsreportorganizerwidget.cpp
  layout/qgsreportsectionmodel.cpp
  layout/qgsreportsectionwidget.cpp

  locator/qgsactionlocatorfilter.cpp
  locator/qgsactivelayerfeatureslocatorfilter.cpp
  locator/qgslayermetadatalocatorfilter.cpp
  locator/qgsalllayersfeatureslocatorfilter.cpp
  locator/qgsbookmarklocatorfilter.cpp
  locator/qgsexpressioncalculatorlocatorfilter.cpp
  locator/qgsgotolocatorfilter.cpp
  locator/qgslayertreelocatorfilter.cpp
  locator/qgslayoutlocatorfilter.cpp
  locator/qgsnominatimlocatorfilter.cpp
  locator/qgssettingslocatorfilter.cpp

  locator/qgslocatoroptionswidget.cpp

  maptools/qgsappmaptools.cpp
  maptools/qgsmaptoolsdigitizingtechniquemanager.cpp

  maptools/qgsmaptoolshapecircleabstract.cpp
  maptools/qgsmaptoolshapecircularstringabstract.cpp
  maptools/qgsmaptoolshapeellipseabstract.cpp
  maptools/qgsmaptoolshaperectangleabstract.cpp
  maptools/qgsmaptoolshaperegularpolygonabstract.cpp

  maptools/qgsmaptoolshapecircle2points.cpp
  maptools/qgsmaptoolshapecircle2tangentspoint.cpp
  maptools/qgsmaptoolshapecircle3points.cpp
  maptools/qgsmaptoolshapecircle3tangents.cpp
  maptools/qgsmaptoolshapecirclecenterpoint.cpp
  maptools/qgsmaptoolshapecircularstringradius.cpp
  maptools/qgsmaptoolshapeellipsecenter2points.cpp
  maptools/qgsmaptoolshapeellipsecenterpoint.cpp
  maptools/qgsmaptoolshapeellipseextent.cpp
  maptools/qgsmaptoolshapeellipsefoci.cpp
  maptools/qgsmaptoolshaperectangle3points.cpp
  maptools/qgsmaptoolshaperectanglecenter.cpp
  maptools/qgsmaptoolshaperectangleextent.cpp
  maptools/qgsmaptoolshaperegularpolygon2points.cpp
  maptools/qgsmaptoolshaperegularpolygoncentercorner.cpp
  maptools/qgsmaptoolshaperegularpolygoncenterpoint.cpp

  options/qgsadvancedoptions.cpp
  options/qgscodeeditoroptions.cpp
  options/qgscustomprojectionoptions.cpp
  options/qgsfontoptions.cpp
  options/qgsgpsdeviceoptions.cpp
  options/qgsgpsoptions.cpp
  options/qgsoptions.cpp
  options/qgsoptionsutils.cpp
  options/qgsrasterrenderingoptions.cpp
  options/qgsrenderingoptions.cpp
  options/qgsvectorrenderingoptions.cpp

  gps/qgsgpsbearingitem.cpp
  gps/qgsgpsinformationwidget.cpp
  gps/qgsgpsmarker.cpp

  project/qgsprojectelevationsettingswidget.cpp

  pluginmanager/qgspluginmanager.cpp
  pluginmanager/qgspluginmanager_texts.cpp
  pluginmanager/qgsapppluginmanagerinterface.cpp
  pluginmanager/qgspluginsortfilterproxymodel.cpp
  pluginmanager/qgspluginitemdelegate.cpp

  qgssettingstree.cpp
  qgssettingsregistryapp.cpp
  qgsvariantdelegate.cpp
  qgscrashhandler.cpp

  mesh/qgsmeshcalculatordialog.cpp
  mesh/qgsnewmeshlayerdialog.cpp
  mesh/qgsmaptooleditmeshframe.cpp
  mesh/qgsmeshelevationpropertieswidget.cpp
  mesh/qgsmeshtransformcoordinatesdockwidget.cpp
  mesh/qgsmeshselectbyexpressiondialog.cpp

  vector/qgsvectorelevationpropertieswidget.cpp
)

#[[
杨小兵-2024-03-04
  如果设置了WITH_SPATIALITE这个宏（相当于是一个开关），那么将会修改QGIS_APP_SRCS变量中的内容，在这个例子中将会添加一个.cpp文件
]]
if (WITH_SPATIALITE)
  set(QGIS_APP_SRCS
    ${QGIS_APP_SRCS}
    qgsnewspatialitelayerdialog.cpp
  )
endif()

#[[
杨小兵-2024-03-04
一、解释  
  如果设置了HAVE_GEOREFERENCER这个宏（相当于是一个开关）那么将会修改QGIS_APP_SRCS变量中的内容，在这个例子中将会添加多个.cpp，这些.cpp文件
和当前cmakelists.txt文件处于相同的目录路径中。HAVE_GEOREFERENCER这个变量在顶层cmakelists.txt中进行了设置。如果定义了`HAVE_GEOREFERENCER`
标志，则会将一系列与地理参考器（Georeferencer）相关的`.cpp`文件添加到`QGIS_APP_SRCS`变量中。地理参考器通常用于将地理坐标系统（GCS）应用于无
坐标信息的图像或地图，从而使之能够在GIS软件中准确地定位。

二、功能描述
- `qgsgcpcanvasitem.cpp`: 可能管理地理参考器画布上的地面控制点（GCPs）的图形表示，这些点用于地图或图像的地理参考。
- `qgsgeorefconfigdialog.cpp`: 提供地理参考过程的配置对话框，可能允许用户设置变换参数、目标坐标系等。
- `qgsgeorefdatapoint.cpp`: 管理单个地理参考数据点的信息，包括它在原图像和目标坐标系中的位置。
- `qgsgeorefdelegates.cpp`: 可能实现自定义UI组件，用于在地理参考的上下文中显示和编辑数据。
- `qgsgeorefmainwindow.cpp`: 地理参考器的主窗口，提供用户界面和访问各种地理参考功能的入口。
- `qgsgeoreftooladdpoint.cpp`, `qgsgeoreftooldeletepoint.cpp`, `qgsgeoreftoolmovepoint.cpp`: 这些文件分别实现了添加、删除和移动地理参考点的工具。
- `qgsgeorefvalidators.cpp`: 提供验证功能，确保用户输入的地理参考数据的有效性。
- `qgsmapcoordsdialog.cpp`: 可能提供一个对话框，允许用户输入或修改地图坐标。
- `qgsresidualplotitem.cpp`: 管理残差图的绘制，残差图用于展示地理参考过程中点的预测位置与实际位置之间的差异。
- `qgstransformsettingsdialog.cpp`: 提供变换设置的对话框，可能让用户选择应用于地理参考过程的变换方法。
- `qgsgcplist.cpp`, `qgsgcplistmodel.cpp`: 管理地面控制点列表及其数据模型，用于地理参考器界面的数据显示和交互。
- `qgsimagewarper.cpp`: 实现图像变形逻辑，将地理参考的坐标变换应用到图像上，以进行校正。
- `qgsgeoreftransform.cpp`: 封装地理参考变换的计算逻辑，包括计算变换矩阵和应用变换。
- `qgsgcplistwidget.cpp`: 提供一个用户界面组件，显示和管理地面控制点的列表。
- `qgsrasterchangecoords.cpp`: 可能实现对栅格数据坐标的修改逻辑，作为地理参考过程的一部分。

]]
if (HAVE_GEOREFERENCER)
  set(QGIS_APP_SRCS
    ${QGIS_APP_SRCS}
    georeferencer/qgsgcpcanvasitem.cpp
    georeferencer/qgsgeorefconfigdialog.cpp
    georeferencer/qgsgeorefdatapoint.cpp
    georeferencer/qgsgeorefdelegates.cpp
    georeferencer/qgsgeorefmainwindow.cpp
    georeferencer/qgsgeoreftooladdpoint.cpp
    georeferencer/qgsgeoreftooldeletepoint.cpp
    georeferencer/qgsgeoreftoolmovepoint.cpp
    georeferencer/qgsgeorefvalidators.cpp
    georeferencer/qgsmapcoordsdialog.cpp
    georeferencer/qgsresidualplotitem.cpp
    georeferencer/qgstransformsettingsdialog.cpp
    georeferencer/qgsgcplist.cpp
    georeferencer/qgsgcplistmodel.cpp
    georeferencer/qgsimagewarper.cpp
    georeferencer/qgsgeoreftransform.cpp
    georeferencer/qgsgcplistwidget.cpp
    georeferencer/qgsrasterchangecoords.cpp
  )
endif()

#[[
杨小兵-2024-03-04
一、解释  
  如果设置了WITH_3D这个宏（相当于是一个开关）那么将会修改QGIS_APP_SRCS变量中的内容，在这个例子中将会添加多个.cpp。
]]
if (WITH_3D)
  set(QGIS_APP_SRCS
    ${QGIS_APP_SRCS}
    3d/qgs3danimationwidget.cpp
    3d/qgs3danimationexportdialog.cpp
    3d/qgs3dapputils.cpp
    3d/qgs3dicongenerator.cpp
    3d/qgs3dmapcanvas.cpp
    3d/qgs3dmapconfigwidget.cpp
    3d/qgs3dmaptool.cpp
    3d/qgs3dmaptoolidentify.cpp
    3d/qgs3dmaptoolmeasureline.cpp
    3d/qgs3dmeasuredialog.cpp
    3d/qgs3dmodelsourcelineedit.cpp
    3d/qgs3dnavigationwidget.cpp
    3d/qgs3doptions.cpp
    3d/qgsgoochmaterialwidget.cpp
    3d/qgslightswidget.cpp
    3d/qgsline3dsymbolwidget.cpp
    3d/qgsmaterialwidget.cpp
    3d/qgsmesh3dsymbolwidget.cpp
    3d/qgsnullmaterialwidget.cpp
    3d/qgspoint3dsymbolwidget.cpp
    3d/qgspolygon3dsymbolwidget.cpp
    3d/qgsphongmaterialwidget.cpp
    3d/qgsphongtexturedmaterialwidget.cpp
    3d/qgsrulebased3drendererwidget.cpp
    3d/qgssimplelinematerialwidget.cpp
    3d/qgssymbol3dwidget.cpp
    3d/qgsvectorlayer3dpropertieswidget.cpp
    3d/qgsvectorlayer3drendererwidget.cpp
    3d/qgsmeshlayer3drendererwidget.cpp
    3d/qgsmap3dexportwidget.cpp
    layout/qgslayout3dmapwidget.cpp
    3d/qgsskyboxrenderingsettingswidget.cpp
    3d/qgsshadowrenderingsettingswidget.cpp
    3d/qgsambientocclusionsettingswidget.cpp
    3d/qgspointcloud3dsymbolwidget.cpp
    3d/qgspointcloudlayer3drendererwidget.cpp
    3d/qgs3dmapcanvaswidget.cpp
    3d/qgs3dviewsmanagerdialog.cpp
  )
endif()

#[[
杨小兵-2024-03-04
一、解释  
  `find_package`命令在CMake中是用来定位外部项目或库的，并加载它们的设置到当前的CMake项目中。这个命令尝试找到指定的包及其提供的库、头文件、
工具等，并设置一些变量来反映找到的包的详细信息或状态。`find_package`命令试图找到一个软件包的库和头文件。它首先查找包的CMake配置文件（通常
是`<package_name>Config.cmake`或`<package-name>-config.cmake`）。如果找到，CMake会读取这个文件来了解如何使用该包。如果未找到配置文件，
CMake会回退到模块模式，尝试使用`Find<package>.cmake`模块来定位和设置包的相关变量。
  这条命令告诉CMake去查找与`${QT_VERSION_BASE}`变量对应版本的Qt，并且特别需要`UiTools`组件。`REQUIRED`指明如果找不到指定的包或组件，CMake
应该报错并终止配置过程。

CMake执行`find_package`时的查找过程大致如下：
1. **配置文件模式**：CMake首先尝试查找包的CMake配置文件。这些文件是由包的开发者提供（原来这部分内容是通过包的开发者来提供的），位于包安装目录
的某个位置。CMake使用`<package_name>Config.cmake`或`<package-name>-config.cmake`模式来查找这些文件。

2. **模块模式**：如果配置文件未找到，CMake将使用`Find<package>.cmake`模块来查找包。这些模块文件通常与CMake一起提供，并包
含了如何查找特定包（包括头文件和库文件的路径）的逻辑。

CMake查找配置文件或模块的路径包括但不限于：
- 通过`CMAKE_PREFIX_PATH`、`CMAKE_FRAMEWORK_PATH`等环境变量指定的目录。
- 包的默认安装路径，如`/usr/local`或`C:/Program Files`等。
- 如果是跨平台项目，还会查找特定于平台的默认路径。

二、总结
1、定位外部项目、外部库（find_package尝试找到指定的包及其提供的库、头文件、工具）
2、find_package找到两个东西：库、这个库配套的头文件
3、查找这个包的CMake配置文件 或者 通过`Find<package>.cmake`模块来定位和设置包的相关变量
4、
]]
find_package(${QT_VERSION_BASE} COMPONENTS UiTools REQUIRED)

set (WITH_QWTPOLAR FALSE CACHE BOOL "Determines whether QwtPolar should be built")

#[[
杨小兵-2024-03-04
一、解释  
1. **`add_definitions(-DWITH_QWTPOLAR)`**:
  - 这一行向项目添加了一个预处理器定义`WITH_QWTPOLAR`。这意味着在编译项目时，编译器会定义一个名为`WITH_QWTPOLAR`的宏。这通常用于条件编译，
例如，在源代码中可以使用`#ifdef WITH_QWTPOLAR`来检查这个宏是否被定义，从而决定是否包含与QwtPolar相关的代码部分。

2. **`find_package(QwtPolar)`**:
  - 这条命令尝试在系统上找到QwtPolar库。`find_package`是CMake用来查找并加载外部库（如QwtPolar）的设置的命令。如果CMake找到了QwtPolar，
它会设置一些变量，用于后续配置过程中引用QwtPolar的头文件和库文件。

二、“如果CMake找到了QwtPolar，它会设置一些变量”将会设置哪些变量？
  当`find_package(QwtPolar)`命令在CMake中执行时，它会根据查找到的包来设置一系列变量。这些变量可以是固定的，也可以是由外部库的开发者通过配
置文件定义的。通常，对于大多数通过`find_package`找到的库，以下是一些常见的变量：
1. **`<PackageName>_FOUND`** 或 **`<PackageName>_FOUND`**:
  - 对于`QwtPolar`，变量会是`QWTPOLAR_FOUND`。如果`QwtPolar`被成功找到，这个变量会被设置为`TRUE`；否则，它会被设置为`FALSE`。这允许CMake
脚本基于库是否可用来进行条件编译或配置。

2. **`<PackageName>_INCLUDE_DIRS`** 或 **`<PackageName>_INCLUDES`**:
  - 这个变量包含了库的头文件目录。这些信息用于设置编译器的包含目录，以便项目可以正确地包含和使用库的头文件。对于`QwtPolar`，这个变量可能是
`QWTPOLAR_INCLUDE_DIRS`。

3. **`<PackageName>_LIBRARIES`** 或 **`<PackageName>_LIBS`**:
  - 包含了库的链接指令，通常是库文件的完整路径。这是链接器需要的信息，以便在构建项目时链接到这个库。对于`QwtPolar`，这个变量可能是
`QWTPOLAR_LIBRARIES`。

4. **`<PackageName>_VERSION`**:
   - 包含了找到的库的版本号。这可以用来检查库的特定版本是否满足项目的需求。
这些变量是否被设置，以及它们具体的命名，可能依赖于库的CMake配置文件（`<PackageName>Config.cmake`或`<package-name>-config.cmake`）是如何
编写的。库的开发者可以通过这些配置文件提供详细的导出设置，包括自定义的变量名和额外的配置选项。

]]
if (WITH_QWTPOLAR)
  add_definitions(-DWITH_QWTPOLAR)
  # Try to find QwtPolar on the system
  find_package(QwtPolar)
  # If not found on the system, offer the possibility to build QwtPolar
  # internally
  if(NOT FOUND_QwtPolar)
    set(DEFAULT_WITH_INTERNAL_QWTPOLAR TRUE)
  else()
    set(DEFAULT_WITH_INTERNAL_QWTPOLAR FALSE)
  endif()
  set (WITH_INTERNAL_QWTPOLAR DEFAULT_WITH_INTERNAL_QWTPOLAR CACHE BOOL "Use internal build of QwtPolar")

  if(WITH_INTERNAL_QWTPOLAR)
    set(QGIS_APP_SRCS
      ${QGIS_APP_SRCS}
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_canvas.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_curve.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_fitter.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_grid.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_item.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_itemdict.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_layout.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_magnifier.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_marker.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_panner.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_picker.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_plot.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_renderer.cpp
      ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1/qwt_polar_spectrogram.cpp
    )

    set(QWTPOLAR_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/qwtpolar-1.1.1)

    set(QWTPOLAR_LIBRARY "")
  endif()
else()
  set(QWTPOLAR_LIBRARY "")
  set(QWTPOLAR_INCLUDE_DIR "")
endif()


#[[
杨小兵-2024-03-04
一、解释  
  如果设置了POSTGRES_FOUND这个宏（相当于是一个开关）并且设置了HAVE_PGCONFIG宏，那么将会添加一个预处理器定义HAVE_PGCONFIG并且将其值设置为1
]]
if (POSTGRES_FOUND)
  if(HAVE_PGCONFIG)
    add_definitions(-DHAVE_PGCONFIG=1)
  endif()
endif()

# Test data dir for QgsAppScreenShots
add_definitions(-DTEST_DATA_DIR="${TEST_DATA_DIR}")

# add test resources, e.g. standard test font
set(TEST_RCCS ../../tests/testdata/testdata.qrc)




#[[
杨小兵-2024-03-04
一、解释  
  如果设置了WIN32这个宏（相当于是一个开关）将会向其中添加一些文件（main.cpp和测试使用的测试资源）这段CMake脚本是针对QGIS项目的一部分，它通过
条件编译指令针对不同的操作系统和编译环境（不同的操作环境、不同的编译环境）设置不同的编译源文件和编译选项。

### 对于Windows平台 (`WIN32`)
1. **添加源文件到`QGIS_APP_SRCS`变量**:
  - `set (QGIS_APP_SRCS ${QGIS_APP_SRCS} main.cpp ${TEST_RCCS})`
  - 这行代码将`main.cpp`和`TEST_RCCS`变量中包含的所有资源文件添加到`QGIS_APP_SRCS`变量中。这意味着这些文件将被编译并链接到最终的应用程序中。
`TEST_RCCS`可能包含了QGIS项目的资源文件。
2. **设置`QGIS_APPMAIN_SRCS`变量**:
  - `set (QGIS_APPMAIN_SRCS mainwin.cpp)`
  - 这行代码设置`QGIS_APPMAIN_SRCS`变量为`mainwin.cpp`，意味着`mainwin.cpp`是Windows平台下主要的源文件之一。
3. **对于非MSVC编译器的特殊处理**:
  - 如果使用的不是Microsoft Visual C++（MSVC）编译器，那么会执行以下几步：
    - `enable_language(RC)`: 启用RC（资源编译器）语言的支持，这对于编译Windows资源文件是必要的。
    - 设置资源编译器和编译指令，以确保资源文件可以正确编译成COFF（Common Object File Format）格式。
    - `set(QGIS_RC_LIBS -mwindows)`: 添加`-mwindows`链接器选项，这通常用于告诉编译器生成的是一个GUI应用程序，而不是控制台应用程序。
]]
if (WIN32)
  set (QGIS_APP_SRCS ${QGIS_APP_SRCS} main.cpp ${TEST_RCCS})
  set (QGIS_APPMAIN_SRCS mainwin.cpp)
  if (NOT MSVC)
    enable_language(RC)
    set(CMAKE_RC_COMPILER_INIT ${CMAKE_GENERATOR_RC})
    set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -i <SOURCE> -o <OBJECT>")
    set(QGIS_RC_LIBS -mwindows)
  endif()
  set (QGIS_APPMAIN_SRCS ${QGIS_APPMAIN_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/qgis_win32.rc)
else()
  set (QGIS_APPMAIN_SRCS main.cpp ${TEST_RCCS})
endif()



#[[
杨小兵-2024-03-04
一、解释  
  如果设置了PEDANTIC这个宏（相当于是一个开关）将会执行代码块中的代码，PEDANTIC这个是关于编译器flag的一些设置，现在可以不用关心
]]
if(PEDANTIC)
  # disable deprecation warnings
  if(MSVC)
    set_source_files_properties(
        qgisappinterface.cpp
        ${CMAKE_BINARY_DIR}/src/app/moc_qgisappinterface.cxx
        PROPERTIES COMPILE_FLAGS "/wd4996"
    )
  else()
    set_source_files_properties(
        qgisappinterface.cpp
        ${CMAKE_BINARY_DIR}/src/app/moc_qgisappinterface.cxx
        PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations"
    )
  endif()
endif()

if(MSVC)
set_source_files_properties(
  qgisapp.cpp
  main.cpp
  qgscrashhandler.cpp
  PROPERTIES COMPILE_FLAGS -wd4091)
endif()

#[[
杨小兵-2024-03-04
一、解释  
1. **`include_directories`解释**:
  `include_directories`命令在CMake中用于向项目添加头文件的搜索路径。这意味着它用于指定头文件（.h或.hpp文件，而非.cpp文件）的目录，
以便编译器在编译项目时可以找到这些头文件。当你在源文件中使用`#include`指令包含一个头文件时，编译器会在这些指定的目录中搜索指定的头文
件。因此，`include_directories`包含的是头文件目录。

2. **`SYSTEM`关键字解释**:
  `SYSTEM`是一个可选关键字，可以与`include_directories`命令一起使用。使用`SYSTEM`标记的目录告诉编译器，这些目录中的头文件是系统头文
件或者第三方库的头文件。这有几个效果：
  - **忽略警告**：最主要的用途是，对于标记为`SYSTEM`的目录中的头文件，编译器通常会忽略在这些文件中发现的警告。这对于第三方库特别有用，
  因为你可能不希望或不能修改这些库文件以修正警告。
  - **搜索优先级**：某些编译器可能还会改变包含搜索顺序，优先搜索标记为`SYSTEM`的目录。


二、总结
1、include_directories是用来包含头文件的搜索目录路径的
2、在源文件中使用`#include`指令包含一个头文件时，编译器会在这些指定的目录中搜索指定的头文件
3、SYSTEM的作用：忽略头文件中出现的警告、更改搜索优先级
]]
if(HAVE_OPENCL)
    include_directories(SYSTEM ${OpenCL_INCLUDE_DIRS})
endif()

if (WITH_PDAL)
  include_directories(SYSTEM ${PDAL_INCLUDE_DIR})
  if(MSVC)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
  endif()
endif()

if(ENABLE_MODELTEST)
  include_directories(../../tests/qt_modeltest)
endif()

if (ANDROID)
  include_directories(SYSTEM ${ANDROID_NDK_TOOLCHAIN_ROOT}/sysroot/usr/include)
endif()

if (APPLE)
  include_directories(${CMAKE_SOURCE_DIR}/src/native/mac)
endif()

if (POSTGRES_FOUND)
  include_directories(SYSTEM ${POSTGRES_INCLUDE_DIR})
endif()





#############
#[[
杨小兵-2024-03-04
一、解释  
  如果设置了ANDROID这个宏（相当于是一个开关）将会执行代码块中的代码，ANDROID是Android中的一些内容，现在可以不用关系其代码块中的内容，如果
不是android平台下，那么将会配置一个生成可执行文件的项目文件。（下面的内容将会生成两个项目文件：名为${QGIS_APP_NAME}、名为qgis_app两个项目文件）

### `MACOSX_BUNDLE`
  - `MACOSX_BUNDLE`是一个特殊的参数，它告诉CMake这个可执行文件是一个Mac OS X的应用程序包。这意味着CMake将生成一个`.app`包，而不是一个普通的可
执行文件。这个参数只在Mac OS X平台上有效，并且会让CMake配置适合Mac OS X应用程序包的特殊设置。

### `WIN32`
  - `WIN32`参数用于指示这个可执行文件是一个Windows应用程序。当这个参数被使用时，如果目标平台是Windows，CMake将生成一个没有控制台窗口的GUI应用程序。
这通常用于Windows的图形界面应用程序，以避免在启动时打开一个不需要的命令行窗口。

]]
if (ANDROID)
  add_library(${QGIS_APP_NAME} ${LIBRARY_TYPE} ${QGIS_APPMAIN_SRCS})

  # require c++17
  target_compile_features(${QGIS_APP_NAME} PRIVATE cxx_std_17)
else()
  add_executable(${QGIS_APP_NAME} MACOSX_BUNDLE WIN32 ${QGIS_APPMAIN_SRCS})

  # require c++17
  target_compile_features(${QGIS_APP_NAME} PRIVATE cxx_std_17)
endif()

# Putting IMAGE_RCC_SRCS into qgis_app lib is causing problems when the lib is
# loaded (by plugin for example) in test unit (qgis_layoutpicturetest).
add_library(qgis_app ${LIBRARY_TYPE} ${QGIS_APP_SRCS})
# require c++17
target_compile_features(qgis_app PRIVATE cxx_std_17)





#[[
杨小兵-2024-03-04
一、解释  
### 1. `target_include_directories`命令的作用
  `target_include_directories`命令在CMake中用于为特定的目标（如可执行文件或库）指定包含（头文件）目录。这些指定的目录将在编译该目标的文件
时被编译器用来搜索头文件。这对于确保编译器可以找到项目中使用的所有外部或内部库的头文件是非常重要的。

- `<INTERFACE|PUBLIC|PRIVATE>`: 这些关键字用于指定包含目录的范围和继承性。
  - `PRIVATE`: 这些包含目录仅用于编译指定的目标，对于链接该目标的其他目标不可见。
  - `INTERFACE`: 这些包含目录不用于编译目标本身，但对于链接该目标的其他目标是可见的。
  - `PUBLIC`: 这些包含目录既用于编译目标本身，也对链接该目标的其他目标可见。

  理解`PRIVATE`、`INTERFACE`、和`PUBLIC`关键词在`target_include_directories`命令中的作用对于管理CMake项目中的依赖关系非常重要。
这些关键词定义了目标的包含目录如何影响目标本身以及依赖于这个目标的其他目标（1、目标本身的头文件包含目录；2、依赖本目标的其他目标）。

### `PRIVATE`
  当你使用`PRIVATE`关键词指定包含目录时，这些目录仅用于编译定义了这些包含目录的目标本身。如果其他目标链接到了这个目标，那么这些`PRIVATE`
包含目录对于链接目标来说是不可见的。
- **使用场景**：当你的源文件中使用了某个头文件，而这个头文件不需要被暴露给其他链接了此目标的目标时，应该使用`PRIVATE`。例如，如果你在实现
文件（`.cpp`）中使用了第三方库的头文件，而这个第三方库不需要被此库的使用者知道，你就应该将这个第三方库的包含目录设置为`PRIVATE`。

### `INTERFACE`
  `INTERFACE`关键词的作用正好相反：指定的包含目录不用于编译目标本身，但是会被传递给链接了这个目标的其他目标。这意味着这些包含目录对于定义它
们的目标来说是不可见的，只对外部使用这个目标的目标可见。
- **使用场景**：这在你创建一个只提供接口（如头文件库）的库时非常有用，库本身不需要编译任何源文件，但使用这个库的目标需要知道接口的位置。例如，
如果你的库是一个头文件集合，你可以使用`INTERFACE`将这些头文件的路径传递给需要它们的目标。

### `PUBLIC`
`PUBLIC`关键词结合了`PRIVATE`和`INTERFACE`的行为：指定的包含目录既用于编译目标本身，也会被传递给链接了这个目标的其他目标。
- **使用场景**：当你的库提供了一些头文件，这些头文件既被库内部使用，也需要被库的使用者（其他链接了这个库的目标）包含时，应该使用`PUBLIC`。
这确保了任何链接了这个库的目标都能自动获得正确的包含目录路径。


二、总结
1、当编译器编译目标文件的时候遇到包含的头文件时，将会从target_include_directories指定的目录中搜索需要的头文件
2、

]]
target_include_directories(qgis_app PUBLIC
  ${CMAKE_SOURCE_DIR}/external/nmea

  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/src/app/decorations
  ${CMAKE_SOURCE_DIR}/src/app/devtools/networklogger
  ${CMAKE_SOURCE_DIR}/src/app/labeling
  ${CMAKE_SOURCE_DIR}/src/app/layout
  ${CMAKE_SOURCE_DIR}/src/app/pluginmanager
  ${CMAKE_SOURCE_DIR}/src/app/gps
  ${CMAKE_SOURCE_DIR}/src/app/dwg
  ${CMAKE_SOURCE_DIR}/src/app/maptools
  ${CMAKE_SOURCE_DIR}/src/app/mesh
  ${CMAKE_SOURCE_DIR}/src/app/locator
  ${CMAKE_SOURCE_DIR}/src/app/pointcloud
  ${CMAKE_SOURCE_DIR}/src/app/vectortile
  ${CMAKE_SOURCE_DIR}/src/plugins
  ${CMAKE_SOURCE_DIR}/src/python
  ${CMAKE_SOURCE_DIR}/src/native

  ${CMAKE_BINARY_DIR}/src/app
)

if (WITH_3D)
  target_include_directories(qgis_app PUBLIC
    ${CMAKE_SOURCE_DIR}/src/app/3d
  )
endif()

target_include_directories(qgis_app SYSTEM PUBLIC
  ${QT_QTUITOOLS_INCLUDE_DIR}
  ${QWTPOLAR_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/external/qt-unix-signals
)


#[[
杨小兵-2024-03-04
一、解释  
### 1. `add_dependencies`命令的作用
  `add_dependencies`命令在CMake中用于为指定的目标（如可执行文件或库）添加构建依赖。这意味着在构建指定的目标之前，必须先构建这些依赖。
该命令确保在构建过程中，依赖项按照正确的顺序被构建。这意味着，在开始构建`qgis_gui`目标之前，`ui`目标必须已经被构建。这常用于确保自动
生成的代码或需要预处理的步骤完成后再进行其他构建任务。

### 2. 为什么使用库的名字而不是`.lib`文件名（使用库的目标名而不是文件名，可以让CMake脚本更加简洁、清晰，并且具有更好的可维护性和可移植性。）
在CMake中，通常使用库的逻辑目标名而不是具体的文件名来指定链接库。这是因为：
  - **跨平台兼容**：使用目标名而不是具体的库文件名（如`qgis_core.lib`）可以使CMake脚本跨平台工作。CMake负责解析这些目标名到实际的库文
件路径，无论这个库是静态链接的`.lib`/`.a`文件还是动态链接的`.dll`/`.so`文件。
  - **抽象化**：这种方式提供了更高级别的抽象，允许CMake处理与特定构建系统和平台相关的复杂性，如选择正确的库版本，处理导入库与静态/动态库
之间的差异等。

]]
add_dependencies(qgis_gui ui)

target_link_libraries(qgis_app
  ${QWT_LIBRARY}
  ${QWTPOLAR_LIBRARY}
  ${QT_VERSION_BASE}::Sql
  ${QT_VERSION_BASE}::UiTools
  ${OPTIONAL_QTWEBKIT}
  #should only be needed for win
  ${QT_QTMAIN_LIBRARY}
  ${QGIS_RC_LIBS}
  qgis_core
  qgis_gui
  qgis_analysis
  qgis_native
  libdxfrw
)

target_compile_definitions(qgis_app PRIVATE "-DQT_NO_FOREACH")

if (WITH_BINDINGS)
  add_definitions(-DWITH_BINDINGS)
  target_link_libraries(qgis_app qgispython)
endif()

if(ENABLE_MODELTEST)
  target_link_libraries(qgis_app ${QT_VERSION_BASE}::Test)
endif()

if (WITH_3D)
  target_link_libraries(qgis_app qgis_3d)
endif()

if (HAVE_GEOREFERENCER)
  include_directories(SYSTEM
    ${GSL_INCLUDE_DIR}
  )
  target_link_libraries(qgis_app
    ${GSL_LIBRARIES}
  )
endif()





#[[
杨小兵-2024-03-04
一、解释  
  `GENERATE_EXPORT_HEADER`是CMake的一个命令，用于为库生成一个导出头文件。这个导出头文件定义了用于导出和导入符号的宏，
这在创建动态链接库（DLL）时特别重要。当你在Windows上构建DLL时，必须显式地标记哪些符号（函数、类等）是要被导出的，这样
其他使用这个DLL的应用或库才能链接和使用这些符号。相应的，使用这个DLL的应用需要知道这些符号是从DLL导入的。`GENERATE_EXPORT_HEADER`
命令自动为你处理这些宏的定义。

### 作用
使用`GENERATE_EXPORT_HEADER`命令生成的头文件提供了如下宏：
- 一个用于导出符号的宏，当编译库本身时使用。
- 一个用于导入符号的宏，当从其他地方引用库时使用。
- 可能还包括一个用于隐藏符号的宏。
  这些宏简化了跨平台DLL的创建和使用，尤其是在Windows平台上，因为它自动处理了`__declspec(dllexport)`和`__declspec(dllimport)`声明的复杂性。

### 没有这段内容的影响
  如果没有使用`GENERATE_EXPORT_HEADER`来生成导出头文件，你需要手动定义导出和导入符号的宏，并在公共接口的符号上正确使用这些宏。对于大型项目
或跨平台项目，这会增加维护的复杂性，并且容易出错。
- **对跨平台的影响**：没有自动生成的导出头文件，开发者需要为不同平台手动编写和维护导出逻辑，这可能导致跨平台兼容性问题。
- **对可维护性的影响**：随着项目的增长，手动管理导出符号变得越来越困难，尤其是当符号频繁添加或更改时。
  因此，虽然可以没有`GENERATE_EXPORT_HEADER`生成的导出头文件，但使用它可以大大简化和自动化跨平台库的构建过程，减少错误，并提高项目的可维护性。
]]
GENERATE_EXPORT_HEADER(
   qgis_app
   BASE_NAME APP
   EXPORT_FILE_NAME qgis_app.h
)


if (NOT ANDROID)
  set_target_properties(qgis_app PROPERTIES
    VERSION ${COMPLETE_VERSION}
    SOVERSION ${COMPLETE_VERSION}
    )
endif()

if (APPLE)
  target_link_libraries(qgis_app ${APP_SERVICES_LIBRARY})
endif()


#[[
杨小兵-2024-03-04
### 条件判断 `if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)`
- `MSVC`: 这个变量在CMake中用来检测是否正在使用Microsoft Visual C++编译器。如果当前的编译器是MSVC，这个条件为真。
- `CMAKE_SIZEOF_VOID_P EQUAL 8`: 这个条件检查指针的大小（以字节为单位）是否等于8，这是在64位系统上的典型值。`CMAKE_SIZEOF_VOID_P`
是CMake在配置时确定的，表示指针类型的大小。64位系统的指针大小通常为8字节，而32位系统的指针大小为4字节。
  组合这两个条件，`if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)`的意思是“如果是在使用MSVC编译器并且目标平台是64位的”。

]]
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  set_target_properties(qgis_app PROPERTIES STATIC_LIBRARY_FLAGS "/machine:x64")
endif()

#[[
杨小兵-2024-03-04
一、解释
  这段CMake代码是用于集成`clang-tidy`工具到CMake构建过程中，以便对指定目标（在这个例子中是`qgis_app`）的代码进行静态分析。
`clang-tidy`是一个基于Clang的C++静态分析工具，可以帮助开发者发现代码中的问题，遵循特定的编码标准和风格规范，以及一些潜在的错误或不一致之处。

二、总结
  如果`CLANG_TIDY_EXE`变量被成功设置（即已经找到了`clang-tidy`的可执行文件），那么这段代码将配置`qgis_app`目标，使得在编译过程中自动运行
`clang-tidy`对其进行静态分析。这意味着每次编译`qgis_app`时，`clang-tidy`都会根据`"${DO_CLANG_TIDY}"`变量中指定的参数来检查代码。
]]
# clang-tidy
if(CLANG_TIDY_EXE)
  set_target_properties(
    qgis_app PROPERTIES
    CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
  )
endif()

if(NOT WIN32)
  target_link_libraries(${QGIS_APP_NAME} qgis_app)
endif()

if (ANDROID)
  target_link_libraries(${QGIS_APP_NAME} log)
endif()

if(WIN32)
  add_definitions(-DQWT_DLL)
  add_definitions(-DQSCINTILLA_DLL)
  target_link_libraries(${QGIS_APP_NAME} dbghelp)
  target_link_libraries(qgis_app dbghelp)
endif()

target_link_libraries(${QGIS_APP_NAME} qgis_native)

if (NOT WIN32)
target_link_libraries(${QGIS_APP_NAME} QTSignal)
endif()

if (APPLE)
  set_target_properties(${QGIS_APP_NAME} PROPERTIES
    INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${QGIS_LIB_DIR}
    INSTALL_RPATH_USE_LINK_PATH true
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/mac/app.info.plist.in")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  find_library(EXECINFO_LIBRARY NAMES execinfo)
  target_link_libraries(${QGIS_APP_NAME} ${EXECINFO_LIBRARY})
endif()

if (POSTGRES_FOUND)
  target_link_libraries (qgis_app ${POSTGRES_LIBRARY})
endif()

if (WITH_PDAL)
  target_link_libraries(qgis_app ${PDAL_LIBRARIES})
endif()

#[[
杨小兵-2024-03-04
一、解释
  这段CMake代码是用于在安装过程中，只有在使用Microsoft Visual C++编译器（即MSVC）构建项目时，将特定的图标文件安装到指定的安装目录下。

  ### `install`命令
- `install(FILES ... DESTINATION ...)`: 这个命令用于在安装时将文件从构建目录复制到目标系统的安装目录。这是CMake提供的一种机制，用于
指定安装过程中文件应该被放置的位置。

- `FILES qgis.ico qgis-mime.ico qgis-qgs.ico qgis-qlr.ico qgis-qml.ico qgis-qpt.ico`: 指定了一系列图标文件，这些文件在安装过程
中将被复制。这些文件通常代表应用程序的图标、与应用程序关联的文件类型的图标等。

- `DESTINATION ${CMAKE_INSTALL_PREFIX}/icons`: 指定了目标安装目录。`${CMAKE_INSTALL_PREFIX}`是CMake中的一个变量，表示安装的根目录
（默认情况下，这可能是`/usr/local`在Linux上，或者某个路径在Windows上，但可以通过CMake配置进行修改）。这里的目标安装目录是该根目录下的`icons`子目录。因此，所有列出的图标文件将被安装到这个目录中。

二、总结
]]
if(MSVC)
  install(FILES starmap.ico qgis.ico qgis-mime.ico qgis-qgs.ico qgis-qlr.ico qgis-qml.ico qgis-qpt.ico DESTINATION ${CMAKE_INSTALL_PREFIX}/icons)
endif()

install(TARGETS qgis_app
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
  LIBRARY DESTINATION ${QGIS_LIB_DIR}
  ARCHIVE DESTINATION ${QGIS_LIB_DIR}
  FRAMEWORK DESTINATION ${QGIS_FW_SUBDIR}
  PUBLIC_HEADER DESTINATION ${QGIS_INCLUDE_DIR})

install(TARGETS ${QGIS_APP_NAME}
  BUNDLE DESTINATION ${QGIS_INSTALL_PREFIX}
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
  LIBRARY DESTINATION ${QGIS_LIB_DIR}
  ARCHIVE DESTINATION ${QGIS_LIB_DIR}
  FRAMEWORK DESTINATION ${QGIS_FW_SUBDIR}
  PUBLIC_HEADER DESTINATION ${QGIS_INCLUDE_DIR})

if (APPLE)
  install(CODE "execute_process(COMMAND /bin/echo -n \"APPLQGIS\" OUTPUT_FILE \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/../PkgInfo\")")
endif()
