"""Metadata helpers shared by QCopilots Processing services and jobs.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import re
from typing import Any

from qcopilots_common.processing_jobs import json_safe_value


VECTOR_DEFINITION_TYPES = {
    "attribute",
    "field",
    "geometry",
    "sink",
    "source",
    "vector",
    "vectordestination",
    "outputvector",
}
RASTER_DEFINITION_TYPES = {
    "band",
    "raster",
    "rasterdestination",
    "outputraster",
}
VECTOR_SOURCE_TYPES = {-1, 0, 1, 2, 5}
RASTER_SOURCE_TYPES = {3}
GENERAL_DEFINITION_TYPES = {
    "database",
    "file",
    "filedestination",
    "folderdestination",
    "layout",
    "layoutitem",
    "mesh",
    "meshdestination",
    "pointcloud",
    "pointclouddestination",
    "providerconnection",
    "schema",
}
GENERAL_GROUP_IDS = {
    "3dtiles",
    "cartography",
    "database",
    "filetools",
    "gps",
    "layertools",
    "mesh",
    "metadatatools",
    "miscellaneous",
    "modelertools",
    "networkanalysis",
    "plots",
    "pointcloudconversion",
    "pointclouddatamanagement",
    "pointcloudextraction",
    "vectortiles",
}
GENERAL_SAFE_GROUP_IDS = {
    "3dtiles",
    "cartography",
    "database",
    "layertools",
    "mesh",
    "modelertools",
    "networkanalysis",
    "plots",
    "pointcloudconversion",
    "pointclouddatamanagement",
    "pointcloudextraction",
}
GENERAL_SAFE_DATABASE_ALGORITHM_IDS = {
    "native:package",
}
GENERAL_ALGORITHM_CLASSIFICATION_OVERRIDES = {
    # This algorithm only reads the supplied layers, including SQLite-backed
    # tables, and publishes their rows to a separately authorized destination.
    "native:exporttospreadsheet": "database-read-only-algorithm",
    "native:package": "database-isolated-output-algorithm",
}
GENERAL_SAFE_GROUP_CLASSIFICATIONS = {
    "3dtiles": "three-dimensional-safe-algorithm",
    "cartography": "cartography-safe-algorithm",
    "layertools": "other-safe-algorithm",
    "mesh": "mesh-safe-algorithm",
    "modelertools": "other-safe-algorithm",
    "networkanalysis": "network-safe-algorithm",
    "plots": "other-safe-algorithm",
    "pointcloudconversion": "pointcloud-safe-algorithm",
    "pointclouddatamanagement": "pointcloud-safe-algorithm",
    "pointcloudextraction": "pointcloud-safe-algorithm",
}
GENERAL_AUDITED_ALGORITHM_GROUPS = {
    "native": {
        "native:angletonearest": "cartography",
        "native:atlaslayouttopdf": "cartography",
        "native:b3dmtogltf": "3dtiles",
        "native:categorizeusingstyle": "cartography",
        "native:combinestyles": "cartography",
        "native:exportlayersinformation": "layertools",
        "native:exportmeshedges": "mesh",
        "native:exportmeshfaces": "mesh",
        "native:exportmeshongrid": "mesh",
        "native:exportmeshvertices": "mesh",
        "native:exporttospreadsheet": "layertools",
        "native:extractlabels": "cartography",
        "native:extractnetworkendpoints": "networkanalysis",
        "native:filterlayersbytype": "modelertools",
        "native:generateelevationprofileimage": "plots",
        "native:gltftovector": "3dtiles",
        "native:meshcontours": "mesh",
        "native:meshexportcrosssection": "mesh",
        "native:meshexporttimeseries": "mesh",
        "native:meshrasterize": "mesh",
        "native:package": "database",
        "native:polygonfromlayerextent": "layertools",
        "native:printlayoutmapextenttolayer": "cartography",
        "native:printlayouttoimage": "cartography",
        "native:printlayouttopdf": "cartography",
        "native:savelog": "modelertools",
        "native:serviceareafromlayer": "networkanalysis",
        "native:serviceareafrompoint": "networkanalysis",
        "native:shortestpathlayertopoint": "networkanalysis",
        "native:shortestpathpointtolayer": "networkanalysis",
        "native:shortestpathpointtopoint": "networkanalysis",
        "native:stylefromproject": "cartography",
        "native:surfacetopolygon": "mesh",
        "native:tinmeshcreation": "mesh",
        "native:validatenetwork": "networkanalysis",
    },
    "pdal": {
        "pdal:assignprojection": "pointclouddatamanagement",
        "pdal:boundary": "pointcloudextraction",
        "pdal:classifyground": "pointclouddatamanagement",
        "pdal:clip": "pointclouddatamanagement",
        "pdal:compare": "pointclouddatamanagement",
        "pdal:convertformat": "pointcloudconversion",
        "pdal:density": "pointcloudextraction",
        "pdal:exportraster": "pointcloudconversion",
        "pdal:exportrastertin": "pointcloudconversion",
        "pdal:exportvector": "pointcloudconversion",
        "pdal:filter": "pointcloudextraction",
        "pdal:filternoiseradius": "pointclouddatamanagement",
        "pdal:filternoisestatistical": "pointclouddatamanagement",
        "pdal:heightabovegroundbynearestneighbor": "pointclouddatamanagement",
        "pdal:heightabovegroundtriangulation": "pointclouddatamanagement",
        "pdal:info": "pointclouddatamanagement",
        "pdal:merge": "pointclouddatamanagement",
        "pdal:reproject": "pointclouddatamanagement",
        "pdal:thinbydecimate": "pointclouddatamanagement",
        "pdal:thinbyradius": "pointclouddatamanagement",
        "pdal:transformpointcloud": "pointclouddatamanagement",
        "pdal:virtualpointcloud": "pointclouddatamanagement",
    },
    "qgis": {
        "qgis:barplot": "plots",
        "qgis:boxplot": "plots",
        "qgis:meanandstandarddeviationplot": "plots",
        "qgis:polarplot": "plots",
        "qgis:rasterlayerhistogram": "plots",
        "qgis:scatter3dplot": "plots",
        "qgis:topologicalcoloring": "cartography",
        "qgis:vectorlayerhistogram": "plots",
        "qgis:vectorlayerscatterplot": "plots",
    },
}
GENERAL_AUDITED_PROVIDER_IDS = frozenset(GENERAL_AUDITED_ALGORITHM_GROUPS)
GENERAL_UNSAFE_GROUP_REASONS = {
    "filetools": "filesystem_mutation_algorithm_unsupported",
    "gps": "external_process_or_network_side_effects_unverified",
    "metadatatools": "in_place_metadata_mutation_unsupported",
    "miscellaneous": "general_side_effects_not_classified",
    "vectortiles": "directory_or_container_output_unsupported",
}


def processing_algorithm_domains(algorithm: Any) -> set[str]:
    return set(processing_algorithm_domain_evidence(algorithm))


def processing_algorithm_matches_domain(algorithm: Any, domain: str) -> bool:
    if domain not in {"vector", "raster", "general"}:
        return False
    return processing_algorithm_owner(algorithm) == domain


def processing_algorithm_owner(algorithm: Any) -> str:
    owner, _evidence = _processing_algorithm_owner_evidence(algorithm)
    return owner


def processing_algorithm_start_policy(algorithm: Any) -> dict[str, Any]:
    owner = processing_algorithm_owner(algorithm)
    unsafe_reason = _processing_algorithm_unsafe_reason(algorithm)
    if unsafe_reason:
        return {"supported": False, "reason": unsafe_reason}

    group_id = _normalized_group_id(algorithm)
    provider_id = _algorithm_provider_id(algorithm)
    parameter_definitions = list(
        _call_optional(algorithm, "parameterDefinitions") or []
    )
    has_destination_parameter = any(
        bool(_call_optional(definition, "isDestination") or False)
        for definition in parameter_definitions
    )
    has_folder_destination = any(
        "folderdestination" in _normalized_definition_type(definition)
        for definition in parameter_definitions
    )
    if owner == "general":
        if provider_id == "script":
            return {
                "supported": False,
                "reason": "arbitrary_processing_script_unsupported",
            }
        if provider_id == "model":
            return {
                "supported": False,
                "reason": "model_composition_side_effects_unverified",
            }
    if has_folder_destination:
        return {
            "supported": False,
            "reason": "folder_destination_atomic_publication_unsupported",
        }
    if not has_destination_parameter:
        return {"supported": False, "reason": "separate_destination_required"}
    if owner == "general":
        audit_reason = _general_algorithm_audit_reason(
            algorithm,
            provider_id=provider_id,
            group_id=group_id,
        )
        if audit_reason:
            return {"supported": False, "reason": audit_reason}
        group_reason = GENERAL_UNSAFE_GROUP_REASONS.get(group_id)
        if group_reason:
            return {"supported": False, "reason": group_reason}
        if group_id not in GENERAL_SAFE_GROUP_IDS:
            return {
                "supported": False,
                "reason": "general_side_effects_not_classified",
            }
    return {
        "supported": True,
        "reason": "destination_preflight_and_staging_required",
    }


def processing_algorithm_classification(algorithm: Any) -> str:
    """Return the stable capability group for an executable General algorithm."""
    if processing_algorithm_owner(algorithm) != "general":
        return ""
    if not processing_algorithm_start_policy(algorithm)["supported"]:
        return ""

    algorithm_id = str(_call_optional(algorithm, "id") or "").strip().lower()
    override = GENERAL_ALGORITHM_CLASSIFICATION_OVERRIDES.get(algorithm_id)
    if override:
        return override
    return GENERAL_SAFE_GROUP_CLASSIFICATIONS.get(
        _normalized_group_id(algorithm),
        "",
    )


def processing_algorithm_domain_evidence(algorithm: Any) -> dict[str, list[str]]:
    evidence: dict[str, list[str]] = {"vector": [], "raster": []}
    definitions = list(_call_optional(algorithm, "parameterDefinitions") or [])
    definitions.extend(list(_call_optional(algorithm, "outputDefinitions") or []))
    for definition in definitions:
        definition_type = str(_call_optional(definition, "type") or "").lower()
        name = str(_call_optional(definition, "name") or "")
        if definition_type in VECTOR_DEFINITION_TYPES:
            evidence["vector"].append(f"definition:{name}:{definition_type}")
        if definition_type in RASTER_DEFINITION_TYPES:
            evidence["raster"].append(f"definition:{name}:{definition_type}")
        for source_type in _definition_source_types(definition):
            if source_type in VECTOR_SOURCE_TYPES:
                evidence["vector"].append(f"source_type:{name}:{source_type}")
            if source_type in RASTER_SOURCE_TYPES:
                evidence["raster"].append(f"source_type:{name}:{source_type}")

    if not evidence["vector"] and not evidence["raster"]:
        metadata_text = " ".join(
            [
                " ".join(str(tag) for tag in (_call_optional(algorithm, "tags") or [])),
                str(_call_optional(algorithm, "groupId") or ""),
            ]
        ).lower()
        tokens = {token for token in re.split(r"[^a-z0-9]+", metadata_text) if token}
        if any(
            token in metadata_text
            for token in ("vector", "feature", "geometry", "attribute")
        ):
            evidence["vector"].append("algorithm_metadata:tags_or_group_id")
        if any(token in metadata_text for token in ("raster", "grid", "dem", "cell")):
            evidence["raster"].append("algorithm_metadata:tags_or_group_id")
        if "vector" in tokens:
            evidence["vector"].append("algorithm_metadata:vector_token")
        if "raster" in tokens:
            evidence["raster"].append("algorithm_metadata:raster_token")

    return {domain: values for domain, values in evidence.items() if values}


def _processing_algorithm_owner_evidence(
    algorithm: Any,
) -> tuple[str, list[str]]:
    unsafe_reason = _processing_algorithm_unsafe_reason(algorithm)
    if unsafe_reason:
        return "general", [f"safety_policy:{unsafe_reason}"]

    provider_id = _algorithm_provider_id(algorithm)
    if provider_id in {"model", "script"}:
        return "general", [f"provider:{provider_id}"]

    group_id = _normalized_group_id(algorithm)
    if group_id in GENERAL_GROUP_IDS:
        return "general", [f"group_id:{group_id}"]

    parameters = list(_call_optional(algorithm, "parameterDefinitions") or [])
    outputs = list(_call_optional(algorithm, "outputDefinitions") or [])
    destination_parameters = [
        definition
        for definition in parameters
        if bool(_call_optional(definition, "isDestination") or False)
    ]
    for label, definitions in (
        ("output", outputs + destination_parameters),
        (
            "input",
            [
                definition
                for definition in parameters
                if not bool(_call_optional(definition, "isDestination") or False)
            ],
        ),
    ):
        domains, evidence = _definition_ownership_evidence(definitions, label)
        if len(domains) == 1:
            return next(iter(domains)), evidence
        if len(domains) > 1:
            group_domain = _group_domain(group_id)
            if group_domain:
                return group_domain, evidence + [f"group_id_tiebreak:{group_id}"]
            return "general", evidence + ["mixed_definition_domains"]

    group_domain = _group_domain(group_id)
    if group_domain:
        return group_domain, [f"group_id:{group_id}"]
    return "general", ["no_vector_or_raster_ownership_evidence"]


def _definition_ownership_evidence(
    definitions: list[Any],
    label: str,
) -> tuple[set[str], list[str]]:
    domains: set[str] = set()
    evidence = []
    for definition in definitions:
        definition_type = _normalized_definition_type(definition)
        name = str(_call_optional(definition, "name") or "")
        if definition_type in VECTOR_DEFINITION_TYPES:
            domains.add("vector")
            evidence.append(f"{label}_definition:{name}:{definition_type}:vector")
        if definition_type in RASTER_DEFINITION_TYPES:
            domains.add("raster")
            evidence.append(f"{label}_definition:{name}:{definition_type}:raster")
        if definition_type in GENERAL_DEFINITION_TYPES:
            domains.add("general")
            evidence.append(f"{label}_definition:{name}:{definition_type}:general")
        for source_type in _definition_source_types(definition):
            if source_type in VECTOR_SOURCE_TYPES:
                domains.add("vector")
                evidence.append(f"{label}_source_type:{name}:{source_type}:vector")
            if source_type in RASTER_SOURCE_TYPES:
                domains.add("raster")
                evidence.append(f"{label}_source_type:{name}:{source_type}:raster")
    return domains, evidence


def _processing_algorithm_unsafe_reason(algorithm: Any) -> str | None:
    raw_algorithm_id = str(_call_optional(algorithm, "id") or "").strip().lower()
    algorithm_id = re.sub(
        r"[^a-z0-9]+",
        "",
        raw_algorithm_id,
    )
    checks = (
        (
            (
                "download",
                "upload",
                "httprequest",
                "openurl",
                "urlopener",
            ),
            "network_access_algorithm_unsupported",
        ),
        (
            ("executesql", "postgis", "postgres", "ogrinfo"),
            "arbitrary_or_external_database_sql_unsupported",
        ),
        (
            (
                "createattributeindex",
                "createspatialindex",
                "defineprojection",
                "rasterizeover",
                "buildoverviews",
                "pyramids",
            ),
            "in_place_input_mutation_unsupported",
        ),
        (
            (
                "createdirectory",
                "deletedirectory",
                "deletefile",
                "movedirectory",
                "movefile",
                "renamedirectory",
                "renamefile",
            ),
            "filesystem_mutation_algorithm_unsupported",
        ),
    )
    for tokens, reason in checks:
        if any(token in algorithm_id for token in tokens):
            return reason
    if _normalized_group_id(algorithm) == "database":
        parameter_definitions = list(
            _call_optional(algorithm, "parameterDefinitions") or []
        )
        destination_types = {
            _normalized_definition_type(definition)
            for definition in parameter_definitions
            if bool(_call_optional(definition, "isDestination") or False)
        }
        external_database_types = {
            "database",
            "databaseschema",
            "databasetable",
            "providerconnection",
        }
        if (
            raw_algorithm_id not in GENERAL_SAFE_DATABASE_ALGORITHM_IDS
            or "filedestination" not in destination_types
            or any(
                _normalized_definition_type(definition) in external_database_types
                for definition in parameter_definitions
            )
        ):
            return "external_database_or_sql_side_effects_unsupported"
    return None


def _algorithm_provider_id(algorithm: Any) -> str:
    algorithm_id = str(_call_optional(algorithm, "id") or "")
    provider = _call_optional(algorithm, "provider")
    if provider is not None:
        provider_id = str(_call_optional(provider, "id") or "").strip().lower()
        if provider_id:
            return provider_id
    return algorithm_id.partition(":")[0].strip().lower()


def _general_algorithm_audit_reason(
    algorithm: Any,
    *,
    provider_id: str,
    group_id: str,
) -> str | None:
    if provider_id not in GENERAL_AUDITED_PROVIDER_IDS:
        return "general_provider_not_audited"
    algorithm_id = str(_call_optional(algorithm, "id") or "").strip().lower()
    audited_group = GENERAL_AUDITED_ALGORITHM_GROUPS[provider_id].get(algorithm_id)
    if audited_group is None:
        return "general_algorithm_not_audited"
    if group_id != audited_group:
        return "general_algorithm_metadata_changed"
    return None


def _normalized_group_id(algorithm: Any) -> str:
    return re.sub(
        r"[^a-z0-9]+",
        "",
        str(_call_optional(algorithm, "groupId") or "").lower(),
    )


def _normalized_definition_type(definition: Any) -> str:
    return re.sub(
        r"[^a-z0-9]+",
        "",
        str(
            _call_optional(definition, "type")
            or type(definition).__name__
        ).lower(),
    )


def _group_domain(group_id: str) -> str | None:
    if group_id.startswith("vector"):
        return "vector"
    if group_id.startswith("raster"):
        return "raster"
    return None


def processing_algorithm_metadata(
    algorithm: Any,
    provider: Any = None,
    *,
    include_definitions: bool = True,
) -> dict[str, Any]:
    algorithm_id = str(_call_optional(algorithm, "id") or "")
    resolved_provider = provider or _call_optional(algorithm, "provider")
    provider_id = algorithm_id.partition(":")[0]
    provider_name = ""
    if resolved_provider is not None:
        provider_id = str(_call_optional(resolved_provider, "id") or provider_id)
        provider_name = str(_call_optional(resolved_provider, "name") or "")
    domain_evidence = processing_algorithm_domain_evidence(algorithm)
    parameter_definitions = list(
        _call_optional(algorithm, "parameterDefinitions") or []
    )
    owner, ownership_evidence = _processing_algorithm_owner_evidence(algorithm)
    metadata = {
        "id": algorithm_id,
        "name": str(_call_optional(algorithm, "name") or ""),
        "display_name": str(_call_optional(algorithm, "displayName") or ""),
        "short_description": str(_call_optional(algorithm, "shortDescription") or ""),
        "short_help": str(_call_optional(algorithm, "shortHelpString") or ""),
        "help_url": str(_call_optional(algorithm, "helpUrl") or ""),
        "group": str(_call_optional(algorithm, "group") or ""),
        "group_id": str(_call_optional(algorithm, "groupId") or ""),
        "provider": provider_id,
        "provider_name": provider_name,
        "tags": [str(tag) for tag in (_call_optional(algorithm, "tags") or [])],
        "flags": _integer_value(_call_optional(algorithm, "flags")),
        "domains": sorted(domain_evidence),
        "domain_evidence": domain_evidence,
        "owner": owner,
        "ownership_evidence": ownership_evidence,
        "classification": processing_algorithm_classification(algorithm),
        "start_policy": processing_algorithm_start_policy(algorithm),
    }
    if include_definitions:
        metadata["parameters"] = [
            processing_definition_metadata(definition, parameter=True)
            for definition in parameter_definitions
        ]
        metadata["outputs"] = [
            processing_definition_metadata(definition, parameter=False)
            for definition in (_call_optional(algorithm, "outputDefinitions") or [])
        ]
    return metadata


def processing_definition_metadata(definition: Any, *, parameter: bool) -> dict[str, Any]:
    definition_type = str(_call_optional(definition, "type") or "")
    metadata = {
        "name": str(_call_optional(definition, "name") or ""),
        "description": str(_call_optional(definition, "description") or ""),
        "type": definition_type,
    }
    if not parameter:
        data_type = _call_optional(definition, "dataType")
        if data_type is not None:
            metadata["data_type"] = _integer_or_text(data_type)
        return metadata

    flags = _call_optional(definition, "flags")
    metadata.update(
        {
            "optional": _parameter_flag_enabled(definition, flags, "Optional", "FlagOptional"),
            "advanced": _parameter_flag_enabled(definition, flags, "Advanced", "FlagAdvanced"),
            "destination": bool(_call_optional(definition, "isDestination") or False),
            "default": json_safe_value(_call_optional(definition, "defaultValue")),
            "help": str(_call_optional(definition, "help") or ""),
            "flags": _integer_value(flags),
        }
    )
    dependencies = _call_optional(definition, "dependsOnOtherParameters")
    if dependencies:
        metadata["depends_on"] = [str(value) for value in dependencies]
    source_types = _definition_source_types(definition)
    if source_types:
        metadata["accepted_source_types"] = source_types
    for method_name, key in (
        ("options", "options"),
        ("minimum", "minimum"),
        ("maximum", "maximum"),
        ("decimals", "decimals"),
        ("allowMultiple", "allow_multiple"),
        ("defaultFileExtension", "default_file_extension"),
        ("supportsNonFileBasedOutput", "supports_non_file_output"),
    ):
        value = _call_optional(definition, method_name)
        if value is not None:
            metadata[key] = json_safe_value(value)
    if definition_type == "enum" and isinstance(metadata.get("options"), list):
        uses_static_strings = bool(
            _call_optional(definition, "usesStaticStrings") or False
        )
        metadata["enum_options"] = [
            {
                "value": str(label) if uses_static_strings else index,
                "label": str(label),
            }
            for index, label in enumerate(metadata["options"])
        ]
    definition_metadata = _call_optional(definition, "metadata")
    if definition_metadata:
        metadata["metadata"] = json_safe_value(definition_metadata)
    return metadata


def _definition_source_types(definition: Any) -> list[int]:
    values: list[Any] = []
    data_types = _call_optional(definition, "dataTypes")
    if data_types:
        values.extend(data_types)
    layer_type = _call_optional(definition, "layerType")
    if layer_type is not None:
        values.append(layer_type)
    normalized = []
    for value in values:
        numeric = _integer_value(value)
        if numeric is not None and numeric not in normalized:
            normalized.append(numeric)
    return normalized


def _parameter_flag_enabled(
    definition: Any,
    flags: Any,
    enum_name: str,
    legacy_name: str,
) -> bool:
    legacy_flag = getattr(definition, legacy_name, None)
    if legacy_flag is not None:
        try:
            return bool(flags & legacy_flag)
        except Exception:
            pass
    try:
        from qgis.core import Qgis

        flag = getattr(Qgis.ProcessingParameterFlag, enum_name)
        return bool(flags & flag)
    except Exception:
        return False


def _call_optional(target: Any, name: str) -> Any:
    value = getattr(target, name, None)
    if not callable(value):
        return value
    try:
        return value()
    except Exception:
        return None


def _integer_value(value: Any) -> int | None:
    if value is None:
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        try:
            return int(value.value)
        except (AttributeError, TypeError, ValueError):
            return None


def _integer_or_text(value: Any) -> int | str:
    numeric = _integer_value(value)
    return numeric if numeric is not None else str(value)
