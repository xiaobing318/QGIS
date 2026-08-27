"""QGIS unit tests for QCopilots Processing MCP tools.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

__author__ = "OpenAI"
__date__ = "2026-07-12"
__copyright__ = "Copyright 2026, The QGIS Project"

import sys
import types
import unittest
from unittest import mock


class FakeProcessingDefinition:
    def __init__(self, name, definition_type, *, destination=False, options=None):
        self._name = name
        self._type = definition_type
        self._destination = destination
        self._options = options

    def name(self):
        return self._name

    def description(self):
        return self._name.replace("_", " ").title()

    def type(self):
        return self._type

    def isDestination(self):
        return self._destination

    def defaultValue(self):
        return None

    def flags(self):
        return 0

    def options(self):
        return self._options


class FakeProcessingAlgorithm:
    def __init__(
        self,
        algorithm_id,
        display_name,
        group_id,
        tags,
        parameters=None,
        outputs=None,
    ):
        self._id = algorithm_id
        self._display_name = display_name
        self._group_id = group_id
        self._tags = tags
        self._parameters = parameters or []
        self._outputs = outputs or []

    def id(self):
        return self._id

    def displayName(self):
        return self._display_name

    def groupId(self):
        return self._group_id

    def tags(self):
        return self._tags

    def shortHelpString(self):
        return self._display_name + " help"

    def parameterDefinitions(self):
        return self._parameters

    def outputDefinitions(self):
        return self._outputs

    def name(self):
        return self._id.partition(":")[-1]

    def group(self):
        return self._group_id


class FakeProcessingBridge:
    def __init__(self):
        self.calls = []

    def run_algorithm(self, algorithm_id, parameters, context=None, feedback=None):
        self.calls.append((algorithm_id, parameters, context, feedback))
        return {
            "algorithm_id": algorithm_id,
            "parameters": parameters,
            "outputs": {"OUTPUT": "memory:"},
        }


class TestQCopilotsMcpServerProcessingTools(unittest.TestCase):
    def test_processing_algorithms_are_filtered_by_category(self):
        from qcopilots_common.processing_tools import (
            classify_processing_algorithm,
            filter_processing_algorithms,
        )

        vector = FakeProcessingAlgorithm(
            "native:buffer",
            "Buffer",
            "vectorgeometry",
            ["vector", "geometry"],
        )
        raster = FakeProcessingAlgorithm(
            "gdal:warpreproject",
            "Warp",
            "rasterprojections",
            ["raster", "projection"],
        )
        other = FakeProcessingAlgorithm(
            "native:setprojectvariable",
            "Set project variable",
            "modelertools",
            ["project"],
        )

        self.assertEqual(classify_processing_algorithm(vector), "vector")
        self.assertEqual(classify_processing_algorithm(raster), "raster")
        self.assertEqual(classify_processing_algorithm(other), "general")
        self.assertEqual(
            [
                algorithm.id()
                for algorithm in filter_processing_algorithms(
                    [raster, other, vector],
                    "vector",
                )
            ],
            ["native:buffer"],
        )
        self.assertEqual(
            [
                algorithm.id()
                for algorithm in filter_processing_algorithms(
                    [raster, other, vector],
                    "raster",
                )
            ],
            ["gdal:warpreproject"],
        )
        self.assertEqual(
            filter_processing_algorithms([raster, other, vector], "general"),
            [],
        )

    def test_processing_domain_classification_prefers_definitions_and_reports_mixed(self):
        from qcopilots_common.processing_metadata import (
            processing_algorithm_metadata,
        )
        from qcopilots_common.processing_tools import (
            classify_processing_algorithm,
            filter_processing_algorithms,
        )

        misleading = FakeProcessingAlgorithm(
            "native:raster_named_tool",
            "Raster named tool",
            "general",
            [],
            parameters=[FakeProcessingDefinition("INPUT", "source")],
        )
        mixed = FakeProcessingAlgorithm(
            "native:vector_to_raster",
            "Vector to raster",
            "conversion",
            [],
            parameters=[FakeProcessingDefinition("INPUT", "source")],
            outputs=[FakeProcessingDefinition("OUTPUT", "outputRaster")],
        )

        self.assertEqual(classify_processing_algorithm(misleading), "vector")
        self.assertEqual(classify_processing_algorithm(mixed), "raster")
        self.assertEqual(
            [algorithm.id() for algorithm in filter_processing_algorithms([mixed], "vector")],
            [],
        )
        self.assertEqual(
            [algorithm.id() for algorithm in filter_processing_algorithms([mixed], "raster")],
            [mixed.id()],
        )
        metadata = processing_algorithm_metadata(mixed)
        self.assertEqual(metadata["domains"], ["raster", "vector"])
        self.assertEqual(metadata["owner"], "raster")
        self.assertEqual(metadata["parameters"][0]["type"], "source")
        self.assertEqual(metadata["outputs"][0]["type"], "outputRaster")

    def test_general_ownership_is_exclusive_and_dangerous_algorithms_are_hidden(self):
        from qcopilots_common.processing_metadata import (
            processing_algorithm_metadata,
            processing_algorithm_start_policy,
        )
        from qcopilots_common.processing_tools import (
            classify_processing_algorithm,
            filter_processing_algorithms,
        )

        safe_route = FakeProcessingAlgorithm(
            "native:shortestpathpointtopoint",
            "Shortest path",
            "networkanalysis",
            ["network", "route"],
            parameters=[
                FakeProcessingDefinition("NETWORK", "source"),
                FakeProcessingDefinition("OUTPUT", "sink", destination=True),
            ],
        )
        unsafe_download = FakeProcessingAlgorithm(
            "native:filedownloader",
            "Download file",
            "networkanalysis",
            ["download", "url"],
            parameters=[
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                )
            ],
        )
        unsafe_sql = FakeProcessingAlgorithm(
            "native:postgisexecutesql",
            "Execute PostGIS SQL",
            "vectormiscellaneous",
            ["sql"],
            parameters=[
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                )
            ],
        )
        folder_output = FakeProcessingAlgorithm(
            "native:renderatlas",
            "Render atlas",
            "cartography",
            ["layout"],
            parameters=[
                FakeProcessingDefinition(
                    "OUTPUT",
                    "folderDestination",
                    destination=True,
                )
            ],
        )

        for algorithm in (safe_route, unsafe_download, unsafe_sql, folder_output):
            self.assertEqual(classify_processing_algorithm(algorithm), "general")
            self.assertEqual(
                sum(
                    algorithm
                    in filter_processing_algorithms(
                        [safe_route, unsafe_download, unsafe_sql, folder_output],
                        category,
                    )
                    for category in ("vector", "raster", "general")
                ),
                1 if algorithm is safe_route else 0,
            )

        self.assertEqual(
            filter_processing_algorithms(
                [safe_route, unsafe_download, unsafe_sql, folder_output],
                "general",
            ),
            [safe_route],
        )
        self.assertTrue(processing_algorithm_start_policy(safe_route)["supported"])
        self.assertEqual(
            processing_algorithm_start_policy(unsafe_download),
            {
                "supported": False,
                "reason": "network_access_algorithm_unsupported",
            },
        )
        sql_metadata = processing_algorithm_metadata(unsafe_sql)
        self.assertEqual(sql_metadata["owner"], "general")
        self.assertEqual(
            sql_metadata["start_policy"]["reason"],
            "arbitrary_or_external_database_sql_unsupported",
        )
        self.assertEqual(
            processing_algorithm_start_policy(folder_output)["reason"],
            "folder_destination_atomic_publication_unsupported",
        )

    def test_general_policy_rejects_unknown_provider_and_audited_metadata_drift(self):
        from qcopilots_common.processing_metadata import (
            processing_algorithm_start_policy,
        )

        class ProviderBackedAlgorithm(FakeProcessingAlgorithm):
            def __init__(self, *args, provider_id, **kwargs):
                super().__init__(*args, **kwargs)
                self._provider_id = provider_id

            def provider(self):
                return types.SimpleNamespace(id=lambda: self._provider_id)

        unknown = ProviderBackedAlgorithm(
            "thirdparty:shortestpathpointtopoint",
            "Third-party shortest path",
            "networkanalysis",
            ["network", "route"],
            parameters=[
                FakeProcessingDefinition("NETWORK", "source"),
                FakeProcessingDefinition("OUTPUT", "sink", destination=True),
            ],
            provider_id="thirdparty",
        )
        drifted = ProviderBackedAlgorithm(
            "native:shortestpathpointtopoint",
            "Shortest path with changed metadata",
            "cartography",
            ["network", "route"],
            parameters=[
                FakeProcessingDefinition("NETWORK", "source"),
                FakeProcessingDefinition("OUTPUT", "sink", destination=True),
            ],
            provider_id="native",
        )

        self.assertEqual(
            processing_algorithm_start_policy(unknown),
            {"supported": False, "reason": "general_provider_not_audited"},
        )
        self.assertEqual(
            processing_algorithm_start_policy(drifted),
            {
                "supported": False,
                "reason": "general_algorithm_metadata_changed",
            },
        )

    def test_live_registry_rejects_untrusted_general_provider(self):
        try:
            from processing.core.Processing import Processing
            from qgis.core import (
                QgsApplication,
                QgsProcessingAlgorithm,
                QgsProcessingParameterFeatureSink,
                QgsProcessingProvider,
            )
        except ImportError:
            self.skipTest("QGIS Processing registry is not available")

        import qcopilots_common.bridge as bridge
        from qcopilots_common.processing_metadata import (
            GENERAL_AUDITED_ALGORITHM_GROUPS,
            processing_algorithm_classification,
            processing_algorithm_start_policy,
        )

        class UnknownGeneralAlgorithm(QgsProcessingAlgorithm):
            def name(self):
                return "safe_route"

            def displayName(self):
                return "Unknown provider route"

            def group(self):
                return "Network analysis"

            def groupId(self):
                return "networkanalysis"

            def createInstance(self):
                return UnknownGeneralAlgorithm()

            def initAlgorithm(self, config=None):
                self.addParameter(
                    QgsProcessingParameterFeatureSink("OUTPUT", "Output")
                )

            def processAlgorithm(self, parameters, context, feedback):
                return {}

        class UnknownGeneralProvider(QgsProcessingProvider):
            PROVIDER_ID = "qcopilots_unknown_general_probe"

            def id(self):
                return self.PROVIDER_ID

            def name(self):
                return "QCopilots unknown General probe"

            def loadAlgorithms(self):
                self.addAlgorithm(UnknownGeneralAlgorithm())

        Processing.initialize()
        registry = QgsApplication.processingRegistry()
        existing = registry.providerById(UnknownGeneralProvider.PROVIDER_ID)
        if existing is not None:
            registry.removeProvider(existing)
        provider = UnknownGeneralProvider()
        self.assertTrue(registry.addProvider(provider))
        algorithm_id = f"{provider.PROVIDER_ID}:safe_route"
        try:
            algorithm = registry.createAlgorithmById(algorithm_id)
            self.assertIsNotNone(algorithm)
            self.assertEqual(
                processing_algorithm_start_policy(algorithm),
                {"supported": False, "reason": "general_provider_not_audited"},
            )

            tools = bridge.QgisBridgeTools(None)
            listed = tools.processing_list_algorithms({"category": "general"})
            listed_ids = {item["id"] for item in listed["algorithms"]}
            self.assertNotIn(algorithm_id, listed_ids)
            for unsafe_database_id in (
                "native:postgisexecutesql",
                "native:spatialiteexecutesql",
                "native:spatialiteexecutesqlregistered",
            ):
                self.assertNotIn(unsafe_database_id, listed_ids)
            self.assertTrue(listed_ids)
            for item in listed["algorithms"]:
                self.assertEqual(
                    GENERAL_AUDITED_ALGORITHM_GROUPS[item["provider"]][item["id"]],
                    item["group_id"],
                )
                algorithm = registry.createAlgorithmById(item["id"])
                self.assertEqual(
                    item["classification"],
                    processing_algorithm_classification(algorithm),
                )

            classifications = {
                item["id"]: item["classification"]
                for item in listed["algorithms"]
            }
            self.assertEqual(
                {
                    algorithm_id: classifications.get(algorithm_id)
                    for algorithm_id in (
                        "native:shortestpathpointtopoint",
                        "native:exporttospreadsheet",
                        "native:package",
                        "native:printlayouttopdf",
                        "native:savelog",
                    )
                },
                {
                    "native:shortestpathpointtopoint": "network-safe-algorithm",
                    "native:exporttospreadsheet": "database-read-only-algorithm",
                    "native:package": "database-isolated-output-algorithm",
                    "native:printlayouttopdf": "cartography-safe-algorithm",
                    "native:savelog": "other-safe-algorithm",
                },
            )

            details = tools.processing_algorithm_details(
                {"category": "general", "algorithm_id": algorithm_id}
            )
            self.assertEqual(
                details["start_policy"],
                {"supported": False, "reason": "general_provider_not_audited"},
            )
        finally:
            current = registry.providerById(UnknownGeneralProvider.PROVIDER_ID)
            if current is not None:
                registry.removeProvider(current)

    def test_database_general_policy_allows_only_atomic_file_package(self):
        from qcopilots_common.processing_metadata import (
            processing_algorithm_classification,
            processing_algorithm_metadata,
            processing_algorithm_start_policy,
        )
        from qcopilots_common.processing_tools import filter_processing_algorithms

        safe_package = FakeProcessingAlgorithm(
            "native:package",
            "Package layers",
            "database",
            ["database", "package"],
            parameters=[
                FakeProcessingDefinition("LAYERS", "multilayer"),
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                ),
            ],
        )
        safe_database_read = FakeProcessingAlgorithm(
            "native:exporttospreadsheet",
            "Export to spreadsheet",
            "layertools",
            ["spreadsheet", "table"],
            parameters=[
                FakeProcessingDefinition("LAYERS", "multilayer"),
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                ),
            ],
        )
        unsafe_import = FakeProcessingAlgorithm(
            "native:importintopostgis",
            "Import into PostGIS",
            "database",
            ["database", "postgis"],
            parameters=[
                FakeProcessingDefinition("INPUT", "source"),
                FakeProcessingDefinition("DATABASE", "providerConnection"),
            ],
        )
        connected_package = FakeProcessingAlgorithm(
            "native:package",
            "Connected package",
            "database",
            ["database", "package"],
            parameters=[
                FakeProcessingDefinition("DATABASE", "providerConnection"),
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                ),
            ],
        )

        self.assertEqual(
            filter_processing_algorithms(
                [safe_package, safe_database_read, unsafe_import, connected_package],
                "general",
            ),
            [safe_package, safe_database_read],
        )
        self.assertTrue(processing_algorithm_start_policy(safe_package)["supported"])
        self.assertEqual(
            processing_algorithm_classification(safe_package),
            "database-isolated-output-algorithm",
        )
        self.assertTrue(
            processing_algorithm_start_policy(safe_database_read)["supported"]
        )
        self.assertEqual(
            processing_algorithm_classification(safe_database_read),
            "database-read-only-algorithm",
        )
        unsafe_metadata = processing_algorithm_metadata(unsafe_import)
        self.assertEqual(unsafe_metadata["owner"], "general")
        self.assertEqual(
            unsafe_metadata["start_policy"],
            {
                "supported": False,
                "reason": "arbitrary_or_external_database_sql_unsupported",
            },
        )
        self.assertEqual(
            processing_algorithm_start_policy(connected_package)["reason"],
            "external_database_or_sql_side_effects_unsupported",
        )

    def test_general_algorithms_have_stable_safe_capability_classifications(self):
        from qcopilots_common.processing_metadata import (
            processing_algorithm_classification,
            processing_algorithm_matches_domain,
            processing_algorithm_metadata,
        )

        algorithms = [
            (
                FakeProcessingAlgorithm(
                    "native:shortestpathpointtopoint",
                    "Shortest path",
                    "networkanalysis",
                    ["network", "route"],
                    parameters=[
                        FakeProcessingDefinition("NETWORK", "source"),
                        FakeProcessingDefinition("OUTPUT", "sink", destination=True),
                    ],
                ),
                "network-safe-algorithm",
            ),
            (
                FakeProcessingAlgorithm(
                    "native:exporttospreadsheet",
                    "Export to spreadsheet",
                    "layertools",
                    ["spreadsheet", "table"],
                    parameters=[
                        FakeProcessingDefinition("LAYERS", "multilayer"),
                        FakeProcessingDefinition(
                            "OUTPUT", "fileDestination", destination=True
                        ),
                    ],
                ),
                "database-read-only-algorithm",
            ),
            (
                FakeProcessingAlgorithm(
                    "native:package",
                    "Package layers",
                    "database",
                    ["database", "package"],
                    parameters=[
                        FakeProcessingDefinition("LAYERS", "multilayer"),
                        FakeProcessingDefinition(
                            "OUTPUT", "fileDestination", destination=True
                        ),
                    ],
                ),
                "database-isolated-output-algorithm",
            ),
            (
                FakeProcessingAlgorithm(
                    "native:printlayouttopdf",
                    "Export print layout to PDF",
                    "cartography",
                    ["layout", "pdf"],
                    parameters=[
                        FakeProcessingDefinition("LAYOUT", "layout"),
                        FakeProcessingDefinition(
                            "OUTPUT", "fileDestination", destination=True
                        ),
                    ],
                ),
                "cartography-safe-algorithm",
            ),
            (
                FakeProcessingAlgorithm(
                    "native:savelog",
                    "Save log",
                    "modelertools",
                    ["log"],
                    parameters=[
                        FakeProcessingDefinition(
                            "OUTPUT", "fileDestination", destination=True
                        )
                    ],
                ),
                "other-safe-algorithm",
            ),
            (
                FakeProcessingAlgorithm(
                    "native:meshcontours",
                    "Export contours",
                    "mesh",
                    ["mesh", "contours"],
                    parameters=[
                        FakeProcessingDefinition("INPUT", "mesh"),
                        FakeProcessingDefinition("OUTPUT", "sink", destination=True),
                    ],
                ),
                "mesh-safe-algorithm",
            ),
            (
                FakeProcessingAlgorithm(
                    "native:b3dmtogltf",
                    "Convert B3DM to glTF",
                    "3dtiles",
                    ["3d", "gltf"],
                    parameters=[
                        FakeProcessingDefinition("INPUT", "file"),
                        FakeProcessingDefinition(
                            "OUTPUT", "fileDestination", destination=True
                        ),
                    ],
                ),
                "three-dimensional-safe-algorithm",
            ),
            (
                FakeProcessingAlgorithm(
                    "pdal:info",
                    "Point cloud information",
                    "pointclouddatamanagement",
                    ["pointcloud", "info"],
                    parameters=[
                        FakeProcessingDefinition("INPUT", "pointcloud"),
                        FakeProcessingDefinition(
                            "OUTPUT", "fileDestination", destination=True
                        ),
                    ],
                ),
                "pointcloud-safe-algorithm",
            ),
        ]

        for algorithm, expected_classification in algorithms:
            with self.subTest(algorithm=algorithm.id()):
                self.assertEqual(
                    processing_algorithm_classification(algorithm),
                    expected_classification,
                )
                metadata = processing_algorithm_metadata(algorithm)
                self.assertEqual(metadata["classification"], expected_classification)
                self.assertTrue(metadata["start_policy"]["supported"])
                self.assertTrue(
                    processing_algorithm_matches_domain(algorithm, "general")
                )
                self.assertFalse(
                    processing_algorithm_matches_domain(algorithm, "vector")
                )
                self.assertFalse(
                    processing_algorithm_matches_domain(algorithm, "raster")
                )

        unsafe_sql = FakeProcessingAlgorithm(
            "native:spatialiteexecutesql",
            "Execute SpatiaLite SQL",
            "database",
            ["database", "sql"],
            parameters=[FakeProcessingDefinition("DATABASE", "vector")],
        )
        self.assertEqual(processing_algorithm_classification(unsafe_sql), "")
        self.assertFalse(
            processing_algorithm_metadata(unsafe_sql)["start_policy"]["supported"]
        )

    def test_general_bridge_lists_only_supported_owner_and_details_explain_rejection(self):
        import qcopilots_common.bridge as bridge

        safe_route = FakeProcessingAlgorithm(
            "native:shortestpathpointtopoint",
            "Shortest path",
            "networkanalysis",
            ["network", "route"],
            parameters=[
                FakeProcessingDefinition("NETWORK", "source"),
                FakeProcessingDefinition("OUTPUT", "sink", destination=True),
            ],
        )
        unsafe_download = FakeProcessingAlgorithm(
            "native:filedownloader",
            "Download file",
            "networkanalysis",
            ["download", "url"],
            parameters=[
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                )
            ],
        )
        safe_package = FakeProcessingAlgorithm(
            "native:package",
            "Package layers",
            "database",
            ["database", "package"],
            parameters=[
                FakeProcessingDefinition("LAYERS", "multilayer"),
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                ),
            ],
        )
        safe_database_read = FakeProcessingAlgorithm(
            "native:exporttospreadsheet",
            "Export to spreadsheet",
            "layertools",
            ["spreadsheet", "table"],
            parameters=[
                FakeProcessingDefinition("LAYERS", "multilayer"),
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                ),
            ],
        )
        unsafe_import = FakeProcessingAlgorithm(
            "native:importintospatialite",
            "Import into SpatiaLite",
            "database",
            ["database", "spatialite"],
            parameters=[
                FakeProcessingDefinition("INPUT", "source"),
                FakeProcessingDefinition("DATABASE", "vector"),
            ],
        )
        vector = FakeProcessingAlgorithm(
            "native:buffer",
            "Buffer",
            "vectorgeometry",
            ["vector", "geometry"],
            parameters=[
                FakeProcessingDefinition("INPUT", "source"),
                FakeProcessingDefinition("OUTPUT", "sink", destination=True),
            ],
        )

        class FakeProvider:
            def id(self):
                return "native"

            def name(self):
                return "Native"

            def algorithms(self):
                return [
                    safe_route,
                    unsafe_download,
                    safe_package,
                    safe_database_read,
                    unsafe_import,
                    vector,
                ]

        class FakeRegistry:
            def providers(self):
                return [FakeProvider()]

            def createAlgorithmById(self, algorithm_id):
                return {
                    algorithm.id(): algorithm
                    for algorithm in (
                        safe_route,
                        unsafe_download,
                        safe_package,
                        safe_database_read,
                        unsafe_import,
                        vector,
                    )
                }.get(algorithm_id)

        class FakeQgsApplication:
            @staticmethod
            def processingRegistry():
                return FakeRegistry()

        qgis_module = types.ModuleType("qgis")
        core_module = types.ModuleType("qgis.core")
        core_module.QgsApplication = FakeQgsApplication
        qgis_module.core = core_module
        with mock.patch.dict(
            sys.modules,
            {"qgis": qgis_module, "qgis.core": core_module},
        ), mock.patch.object(bridge, "_ensure_processing_initialized"):
            tools = bridge.QgisBridgeTools(None)
            general = tools.processing_list_algorithms({"category": "general"})
            vector_result = tools.processing_list_algorithms({"category": "vector"})
            raster = tools.processing_list_algorithms({"category": "raster"})
            unsafe_details = tools.processing_algorithm_details(
                {
                    "category": "general",
                    "algorithm_id": unsafe_download.id(),
                }
            )
            unsafe_database_details = tools.processing_algorithm_details(
                {
                    "category": "general",
                    "algorithm_id": unsafe_import.id(),
                }
            )

        self.assertEqual(
            [algorithm["id"] for algorithm in general["algorithms"]],
            [safe_route.id(), safe_package.id(), safe_database_read.id()],
        )
        self.assertEqual(
            {
                algorithm["id"]: algorithm["classification"]
                for algorithm in general["algorithms"]
            },
            {
                safe_route.id(): "network-safe-algorithm",
                safe_package.id(): "database-isolated-output-algorithm",
                safe_database_read.id(): "database-read-only-algorithm",
            },
        )
        self.assertEqual(
            [algorithm["id"] for algorithm in vector_result["algorithms"]],
            [vector.id()],
        )
        self.assertEqual(raster["algorithms"], [])
        self.assertFalse(unsafe_details["start_policy"]["supported"])
        self.assertEqual(
            unsafe_details["start_policy"]["reason"],
            "network_access_algorithm_unsupported",
        )
        self.assertFalse(unsafe_database_details["start_policy"]["supported"])
        self.assertEqual(
            unsafe_database_details["start_policy"]["reason"],
            "external_database_or_sql_side_effects_unsupported",
        )

    def test_processing_bridge_paginates_algorithm_metadata_without_gaps(self):
        import qcopilots_common.bridge as bridge

        algorithm_values = [
            FakeProcessingAlgorithm(
                f"native:vector_algorithm_{index:03d}",
                f"Vector algorithm {index:03d}",
                "vectorgeometry",
                ["vector"],
            )
            for index in range(121)
        ]

        class FakeProvider:
            def id(self):
                return "native"

            def name(self):
                return "Native"

            def algorithms(self):
                return list(algorithm_values)

        class FakeRegistry:
            def providers(self):
                return [FakeProvider()]

        class FakeQgsApplication:
            @staticmethod
            def processingRegistry():
                return FakeRegistry()

        qgis_module = types.ModuleType("qgis")
        core_module = types.ModuleType("qgis.core")
        core_module.QgsApplication = FakeQgsApplication
        qgis_module.core = core_module
        with mock.patch.dict(
            sys.modules,
            {"qgis": qgis_module, "qgis.core": core_module},
        ), mock.patch.object(bridge, "_ensure_processing_initialized"), mock.patch.object(
            bridge,
            "processing_algorithm_metadata",
            wraps=bridge.processing_algorithm_metadata,
        ) as metadata:
            tools = bridge.QgisBridgeTools(None)

            default_page = tools.processing_list_algorithms({"category": "vector"})
            self.assertEqual(len(default_page["algorithms"]), 50)
            self.assertEqual(default_page["returned_count"], 50)
            self.assertEqual(default_page["returned_total"], 50)
            self.assertEqual(default_page["max_results"], 50)
            self.assertIsNone(default_page["next_cursor"])
            self.assertTrue(default_page["truncated"])

            metadata.reset_mock()
            first_page = tools.processing_list_algorithms(
                {"category": "vector", "max_results": 120}
            )
            self.assertEqual(first_page["returned_count"], 50)
            self.assertEqual(first_page["returned_total"], 50)
            self.assertEqual(first_page["max_results"], 120)
            self.assertIsInstance(first_page["next_cursor"], str)
            self.assertFalse(first_page["truncated"])
            self.assertEqual(metadata.call_count, 50)

            second_page = tools.processing_list_algorithms(
                {"category": "vector", "cursor": first_page["next_cursor"]}
            )
            self.assertEqual(second_page["returned_count"], 50)
            self.assertEqual(second_page["returned_total"], 100)
            self.assertEqual(second_page["max_results"], 120)
            self.assertIsInstance(second_page["next_cursor"], str)
            self.assertFalse(second_page["truncated"])
            self.assertEqual(metadata.call_count, 100)

            third_page = tools.processing_list_algorithms(
                {"category": "vector", "cursor": second_page["next_cursor"]}
            )
            self.assertEqual(third_page["returned_count"], 20)
            self.assertEqual(third_page["returned_total"], 120)
            self.assertEqual(third_page["max_results"], 120)
            self.assertIsNone(third_page["next_cursor"])
            self.assertTrue(third_page["truncated"])
            self.assertEqual(metadata.call_count, 120)

            returned_ids = [
                item["id"]
                for page in (first_page, second_page, third_page)
                for item in page["algorithms"]
            ]
            self.assertEqual(
                returned_ids,
                [algorithm.id() for algorithm in algorithm_values[:120]],
            )
            self.assertEqual(len(returned_ids), len(set(returned_ids)))

            algorithm_values[:] = algorithm_values[:37]
            metadata.reset_mock()
            short_page = tools.processing_list_algorithms(
                {"category": "vector", "max_results": 120}
            )
            self.assertEqual(short_page["returned_count"], 37)
            self.assertEqual(short_page["returned_total"], 37)
            self.assertEqual(short_page["max_results"], 120)
            self.assertIsNone(short_page["next_cursor"])
            self.assertFalse(short_page["truncated"])
            self.assertEqual(metadata.call_count, 37)

    def test_processing_bridge_filters_before_applying_general_result_limit(self):
        import qcopilots_common.bridge as bridge

        vector = FakeProcessingAlgorithm(
            "native:buffer",
            "Buffer",
            "vectorgeometry",
            ["vector"],
        )
        unsafe_download = FakeProcessingAlgorithm(
            "native:filedownloader",
            "Download file",
            "networkanalysis",
            ["download", "url"],
            parameters=[
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                )
            ],
        )
        safe_route = FakeProcessingAlgorithm(
            "native:shortestpathpointtopoint",
            "Shortest path",
            "networkanalysis",
            ["network", "route"],
            parameters=[
                FakeProcessingDefinition("NETWORK", "source"),
                FakeProcessingDefinition("OUTPUT", "sink", destination=True),
            ],
        )
        safe_log = FakeProcessingAlgorithm(
            "native:savelog",
            "Save log",
            "modelertools",
            ["log"],
            parameters=[
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                )
            ],
        )
        safe_layout = FakeProcessingAlgorithm(
            "native:printlayouttopdf",
            "Export print layout to PDF",
            "cartography",
            ["layout", "pdf"],
            parameters=[
                FakeProcessingDefinition("LAYOUT", "layout"),
                FakeProcessingDefinition(
                    "OUTPUT",
                    "fileDestination",
                    destination=True,
                ),
            ],
        )
        algorithm_values = [
            vector,
            unsafe_download,
            safe_route,
            unsafe_download,
            safe_log,
            safe_layout,
        ]

        class FakeProvider:
            def id(self):
                return "native"

            def name(self):
                return "Native"

            def algorithms(self):
                return list(algorithm_values)

        class FakeRegistry:
            def providers(self):
                return [FakeProvider()]

        class FakeQgsApplication:
            @staticmethod
            def processingRegistry():
                return FakeRegistry()

        qgis_module = types.ModuleType("qgis")
        core_module = types.ModuleType("qgis.core")
        core_module.QgsApplication = FakeQgsApplication
        qgis_module.core = core_module
        with mock.patch.dict(
            sys.modules,
            {"qgis": qgis_module, "qgis.core": core_module},
        ), mock.patch.object(bridge, "_ensure_processing_initialized"), mock.patch.object(
            bridge,
            "processing_algorithm_metadata",
            wraps=bridge.processing_algorithm_metadata,
        ) as metadata:
            result = bridge.QgisBridgeTools(None).processing_list_algorithms(
                {"category": "general", "max_results": 2}
            )

        self.assertEqual(
            [item["id"] for item in result["algorithms"]],
            [safe_route.id(), safe_log.id()],
        )
        self.assertEqual(result["returned_count"], 2)
        self.assertEqual(result["returned_total"], 2)
        self.assertEqual(result["max_results"], 2)
        self.assertIsNone(result["next_cursor"])
        self.assertTrue(result["truncated"])
        self.assertEqual(metadata.call_count, 2)

    def test_processing_bridge_rejects_invalid_or_drifted_cursors(self):
        import qcopilots_common.bridge as bridge

        algorithm_values = [
            FakeProcessingAlgorithm(
                f"native:vector_algorithm_{index:03d}",
                f"Vector algorithm {index:03d}",
                "vectorgeometry",
                ["vector"],
            )
            for index in range(52)
        ]

        class FakeProvider:
            def id(self):
                return "native"

            def name(self):
                return "Native"

            def algorithms(self):
                return list(algorithm_values)

        class FakeRegistry:
            def providers(self):
                return [FakeProvider()]

        class FakeQgsApplication:
            @staticmethod
            def processingRegistry():
                return FakeRegistry()

        qgis_module = types.ModuleType("qgis")
        core_module = types.ModuleType("qgis.core")
        core_module.QgsApplication = FakeQgsApplication
        qgis_module.core = core_module
        with mock.patch.dict(
            sys.modules,
            {"qgis": qgis_module, "qgis.core": core_module},
        ), mock.patch.object(bridge, "_ensure_processing_initialized"):
            tools = bridge.QgisBridgeTools(None)
            first_page = tools.processing_list_algorithms(
                {"category": "vector", "max_results": 51}
            )
            cursor = first_page["next_cursor"]
            self.assertIsInstance(cursor, str)

            replacement = "A" if cursor[0] != "A" else "B"
            with self.assertRaisesRegex(RuntimeError, "(?i)cursor"):
                tools.processing_list_algorithms(
                    {"category": "vector", "cursor": replacement + cursor[1:]}
                )
            with self.assertRaisesRegex(RuntimeError, "(?i)cursor"):
                tools.processing_list_algorithms(
                    {"category": "raster", "cursor": cursor}
                )
            with self.assertRaisesRegex(RuntimeError, "(?i)cursor"):
                bridge.QgisBridgeTools(None).processing_list_algorithms(
                    {"category": "vector", "cursor": cursor}
                )

            algorithm_values.insert(
                0,
                FakeProcessingAlgorithm(
                    "native:inserted_algorithm",
                    "Inserted algorithm",
                    "vectorgeometry",
                    ["vector"],
                ),
            )
            with self.assertRaisesRegex(RuntimeError, "(?i)cursor"):
                tools.processing_list_algorithms(
                    {"category": "vector", "cursor": cursor}
                )

    def test_processing_tool_registry_bridges_calls_to_processing_runner(self):
        from qcopilots_common.processing_tools import ProcessingToolRegistry

        vector = FakeProcessingAlgorithm(
            "native:buffer",
            "Buffer",
            "vectorgeometry",
            ["vector", "geometry"],
        )
        raster = FakeProcessingAlgorithm(
            "gdal:warpreproject",
            "Warp",
            "rasterprojections",
            ["raster", "projection"],
        )
        bridge = FakeProcessingBridge()
        registry = ProcessingToolRegistry(
            category="vector",
            algorithms=[raster, vector],
            bridge=bridge,
        )

        tools = registry.list_tools()
        self.assertEqual([tool.name for tool in tools], ["processing_native_buffer"])
        self.assertEqual(tools[0].description, "Buffer help")
        self.assertEqual(tools[0].algorithm_id, "native:buffer")

        result = registry.call_tool(
            "processing_native_buffer",
            {"INPUT": "memory:", "DISTANCE": 10, "OUTPUT": "memory:"},
        )
        self.assertEqual(
            result,
            {
                "algorithm_id": "native:buffer",
                "parameters": {
                    "INPUT": "memory:",
                    "DISTANCE": 10,
                    "OUTPUT": "memory:",
                },
                "outputs": {"OUTPUT": "memory:"},
            },
        )
        self.assertEqual(
            [(algorithm_id, parameters) for algorithm_id, parameters, _, _ in bridge.calls],
            [
                (
                    "native:buffer",
                    {"INPUT": "memory:", "DISTANCE": 10, "OUTPUT": "memory:"},
                )
            ],
        )

        with self.assertRaises(KeyError):
            registry.call_tool("processing_gdal_warpreproject", {})

    def test_processing_list_tools_share_schema_and_forward_pagination_arguments(self):
        import qcopilots_common.processing_tools as processing_tools
        from qcopilots_common.constants import (
            DEFAULT_PROCESSING_ALGORITHM_RESULTS,
            MAX_PROCESSING_ALGORITHM_CURSOR_LENGTH,
            MAX_PROCESSING_ALGORITHM_RESULTS,
        )

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {"tool": tool, "arguments": arguments}

        with mock.patch.object(processing_tools, "BridgeClient", FakeBridgeClient):
            tools_by_category = {
                category: processing_tools.build_processing_tools(category)
                for category in ("vector", "raster", "general")
            }
            schemas = [
                tools[0].descriptor()["inputSchema"]
                for tools in tools_by_category.values()
            ]
            self.assertEqual(schemas[1:], [schemas[0], schemas[0]])
            schema = schemas[0]
            self.assertFalse(schema["additionalProperties"])
            self.assertEqual(len(schema["oneOf"]), 2)
            max_results_schema = schema["properties"]["max_results"]
            self.assertEqual(max_results_schema["type"], "integer")
            self.assertEqual(max_results_schema["minimum"], 1)
            self.assertEqual(
                max_results_schema["maximum"],
                MAX_PROCESSING_ALGORITHM_RESULTS,
            )
            self.assertEqual(
                max_results_schema["default"],
                DEFAULT_PROCESSING_ALGORITHM_RESULTS,
            )
            self.assertEqual(schema["properties"]["cursor"]["type"], "string")
            self.assertEqual(schema["properties"]["cursor"]["minLength"], 1)
            self.assertEqual(
                schema["properties"]["cursor"]["maxLength"],
                MAX_PROCESSING_ALGORITHM_CURSOR_LENGTH,
            )

            for category, tools in tools_by_category.items():
                tools[0].handler({})
                self.assertEqual(
                    calls[-1],
                    (
                        "processing_list_algorithms",
                        {
                            "max_results": DEFAULT_PROCESSING_ALGORITHM_RESULTS,
                            "category": category,
                        },
                    ),
                )

            vector_list = tools_by_category["vector"][0]
            for max_results in (1, 51, 120, 2000):
                vector_list.handler({"max_results": max_results})
                self.assertEqual(
                    calls[-1],
                    (
                        "processing_list_algorithms",
                        {"max_results": max_results, "category": "vector"},
                    ),
                )

            cursor = "opaque-cursor"
            tools_by_category["raster"][0].handler({"cursor": cursor})
            self.assertEqual(
                calls[-1],
                (
                    "processing_list_algorithms",
                    {"cursor": cursor, "category": "raster"},
                ),
            )

    def test_build_processing_tools_uses_service_bridge_payloads(self):
        import qcopilots_common.processing_tools as processing_tools

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {"tool": tool, "arguments": arguments}

        original_bridge_client = processing_tools.BridgeClient
        processing_tools.BridgeClient = FakeBridgeClient
        try:
            tools = processing_tools.build_processing_tools("vector")
            self.assertEqual(
                [tool.name for tool in tools],
                [
                    "list_vector_processing_algorithms",
                    "get_vector_processing_algorithm_details",
                    "start_vector_processing_algorithm",
                    "get_vector_processing_job",
                    "list_vector_processing_jobs",
                    "cancel_vector_processing_job",
                ],
            )
            raster_tools = processing_tools.build_processing_tools("raster")
            self.assertEqual(
                [tool.name for tool in raster_tools],
                [
                    "list_raster_processing_algorithms",
                    "get_raster_processing_algorithm_details",
                    "start_raster_processing_algorithm",
                    "get_raster_processing_job",
                    "list_raster_processing_jobs",
                    "cancel_raster_processing_job",
                ],
            )
            general_tools = processing_tools.build_processing_tools("general")
            self.assertEqual(
                [tool.name for tool in general_tools],
                [
                    "list_general_processing_algorithms",
                    "get_general_processing_algorithm_details",
                    "start_general_processing_algorithm",
                    "get_general_processing_job",
                    "list_general_processing_jobs",
                    "cancel_general_processing_job",
                ],
            )
            self.assertTrue(
                all(
                    "requires_auth" not in tool.descriptor()
                    for tool in tools + raster_tools + general_tools
                )
            )
            for category_tools in (tools, raster_tools, general_tools):
                details_schema = category_tools[1].descriptor()["inputSchema"]
                self.assertEqual(
                    details_schema["properties"]["algorithm_id"]["minLength"],
                    1,
                )
                start_description = category_tools[2].description
                self.assertIn("asynchronous", start_description)
                self.assertIn("current project by default", start_description)
                self.assertIn("one-use confirmation token", start_description)
            start_schema = tools[2].descriptor()["inputSchema"]
            self.assertEqual(start_schema["required"], ["algorithm_id"])
            self.assertEqual(start_schema["properties"]["parameters"]["default"], {})
            self.assertTrue(
                start_schema["properties"]["add_outputs_to_project"]["default"]
            )
            self.assertIn(
                "successful outputs",
                start_schema["properties"]["add_outputs_to_project"]["description"],
            )
            self.assertFalse(start_schema["properties"]["overwrite_outputs"]["default"])
            self.assertEqual(
                start_schema["properties"]["client_request_id"]["maxLength"],
                128,
            )
            list_schema = tools[4].descriptor()["inputSchema"]
            self.assertEqual(list_schema["properties"]["limit"]["maximum"], 200)
            self.assertEqual(
                list_schema["properties"]["states"]["items"]["enum"],
                [
                    "queued",
                    "running",
                    "cancelling",
                    "succeeded",
                    "failed",
                    "cancelled",
                ],
            )
            tools[0].handler({})
            tools[1].handler({"algorithm_id": "native:buffer"})
            tools[2].handler({"algorithm_id": "native:buffer"})
            tools[3].handler({"job_id": "vector-job"})
            tools[4].handler({"states": ["running", "cancelling"], "limit": 25})
            tools[5].handler({"job_id": "vector-job"})
            raster_tools[0].handler({})
            raster_tools[1].handler({"algorithm_id": "gdal:warpreproject"})
            raster_tools[2].handler(
                {
                    "algorithm_id": "gdal:warpreproject",
                    "parameters": {"OUTPUT": "memory:"},
                    "add_outputs_to_project": False,
                    "client_request_id": "raster-request",
                }
            )
            raster_tools[3].handler({"job_id": "raster-job"})
            raster_tools[4].handler({})
            raster_tools[5].handler({"job_id": "raster-job"})
            general_tools[0].handler({})
            general_tools[1].handler({"algorithm_id": "native:shortestpathpointtopoint"})
            general_tools[2].handler(
                {
                    "algorithm_id": "native:shortestpathpointtopoint",
                    "parameters": {"OUTPUT": "route.gpkg"},
                    "add_outputs_to_project": False,
                    "client_request_id": "general-request",
                }
            )
            general_tools[3].handler({"job_id": "general-job"})
            general_tools[4].handler({})
            general_tools[5].handler({"job_id": "general-job"})
        finally:
            processing_tools.BridgeClient = original_bridge_client

        self.assertEqual(
            calls,
            [
                (
                    "processing_list_algorithms",
                    {"max_results": 50, "category": "vector"},
                ),
                (
                    "processing_algorithm_details",
                    {"algorithm_id": "native:buffer", "category": "vector"},
                ),
                (
                    "processing_start_algorithm",
                    {
                        "algorithm_id": "native:buffer",
                        "parameters": {},
                        "add_outputs_to_project": True,
                        "overwrite_outputs": False,
                        "category": "vector",
                    },
                ),
                (
                    "processing_get_job",
                    {"job_id": "vector-job", "category": "vector"},
                ),
                (
                    "processing_list_jobs",
                    {
                        "states": ["running", "cancelling"],
                        "limit": 25,
                        "category": "vector",
                    },
                ),
                (
                    "processing_cancel_job",
                    {"job_id": "vector-job", "category": "vector"},
                ),
                (
                    "processing_list_algorithms",
                    {"max_results": 50, "category": "raster"},
                ),
                (
                    "processing_algorithm_details",
                    {"algorithm_id": "gdal:warpreproject", "category": "raster"},
                ),
                (
                    "processing_start_algorithm",
                    {
                        "algorithm_id": "gdal:warpreproject",
                        "parameters": {"OUTPUT": "memory:"},
                        "add_outputs_to_project": False,
                        "overwrite_outputs": False,
                        "client_request_id": "raster-request",
                        "category": "raster",
                    },
                ),
                (
                    "processing_get_job",
                    {"job_id": "raster-job", "category": "raster"},
                ),
                ("processing_list_jobs", {"category": "raster"}),
                (
                    "processing_cancel_job",
                    {"job_id": "raster-job", "category": "raster"},
                ),
                (
                    "processing_list_algorithms",
                    {"max_results": 50, "category": "general"},
                ),
                (
                    "processing_algorithm_details",
                    {
                        "algorithm_id": "native:shortestpathpointtopoint",
                        "category": "general",
                    },
                ),
                (
                    "processing_start_algorithm",
                    {
                        "algorithm_id": "native:shortestpathpointtopoint",
                        "parameters": {"OUTPUT": "route.gpkg"},
                        "add_outputs_to_project": False,
                        "overwrite_outputs": False,
                        "client_request_id": "general-request",
                        "category": "general",
                    },
                ),
                (
                    "processing_get_job",
                    {"job_id": "general-job", "category": "general"},
                ),
                ("processing_list_jobs", {"category": "general"}),
                (
                    "processing_cancel_job",
                    {"job_id": "general-job", "category": "general"},
                ),
            ],
        )

    def test_build_interactive_tools_uses_service_bridge_payloads(self):
        import qcopilots_common.interactive_layer_tools as interactive_layer_tools
        import qcopilots_common.processing_tools as processing_tools

        calls = []

        class FakeBridgeClient:
            def __init__(self, base_url):
                self.base_url = base_url

            def call(self, tool, arguments):
                calls.append((tool, arguments))
                return {"tool": tool, "arguments": arguments}

        original_bridge_client = processing_tools.BridgeClient
        original_interactive_layer_bridge_client = interactive_layer_tools.BridgeClient
        processing_tools.BridgeClient = FakeBridgeClient
        interactive_layer_tools.BridgeClient = FakeBridgeClient
        try:
            tools = processing_tools.build_interactive_tools()
            self.assertEqual(
                [tool.name for tool in tools],
                [
                    "list_layers",
                    "set_layer_visibility",
                    "zoom_to_layer",
                    "zoom_to_extent",
                    "zoom_in",
                    "zoom_out",
                    "zoom_full",
                    "zoom_to_selection",
                    "zoom_to_native_resolution",
                    "zoom_to_last_extent",
                    "zoom_to_next_extent",
                    "save_project",
                    "refresh_canvas",
                    "export_map_image",
                    "describe_layer_sources",
                    "list_map_layers",
                    "load_map_layer",
                    "remove_map_layers",
                    "get_layer_metadata",
                    "query_vector_features",
                    "set_vector_selection",
                    "delete_vector_features",
                    "get_project_crs",
                    "set_project_crs",
                    "get_raster_statistics",
                    "apply_layer_style",
                    "configure_vector_labels",
                    "create_print_layout",
                    "list_print_layouts",
                    "export_print_layout",
                    "create_vector_layer",
                    "add_vector_features",
                    "update_vector_features",
                ],
            )
            self.assertTrue(all("requires_auth" not in tool.descriptor() for tool in tools))
            tools_by_name = {tool.name: tool for tool in tools}
            tools_by_name["list_layers"].handler({})
            tools_by_name["set_layer_visibility"].handler({"layer_id": "abc", "visible": True})
            tools_by_name["export_map_image"].handler({"path": "map.png"})
            tools_by_name["describe_layer_sources"].handler({})
            tools_by_name["create_vector_layer"].handler({"name": "scratch", "path": "memory:"})
        finally:
            processing_tools.BridgeClient = original_bridge_client
            interactive_layer_tools.BridgeClient = original_interactive_layer_bridge_client

        self.assertEqual(
            calls,
            [
                ("list_layers", {}),
                ("set_layer_visibility", {"layer_id": "abc", "visible": True}),
                ("export_map_image", {"path": "map.png"}),
                ("interactive_layer_describe_sources", {}),
                ("create_vector_layer", {"name": "scratch", "path": "memory:"}),
            ],
        )

    def test_service_main_wiring_uses_expected_tools_and_ports(self):
        import os

        from qcopilots_common.constants import DEFAULT_SERVICE_PORTS

        modules = [
            (
                "qcopilots_mcp_server_builtin_tools.server",
                "qcopilots.mcp_server_builtin_tools",
                "QCopilots MCP Server Builtin Tools",
                "read_file",
            ),
            (
                "qcopilots_mcp_server_skills.server",
                "qcopilots.mcp_server_skills",
                "QCopilots MCP Server Skills",
                "list_skills",
            ),
            (
                "qcopilots_mcp_server_interactive_tools.server",
                "qcopilots.mcp_server_interactive_tools",
                "QCopilots MCP Server Interactive Tools",
                [
                    "list_layers",
                    "set_layer_visibility",
                    "zoom_to_layer",
                    "zoom_to_extent",
                    "zoom_in",
                    "zoom_out",
                    "zoom_full",
                    "zoom_to_selection",
                    "zoom_to_native_resolution",
                    "zoom_to_last_extent",
                    "zoom_to_next_extent",
                    "save_project",
                    "refresh_canvas",
                    "export_map_image",
                    "describe_layer_sources",
                    "list_map_layers",
                    "load_map_layer",
                    "remove_map_layers",
                    "get_layer_metadata",
                    "query_vector_features",
                    "set_vector_selection",
                    "delete_vector_features",
                    "get_project_crs",
                    "set_project_crs",
                    "get_raster_statistics",
                    "apply_layer_style",
                    "configure_vector_labels",
                    "create_print_layout",
                    "list_print_layouts",
                    "export_print_layout",
                    "create_vector_layer",
                    "add_vector_features",
                    "update_vector_features",
                ],
            ),
            (
                "qcopilots_mcp_server_processing_vector.server",
                "qcopilots.mcp_server_processing_vector",
                "QCopilots MCP Server Processing Vector",
                [
                    "list_vector_processing_algorithms",
                    "get_vector_processing_algorithm_details",
                    "start_vector_processing_algorithm",
                    "get_vector_processing_job",
                    "list_vector_processing_jobs",
                    "cancel_vector_processing_job",
                ],
            ),
            (
                "qcopilots_mcp_server_processing_raster.server",
                "qcopilots.mcp_server_processing_raster",
                "QCopilots MCP Server Processing Raster",
                [
                    "list_raster_processing_algorithms",
                    "get_raster_processing_algorithm_details",
                    "start_raster_processing_algorithm",
                    "get_raster_processing_job",
                    "list_raster_processing_jobs",
                    "cancel_raster_processing_job",
                ],
            ),
            (
                "qcopilots_mcp_server_processing_general.server",
                "qcopilots.mcp_server_processing_general",
                "QCopilots MCP Server Processing General",
                [
                    "list_general_processing_algorithms",
                    "get_general_processing_algorithm_details",
                    "start_general_processing_algorithm",
                    "get_general_processing_job",
                    "list_general_processing_jobs",
                    "cancel_general_processing_job",
                ],
            ),
            (
                "qcopilots_mcp_server_qgis_binary.server",
                "qcopilots.mcp_server_qgis_binary",
                "QCopilots MCP Server QGIS Binary",
                [
                    "list_qgis_binaries",
                    "get_qgis_binary_details",
                    "start_qgis_binary",
                    "get_qgis_binary_job",
                    "list_qgis_binary_jobs",
                    "cancel_qgis_binary_job",
                ],
            ),
        ]
        old_codex_home = os.environ.get("CODEX_HOME")
        captures = []

        def fake_run_mcp_server(
            name,
            version,
            tools,
            default_port,
            description,
            logger,
            resources=None,
            prompts=None,
        ):
            resolved_tools = tools() if callable(tools) else tools
            resolved_resources = resources() if callable(resources) else resources
            resolved_prompts = prompts() if callable(prompts) else prompts
            captures.append(
                {
                    "name": name,
                    "version": version,
                    "tool_names": [tool.name for tool in resolved_tools],
                    "default_port": default_port,
                    "description": description,
                    "logger": logger.name,
                    "resource_count": len(resolved_resources or []),
                    "prompt_count": len(resolved_prompts or []),
                }
            )

        try:
            os.environ["CODEX_HOME"] = ""
            for module_name, service_id, expected_name, expected_tools in modules:
                module = __import__(module_name, fromlist=["main"])
                original_runner = module.run_mcp_server
                module.run_mcp_server = fake_run_mcp_server
                try:
                    module.main()
                finally:
                    module.run_mcp_server = original_runner

                capture = captures[-1]
                self.assertEqual(capture["name"], expected_name)
                self.assertEqual(capture["version"], "0.1.0")
                if isinstance(expected_tools, list):
                    self.assertEqual(capture["tool_names"], expected_tools)
                else:
                    self.assertIn(expected_tools, capture["tool_names"])
                self.assertNotIn("add_vector_layer", capture["tool_names"])
                self.assertNotIn("add_raster_layer", capture["tool_names"])
                self.assertNotIn("remove_layer", capture["tool_names"])
                self.assertEqual(
                    capture["default_port"],
                    DEFAULT_SERVICE_PORTS[service_id],
                )
                self.assertEqual(capture["logger"], service_id)
        finally:
            if old_codex_home is None:
                os.environ.pop("CODEX_HOME", None)
            else:
                os.environ["CODEX_HOME"] = old_codex_home


if __name__ == "__main__":
    unittest.main()
