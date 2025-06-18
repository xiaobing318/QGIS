############################################################
# sources
#[[
杨小兵-2024-03-04
  这段代码是在CMake配置文件中使用的，它使用Bison和Flex工具自动生成解析器(parser)和词法分析器(lexer)的代码（1、使用Bison自动生成解析器代码；
2、使用Flex工具自动生成分析器的代码）。

### 应用场景
  在编程中，尤其是在处理编译器、解释器或复杂的文本分析任务时，经常需要将输入文本（如代码或查询语句）转换为可操作的内部表示形式。这个转换过程
通常涉及两个主要步骤：词法分析和语法分析。词法分析器(lexer)负责将输入文本分解成一系列标记(tokens)，而语法分析器(parser)则根据这些标记构建抽
象语法树（AST），这是进一步处理的基础。

### 数学角度
  从数学的角度来看，这个过程可以被视为一种形式语言的解析，其中词法分析相当于识别语言的字母表中的符号，而语法分析则是根据这些符号的组合规则
（即语法）构建表达式或句子。这个过程类似于使用形式文法（如上下文无关文法）描述和生成语言的元素。

### 计算机实现
#### Bison和Flex的使用
- **Bison** 是一个用于生成语法分析器的工具，它读取包含语法规则的描述文件（以`.yy`或`.y`为扩展名），并生成一个解析输入文本的C或C++代码。
- **Flex** 是一个生成词法分析器的工具，它读取包含词法规则的描述文件（以`.ll`或`.l`为扩展名），并生成用于识别标记的C或C++代码。

#### CMake指令详解
- **BISON_TARGET**: 定义了一个Bison目标，用于生成语法分析器的源代码。例如，`QgsExpressionParser`是目标名，`qgsexpressionparser.yy`
是Bison的输入文件，生成的源代码将是`qgsexpressionparser.cpp`。
- **FLEX_TARGET**: 定义了一个Flex目标，用于生成词法分析器的源代码。如`QgsExpressionLexer`是目标名，`qgsexpressionlexer.ll`是Flex的
输入文件，生成的源代码将是`qgsexpressionlexer.cpp`。
- **ADD_FLEX_BISON_DEPENDENCY**: 建立Flex和Bison生成的目标之间的依赖关系，确保在构建过程中词法分析器和语法分析器的源代码能够以正确的顺序生成。

  这些指令在CMake配置中用于自动化地生成处理不同种类的输入文本所需的解析器和词法分析器代码。例如，`QgsExpressionParser`和`QgsExpressionLexer`
用于解析和词法分析表达式，而`QgsSqlStatementParser`和`QgsSqlStatementLexer`用于SQL语句，`QgsMeshCalcParser`和`QgsMeshCalcLexer`则可能用于
某种特定于应用程序的网格计算语言。通过指定不同的编译标志（`COMPILE_FLAGS`），可以为每个解析器和词法分析器定制前缀，以避免命名冲突。

二、为什么使用Bison和Flex？
1. 在QGIS 3.28.15项目的编译过程中，词法分析和语法分析的需求主要源于QGIS内部的特定功能，如表达式解析、SQL语句解析、或其他专门的文本解析需求
（也就是说是QGIS内部自定义了一些功能，这些功能是需要定制化的实现，编译器中的词法分析和语法分析只能满足编译过程中的需求，并不能满足QGIS内部的
特定功能）。QGIS是一个复杂的地理信息系统软件，它允许用户通过表达式、SQL语句等方式进行数据查询、过滤和计算。这些功能需要将用户输入的文本（如表达
式或SQL查询）转换为软件能够理解和操作的内部表示形式。词法分析器和语法分析器在这里用于解析这些复杂文本，生成抽象语法树（AST），AST随后可用于进一
步的计算或数据检索操作。这是确保QGIS能够正确理解和执行用户命令的关键步骤。

2. 您的理解是正确的：当编译QGIS项目（或任何C++项目）时，MSVC（Microsoft Visual C++）编译器或其他编译器确实会对项目代码进行词法分析和语法分
析，以将C++代码转换为机器代码。这是编译任何程序的标准过程，确保代码符合C++的语法规则，并且可以被转换成可执行的程序。

  然而，Bison和Flex的使用与编译QGIS的C++代码本身是分开的。这两个工具用于生成QGIS内部使用的特定功能的解析器和词法分析器代码，而不是用于编译QGIS
的C++代码本身。当QGIS运行时，这些自动生成的解析器和词法分析器代码用于解析如表达式和SQL查询等用户输入，这是QGIS软件功能的一部分。简而言之，MSVC编
译器负责将QGIS的源代码编译成可执行文件，而Bison和Flex生成的代码则是QGIS软件内部的一部分，用于运行时解析用户输入。这两个过程虽然都涉及“解析”，但目
的和上下文完全不同。Bison和Flex在这里的使用是为了增强QGIS的功能，使其能够理解和处理复杂的用户输入，而这些是MSVC编译器在编译时不会也无法处理的任务。

问题：“在QGIS 3.28.15项目的编译过程中，词法分析和语法分析的需求主要源于QGIS内部的特定功能，如表达式解析、SQL语句解析、或其他专门的文本解析需求” 
也就是说是QGIS内部自定义了一些功能，这些功能是需要定制化的实现，编译器中的词法分析和语法分析只能满足编译过程中的需求，并不能满足QGIS内部的特定功能，
我的这个理解是否正确？
  是的，您的理解是正确的。QGIS内部自定义的功能，如表达式解析、SQL语句解析等，确实需要定制化的实现。这些功能的实现涉及到解析用户输入的文本，这种文
本解析通常比编译器在编译过程中执行的标准词法分析和语法分析更为复杂和特定。
  编译器中的词法分析和语法分析是为了将源代码转换为机器代码的过程中，确保源代码符合编程语言的语法规则。这个过程是通用的，针对的是编程语言本身的语法，
如C++、Java等。
  而QGIS内部的特定功能，如表达式解析器和SQL语句解析器，则需要理解和转换QGIS特定的语法或者用户定义的语法，这些语法是为了在QGIS环境中执行特定的地理
信息系统操作或查询而设计的。这种解析过程是特定于应用程序的，需要开发者使用工具如Bison和Flex来创建能够解析这些特定语法的词法分析器和语法分析器。
  因此，虽然编译器的词法分析和语法分析对于将QGIS源代码转换为可执行代码是必要的，但它们并不能直接用于QGIS内部需要的特定文本解析任务。这些任务需要通
过定制化的词法分析器和语法分析器来实现，这就是为什么在QGIS项目中使用Bison和Flex这样的工具来生成这些解析器的代码。

]]
BISON_TARGET(QgsExpressionParser   qgsexpressionparser.yy   ${CMAKE_CURRENT_BINARY_DIR}/qgsexpressionparser.cpp COMPILE_FLAGS "-p exp_")
FLEX_TARGET(QgsExpressionLexer qgsexpressionlexer.ll ${CMAKE_CURRENT_BINARY_DIR}/qgsexpressionlexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(QgsExpressionLexer QgsExpressionParser)
BISON_TARGET(QgsSqlStatementParser qgssqlstatementparser.yy ${CMAKE_CURRENT_BINARY_DIR}/qgssqlstatementparser.cpp  COMPILE_FLAGS "-p sqlstatement_")
FLEX_TARGET(QgsSqlStatementLexer qgssqlstatementlexer.ll ${CMAKE_CURRENT_BINARY_DIR}/qgssqlstatementlexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(QgsSqlStatementLexer QgsSqlStatementParser)
BISON_TARGET(QgsMeshCalcParser mesh/qgsmeshcalcparser.yy ${CMAKE_CURRENT_BINARY_DIR}/qgsmeshcalcparser.cpp COMPILE_FLAGS "-p mesh_")
FLEX_TARGET(QgsMeshCalcLexer mesh/qgsmeshcalclexer.ll ${CMAKE_CURRENT_BINARY_DIR}/qgsmeshcalclexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(QgsMeshCalcLexer QgsMeshCalcParser)

#[[
杨小兵-2024-03-04
将多个*.cpp文件添加到QGIS_CORE_SRCS变量中，在后面将会使用到
]]
set(QGIS_CORE_SRCS
  ${CMAKE_SOURCE_DIR}/external/kdbush/include/kdbush.hpp

  ${CMAKE_SOURCE_DIR}/external/nmea/context.c
  ${CMAKE_SOURCE_DIR}/external/nmea/gmath.c
  ${CMAKE_SOURCE_DIR}/external/nmea/info.c
  ${CMAKE_SOURCE_DIR}/external/nmea/parse.c
  ${CMAKE_SOURCE_DIR}/external/nmea/sentence.c
  ${CMAKE_SOURCE_DIR}/external/nmea/time.c
  ${CMAKE_SOURCE_DIR}/external/nmea/tok.c

  ${CMAKE_SOURCE_DIR}/external/meshOptimizer/simplifier.cpp

  ${FLEX_QgsExpressionLexer_OUTPUTS}
  ${BISON_QgsExpressionParser_OUTPUTS}
  ${FLEX_QgsSqlStatementLexer_OUTPUTS}
  ${BISON_QgsSqlStatementParser_OUTPUTS}
  ${FLEX_QgsMeshCalcLexer_OUTPUTS}
  ${BISON_QgsMeshCalcParser_OUTPUTS}

  callouts/qgscallout.cpp
  callouts/qgscalloutsregistry.cpp

  classification/qgsclassificationmethodregistry.cpp
  classification/qgsclassificationmethod.cpp
  classification/qgsclassificationcustom.cpp
  classification/qgsclassificationequalinterval.cpp
  classification/qgsclassificationfixedinterval.cpp
  classification/qgsclassificationprettybreaks.cpp
  classification/qgsclassificationquantile.cpp
  classification/qgsclassificationjenks.cpp
  classification/qgsclassificationstandarddeviation.cpp
  classification/qgsclassificationlogarithmic.cpp

  elevation/qgsabstractprofilegenerator.cpp
  elevation/qgsabstractprofilesurfacegenerator.cpp
  elevation/qgsabstractprofilesource.cpp
  elevation/qgsprofilerenderer.cpp
  elevation/qgsprofilerequest.cpp
  elevation/qgsprofilesnapping.cpp
  elevation/qgsterrainprovider.cpp

  geocoding/qgsabstractgeocoderlocatorfilter.cpp
  geocoding/qgsgeocoder.cpp
  geocoding/qgsgeocodercontext.cpp
  geocoding/qgsgeocoderresult.cpp
  geocoding/qgsnominatimgeocoder.cpp
  geocoding/qgsgooglemapsgeocoder.cpp

  gps/qgsbabelformat.cpp
  gps/qgsbabelformatregistry.cpp
  gps/qgsbabelgpsdevice.cpp
  gps/qgsgpsconnection.cpp
  gps/qgsgpsconnectionregistry.cpp
  gps/qgsgpsdconnection.cpp
  gps/qgsgpsdetector.cpp
  gps/qgsnmeaconnection.cpp

  plot/qgsplot.cpp

  symbology/qgs25drenderer.cpp
  symbology/qgsarrowsymbollayer.cpp
  symbology/qgscategorizedsymbolrenderer.cpp
  symbology/qgscolorbrewerpalette.cpp
  symbology/qgscombinedstylemodel.cpp
  symbology/qgscptcityarchive.cpp
  symbology/qgsellipsesymbollayer.cpp
  symbology/qgsembeddedsymbolrenderer.cpp
  symbology/qgsfeaturerenderergenerator.cpp
  symbology/qgsfillsymbol.cpp
  symbology/qgsfillsymbollayer.cpp
  symbology/qgsgeometrygeneratorsymbollayer.cpp
  symbology/qgsgraduatedsymbolrenderer.cpp
  symbology/qgsheatmaprenderer.cpp
  symbology/qgsinterpolatedlinerenderer.cpp
  symbology/qgsinvertedpolygonrenderer.cpp
  symbology/qgslegendsymbolitem.cpp
  symbology/qgslinesymbol.cpp
  symbology/qgslinesymbollayer.cpp
  symbology/qgsmapinfosymbolconverter.cpp
  symbology/qgsmarkersymbol.cpp
  symbology/qgsmarkersymbollayer.cpp
  symbology/qgsmasksymbollayer.cpp
  symbology/qgsmergedfeaturerenderer.cpp
  symbology/qgsnullsymbolrenderer.cpp
  symbology/qgspainterswapper.cpp
  symbology/qgspointclusterrenderer.cpp
  symbology/qgspointdisplacementrenderer.cpp
  symbology/qgspointdistancerenderer.cpp
  symbology/qgsrenderer.cpp
  symbology/qgsrendererrange.cpp
  symbology/qgsrendererregistry.cpp
  symbology/qgsrulebasedrenderer.cpp
  symbology/qgssinglesymbolrenderer.cpp
  symbology/qgsstyle.cpp
  symbology/qgsstylemodel.cpp
  symbology/qgssvgcache.cpp
  symbology/qgssymbollayer.cpp
  symbology/qgssymbollayerreference.cpp
  symbology/qgssymbollayerregistry.cpp
  symbology/qgssymbollayerutils.cpp
  symbology/qgssymbol.cpp
  symbology/qgssymbolrendercontext.cpp
  symbology/qgsvectorfieldsymbollayer.cpp
  simplify/effectivearea.cpp

  diagram/qgsdiagram.cpp
  diagram/qgshistogramdiagram.cpp
  diagram/qgspiediagram.cpp
  diagram/qgsstackedbardiagram.cpp
  diagram/qgstextdiagram.cpp

  editform/qgseditformconfig.cpp
  editform/qgsattributeeditoraction.cpp
  editform/qgsattributeeditorelement.cpp
  editform/qgsattributeeditorcontainer.cpp
  editform/qgsattributeeditorfield.cpp
  editform/qgsattributeeditorrelation.cpp
  editform/qgsattributeeditorhtmlelement.cpp
  editform/qgsattributeeditorqmlelement.cpp

  effects/qgsblureffect.cpp
  effects/qgscoloreffect.cpp
  effects/qgseffectstack.cpp
  effects/qgsgloweffect.cpp
  effects/qgsimageoperation.cpp
  effects/qgspainteffect.cpp
  effects/qgspainteffectregistry.cpp
  effects/qgsshadoweffect.cpp
  effects/qgstransformeffect.cpp

  externalstorage/qgsexternalstorage.cpp
  externalstorage/qgsexternalstorageregistry.cpp
  externalstorage/qgssimplecopyexternalstorage.cpp
  externalstorage/qgswebdavexternalstorage.cpp

  layertree/qgscolorramplegendnode.cpp
  layertree/qgscolorramplegendnodesettings.cpp
  layertree/qgslayertreegroup.cpp
  layertree/qgslayertreelayer.cpp
  layertree/qgslayertreemodel.cpp
  layertree/qgslayertreemodellegendnode.cpp
  layertree/qgslayertreenode.cpp
  layertree/qgslayertreeregistrybridge.cpp
  layertree/qgslayertreefilterproxymodel.cpp
  layertree/qgslayertreeutils.cpp
  layertree/qgslayertree.cpp
  layertree/qgslegendpatchshape.cpp

  metadata/qgsabstractmetadatabase.cpp
  metadata/qgslayermetadata.cpp
  metadata/qgslayermetadatavalidator.cpp
  metadata/qgslayermetadataformatter.cpp
  metadata/qgsmetadatautils.cpp
  metadata/qgsprojectmetadata.cpp
  metadata/qgsabstractlayermetadataprovider.cpp
  metadata/qgslayermetadataproviderregistry.cpp

  numericformats/qgsbasicnumericformat.cpp
  numericformats/qgsbearingnumericformat.cpp
  numericformats/qgscoordinatenumericformat.cpp
  numericformats/qgscurrencynumericformat.cpp
  numericformats/qgsfallbacknumericformat.cpp
  numericformats/qgsfractionnumericformat.cpp
  numericformats/qgsnumericformat.cpp
  numericformats/qgsnumericformatregistry.cpp
  numericformats/qgspercentagenumericformat.cpp
  numericformats/qgsscientificnumericformat.cpp

  auth/qgsauthcertutils.cpp
  auth/qgsauthconfig.cpp
  auth/qgsauthcrypto.cpp
  auth/qgsauthmanager.cpp
  auth/qgsauthmethod.cpp
  auth/qgsauthmethodmetadata.cpp
  auth/qgsauthmethodregistry.cpp

  annotations/qgsannotation.cpp
  annotations/qgsannotationitem.cpp
  annotations/qgsannotationitemeditoperation.cpp
  annotations/qgsannotationitemregistry.cpp
  annotations/qgsannotationlayer.cpp
  annotations/qgsannotationlayerrenderer.cpp
  annotations/qgsannotationlineitem.cpp
  annotations/qgsannotationmarkeritem.cpp
  annotations/qgsannotationmanager.cpp
  annotations/qgsannotationpointtextitem.cpp
  annotations/qgsannotationpolygonitem.cpp
  annotations/qgshtmlannotation.cpp
  annotations/qgsrenderedannotationitemdetails.cpp
  annotations/qgssvgannotation.cpp
  annotations/qgstextannotation.cpp

  expression/qgsexpression.cpp
  expression/qgsexpressioncontextutils.cpp
  expression/qgsexpressionnode.cpp
  expression/qgsexpressionnodeimpl.cpp
  expression/qgsexpressionfunction.cpp
  expression/qgsexpressionutils.cpp

  locator/qgslocator.cpp
  locator/qgslocatorfilter.cpp
  locator/qgslocatormodel.cpp
  locator/qgslocatormodelbridge.cpp

  network/qgsblockingnetworkrequest.cpp
  network/qgsfiledownloader.cpp
  network/qgsnetworkaccessmanager.cpp
  network/qgsnetworkdiskcache.cpp
  network/qgsnetworkcontentfetcher.cpp
  network/qgsnetworkcontentfetcherregistry.cpp
  network/qgsnetworkcontentfetchertask.cpp
  network/qgsnetworkreply.cpp
  network/qgsnetworkreplyparser.cpp
  network/qgsnewsfeedmodel.cpp
  network/qgsnewsfeedparser.cpp
  network/qgshttpheaders.cpp
  network/qgsrangerequestcache.cpp

  processing/qgsprocessing.cpp
  processing/qgsprocessingalgorithm.cpp
  processing/qgsprocessingalgrunnertask.cpp
  processing/qgsprocessingbatch.cpp
  processing/qgsprocessingcontext.cpp
  processing/qgsprocessingfeedback.cpp
  processing/qgsprocessingoutputs.cpp
  processing/qgsprocessingparameteraggregate.cpp
  processing/qgsprocessingparameterdxflayers.cpp
  processing/qgsprocessingparameterfieldmap.cpp
  processing/qgsprocessingparametermeshdataset.cpp
  processing/qgsprocessingparameters.cpp
  processing/qgsprocessingparametertininputlayers.cpp
  processing/qgsprocessingparametertype.cpp
  processing/qgsprocessingparametervectortilewriterlayers.cpp
  processing/qgsprocessingprovider.cpp
  processing/qgsprocessingregistry.cpp
  processing/qgsprocessingutils.cpp
  processing/models/qgsprocessingmodelalgorithm.cpp
  processing/models/qgsprocessingmodelchildalgorithm.cpp
  processing/models/qgsprocessingmodelchildparametersource.cpp
  processing/models/qgsprocessingmodelcomment.cpp
  processing/models/qgsprocessingmodelcomponent.cpp
  processing/models/qgsprocessingmodelgroupbox.cpp
  processing/models/qgsprocessingmodelparameter.cpp
  processing/models/qgsprocessingmodeloutput.cpp

  providers/qgsabstractproviderconnection.cpp
  providers/qgsabstractdatabaseproviderconnection.cpp
  providers/qgsdataprovider.cpp
  providers/qgsprovidermetadata.cpp
  providers/qgsproviderregistry.cpp
  providers/qgsprovidersqlquerybuilder.cpp
  providers/qgsprovidersublayerdetails.cpp
  providers/qgsprovidersublayermodel.cpp
  providers/qgsprovidersublayertask.cpp
  providers/qgsproviderutils.cpp

  providers/arcgis/qgsarcgisportalutils.cpp
  providers/arcgis/qgsarcgisrestquery.cpp
  providers/arcgis/qgsarcgisrestutils.cpp

  providers/gdal/qgsgdalproviderbase.cpp
  providers/gdal/qgsgdalprovider.cpp

  providers/memory/qgsmemoryfeatureiterator.cpp
  providers/memory/qgsmemoryprovider.cpp
  providers/memory/qgsmemoryproviderutils.cpp

  providers/meshmemory/qgsmeshmemorydataprovider.cpp

  providers/ogr/qgsogrlayermetadataprovider.cpp
  providers/ogr/qgsogrprovider.cpp
  providers/ogr/qgsogrprovidermetadata.cpp
  providers/ogr/qgsogrproviderutils.cpp
  providers/ogr/qgsogrfeatureiterator.cpp
  providers/ogr/qgsogrconnpool.cpp
  providers/ogr/qgsogrexpressioncompiler.cpp
  providers/ogr/qgsgeopackagedataitems.cpp
  providers/ogr/qgsgeopackageproviderconnection.cpp
  providers/ogr/qgsgeopackagerasterwriter.cpp
  providers/ogr/qgsgeopackagerasterwritertask.cpp
  providers/ogr/qgsgeopackageprojectstorage.cpp
  providers/ogr/qgsogrdbconnection.cpp
  providers/ogr/qgsogrproviderconnection.cpp
  providers/ogr/qgsogrtransaction.cpp

  scalebar/qgsdoubleboxscalebarrenderer.cpp
  scalebar/qgshollowscalebarrenderer.cpp
  scalebar/qgsnumericscalebarrenderer.cpp
  scalebar/qgsscalebarrenderer.cpp
  scalebar/qgsscalebarrendererregistry.cpp
  scalebar/qgsscalebarsettings.cpp
  scalebar/qgssingleboxscalebarrenderer.cpp
  scalebar/qgssteppedlinescalebarrenderer.cpp
  scalebar/qgsticksscalebarrenderer.cpp

  textrenderer/qgsfontmanager.cpp
  textrenderer/qgstextbackgroundsettings.cpp
  textrenderer/qgstextblock.cpp
  textrenderer/qgstextbuffersettings.cpp
  textrenderer/qgstextcharacterformat.cpp
  textrenderer/qgstextdocument.cpp
  textrenderer/qgstextdocumentmetrics.cpp
  textrenderer/qgstextformat.cpp
  textrenderer/qgstextfragment.cpp
  textrenderer/qgstextmasksettings.cpp
  textrenderer/qgstextrenderer.cpp
  textrenderer/qgstextrendererutils.cpp
  textrenderer/qgstextshadowsettings.cpp

  fromencodedcomponenthelper.cpp
  qgis.cpp
  qgsabstractcontentcache.cpp
  qgsabstractgeopdfexporter.cpp
  qgsapplication.cpp
  qgsaction.cpp
  qgsactionscope.cpp
  qgsactionscoperegistry.cpp
  qgsactionmanager.cpp
  qgsaggregatecalculator.cpp
  qgsanimatedicon.cpp
  qgspostgresstringutils.cpp
  qgsattributes.cpp
  qgsattributetableconfig.cpp
  qgsauxiliarystorage.cpp
  qgsbearingutils.cpp
  qgsbookmarkmanager.cpp
  qgsbookmarkmodel.cpp
  qgscachedfeatureiterator.cpp
  qgscacheindex.cpp
  qgscacheindexfeatureid.cpp
  qgscadutils.cpp
  qgsclipper.cpp
  qgscolorramp.cpp
  qgscolorrampimpl.cpp
  qgscolorscheme.cpp
  qgscolorschemeregistry.cpp
  qgscolorutils.cpp
  qgscommandlineutils.cpp
  qgsconditionalstyle.cpp
  qgsconnectionregistry.cpp
  qgscoordinateformatter.cpp
  qgscoordinateutils.cpp
  qgscopyfiletask.cpp
  qgscplhttpfetchoverrider.cpp
  qgscredentials.cpp
  qgsdartmeasurement.cpp
  qgsdatabaseschemamodel.cpp
  qgsdatabasetablemodel.cpp
  qgsdatadefinedsizelegend.cpp
  qgsdatasourceuri.cpp
  qgsdataprovidertemporalcapabilities.cpp
  qgsdatetimestatisticalsummary.cpp
  qgsdbfilterproxymodel.cpp
  qgsdbquerylog.cpp
  qgsdefaultvalue.cpp
  qgsdiagramrenderer.cpp
  qgsdistancearea.cpp
  qgselevationmap.cpp
  qgselevationutils.cpp
  qgserror.cpp
  qgseventtracing.cpp
  qgsexpressioncontext.cpp
  qgsexpressionfieldbuffer.cpp
  qgsfeature.cpp
  qgsfeaturepickermodel.cpp
  qgsfeaturepickermodelbase.cpp
  qgsfeatureiterator.cpp
  qgsfeaturerequest.cpp
  qgsfeaturesink.cpp
  qgsfeaturesource.cpp
  qgsfeaturestore.cpp
  qgsfeaturefiltermodel.cpp
  qgsfield.cpp
  qgsfieldconstraints.cpp
  qgsfieldformatter.cpp
  qgsfieldformatterregistry.cpp
  qgsfieldmodel.cpp
  qgsfieldproxymodel.cpp
  qgsfields.cpp
  qgsfileutils.cpp
  qgsfontutils.cpp
  qgsgdalutils.cpp
  qgsgeometrysimplifier.cpp
  qgsgeometryvalidator.cpp
  qgsgeometryoptions.cpp
  qgsgml.cpp
  qgsgmlschema.cpp
  qgsgrouplayer.cpp
  qgsgrouplayerrenderer.cpp
  qgshistogram.cpp
  qgshstoreutils.cpp
  qgshtmlutils.cpp
  qgsiconutils.cpp
  qgsidentifycontext.cpp
  qgsimagecache.cpp
  qgsinterval.cpp
  qgsjsonutils.cpp
  qgslayerdefinition.cpp
  qgslayernotesutils.cpp
  qgslegendrenderer.cpp
  qgslegendsettings.cpp
  qgslegendstyle.cpp
  qgslocaldefaultsettings.cpp
  qgslocalec.cpp
  qgslocalizeddatapathregistry.cpp
  qgslogger.cpp
  qgsmapclippingregion.cpp
  qgsmapclippingutils.cpp
  qgsmapdecoration.cpp
  qgsmaphittest.cpp
  qgsmaplayer.cpp
  qgsmaplayerelevationproperties.cpp
  qgsmaplayerfactory.cpp
  qgsmaplayerlegend.cpp
  qgsmaplayermodel.cpp
  qgsmaplayerproxymodel.cpp
  qgsmaplayerrenderer.cpp
  qgsmaplayerserverproperties.cpp
  qgsmaplayerstore.cpp
  qgsmaplayerstyle.cpp
  qgsmaplayerstylemanager.cpp
  qgsmaplayertemporalproperties.cpp
  qgsmaplayerutils.cpp
  qgsmapsettings.cpp
  qgsmapsettingsutils.cpp
  qgsmaptopixel.cpp
  qgsmaptopixelgeometrysimplifier.cpp
  qgsmapunitscale.cpp
  qgsmargins.cpp
  qgsmaskidprovider.cpp
  qgsmbtiles.cpp
  qgsmaskpaintdevice.cpp
  qgsmessagelog.cpp
  qgsmessageoutput.cpp
  qgsmimedatautils.cpp
  qgsmultirenderchecker.cpp
  qgsnullpainterdevice.cpp
  qgsobjectcustomproperties.cpp
  qgsofflineediting.cpp
  qgsogcutils.cpp
  qgsogrproxytextcodec.cpp
  qgsogrutils.cpp
  qgsoptionalexpression.cpp
  qgsowsconnection.cpp
  qgspaintenginehack.cpp
  qgspainting.cpp
  qgspathresolver.cpp
  qgspluginlayer.cpp
  qgspluginlayerregistry.cpp
  qgspointxy.cpp
  qgspointlocator.cpp
  qgspointlocatorinittask.cpp
  qgsqueryresultmodel.cpp
  qgssnappingconfig.cpp
  qgsproperty.cpp
  qgspropertycollection.cpp
  qgspropertytransformer.cpp
  qgsproviderconnectionmodel.cpp
  qgsproxyfeaturesink.cpp
  qgsproxyprogresstask.cpp
  qgspythonrunner.cpp
  qgsreadwritecontext.cpp
  qgsreadwritelocker.cpp
  qgsrelation.cpp
  qgspolymorphicrelation.cpp
  qgsrecentstylehandler.cpp
  qgsrelationcontext.cpp
  qgsweakrelation.cpp
  qgsrelationmanager.cpp
  qgsremappingproxyfeaturesink.cpp
  qgsrenderchecker.cpp
  qgsrendercontext.cpp
  qgsrendereditemdetails.cpp
  qgsrunprocess.cpp
  qgsruntimeprofiler.cpp
  qgsscalecalculator.cpp
  qgsscaleutils.cpp
  qgsselectioncontext.cpp
  qgsshapegenerator.cpp
  qgssimplifymethod.cpp
  qgssingleitemmodel.cpp
  qgssnappingutils.cpp
  qgsspatialindex.cpp
  qgsspatialindexkdbush.cpp
  qgsspatialindexutils.cpp
  qgssqlexpressioncompiler.cpp
  qgssqliteexpressioncompiler.cpp
  qgssqlstatement.cpp
  qgssqliteutils.cpp
  qgssourcecache.cpp
  qgsspatialiteutils.cpp
  qgsstatisticalsummary.cpp
  qgsstoredexpressionmanager.cpp
  qgsstringstatisticalsummary.cpp
  qgsstringutils.cpp
  qgstablecell.cpp
  qgstaskmanager.cpp
  qgstemporalcontroller.cpp
  qgstemporalnavigationobject.cpp
  qgstemporalproperty.cpp
  qgstemporalrangeobject.cpp
  qgstemporalutils.cpp
  qgstessellator.cpp
  qgstilecache.cpp
  qgstiledownloadmanager.cpp
  qgstiles.cpp
  qgstolerance.cpp
  qgstracer.cpp
  qgstranslationcontext.cpp
  qgstrackedvectorlayertools.cpp
  qgstransaction.cpp
  qgstransactiongroup.cpp
  qgsunittypes.cpp
  qgsvariantutils.cpp
  qgsvector.cpp
  qgsvector3d.cpp
  qgsvectorfilewriter.cpp
  qgsvectorfilewritertask.cpp
  qgsvirtuallayertask.cpp
  qgsvectorsimplifymethod.cpp
  qgsvirtuallayerdefinition.cpp
  qgsvirtuallayerdefinitionutils.cpp
  qgsunsetattributevalue.cpp
  qgsmapthemecollection.cpp
  qgsxmlutils.cpp
  qgsarchive.cpp
  qgstestutils.cpp
  qgsziputils.cpp
  qgsptpfiles.cpp

  browser/qgsbrowsermodel.cpp
  browser/qgsbrowserproxymodel.cpp
  browser/qgsconnectionsitem.cpp
  browser/qgsdatabaseschemaitem.cpp
  browser/qgsdatacollectionitem.cpp
  browser/qgsdataitem.cpp
  browser/qgsdataitemprovider.cpp
  browser/qgsdataitemproviderregistry.cpp
  browser/qgsdirectoryitem.cpp
  browser/qgsfavoritesitem.cpp
  browser/qgsfielddomainsitem.cpp
  browser/qgsfieldsitem.cpp
  browser/qgsfilebaseddataitemprovider.cpp
  browser/qgslayeritem.cpp
  browser/qgsprojectitem.cpp
  browser/qgsrelationshipsitem.cpp
  browser/qgszipitem.cpp

  dxf/qgsdxfexport.cpp
  dxf/qgsdxfpaintdevice.cpp
  dxf/qgsdxfpaintengine.cpp

  layout/qgsabstractreportsection.cpp
  layout/qgslayout.cpp
  layout/qgslayoutaligner.cpp
  layout/qgslayoutatlas.cpp
  layout/qgslayouteffect.cpp
  layout/qgslayoutexporter.cpp
  layout/qgslayoutgridsettings.cpp
  layout/qgslayoutguidecollection.cpp
  layout/qgslayoutframe.cpp
  layout/qgslayoutgeopdfexporter.cpp
  layout/qgslayoutitem.cpp
  layout/qgslayoutitemattributetable.cpp
  layout/qgslayoutitemgroup.cpp
  layout/qgslayoutitemgroupundocommand.cpp
  layout/qgslayoutitemhtml.cpp
  layout/qgslayoutitemlabel.cpp
  layout/qgslayoutitemlegend.cpp
  layout/qgslayoutitemmanualtable.cpp
  layout/qgslayoutitemmap.cpp
  layout/qgslayoutitemmapgrid.cpp
  layout/qgslayoutitemmapitem.cpp
  layout/qgslayoutitemmapoverview.cpp
  layout/qgslayoutitemmarker.cpp
  layout/qgslayoutitemnodeitem.cpp
  layout/qgslayoutitempage.cpp
  layout/qgslayoutitempicture.cpp
  layout/qgslayoutitempolygon.cpp
  layout/qgslayoutitempolyline.cpp
  layout/qgslayoutitemregistry.cpp
  layout/qgslayoutitemscalebar.cpp
  layout/qgslayoutitemshape.cpp
  layout/qgslayoutitemtexttable.cpp
  layout/qgslayoutitemundocommand.cpp
  layout/qgslayoutmanager.cpp
  layout/qgslayoutmeasurement.cpp
  layout/qgslayoutmeasurementconverter.cpp
  layout/qgslayoutmodel.cpp
  layout/qgslayoutmultiframe.cpp
  layout/qgslayoutmultiframeundocommand.cpp
  layout/qgslayoutnortharrowhandler.cpp
  layout/qgslayoutobject.cpp
  layout/qgslayoutpagecollection.cpp
  layout/qgslayoutrendercontext.cpp
  layout/qgslayoutreportcontext.cpp
  layout/qgslayoutserializableobject.cpp
  layout/qgslayoutsnapper.cpp
  layout/qgslayouttable.cpp
  layout/qgslayouttablecolumn.cpp
  layout/qgslayoutundocommand.cpp
  layout/qgslayoutundostack.cpp
  layout/qgslayoututils.cpp
  layout/qgspagesizeregistry.cpp
  layout/qgslayoutpoint.cpp
  layout/qgslayoutserializableobject.cpp
  layout/qgslayoutsize.cpp
  layout/qgsprintlayout.cpp
  layout/qgsreport.cpp
  layout/qgsreportsectionfieldgroup.cpp
  layout/qgsreportsectionlayout.cpp
  layout/qgscompositionconverter.cpp

  maprenderer/qgsmaprenderercache.cpp
  maprenderer/qgsmaprenderercustompainterjob.cpp
  maprenderer/qgsmaprendererjob.cpp
  maprenderer/qgsmaprendererparalleljob.cpp
  maprenderer/qgsmaprenderersequentialjob.cpp
  maprenderer/qgsmaprendererstagedrenderjob.cpp
  maprenderer/qgsmaprenderertask.cpp
  maprenderer/qgsrendereditemresults.cpp

  pal/costcalculator.cpp
  pal/feature.cpp
  pal/geomfunction.cpp
  pal/labelposition.cpp
  pal/layer.cpp
  pal/pal.cpp
  pal/palstat.cpp
  pal/pointset.cpp
  pal/priorityqueue.cpp
  pal/problem.cpp
  pal/util.cpp

  proj/qgscoordinatereferencesystem.cpp
  proj/qgscoordinatereferencesystemregistry.cpp
  proj/qgscoordinatereferencesystemutils.cpp
  proj/qgscoordinatetransform.cpp
  proj/qgscoordinatetransform_p.cpp
  proj/qgscoordinatetransformcontext.cpp
  proj/qgsdatumtransform.cpp
  proj/qgsellipsoidutils.cpp
  proj/qgsprojutils.cpp

  project/qgsproject.cpp
  project/qgsprojectbadlayerhandler.cpp
  project/qgsprojectelevationproperties.cpp
  project/qgsprojectfiletransform.cpp
  project/qgsprojectdisplaysettings.cpp
  project/qgsprojectproperty.cpp
  project/qgsprojectservervalidator.cpp
  project/qgsprojectstorage.cpp
  project/qgsprojectstorageregistry.cpp
  project/qgsprojectstylesettings.cpp
  project/qgsprojecttimesettings.cpp
  project/qgsprojectutils.cpp
  project/qgsprojectversion.cpp
  project/qgsprojectviewsettings.cpp
  project/qgsmapviewsmanager.cpp

  raster/qgscliptominmaxenhancement.cpp
  raster/qgscolorrampshader.cpp
  raster/qgscontrastenhancement.cpp
  raster/qgscontrastenhancementfunction.cpp
  raster/qgsexiftools.cpp
  raster/qgslinearminmaxenhancement.cpp
  raster/qgslinearminmaxenhancementwithclip.cpp
  raster/qgsraster.cpp
  raster/qgsrasterblock.cpp
  raster/qgsrasterchecker.cpp
  raster/qgsrastercontourrenderer.cpp
  raster/qgsrasterdataprovider.cpp
  raster/qgsrasterdataprovidertemporalcapabilities.cpp
  raster/qgsrasterfilewritertask.cpp
  raster/qgsrasteridentifyresult.cpp
  raster/qgsrasterinterface.cpp
  raster/qgsrasteriterator.cpp
  raster/qgsrasterlayer.cpp
  raster/qgsrasterlayerelevationproperties.cpp
  raster/qgsrasterlayerprofilegenerator.cpp
  raster/qgsrasterlayerrenderer.cpp
  raster/qgsrasterlayertemporalproperties.cpp
  raster/qgsrasterminmaxorigin.cpp
  raster/qgsrasternuller.cpp
  raster/qgsrasterpipe.cpp
  raster/qgsrasterprojector.cpp
  raster/qgsrasterrange.cpp
  raster/qgsrastershader.cpp
  raster/qgsrastershaderfunction.cpp
  raster/qgsrastertransparency.cpp

  raster/qgsbilinearrasterresampler.cpp
  raster/qgsbrightnesscontrastfilter.cpp
  raster/qgscubicrasterresampler.cpp
  raster/qgshuesaturationfilter.cpp
  raster/qgsmultibandcolorrenderer.cpp
  raster/qgspalettedrasterrenderer.cpp
  raster/qgsrasterdrawer.cpp
  raster/qgsrasterfilewriter.cpp
  raster/qgsrasterrenderer.cpp
  raster/qgsrasterrendererregistry.cpp
  raster/qgsrasterrendererutils.cpp
  raster/qgsrasterresamplefilter.cpp
  raster/qgssinglebandcolordatarenderer.cpp
  raster/qgssinglebandgrayrenderer.cpp
  raster/qgssinglebandpseudocolorrenderer.cpp
  raster/qgshillshaderenderer.cpp

  mesh/qgsmesh3daveraging.cpp
  mesh/qgsmesheditor.cpp
  mesh/qgsmeshdataprovider.cpp
  mesh/qgsmeshdataprovidertemporalcapabilities.cpp
  mesh/qgsmeshdataset.cpp
  mesh/qgsmeshdatasetgroupstore.cpp
  mesh/qgsmeshlayer.cpp
  mesh/qgsmeshlayerelevationproperties.cpp
  mesh/qgsmeshlayerinterpolator.cpp
  mesh/qgsmeshlayerprofilegenerator.cpp
  mesh/qgsmeshlayerrenderer.cpp
  mesh/qgsmeshlayertemporalproperties.cpp
  mesh/qgsmeshlayerutils.cpp
  mesh/qgsmeshrenderersettings.cpp
  mesh/qgsmeshsimplificationsettings.cpp
  mesh/qgsmeshspatialindex.cpp
  mesh/qgsmeshtimesettings.cpp
  mesh/qgsmeshtracerenderer.cpp
  mesh/qgsmeshvectorrenderer.cpp
  mesh/qgstriangularmesh.cpp
  mesh/qgsmeshcalcnode.cpp
  mesh/qgsmeshcalculator.cpp
  mesh/qgsmeshcalcutils.cpp
  mesh/qgsmeshvirtualdatasetgroup.cpp
  mesh/qgstopologicalmesh.cpp
  mesh/qgsmeshadvancedediting.cpp
  mesh/qgsmeshforcebypolylines.cpp

  pointcloud/qgspointcloudattribute.cpp
  pointcloud/qgspointcloudattributebyramprenderer.cpp
  pointcloud/qgspointcloudattributemodel.cpp
  pointcloud/qgspointcloudclassifiedrenderer.cpp
  pointcloud/qgspointcloudextentrenderer.cpp
  pointcloud/qgspointcloudrequest.cpp
  pointcloud/qgspointcloudblock.cpp
  pointcloud/qgspointcloudblockrequest.cpp
  pointcloud/qgspointcloudlayer.cpp
  pointcloud/qgspointcloudlayerelevationproperties.cpp
  pointcloud/qgspointcloudlayerprofilegenerator.cpp
  pointcloud/qgspointcloudlayerrenderer.cpp
  pointcloud/qgspointcloudindex.cpp
  pointcloud/qgspointclouddataprovider.cpp
  pointcloud/qgspointcloudrenderer.cpp
  pointcloud/qgspointcloudrendererregistry.cpp
  pointcloud/qgspointcloudrgbrenderer.cpp
  pointcloud/qgspointcloudlayerexporter.cpp

  pointcloud/expression/qgspointcloudexpression.cpp
  pointcloud/expression/qgspointcloudexpressionnode.cpp
  pointcloud/expression/qgspointcloudexpressionnodeimpl.cpp

  pointcloud/qgspointcloudstatscalculator.cpp
  pointcloud/qgspointcloudstatscalculationtask.cpp
  pointcloud/qgspointcloudstatistics.cpp

  labeling/qgslabelfeature.cpp
  labeling/qgslabelingengine.cpp
  labeling/qgslabelingenginesettings.cpp
  labeling/qgslabelingresults.cpp
  labeling/qgslabellinesettings.cpp
  labeling/qgslabelplacementsettings.cpp
  labeling/qgslabelobstaclesettings.cpp
  labeling/qgslabelsearchtree.cpp
  labeling/qgslabelsink.cpp
  labeling/qgslabelthinningsettings.cpp
  labeling/qgspallabeling.cpp
  labeling/qgsrulebasedlabeling.cpp
  labeling/qgstextlabelfeature.cpp
  labeling/qgsvectorlayerlabeling.cpp
  labeling/qgsvectorlayerlabelprovider.cpp

  geometry/qgsabstractgeometry.cpp
  geometry/qgsbox3d.cpp
  geometry/qgscircle.cpp
  geometry/qgscircularstring.cpp
  geometry/qgscompoundcurve.cpp
  geometry/qgscurvepolygon.cpp
  geometry/qgscurve.cpp
  geometry/qgsellipse.cpp
  geometry/qgsgeometry.cpp
  geometry/qgsgeometrycollection.cpp
  geometry/qgsgeometryeditutils.cpp
  geometry/qgsgeometryfactory.cpp
  geometry/qgsgeometryutils.cpp
  geometry/qgsgeos.cpp
  geometry/qgsinternalgeometryengine.cpp
  geometry/qgslinesegment.cpp
  geometry/qgslinestring.cpp
  geometry/qgsmulticurve.cpp
  geometry/qgsmultilinestring.cpp
  geometry/qgsmultipoint.cpp
  geometry/qgsmultipolygon.cpp
  geometry/qgsmultisurface.cpp
  geometry/qgspoint.cpp
  geometry/qgspolygon.cpp
  geometry/qgsquadrilateral.cpp
  geometry/qgsrectangle.cpp
  geometry/qgsreferencedgeometry.cpp
  geometry/qgsregularpolygon.cpp
  geometry/qgssurface.cpp
  geometry/qgstriangle.cpp
  geometry/qgsvertexid.cpp
  geometry/qgswkbptr.cpp
  geometry/qgswkbtypes.cpp
  geometry/qgsray3d.cpp

  3d/qgs3drendererregistry.cpp
  3d/qgs3dsymbolregistry.cpp
  3d/qgsabstract3dsymbol.cpp
  3d/qgsabstract3drenderer.cpp


  fieldformatter/qgscheckboxfieldformatter.cpp
  fieldformatter/qgsrangefieldformatter.cpp
  fieldformatter/qgsdatetimefieldformatter.cpp
  fieldformatter/qgsfallbackfieldformatter.cpp
  fieldformatter/qgskeyvaluefieldformatter.cpp
  fieldformatter/qgslistfieldformatter.cpp
  fieldformatter/qgsrelationreferencefieldformatter.cpp
  fieldformatter/qgsvaluemapfieldformatter.cpp
  fieldformatter/qgsvaluerelationfieldformatter.cpp

  geocms/geonode/qgsgeonodeconnection.cpp
  geocms/geonode/qgsgeonoderequest.cpp

  settings/qgssettings.cpp
  settings/qgssettingsentry.cpp
  settings/qgssettingsentryimpl.cpp
  settings/qgssettingsregistry.cpp
  settings/qgssettingsregistrycore.cpp

  validity/qgsabstractvaliditycheck.cpp
  validity/qgsvaliditycheckcontext.cpp
  validity/qgsvaliditycheckregistry.cpp

  vector/qgsfielddomain.cpp
  vector/qgsvectordataprovider.cpp
  vector/qgsvectordataprovidertemporalcapabilities.cpp
  vector/qgsvectorlayer.cpp
  vector/qgsvectorlayerfeaturecounter.cpp
  vector/qgsvectorlayercache.cpp
  vector/qgsvectorlayerdiagramprovider.cpp
  vector/qgsvectorlayereditbuffer.cpp
  vector/qgsvectorlayereditbuffergroup.cpp
  vector/qgsvectorlayereditpassthrough.cpp
  vector/qgsvectorlayereditutils.cpp
  vector/qgsvectorlayerelevationproperties.cpp
  vector/qgsvectorlayerfeatureiterator.cpp
  vector/qgsvectorlayerexporter.cpp
  vector/qgsvectorlayerjoinbuffer.cpp
  vector/qgsvectorlayerjoininfo.cpp
  vector/qgsvectorlayerprofilegenerator.cpp
  vector/qgsvectorlayerrenderer.cpp
  vector/qgsvectorlayertemporalproperties.cpp
  vector/qgsvectorlayertools.cpp
  vector/qgsvectorlayerundocommand.cpp
  vector/qgsvectorlayerundopassthroughcommand.cpp
  vector/qgsvectorlayerutils.cpp

  vectortile/qgsmapboxglstyleconverter.cpp
  vectortile/qgsvectortilebasiclabeling.cpp
  vectortile/qgsvectortilebasicrenderer.cpp
  vectortile/qgsvectortileconnection.cpp
  vectortile/qgsvectortiledataitems.cpp
  vectortile/qgsvectortilelabeling.cpp
  vectortile/qgsvectortilelayer.cpp
  vectortile/qgsvectortilelayerrenderer.cpp
  vectortile/qgsvectortileloader.cpp
  vectortile/qgsvectortilematrixset.cpp
  vectortile/qgsvectortilemvtdecoder.cpp
  vectortile/qgsvectortilemvtencoder.cpp
  vectortile/qgsvectortilemvtutils.cpp
  vectortile/qgsvectortileprovidermetadata.cpp
  vectortile/qgsvectortileutils.cpp
  vectortile/qgsvectortilewriter.cpp
  vectortile/qgsvtpktiles.cpp

  ${CMAKE_CURRENT_BINARY_DIR}/qgsexpression_texts.cpp

  qgsuserprofile.cpp
  qgsuserprofilemanager.cpp

)

#[[
杨小兵-2024-03-04
  如果定义了WITH_INTERNAL_POLY2TRI宏，那么将多个*.cpp文件再次添加到QGIS_CORE_SRCS变量中，并且设置一些头文件的搜索路径。
]]
if (WITH_INTERNAL_POLY2TRI)
  set(QGIS_CORE_SRCS ${QGIS_CORE_SRCS}
    ${CMAKE_SOURCE_DIR}/external/poly2tri/common/shapes.cc
    ${CMAKE_SOURCE_DIR}/external/poly2tri/sweep/advancing_front.cc
    ${CMAKE_SOURCE_DIR}/external/poly2tri/sweep/cdt.cc
    ${CMAKE_SOURCE_DIR}/external/poly2tri/sweep/sweep_context.cc
    ${CMAKE_SOURCE_DIR}/external/poly2tri/sweep/sweep.cc
  )
  include_directories(${CMAKE_SOURCE_DIR}/external/poly2tri)
else ()
  include_directories(${Poly2Tri_INCLUDE_DIR})
endif ()

#[[
杨小兵-2024-03-04
一、解释
  这段CMake脚本涉及到一系列操作，主要目的是处理QGIS项目中的函数帮助文件，并生成一个特定的C++源文件。下面逐步解释这些命令的作用：

1. **`file(GLOB JSON_HELP_FILES "${CMAKE_SOURCE_DIR}/resources/function_help/json/*")`**：
   - 这个命令使用GLOB模式收集`${CMAKE_SOURCE_DIR}/resources/function_help/json/`目录下的所有文件路径，并将这些路径存储在变量
   `JSON_HELP_FILES`中。这里主要是收集JSON格式的帮助文件，这些文件可能用于定义QGIS中各种函数的帮助文档。

2. **条件判断和字符串替换**:
   - `if(CMAKE_VERSION VERSION_LESS "3.18" AND NOT USING_NINJA)`检查CMake的版本是否低于3.18，并且构建系统不是Ninja。如果条
   件为真，执行内部的字符串替换命令。
   - `string(REPLACE "$" "$$" JSON_HELP_FILES "${JSON_HELP_FILES}")`是为了在较早版本的CMake中处理路径字符串时避免由于`$`符
   号引起的问题，通过将`$`替换为`$$`来解决。
   - 接下来的两行`string(REPLACE "\(" "\\(" JSON_HELP_FILES "${JSON_HELP_FILES}")`和`string(REPLACE "\)" "\\)" JSON_HELP_FILES 
   "${JSON_HELP_FILES}")`将路径字符串中的括号`(`和`)`转义，因为在某些上下文中，这些字符可能会被错误地解析。

3. **`add_custom_command`**:
   - 这个命令定义了一个自定义的构建命令，它指示CMake在构建过程中执行一系列指定的命令，以生成输出文件`${CMAKE_CURRENT_BINARY_DIR}/qgsexpression_texts.cpp`。
   - `COMMAND ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/process_function_template.py ${CMAKE_CURRENT_BINARY_DIR}/
   qgsexpression_texts.cpp.temp`：这一步调用Python脚本`process_function_template.py`，以处理函数模板，并生成一个临时文件。这个脚本
   可能会读取JSON帮助文件，并根据这些文件的内容生成C++源代码。
   - `COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_BINARY_DIR}/qgsexpression_texts.cpp.temp -DDST=${CMAKE_CURRENT_BINARY_DIR}
   /qgsexpression_texts.cpp -P ${CMAKE_SOURCE_DIR}/cmake/CopyIfChanged.cmake`：这一步调用CMake命令执行`CopyIfChanged.cmake`脚本，
   这个脚本可能会检查临时文件是否有更新，如果有，则将其复制到最终的目标文件位置，这样可以避免不必要的重新编译。
   - `DEPENDS ${JSON_HELP_FILES}`：指定这个自定义命令依赖于前面通过GLOB收集的JSON帮助文件。这意味着如果这些帮助文件被修改，上述命令将被
   重新执行以更新输出文件。
   - `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}`：设置工作目录为`${CMAKE_SOURCE_DIR}`，确保上述命令在正确的目录下执行。

  总结来说，这段CMake脚本的作用是自动处理QGIS函数的帮助文件，通过一个Python脚本将这些帮助文件转换为C++代码，然后将生成的代码保存到项目的构建目录中。这个过程通过自动化地处理和更新帮助文档内容，简化了QGIS开发和维护过程。
二、总结
1、用来自动处理QGIS函数的帮助文档
2、这部分内容现在可以不用关心
]]
file(GLOB JSON_HELP_FILES "${CMAKE_SOURCE_DIR}/resources/function_help/json/*")
if(CMAKE_VERSION VERSION_LESS "3.18" AND NOT USING_NINJA)
  string(REPLACE "$" "$$" JSON_HELP_FILES "${JSON_HELP_FILES}")
endif()
string(REPLACE "\(" "\\(" JSON_HELP_FILES "${JSON_HELP_FILES}")
string(REPLACE "\)" "\\)" JSON_HELP_FILES "${JSON_HELP_FILES}")
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/qgsexpression_texts.cpp
  COMMAND ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/process_function_template.py ${CMAKE_CURRENT_BINARY_DIR}/qgsexpression_texts.cpp.temp
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_BINARY_DIR}/qgsexpression_texts.cpp.temp -DDST=${CMAKE_CURRENT_BINARY_DIR}/qgsexpression_texts.cpp -P ${CMAKE_SOURCE_DIR}/cmake/CopyIfChanged.cmake
  DEPENDS ${JSON_HELP_FILES}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#[[
杨小兵-2024-03-04
一、解释
  如果设置了ENABLE_MODELTEST标志，那么将会设置一个变量，这个变量中存储的是测试相关的代码文件
]]
if(ENABLE_MODELTEST)
  set(MODELTEST_SRCS
    ../../tests/qt_modeltest/modeltest.cpp
    ../../tests/qt_modeltest/dynamictreemodel.cpp
  )
  set(QGIS_CORE_SRCS ${QGIS_CORE_SRCS}
    ${MODELTEST_SRCS}
  )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果编译器是Clang编译器，那么需要对一个cpp文件进行特殊处理
]]
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # spatialindex headers produce warnings
  set_source_files_properties(qgsspatialindex.cpp PROPERTIES COMPILE_FLAGS -Wno-overloaded-virtual)
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果${QT_VERSION_BASE}Positioning_FOUND变量存在，那么将一个新的cpp文件添加到QGIS_CORE_SRCS变量中
]]
if (${QT_VERSION_BASE}Positioning_FOUND)
  set(QGIS_CORE_SRCS ${QGIS_CORE_SRCS}
    gps/qgsqtlocationconnection.cpp
  )
endif()


#[[
杨小兵-2024-03-04
一、解释
1、如果不是MSVC编译器，将对一些文件进行属性的配置
2、如果是MSVC编译器，将会都一些文件进行属性的配置
3、根据不同的编译器采用不同的属性配置设置
]]
if(NOT MSVC)
  set_source_files_properties(
    ${FLEX_QgsExpressionLexer_OUTPUTS}
    ${BISON_QgsExpressionParser_OUTPUTS}
    ${FLEX_QgsSqlStatementLexer_OUTPUTS}
    ${BISON_QgsSqlStatementParser_OUTPUTS}
    ${FLEX_QgsMeshCalcLexer_OUTPUTS}
    ${BISON_QgsMeshCalcParser_OUTPUTS}
    PROPERTIES COMPILE_FLAGS -w)
else()
  # -wd4702 unreachable code
  set_source_files_properties(
    geometry/qgsgeos.cpp
    pal/feature.cpp
    pal/pointset.cpp
    ${BISON_QgsMeshCalcParser_OUTPUTS}
    PROPERTIES COMPILE_FLAGS -wd4702)
  set_source_files_properties(
    ${FLEX_QgsExpressionLexer_OUTPUTS}
    ${BISON_QgsExpressionParser_OUTPUTS}
    ${FLEX_QgsSqlStatementLexer_OUTPUTS}
    ${BISON_QgsSqlStatementParser_OUTPUTS}
    ${FLEX_QgsMeshCalcLexer_OUTPUTS}
    ${BISON_QgsMeshCalcParser_OUTPUTS}
    PROPERTIES COMPILE_FLAGS /W0)
endif()


#[[
杨小兵-2024-03-04
一、解释
  设置QGIS_CORE_HDRS变量，其中包含qgis_core项目需要的头文件
]]
set(QGIS_CORE_HDRS
  ${CMAKE_SOURCE_DIR}/external/nlohmann/json_fwd.hpp

  ${CMAKE_BINARY_DIR}/qgsconfig.h

  ../plugins/qgisplugin.h

  fromencodedcomponenthelper.h
  qgis.h
  qgis_sip.h
  qgsabstractcontentcache.h
  qgsabstractgeopdfexporter.h
  qgsaction.h
  qgsactionmanager.h
  qgsactionscope.h
  qgsactionscoperegistry.h
  qgsaggregatecalculator.h
  qgsanimatedicon.h
  qgsapplication.h
  qgsarchive.h
  qgsattributes.h
  qgsattributetableconfig.h
  qgsauxiliarystorage.h
  qgsbearingutils.h
  qgsbookmarkmanager.h
  qgsbookmarkmodel.h
  qgscachedfeatureiterator.h
  qgscacheindex.h
  qgscacheindexfeatureid.h
  qgscadutils.h
  qgsclipper.h
  qgscommandlineutils.h
  qgscolorramp.h
  qgscolorrampimpl.h
  qgscolorscheme.h
  qgscolorschemeregistry.h
  qgscolorutils.h
  qgsconditionalstyle.h
  qgsconnectionpool.h
  qgsconnectionregistry.h
  qgscoordinateformatter.h
  qgscoordinateutils.h
  qgscopyfiletask.h
  qgscredentials.h
  qgsdartmeasurement.h
  qgsdatabaseschemamodel.h
  qgsdatabasetablemodel.h
  qgsdatadefinedsizelegend.h
  qgsdataprovidertemporalcapabilities.h
  qgsdatasourceuri.h
  qgsdatetimestatisticalsummary.h
  qgsdbfilterproxymodel.h
  qgsdbquerylog.h
  qgsdefaultvalue.h
  qgsdiagramrenderer.h
  qgsdistancearea.h
  qgseditorwidgetsetup.h
  qgselevationmap.h
  qgselevationutils.h
  qgserror.h
  qgseventtracing.h
  qgsexception.h
  qgsexpressioncontext.h
  qgsexpressioncontextgenerator.h
  qgsexpressioncontextscopegenerator.h
  qgsexpressionfieldbuffer.h
  qgsfeature.h
  qgsfeaturepickermodel.h
  qgsfeaturepickermodelbase.h
  qgsfeatureexpressionvaluesgatherer.h
  qgsfeaturefiltermodel.h
  qgsfeaturefilterprovider.h
  qgsfeatureid.h
  qgsfeatureiterator.h
  qgsfeaturerequest.h
  qgsfeaturesink.h
  qgsfeaturesource.h
  qgsfeaturestore.h
  qgsfeedback.h
  qgsfield.h
  qgsfieldconstraints.h
  qgsfieldformatter.h
  qgsfieldformatterregistry.h
  qgsfieldmodel.h
  qgsfieldproxymodel.h
  qgsfields.h
  qgsfilefiltergenerator.h
  qgsfileutils.h
  qgsfontutils.h
  qgsgdalutils.h
  qgsgenericspatialindex.h
  qgsgeometryoptions.h
  qgsgeometrysimplifier.h
  qgsgeometryvalidator.h
  qgsgml.h
  qgsgmlschema.h
  qgsgrouplayer.h
  qgsgrouplayerrenderer.h
  qgshistogram.h
  qgshstoreutils.h
  qgshtmlutils.h
  qgsiconutils.h
  qgsidentifycontext.h
  qgsimagecache.h
  qgsindexedfeature.h
  qgsinterval.h
  qgsjsonutils.h
  qgslayerdefinition.h
  qgslayernotesutils.h
  qgslegendrenderer.h
  qgslegendsettings.h
  qgslegendstyle.h
  qgslocaldefaultsettings.h
  qgslocalec.h
  qgslocalizeddatapathregistry.h
  qgslogger.h
  qgsmapclippingregion.h
  qgsmapclippingutils.h
  qgsmapdecoration.h
  qgsmaphittest.h
  qgsmaplayer.h
  qgsmaplayerdependency.h
  qgsmaplayerfactory.h
  qgsmaplayerlegend.h
  qgsmaplayermodel.h
  qgsmaplayerproxymodel.h
  qgsmaplayerref.h
  qgsmaplayerrenderer.h
  qgsmaplayerserverproperties.h
  qgsmaplayerstore.h
  qgsmaplayerstyle.h
  qgsmaplayerstylemanager.h
  qgsmaplayerelevationproperties.h
  qgsmaplayertemporalproperties.h
  qgsmaplayerutils.h
  qgsmapsettings.h
  qgsmapsettingsutils.h
  qgsmapthemecollection.h
  qgsmaptopixel.h
  qgsmaptopixelgeometrysimplifier.h
  qgsmapunitscale.h
  qgsmargins.h
  qgsmaskidprovider.h
  qgsmaskpaintdevice.h
  qgsmbtiles.h
  qgsmessagelog.h
  qgsmessageoutput.h
  qgsmimedatautils.h
  qgsmultirenderchecker.h
  qgsnullpainterdevice.h
  qgsobjectcustomproperties.h
  qgsofflineediting.h
  qgsogcutils.h
  qgsogrproxytextcodec.h
  qgsogrutils.h
  qgsoptional.h
  qgsoptionalexpression.h
  qgsowsconnection.h
  qgspaintenginehack.h
  qgspainting.h
  qgspathresolver.h
  qgspluginlayer.h
  qgspluginlayerregistry.h
  qgspointlocator.h
  qgspointlocatorinittask.h
  qgspointxy.h
  qgspostgresstringutils.h
  qgsproperty.h
  qgspropertycollection.h
  qgspropertytransformer.h
  qgsproviderconnectionmodel.h
  qgsproxyfeaturesink.h
  qgsproxyprogresstask.h
  qgspythonrunner.h
  qgsqueryresultmodel.h
  qgsrange.h
  qgsreadwritecontext.h
  qgsreadwritelocker.h
  qgsrecentstylehandler.h
  qgsrelation.h
  qgspolymorphicrelation.h
  qgsrelationcontext.h
  qgsremappingproxyfeaturesink.h
  qgsweakrelation.h
  qgsrelationmanager.h
  qgsrenderchecker.h
  qgsrendercontext.h
  qgsrenderedfeaturehandlerinterface.h
  qgsrendereditemdetails.h
  qgsrunprocess.h
  qgsruntimeprofiler.h
  qgsscalecalculator.h
  qgsscaleutils.h
  qgsselectioncontext.h
  qgsshapegenerator.h
  qgssimplifymethod.h
  qgssingleitemmodel.h
  qgssnappingconfig.h
  qgssnappingutils.h
  qgsspatialindex.h
  qgsspatialindexkdbush.h
  qgsspatialindexkdbushdata.h
  qgsspatialindexutils.h
  qgssourcecache.h
  qgsspatialiteutils.h
  qgssqlexpressioncompiler.h
  qgssqliteutils.h
  qgssqlstatement.h
  qgsstatisticalsummary.h
  qgsstoredexpressionmanager.h
  qgsstringstatisticalsummary.h
  qgsstringutils.h
  qgstablecell.h
  qgstaskmanager.h
  qgstemporalcontroller.h
  qgstemporalnavigationobject.h
  qgstemporalproperty.h
  qgstemporalrangeobject.h
  qgstemporalutils.h
  qgstessellator.h
  qgstestutils.h
  qgsthreadingutils.h
  qgstilecache.h
  qgstiledownloadmanager.h
  qgstiles.h
  qgstolerance.h
  qgstracer.h
  qgstrackedvectorlayertools.h
  qgstransaction.h
  qgstransactiongroup.h
  qgstranslationcontext.h
  qgsunittypes.h
  qgsunsetattributevalue.h
  qgsuserprofile.h
  qgsuserprofilemanager.h
  qgsvariantutils.h
  qgsvector.h
  qgsvector3d.h
  qgsvectorfilewriter.h
  qgsvectorfilewritertask.h
  qgsvectorsimplifymethod.h
  qgsvirtuallayerdefinition.h
  qgsvirtuallayerdefinitionutils.h
  qgsvirtuallayertask.h
  qgswebpage.h
  qgswebview.h
  qgsxmlutils.h
  qgsziputils.h
  qobjectuniqueptr.h
  qgsptpfiles.h

  3d/qgs3drendererregistry.h
  3d/qgs3dsymbolregistry.h
  3d/qgsabstract3dsymbol.h
  3d/qgsabstract3drenderer.h
  3d/qgsabstractpointcloud3drenderer.h

  annotations/qgsannotation.h
  annotations/qgsannotationitem.h
  annotations/qgsannotationitemeditoperation.h
  annotations/qgsannotationitemnode.h
  annotations/qgsannotationitemregistry.h
  annotations/qgsannotationlayer.h
  annotations/qgsannotationlayerrenderer.h
  annotations/qgsannotationlineitem.h
  annotations/qgsannotationmarkeritem.h
  annotations/qgsannotationmanager.h
  annotations/qgsannotationpointtextitem.h
  annotations/qgsannotationpolygonitem.h
  annotations/qgsannotationregistry.h
  annotations/qgshtmlannotation.h
  annotations/qgsrenderedannotationitemdetails.h
  annotations/qgssvgannotation.h
  annotations/qgstextannotation.h

  auth/qgsauthcertutils.h
  auth/qgsauthconfig.h
  auth/qgsauthcrypto.h
  auth/qgsauthmanager.h
  auth/qgsauthmethod.h
  auth/qgsauthmethodmetadata.h
  auth/qgsauthmethodregistry.h

  browser/qgsbrowsermodel.h
  browser/qgsbrowserproxymodel.h
  browser/qgsconnectionsitem.h
  browser/qgsdatabaseschemaitem.h
  browser/qgsdatacollectionitem.h
  browser/qgsdataitem.h
  browser/qgsdataitemprovider.h
  browser/qgsdataitemproviderregistry.h
  browser/qgsdirectoryitem.h
  browser/qgsfavoritesitem.h
  browser/qgsfielddomainsitem.h
  browser/qgsfieldsitem.h
  browser/qgsfilebaseddataitemprovider.h
  browser/qgslayeritem.h
  browser/qgsprojectitem.h
  browser/qgsrelationshipsitem.h
  browser/qgszipitem.h

  callouts/qgscallout.h
  callouts/qgscalloutsregistry.h

  classification/qgsclassificationcustom.h
  classification/qgsclassificationequalinterval.h
  classification/qgsclassificationfixedinterval.h
  classification/qgsclassificationjenks.h
  classification/qgsclassificationlogarithmic.h
  classification/qgsclassificationmethod.h
  classification/qgsclassificationmethodregistry.h
  classification/qgsclassificationprettybreaks.h
  classification/qgsclassificationquantile.h
  classification/qgsclassificationstandarddeviation.h

  diagram/qgsdiagram.h
  diagram/qgshistogramdiagram.h
  diagram/qgspiediagram.h
  diagram/qgsstackedbardiagram.h
  diagram/qgstextdiagram.h

  dxf/qgsdxfexport.h

  editform/qgseditformconfig.h
  editform/qgsattributeeditoraction.h
  editform/qgsattributeeditorelement.h
  editform/qgsattributeeditorcontainer.h
  editform/qgsattributeeditorfield.h
  editform/qgsattributeeditorrelation.h
  editform/qgsattributeeditorhtmlelement.h
  editform/qgsattributeeditorqmlelement.h

  elevation/qgsabstractprofilegenerator.h
  elevation/qgsabstractprofilesurfacegenerator.h
  elevation/qgsabstractprofilesource.h
  elevation/qgsprofilepoint.h
  elevation/qgsprofilerenderer.h
  elevation/qgsprofilerequest.h
  elevation/qgsprofilesnapping.h
  elevation/qgsterrainprovider.h

  externalstorage/qgsexternalstorage.h
  externalstorage/qgsexternalstorageregistry.h

  effects/qgsblureffect.h
  effects/qgscoloreffect.h
  effects/qgseffectstack.h
  effects/qgsgloweffect.h
  effects/qgsimageoperation.h
  effects/qgspainteffect.h
  effects/qgspainteffectregistry.h
  effects/qgsshadoweffect.h
  effects/qgstransformeffect.h

  expression/qgsexpression.h
  expression/qgsexpressioncontextutils.h
  expression/qgsexpressionfunction.h
  expression/qgsexpressionnode.h
  expression/qgsexpressionnodeimpl.h
  expression/qgsexpressionutils.h

  fieldformatter/qgscheckboxfieldformatter.h
  fieldformatter/qgsdatetimefieldformatter.h
  fieldformatter/qgsfallbackfieldformatter.h
  fieldformatter/qgskeyvaluefieldformatter.h
  fieldformatter/qgslistfieldformatter.h
  fieldformatter/qgsrangefieldformatter.h
  fieldformatter/qgsrelationreferencefieldformatter.h
  fieldformatter/qgsvaluemapfieldformatter.h
  fieldformatter/qgsvaluerelationfieldformatter.h

  geocms/geonode/qgsgeonodeconnection.h
  geocms/geonode/qgsgeonoderequest.h

  geocoding/qgsabstractgeocoderlocatorfilter.h
  geocoding/qgsgeocoder.h
  geocoding/qgsgeocodercontext.h
  geocoding/qgsgeocoderresult.h
  geocoding/qgsnominatimgeocoder.h
  geocoding/qgsgooglemapsgeocoder.h

  geometry/qgsabstractgeometry.h
  geometry/qgsbox3d.h
  geometry/qgscircle.h
  geometry/qgscircularstring.h
  geometry/qgscompoundcurve.h
  geometry/qgscurve.h
  geometry/qgscurvepolygon.h
  geometry/qgsellipse.h
  geometry/qgsgeometry.h
  geometry/qgsgeometrycollection.h
  geometry/qgsgeometryeditutils.h
  geometry/qgsgeometryengine.h
  geometry/qgsgeometryfactory.h
  geometry/qgsgeometrytransformer.h
  geometry/qgsgeometryutils.h
  geometry/qgsgeos.h
  geometry/qgsinternalgeometryengine.h
  geometry/qgslinesegment.h
  geometry/qgslinestring.h
  geometry/qgsmulticurve.h
  geometry/qgsmultilinestring.h
  geometry/qgsmultipoint.h
  geometry/qgsmultipolygon.h
  geometry/qgsmultisurface.h
  geometry/qgspoint.h
  geometry/qgspolygon.h
  geometry/qgsquadrilateral.h
  geometry/qgsrectangle.h
  geometry/qgsreferencedgeometry.h
  geometry/qgsregularpolygon.h
  geometry/qgssurface.h
  geometry/qgstriangle.h
  geometry/qgsvertexid.h
  geometry/qgswkbptr.h
  geometry/qgswkbtypes.h
  geometry/qgsray3d.h

  gps/qgsbabelformat.h
  gps/qgsbabelformatregistry.h
  gps/qgsbabelgpsdevice.h
  gps/qgsgpsconnection.h
  gps/qgsgpsdconnection.h
  gps/qgsgpsdetector.h
  gps/qgsnmeaconnection.h

  labeling/qgscalloutposition.h
  labeling/qgslabelfeature.h
  labeling/qgslabeling.h
  labeling/qgslabelingengine.h
  labeling/qgslabelingenginesettings.h
  labeling/qgslabelingresults.h
  labeling/qgslabellinesettings.h
  labeling/qgslabelobstaclesettings.h
  labeling/qgslabelplacementsettings.h
  labeling/qgslabelposition.h
  labeling/qgslabelsearchtree.h
  labeling/qgslabelsink.h
  labeling/qgslabelthinningsettings.h
  labeling/qgspallabeling.h
  labeling/qgsrulebasedlabeling.h
  labeling/qgstextlabelfeature.h
  labeling/qgsvectorlayerlabeling.h
  labeling/qgsvectorlayerlabelprovider.h

  layertree/qgscolorramplegendnode.h
  layertree/qgscolorramplegendnodesettings.h
  layertree/qgslayertree.h
  layertree/qgslayertreegroup.h
  layertree/qgslayertreelayer.h
  layertree/qgslayertreemodel.h
  layertree/qgslayertreemodellegendnode.h
  layertree/qgslayertreenode.h
  layertree/qgslayertreeregistrybridge.h
  layertree/qgslayertreefilterproxymodel.h
  layertree/qgslayertreeutils.h
  layertree/qgslegendpatchshape.h

  layout/qgsabstractlayoutiterator.h
  layout/qgsabstractreportsection.h
  layout/qgscompositionconverter.h
  layout/qgslayout.h
  layout/qgslayoutaligner.h
  layout/qgslayoutatlas.h
  layout/qgslayouteffect.h
  layout/qgslayoutexporter.h
  layout/qgslayoutframe.h
  layout/qgslayoutgeopdfexporter.h
  layout/qgslayoutgridsettings.h
  layout/qgslayoutguidecollection.h
  layout/qgslayoutitem.h
  layout/qgslayoutitemattributetable.h
  layout/qgslayoutitemgroup.h
  layout/qgslayoutitemgroupundocommand.h
  layout/qgslayoutitemhtml.h
  layout/qgslayoutitemlabel.h
  layout/qgslayoutitemlegend.h
  layout/qgslayoutitemmanualtable.h
  layout/qgslayoutitemmap.h
  layout/qgslayoutitemmapgrid.h
  layout/qgslayoutitemmapitem.h
  layout/qgslayoutitemmapoverview.h
  layout/qgslayoutitemmarker.h
  layout/qgslayoutitemnodeitem.h
  layout/qgslayoutitempage.h
  layout/qgslayoutitempicture.h
  layout/qgslayoutitempolygon.h
  layout/qgslayoutitempolyline.h
  layout/qgslayoutitemregistry.h
  layout/qgslayoutitemscalebar.h
  layout/qgslayoutitemshape.h
  layout/qgslayoutitemtexttable.h
  layout/qgslayoutitemundocommand.h
  layout/qgslayoutmanager.h
  layout/qgslayoutmeasurement.h
  layout/qgslayoutmeasurementconverter.h
  layout/qgslayoutmodel.h
  layout/qgslayoutmultiframe.h
  layout/qgslayoutmultiframeundocommand.h
  layout/qgslayoutnortharrowhandler.h
  layout/qgslayoutobject.h
  layout/qgslayoutpagecollection.h
  layout/qgslayoutpoint.h
  layout/qgslayoutrendercontext.h
  layout/qgslayoutreportcontext.h
  layout/qgslayoutserializableobject.h
  layout/qgslayoutsize.h
  layout/qgslayoutsnapper.h
  layout/qgslayouttable.h
  layout/qgslayouttablecolumn.h
  layout/qgslayoutundocommand.h
  layout/qgslayoutundostack.h
  layout/qgslayoututils.h
  layout/qgsmasterlayoutinterface.h
  layout/qgspagesizeregistry.h
  layout/qgsprintlayout.h
  layout/qgsreport.h
  layout/qgsreportsectionfieldgroup.h
  layout/qgsreportsectionlayout.h

  locator/qgslocator.h
  locator/qgslocatorcontext.h
  locator/qgslocatorfilter.h
  locator/qgslocatormodel.h
  locator/qgslocatormodelbridge.h

  maprenderer/qgsmaprenderercache.h
  maprenderer/qgsmaprenderercustompainterjob.h
  maprenderer/qgsmaprendererjob.h
  maprenderer/qgsmaprendererparalleljob.h
  maprenderer/qgsmaprenderersequentialjob.h
  maprenderer/qgsmaprendererstagedrenderjob.h
  maprenderer/qgsmaprenderertask.h
  maprenderer/qgsrendereditemresults.h

  mesh/qgsmesh3daveraging.h
  mesh/qgsmesheditor.h
  mesh/qgsmeshdataprovider.h
  mesh/qgsmeshdataprovidertemporalcapabilities.h
  mesh/qgsmeshdataset.h
  mesh/qgsmeshdatasetgroupstore.h
  mesh/qgsmeshlayer.h
  mesh/qgsmeshlayerelevationproperties.h
  mesh/qgsmeshlayerinterpolator.h
  mesh/qgsmeshlayerprofilegenerator.h
  mesh/qgsmeshlayerrenderer.h
  mesh/qgsmeshlayertemporalproperties.h
  mesh/qgsmeshlayerutils.h
  mesh/qgsmeshrenderersettings.h
  mesh/qgsmeshsimplificationsettings.h
  mesh/qgsmeshspatialindex.h
  mesh/qgsmeshtimesettings.h
  mesh/qgsmeshtracerenderer.h
  mesh/qgsmeshvectorrenderer.h
  mesh/qgstriangularmesh.h
  mesh/qgsmeshcalcnode.h
  mesh/qgsmeshcalculator.h
  mesh/qgsmeshcalcutils.h
  mesh/qgsmeshvirtualdatasetgroup.h
  mesh/qgstopologicalmesh.h
  mesh/qgsmeshadvancedediting.h
  mesh/qgsmeshforcebypolylines.h

  pal/costcalculator.h
  pal/feature.h
  pal/geomfunction.h
  pal/internalexception.h
  pal/labelposition.h
  pal/layer.h
  pal/palexception.h
  pal/pal.h
  pal/palrtree.h
  pal/palstat.h
  pal/pointset.h
  pal/priorityqueue.h
  pal/problem.h
  pal/util.h
  ${PROJECT_SOURCE_DIR}/external/rtree/include/RTree.h

  plot/qgsplot.h

  pointcloud/qgspointcloudattribute.h
  pointcloud/qgspointcloudattributebyramprenderer.h
  pointcloud/qgspointcloudattributemodel.h
  pointcloud/qgspointcloudclassifiedrenderer.h
  pointcloud/qgspointcloudextentrenderer.h
  pointcloud/qgspointcloudrequest.h
  pointcloud/qgspointcloudblock.h
  pointcloud/qgspointcloudblockrequest.h
  pointcloud/qgspointcloudlayer.h
  pointcloud/qgspointcloudlayerelevationproperties.h
  pointcloud/qgspointcloudlayerprofilegenerator.h
  pointcloud/qgspointcloudlayerrenderer.h
  pointcloud/qgspointcloudindex.h
  pointcloud/qgspointclouddataprovider.h
  pointcloud/qgspointcloudrenderer.h
  pointcloud/qgspointcloudrendererregistry.h
  pointcloud/qgspointcloudrgbrenderer.h
  pointcloud/qgspointcloudlayerexporter.h

  pointcloud/expression/qgspointcloudexpression.h
  pointcloud/expression/qgspointcloudexpressionnode.h
  pointcloud/expression/qgspointcloudexpressionnodeimpl.h

  pointcloud/qgspointcloudstatscalculator.h
  pointcloud/qgspointcloudstatscalculationtask.h
  pointcloud/qgspointcloudstatistics.h

  proj/qgscelestialbody.h
  proj/qgscoordinatereferencesystem.h
  proj/qgscoordinatereferencesystemregistry.h
  proj/qgscoordinatereferencesystemutils.h
  proj/qgscoordinatetransform.h
  proj/qgscoordinatetransformcontext.h
  proj/qgsdatums.h
  proj/qgsdatumtransform.h
  proj/qgsellipsoidutils.h
  proj/qgsprojoperation.h
  proj/qgsprojutils.h
  proj/qgsprojectionfactors.h

  metadata/qgsabstractmetadatabase.h
  metadata/qgslayermetadata.h
  metadata/qgslayermetadataformatter.h
  metadata/qgslayermetadatavalidator.h
  metadata/qgsmetadatautils.h
  metadata/qgsprojectmetadata.h
  metadata/qgsabstractlayermetadataprovider.h
  metadata/qgslayermetadataproviderregistry.h

  network/qgsblockingnetworkrequest.h
  network/qgsfiledownloader.h
  network/qgsnetworkaccessmanager.h
  network/qgsnetworkcontentfetcher.h
  network/qgsnetworkcontentfetcherregistry.h
  network/qgsnetworkcontentfetchertask.h
  network/qgsnetworkdiskcache.h
  network/qgsnetworkreply.h
  network/qgsnetworkreplyparser.h
  network/qgsnewsfeedmodel.h
  network/qgsnewsfeedparser.h
  network/qgshttpheaders.h
  network/qgsrangerequestcache.h

  numericformats/qgsbasicnumericformat.h
  numericformats/qgsbearingnumericformat.h
  numericformats/qgscoordinatenumericformat.h
  numericformats/qgscurrencynumericformat.h
  numericformats/qgsfallbacknumericformat.h
  numericformats/qgsfractionnumericformat.h
  numericformats/qgsnumericformat.h
  numericformats/qgsnumericformatregistry.h
  numericformats/qgspercentagenumericformat.h
  numericformats/qgsscientificnumericformat.h

  processing/models/qgsprocessingmodelalgorithm.h
  processing/models/qgsprocessingmodelchildalgorithm.h
  processing/models/qgsprocessingmodelchilddependency.h
  processing/models/qgsprocessingmodelchildparametersource.h
  processing/models/qgsprocessingmodelcomment.h
  processing/models/qgsprocessingmodelcomponent.h
  processing/models/qgsprocessingmodelgroupbox.h
  processing/models/qgsprocessingmodeloutput.h
  processing/models/qgsprocessingmodelparameter.h
  processing/qgsprocessing.h
  processing/qgsprocessingalgorithm.h
  processing/qgsprocessingalgrunnertask.h
  processing/qgsprocessingbatch.h
  processing/qgsprocessingcontext.h
  processing/qgsprocessingfeedback.h
  processing/qgsprocessingoutputs.h
  processing/qgsprocessingparameteraggregate.h
  processing/qgsprocessingparameterdxflayers.h
  processing/qgsprocessingparameterfieldmap.h
  processing/qgsprocessingparametermeshdataset.h
  processing/qgsprocessingparameters.h
  processing/qgsprocessingparametertininputlayers.h
  processing/qgsprocessingparametertype.h
  processing/qgsprocessingparametertypeimpl.h
  processing/qgsprocessingparametervectortilewriterlayers.h
  processing/qgsprocessingprovider.h
  processing/qgsprocessingregistry.h
  processing/qgsprocessingutils.h

  project/qgsproject.h
  project/qgsprojectbadlayerhandler.h
  project/qgsprojectdisplaysettings.h
  project/qgsprojectelevationproperties.h
  project/qgsprojectfiletransform.h
  project/qgsprojectproperty.h
  project/qgsprojectservervalidator.h
  project/qgsprojectstorage.h
  project/qgsprojectstorageregistry.h
  project/qgsprojectstylesettings.h
  project/qgsprojecttimesettings.h
  project/qgsprojecttranslator.h
  project/qgsprojectutils.h
  project/qgsprojectversion.h
  project/qgsprojectviewsettings.h
  project/qgsmapviewsmanager.h

  providers/qgsabstractdbtablemodel.h
  providers/qgsabstractdatabaseproviderconnection.h
  providers/qgsabstractproviderconnection.h
  providers/qgsdataprovider.h
  providers/qgsprovidermetadata.h
  providers/qgsproviderregistry.h
  providers/qgsprovidersqlquerybuilder.h
  providers/qgsprovidersublayerdetails.h
  providers/qgsprovidersublayermodel.h
  providers/qgsprovidersublayertask.h
  providers/qgsproviderutils.h

  providers/arcgis/qgsarcgisportalutils.h
  providers/arcgis/qgsarcgisrestquery.h
  providers/arcgis/qgsarcgisrestutils.h

  providers/gdal/qgsgdalprovider.h

  providers/memory/qgsmemoryfeatureiterator.h
  providers/memory/qgsmemoryprovider.h
  providers/memory/qgsmemoryproviderutils.h

  providers/meshmemory/qgsmeshmemorydataprovider.h

  providers/ogr/qgsogrlayermetadataprovider.h
  providers/ogr/qgsgeopackagedataitems.h
  providers/ogr/qgsgeopackageprojectstorage.h
  providers/ogr/qgsgeopackageproviderconnection.h
  providers/ogr/qgsgeopackagerasterwritertask.h
  providers/ogr/qgsogrconnpool.h
  providers/ogr/qgsogrdbconnection.h
  providers/ogr/qgsogrprovider.h
  providers/ogr/qgsogrproviderconnection.h
  providers/ogr/qgsogrprovidermetadata.h
  providers/ogr/qgsogrproviderutils.h
  providers/ogr/qgsogrtransaction.h

  raster/qgsbilinearrasterresampler.h
  raster/qgsbrightnesscontrastfilter.h
  raster/qgscliptominmaxenhancement.h
  raster/qgscolorrampshader.h
  raster/qgscontrastenhancement.h
  raster/qgscontrastenhancementfunction.h
  raster/qgscubicrasterresampler.h
  raster/qgsexiftools.h
  raster/qgshillshaderenderer.h
  raster/qgshuesaturationfilter.h
  raster/qgslinearminmaxenhancement.h
  raster/qgslinearminmaxenhancementwithclip.h
  raster/qgsmultibandcolorrenderer.h
  raster/qgspalettedrasterrenderer.h
  raster/qgsraster.h
  raster/qgsrasterbandstats.h
  raster/qgsrasterblock.h
  raster/qgsrasterchecker.h
  raster/qgsrastercontourrenderer.h
  raster/qgsrasterdataprovider.h
  raster/qgsrasterdataprovidertemporalcapabilities.h
  raster/qgsrasterdrawer.h
  raster/qgsrasterfilewriter.h
  raster/qgsrasterfilewritertask.h
  raster/qgsrasterhistogram.h
  raster/qgsrasteridentifyresult.h
  raster/qgsrasterinterface.h
  raster/qgsrasteriterator.h
  raster/qgsrasterlayer.h
  raster/qgsrasterlayerelevationproperties.h
  raster/qgsrasterlayerprofilegenerator.h
  raster/qgsrasterlayerrenderer.h
  raster/qgsrasterlayertemporalproperties.h
  raster/qgsrasterminmaxorigin.h
  raster/qgsrasternuller.h
  raster/qgsrasterpipe.h
  raster/qgsrasterprojector.h
  raster/qgsrasterpyramid.h
  raster/qgsrasterrange.h
  raster/qgsrasterrenderer.h
  raster/qgsrasterrendererregistry.h
  raster/qgsrasterrendererutils.h
  raster/qgsrasterresamplefilter.h
  raster/qgsrasterresampler.h
  raster/qgsrastershader.h
  raster/qgsrastershaderfunction.h
  raster/qgsrastertransparency.h
  raster/qgsrasterviewport.h
  raster/qgssinglebandcolordatarenderer.h
  raster/qgssinglebandgrayrenderer.h
  raster/qgssinglebandpseudocolorrenderer.h

  scalebar/qgsdoubleboxscalebarrenderer.h
  scalebar/qgshollowscalebarrenderer.h
  scalebar/qgsnumericscalebarrenderer.h
  scalebar/qgsscalebarrenderer.h
  scalebar/qgsscalebarrendererregistry.h
  scalebar/qgsscalebarsettings.h
  scalebar/qgssingleboxscalebarrenderer.h
  scalebar/qgssteppedlinescalebarrenderer.h
  scalebar/qgsticksscalebarrenderer.h

  symbology/qgs25drenderer.h
  symbology/qgsarrowsymbollayer.h
  symbology/qgscategorizedsymbolrenderer.h
  symbology/qgscolorbrewerpalette.h
  symbology/qgscombinedstylemodel.h
  symbology/qgscptcityarchive.h
  symbology/qgsellipsesymbollayer.h
  symbology/qgsembeddedsymbolrenderer.h
  symbology/qgsfeaturerenderergenerator.h
  symbology/qgsfillsymbol.h
  symbology/qgsfillsymbollayer.h
  symbology/qgsgeometrygeneratorsymbollayer.h
  symbology/qgsgraduatedsymbolrenderer.h
  symbology/qgsheatmaprenderer.h
  symbology/qgsinterpolatedlinerenderer.h
  symbology/qgsinvertedpolygonrenderer.h
  symbology/qgslegendsymbolitem.h
  symbology/qgslinesymbol.h
  symbology/qgslinesymbollayer.h
  symbology/qgsmapinfosymbolconverter.h
  symbology/qgsmarkersymbol.h
  symbology/qgsmarkersymbollayer.h
  symbology/qgsmergedfeaturerenderer.h
  symbology/qgsnullsymbolrenderer.h
  symbology/qgspointclusterrenderer.h
  symbology/qgspointdisplacementrenderer.h
  symbology/qgspointdistancerenderer.h
  symbology/qgsrenderer.h
  symbology/qgsrendererrange.h
  symbology/qgsrendererregistry.h
  symbology/qgsrulebasedrenderer.h
  symbology/qgssinglesymbolrenderer.h
  symbology/qgsstyle.h
  symbology/qgsstyleentityvisitor.h
  symbology/qgsstylemodel.h
  symbology/qgssvgcache.h
  symbology/qgssymbol.h
  symbology/qgssymbollayer.h
  symbology/qgssymbollayerreference.h
  symbology/qgssymbollayerregistry.h
  symbology/qgssymbollayerutils.h
  symbology/qgssymbolrendercontext.h
  symbology/qgsvectorfieldsymbollayer.h

  textrenderer/qgsfontmanager.h
  textrenderer/qgstextbackgroundsettings.h
  textrenderer/qgstextblock.h
  textrenderer/qgstextbuffersettings.h
  textrenderer/qgstextcharacterformat.h
  textrenderer/qgstextdocument.h
  textrenderer/qgstextdocumentmetrics.h
  textrenderer/qgstextformat.h
  textrenderer/qgstextfragment.h
  textrenderer/qgstextmasksettings.h
  textrenderer/qgstextmetrics.h
  textrenderer/qgstextrenderer.h
  textrenderer/qgstextrendererutils.h
  textrenderer/qgstextshadowsettings.h

  settings/qgssettings.h
  settings/qgssettingsentry.h
  settings/qgssettingsentryenumflag.h
  settings/qgssettingsentryimpl.h
  settings/qgssettingsregistry.h
  settings/qgssettingsregistrycore.h

  validity/qgsabstractvaliditycheck.h
  validity/qgsvaliditycheckcontext.h
  validity/qgsvaliditycheckregistry.h

  vector/qgsfielddomain.h
  vector/qgsvectordataprovider.h
  vector/qgsvectordataprovidertemporalcapabilities.h
  vector/qgsvectorlayer.h
  vector/qgsvectorlayerref.h
  vector/qgsvectorlayercache.h
  vector/qgsvectorlayerdiagramprovider.h
  vector/qgsvectorlayereditbuffer.h
  vector/qgsvectorlayereditbuffergroup.h
  vector/qgsvectorlayereditpassthrough.h
  vector/qgsvectorlayereditutils.h
  vector/qgsvectorlayerelevationproperties.h
  vector/qgsvectorlayerexporter.h
  vector/qgsvectorlayerfeaturecounter.h
  vector/qgsvectorlayerfeatureiterator.h
  vector/qgsvectorlayerjoinbuffer.h
  vector/qgsvectorlayerjoininfo.h
  vector/qgsvectorlayerprofilegenerator.h
  vector/qgsvectorlayerrenderer.h
  vector/qgsvectorlayertemporalproperties.h
  vector/qgsvectorlayertools.h
  vector/qgsvectorlayerundocommand.h
  vector/qgsvectorlayerundopassthroughcommand.h
  vector/qgsvectorlayerutils.h

  vectortile/qgsmapboxglstyleconverter.h
  vectortile/qgsvectortilebasiclabeling.h
  vectortile/qgsvectortilebasicrenderer.h
  vectortile/qgsvectortileconnection.h
  vectortile/qgsvectortiledataitems.h
  vectortile/qgsvectortilelabeling.h
  vectortile/qgsvectortilelayer.h
  vectortile/qgsvectortilelayerrenderer.h
  vectortile/qgsvectortileloader.h
  vectortile/qgsvectortilematrixset.h
  vectortile/qgsvectortilemvtdecoder.h
  vectortile/qgsvectortilemvtencoder.h
  vectortile/qgsvectortilemvtutils.h
  vectortile/qgsvectortileprovidermetadata.h
  vectortile/qgsvectortilerenderer.h
  vectortile/qgsvectortileutils.h
  vectortile/qgsvectortilewriter.h
  vectortile/qgsvtpktiles.h
)

#[[
杨小兵-2024-03-04
一、解释
  设置QGIS_CORE_PRIVATE_HDRS变量，其中包含qgis_core项目需要的私有头文件（只有qgis_core需要的头文件）
]]
set(QGIS_CORE_PRIVATE_HDRS
  qgsexpressionsorter_p.h
  qgsfeature_p.h
  qgsfield_p.h
  qgsfields_p.h
  qgsmaplayerlistutils_p.h
  qgspolymorphicrelation_p.h
  qgsproperty_p.h
  qgsrelation_p.h
  qgsspatialindexkdbush_p.h

  dxf/qgsdxfexport_p.h

  editform/qgseditformconfig_p.h

  expression/qgsexpression_p.h

  externalstorage/qgssimplecopyexternalstorage_p.h
  externalstorage/qgswebdavexternalstorage_p.h

  proj/qgscoordinatereferencesystem_p.h
  proj/qgscoordinatetransformcontext_p.h
  proj/qgscoordinatetransform_p.h
  proj/qgscoordinatereferencesystem_legacy_p.h

  providers/ogr/qgscplerrorhandler_p.h

  textrenderer/qgstextrenderer_p.h
)

#[[
杨小兵-2024-03-04
一、解释
  如果没有设置WITH_QTWEBKIT变量，那么需要向QGIS_CORE_HDRS添加一个额外的头文件
]]
if (NOT WITH_QTWEBKIT)
  set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS}
    qgswebframe.h
  )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果设置${QT_VERSION_BASE}Positioning_FOUND变量，那么需要向QGIS_CORE_HDRS添加一个额外的头文件
]]
if (${QT_VERSION_BASE}Positioning_FOUND)
  set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS}
    gps/qgsqtlocationconnection.h
  )
endif()


#[[
杨小兵-2024-03-04
一、解释
  如果设置HAVE_OPENCL变量，那么需要向QGIS_CORE_HDRS添加一个额外的头文件
]]
if (HAVE_OPENCL)
    set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS}
        qgsopenclutils.h
    )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果设置了MSVC变量，那么需要向特定的文件添加文件属性，如果没有设置MSVC则需要向其他一些文件设置属性
]]
if(MSVC)
  set_source_files_properties(
      ${CMAKE_CURRENT_BINARY_DIR}/qgsexpression_texts.cpp
      proj/qgscoordinatereferencesystem.cpp
      PROPERTIES COMPILE_FLAGS "/bigobj"
  )
else()
  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" AND (CMAKE_BUILD_TYPE MATCHES Release OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo))
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/qgsexpression_texts.cpp PROPERTIES COMPILE_FLAGS "-O1")
    set_source_files_properties(proj/qgscoordinatereferencesystem.cpp PROPERTIES COMPILE_FLAGS "-mllvm -inline-threshold=128")
  endif()
endif()

#[[
杨小兵-2024-03-04
一、解释
  需要使用protoc工具生成头文件，这个头文件用来支持vector tiles的MVT编码

二、总结
1、protoc是一个工具
2、protoc这个工具可以用来生成头文件
3、protoc这个工具的输入是.proto文件，输出是.hpp文件
]]
# Generate cpp+header file from .proto file using "protoc" tool (to support MVT encoding of vector tiles)
protobuf_generate_cpp(VECTOR_TILE_PROTO_SRCS VECTOR_TILE_PROTO_HDRS vectortile/vector_tile.proto)
set(QGIS_CORE_SRCS ${QGIS_CORE_SRCS} ${VECTOR_TILE_PROTO_SRCS})
set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS} ${VECTOR_TILE_PROTO_HDRS})
if (NOT MSVC)
  # automatically generated file produces warnings (unused-parameter, unused-variable, misleading-indentation)
  set_source_files_properties(${VECTOR_TILE_PROTO_SRCS} PROPERTIES COMPILE_FLAGS -w)
endif()


#[[
杨小兵-2024-03-04
一、解释
  安装头文件，用来支持OS X的框架目标
]]
# install headers
# install qgsconfig.h and plugin.h here so they can get into
# the OS X framework target
if (${QT_VERSION_BASE}Positioning_FOUND)
  set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS}
    gps/qgsqtlocationconnection.h
    gps/qgsgpsconnectionregistry.h
  )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，则添加头文件的搜索路径
]]
if (WITH_PDAL)
  include_directories(SYSTEM
    ${PDAL_INCLUDE_DIR}
  )
  if(MSVC)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
  endif()
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，则添加头文件的搜索路径
]]
if (HAVE_OPENCL)
    set(QGIS_CORE_SRCS ${QGIS_CORE_SRCS}
        qgsopenclutils.cpp
    )
    set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS}
        qgsopenclutils.h
    )
    include_directories(SYSTEM ${OpenCL_INCLUDE_DIRS})
endif()


#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，则添加头文件的搜索路径
]]
if (WITH_EPT)
  include_directories(providers/ept)

  include_directories(SYSTEM
    ${ZSTD_INCLUDE_DIR}
  )

  set(QGIS_CORE_SRCS ${QGIS_CORE_SRCS}
      providers/ept/qgseptprovider.cpp
      pointcloud/qgseptpointcloudindex.cpp
      pointcloud/qgsremoteeptpointcloudindex.cpp
      pointcloud/qgseptpointcloudblockrequest.cpp
  )
  set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS}
      providers/ept/qgseptprovider.h
      pointcloud/qgseptpointcloudindex.h
      pointcloud/qgsremoteeptpointcloudindex.h
      pointcloud/qgseptpointcloudblockrequest.h
  )

  add_definitions( -DWITH_EPT )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，则添加头文件的搜索路径
]]
if (WITH_COPC)
  include_directories(providers/copc)

  set(QGIS_CORE_SRCS ${QGIS_CORE_SRCS}
      providers/copc/qgscopcprovider.cpp
      pointcloud/qgscopcpointcloudindex.cpp
      pointcloud/qgsremotecopcpointcloudindex.cpp
      pointcloud/qgscopcpointcloudblockrequest.cpp
  )
  set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS}
      providers/copc/qgscopcprovider.h
      pointcloud/qgscopcpointcloudindex.h
      pointcloud/qgsremotecopcpointcloudindex.h
      pointcloud/qgscopcpointcloudblockrequest.h
  )

  add_definitions( -DWITH_COPC )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，则添加头文件的搜索路径
]]
if (WITH_EPT OR WITH_COPC)
  if (LazPerf_FOUND)
  # Use system laz-perf
  include_directories(SYSTEM
    ${LazPerf_INCLUDE_DIR}
  )
  else()
  # Use embedded laz-perf from external/laz-perf
  include_directories(SYSTEM
  )

  set(QGIS_CORE_SRCS ${QGIS_CORE_SRCS}
    ${CMAKE_SOURCE_DIR}/external/lazperf/charbuf.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/filestream.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/header.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/lazperf.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/readers.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/vlr.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/detail/field_byte10.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/detail/field_byte14.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/detail/field_gpstime10.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/detail/field_nir14.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/detail/field_point10.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/detail/field_point14.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/detail/field_rgb10.cpp
    ${CMAKE_SOURCE_DIR}/external/lazperf/detail/field_rgb14.cpp
  )
  endif()

  set(QGIS_CORE_SRCS ${QGIS_CORE_SRCS}
      pointcloud/qgseptdecoder.cpp
      pointcloud/qgslazdecoder.cpp
      pointcloud/qgslazinfo.cpp
  )
  set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS}
      pointcloud/qgseptdecoder.h
      pointcloud/qgslazdecoder.h
      pointcloud/qgslazinfo.h
  )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，则添加头文件的搜索路径
]]
if (APPLE)
  # Libtasn1 is for DER-encoded PKI ASN.1 parsing/extracting workarounds
  include_directories(SYSTEM
    ${LIBTASN1_INCLUDE_DIR}
  )
endif()


#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，则设置文件的预处理器选项
]]
#for PAL classes
if (WIN32)
    add_definitions(-D_HAVE_WINDOWS_H_)
else()
    add_definitions(-D_HAVE_PTHREAD_)
endif()

# Test data dir for QgsRenderChecker
add_definitions(-DTEST_DATA_DIR="${TEST_DATA_DIR}")

set(IMAGE_RCCS ../../images/images.qrc)




















#############################################################
#[[
杨小兵-2024-03-04
一、解释
  前面的内容设置了*.cpp文件和*.h文件，通过不同的操作系统平台、编译器设置一些特定文件的属性，基本上就是包含了.cpp文件和.hpp文件，现在
将会构建qgis_core库
]]
# qgis_core library
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(qgis_core ${LIBRARY_TYPE} ${QGIS_CORE_SRCS} ${QGIS_CORE_HDRS} ${QGIS_CORE_PRIVATE_HDRS} ${IMAGE_RCCS})


# require c++17
target_compile_features(qgis_core PRIVATE cxx_std_17)

#[[
杨小兵-2024-03-04
一、解释
  根据不同的编译器设置qgis_core动态链接库的一些编译选项
]]
# Exiv still uses std::auto_ptr
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_definitions(qgis_core PUBLIC _LIBCPP_ENABLE_CXX17_REMOVED_AUTO_PTR)
endif()
if (MSVC)
  target_compile_definitions(qgis_core PUBLIC _HAS_AUTO_PTR_ETC=1)
endif()


#[[
杨小兵-2024-03-04
一、解释
  为qgis_core添加一些头文件的搜索路径
]]
target_include_directories(qgis_core SYSTEM PUBLIC
  ${${QT_VERSION_BASE}Core_INCLUDE_DIRS}
  ${${QT_VERSION_BASE}Gui_INCLUDE_DIRS}
  ${${QT_VERSION_BASE}Xml_INCLUDE_DIRS}
  ${${QT_VERSION_BASE}Widgets_INCLUDE_DIRS}
  ${${QT_VERSION_BASE}Svg_INCLUDE_DIRS}
  ${${QT_VERSION_BASE}PrintSupport_INCLUDE_DIRS}
  ${${QT_VERSION_BASE}Network_INCLUDE_DIRS}
  ${${QT_VERSION_BASE}Sql_INCLUDE_DIRS}
  ${${QT_VERSION_BASE}Concurrent_INCLUDE_DIRS}
  ${PROJ_INCLUDE_DIR}
  ${GDAL_INCLUDE_DIR}
  ${LIBZIP_INCLUDE_DIRS}
  ${SPATIALINDEX_INCLUDE_DIR} # before GEOS for case-insensitive filesystems
  ${GEOS_INCLUDE_DIR}
  ${EXPAT_INCLUDE_DIR}
  ${SQLITE3_INCLUDE_DIR}
  ${QCA_INCLUDE_DIR}
  ${QTKEYCHAIN_INCLUDE_DIR}
  ${${QT_VERSION_BASE}SerialPort_INCLUDE_DIRS}
  ${Protobuf_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
  ${EXIV2_INCLUDE_DIR}
)


#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，那么将会包含头文件的搜索路径
]]
if (WITH_SPATIALITE)
  target_include_directories(qgis_core SYSTEM PUBLIC
    ${SPATIALITE_INCLUDE_DIR}
  )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，那么将会包含头文件的搜索路径
]]
target_include_directories(qgis_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  3d
  annotations
  auth
  browser
  callouts
  classification
  diagram
  dxf
  editform
  effects
  elevation
  expression
  externalstorage
  fieldformatter
  geometry
  geocms
  geocms/geonode
  geocoding
  gps
  labeling
  layertree
  layout
  locator
  maprenderer
  mesh
  metadata
  network
  numericformats
  pal
  plot
  pointcloud
  pointcloud/expression
  processing
  processing/models
  proj
  project
  providers
  providers/arcgis
  providers/memory
  providers/gdal
  providers/ogr
  providers/meshmemory
  raster
  renderer
  scalebar
  settings
  symbology
  textrenderer
  validity
  vector
  vectortile
  ${CMAKE_SOURCE_DIR}/external
  ${CMAKE_SOURCE_DIR}/external/nlohmann
  ${CMAKE_SOURCE_DIR}/external/kdbush/include
  ${CMAKE_SOURCE_DIR}/external/nmea
  ${CMAKE_SOURCE_DIR}/external/rtree/include
  ${CMAKE_SOURCE_DIR}/external/meshOptimizer

  # 杨小兵-2024-07-29:包含ptp相关的头文件
  ${CMAKE_SOURCE_DIR}/src/starmap_src/geo_algorithm_h
)

#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，那么将会包含头文件的搜索路径
]]
if (WITH_EPT)
  target_include_directories(qgis_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src/core/providers/ept)
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，那么将会包含头文件的搜索路径
]]
if (WITH_COPC)
  target_include_directories(qgis_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src/core/providers/copc)
endif()


#[[
杨小兵-2024-03-04
一、解释
  生成qgis_core动态链接库（一般是动态链接库）的导出头文件
]]
GENERATE_EXPORT_HEADER(
  qgis_core
  BASE_NAME CORE
  EXPORT_FILE_NAME qgis_core.h
)

#[[
杨小兵-2024-03-04
一、解释
  添加头文件
]]
set(QGIS_CORE_HDRS ${QGIS_CORE_HDRS} ${CMAKE_CURRENT_BINARY_DIR}/qgis_core.h)

#[[
杨小兵-2024-03-04
一、解释
  添加第三方库的测试库
]]
if(ENABLE_MODELTEST)
  target_link_libraries(qgis_core ${QT_VERSION_BASE}::Test)
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果没有设置标志，则添加第三方库
]]
if (NOT WITH_INTERNAL_POLY2TRI)
   target_link_libraries(qgis_core ${Poly2Tri_LIBRARY})
endif()


#[[
杨小兵-2024-03-04
一、解释
  如果设置了标志，则添加第三方库
]]
if(HAVE_OPENCL)
  target_link_libraries(qgis_core ${OpenCL_LIBRARY})
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果没有设置APPLE或者没有设置QGIS_MACAPP_FRAMEWORK标志，则将QGIS_CORE_HDRS头文件输出到构建目录中
]]
if(NOT APPLE OR NOT QGIS_MACAPP_FRAMEWORK)
  install(FILES ${QGIS_CORE_HDRS} DESTINATION ${QGIS_INCLUDE_DIR})
else()

  set_target_properties(qgis_core PROPERTIES
    CLEAN_DIRECT_OUTPUT 1
    FRAMEWORK 1
    FRAMEWORK_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}"
    MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_SOURCE_DIR}/mac/framework.info.plist.in"
    MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${COMPLETE_VERSION}
    MACOSX_FRAMEWORK_IDENTIFIER org.qgis.qgis3_core
    BUILD_WITH_INSTALL_RPATH TRUE
    PUBLIC_HEADER "${QGIS_CORE_HDRS}"
    LINK_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}"
  )
  # generated export header does not get copied with PUBLIC_HEADER files
  add_custom_command(TARGET qgis_core
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy qgis_core.h
            "${QGIS_OUTPUT_DIRECTORY}/${QGIS_LIB_SUBDIR}/qgis_core.framework/Headers"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS qgis_core.h
  )
endif()


#[[
杨小兵-2024-03-04
一、解释
  设置android的版本信息
]]
#generate unversioned libs for android
if (NOT ANDROID)
  set_target_properties(qgis_core PROPERTIES
    VERSION ${COMPLETE_VERSION}
    SOVERSION ${COMPLETE_VERSION}
    )
endif()


#[[
杨小兵-2024-03-04
一、解释
  确保version库是存在的
]]
# make sure to create qgsversion.h before compiling
add_dependencies(qgis_core version)


#[[
杨小兵-2024-03-04
一、解释
  如果设置了WIN32则需要找到一些底层的库文件
]]
# because of htonl
if (WIN32)
  find_library(SETUPAPI_LIBRARY NAMES setupapi PATHS $ENV{LIB})
  find_library(VERSION_LIBRARY NAMES version PATHS $ENV{LIB})
  target_link_libraries(qgis_core wsock32 ${SETUPAPI_LIBRARY} dbghelp ${VERSION_LIBRARY})
endif()

if (APPLE)
  target_link_libraries(qgis_core ${LIBTASN1_LIBRARY})
endif()


#[[
杨小兵-2024-03-04
一、解释
  qgis_core将会依赖QT中的一些库，例如Core、Gui等等
]]
target_link_libraries(qgis_core
  ${QT_VERSION_BASE}::Core
  ${QT_VERSION_BASE}::Gui
  ${QT_VERSION_BASE}::Xml
  ${QT_VERSION_BASE}::Widgets
  ${QT_VERSION_BASE}::Svg
  ${QT_VERSION_BASE}::Network
  ${QT_VERSION_BASE}::Sql
  ${QT_VERSION_BASE}::Concurrent
  ${OPTIONAL_QTWEBKIT}
  ${QCA_LIBRARY}
  ${QTKEYCHAIN_LIBRARY}
  ${PROJ_LIBRARY}
  ${GEOS_LIBRARY}
  ${GDAL_LIBRARY}
  ${SPATIALINDEX_LIBRARY}
  ${EXPAT_LIBRARY}
  ${SQLITE3_LIBRARY}
  ${LIBZIP_LIBRARY}
  ${Protobuf_LITE_LIBRARY}
  ${ZLIB_LIBRARIES}
  ${EXIV2_LIBRARY}
  # 杨小兵-2024-07-29:链接libptp
  ${CMAKE_SOURCE_DIR}/external/starmap_external/libptp/lib/windows/libmapboom_base_lib_1.52.lib
)

#[[
杨小兵-2024-03-04
一、解释
  如果不是IOS平台，还需要链接QT中的第三方库
]]
if (NOT IOS)
  target_link_libraries(qgis_core ${QT_VERSION_BASE}::PrintSupport)
endif()


#[[
杨小兵-2024-03-04
一、解释
  如果标志存在，还需要链接第三方库
]]
if (WITH_SPATIALITE)
  target_link_libraries(qgis_core ${SPATIALITE_LIBRARY})
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果标志存在，还需要链接第三方库
]]
if (BUILD_WITH_QT6)
  target_link_libraries(qgis_core Qt6::Core5Compat)
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果标志存在，还需要链接第三方库
]]
if (WITH_EPT)
  target_link_libraries(qgis_core
    ${ZSTD_LIBRARY}
  )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果标志存在，还需要链接第三方库
]]
if (WITH_EPT OR WITH_COPC)
  if (LazPerf_FOUND)
    target_link_libraries(qgis_core ${LazPerf_LIBRARY})
  endif()
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果标志存在，还需要链接第三方库
]]
if (WITH_PDAL)
  target_link_libraries(qgis_core
    ${PDAL_LIBRARIES}
  )
endif()

#[[
杨小兵-2024-03-04
一、解释
  如果标志存在，还需要链接第三方库
]]
if (FORCE_STATIC_LIBS)
  target_link_libraries(qgis_core
    provider_wms_a
    provider_wcs_a
    provider_delimitedtext_a
    provider_arcgisfeatureserver_a
    provider_arcgismapserver_a
  )

  
  if (HAVE_SPATIALITE)
    target_link_libraries(qgis_core
      provider_spatialite_a
      provider_wfs_a
      provider_virtuallayer_a
    )
  endif()
  if (HAVE_POSTGRESQL)
    target_link_libraries(qgis_core
      provider_postgres_a
    )
  endif()

  if (WITH_AUTH)
    target_link_libraries(qgis_core
      authmethod_basic_a
      authmethod_esritoken_a
      authmethod_identcert_a
      authmethod_pkipaths_a
      authmethod_pkcs12_a
    )
    if(WITH_OAUTH2_PLUGIN)
      target_link_libraries(qgis_core authmethod_oauth2_a)
    endif()
  endif()
endif()

  
#[[
杨小兵-2024-03-04
一、解释
  如果是MSVC编译器，还需要链接odbc32 odbccp32，因为gdal将会需要odbc库
]]
if (MSVC)
  #needed for linking to gdal which needs odbc
  set(TARGET_LINK_LIBRARIES qgis_core odbc32 odbccp32)
endif()


#[[
杨小兵-2024-03-04
一、解释
  如果标志存在，还需要链接第三方库
]]
if (${QT_VERSION_BASE}SerialPort_FOUND)
  target_link_libraries(qgis_core
    ${QT_VERSION_BASE}::SerialPort
  )
endif()


#[[
杨小兵-2024-03-04
一、解释
  如果标志存在，还需要链接第三方库
]]
if (${QT_VERSION_BASE}Positioning_FOUND)
  target_link_libraries(qgis_core
    ${QT_VERSION_BASE}::Positioning
  )
endif()



#[[
杨小兵-2024-03-04
一、解释
  为qgis_core添加预处理器选项
]]
target_compile_definitions(qgis_core PRIVATE "-DQT_NO_FOREACH")


#[[
杨小兵-2024-03-04
一、解释
  如果CLANG_TIDY_EXE工具存在，需要使用CLANG_TIDY_EXE这个工具设置qgis_core的属性，用来检查代码
]]
# clang-tidy
if(CLANG_TIDY_EXE)
  set_target_properties(
    qgis_core PROPERTIES
    CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
  )
endif()


#[[
杨小兵-2024-03-04
一、解释
  设置这个库的安装位置
]]
# install
install(TARGETS qgis_core
  RUNTIME DESTINATION ${QGIS_BIN_DIR}
  LIBRARY DESTINATION ${QGIS_LIB_DIR}
  ARCHIVE DESTINATION ${QGIS_LIB_DIR}
  FRAMEWORK DESTINATION ${QGIS_FW_SUBDIR}
  PUBLIC_HEADER DESTINATION ${QGIS_INCLUDE_DIR})

#[[
杨小兵-2024-03-04
一、解释
  如果标志存在，则需要设置安装位置
]]
# Mac dev frameworks
if (APPLE AND QGIS_MACAPP_INSTALL_DEV)
  install(TARGETS qgis_core FRAMEWORK DESTINATION ${QGIS_MACAPP_DEV_PREFIX})
  install(CODE "execute_process(COMMAND install_name_tool -id \"${QGIS_MACAPP_DEV_PREFIX}/qgis_core.framework/Versions/${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}/qgis_core\" \"$ENV{DESTDIR}${QGIS_MACAPP_DEV_PREFIX}/qgis_core.framework/qgis_core\")")
endif()
