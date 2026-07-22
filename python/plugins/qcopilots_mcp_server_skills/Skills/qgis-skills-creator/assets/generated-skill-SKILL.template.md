---
name: "{{skill_name}}"
description: "{{skill_description_yaml_escaped}}"
---

# {{display_name}}

## Core contract

Perform {{workflow_summary}} with the QGIS and geospatial tools selected and review-bound in plan `{{plan_id}}` revision `{{plan_revision}}`.

Before processing any data:

1. Confirm that every required input, output, CRS, unit, format, provider and side effect is known.
2. Resolve all relative skill resources from this skill root, never from the process current directory.
3. Refuse to overwrite an input or existing output unless the user explicitly approves that exact target.
4. Re-discover the selected tool and exact QGIS algorithm before use. Recompute its description, input-schema and output-schema hashes; if any differs from `references/tool-contract.json`, stop and report capability drift.
5. For an algorithm capability, require the algorithm ID prefix, descriptor provider, live available provider and skill-required provider to be identical.

## Load supporting resources

Read only the resources that apply to the current request:

- Read `references/tool-contract.json` before selecting or invoking a tool.
- Read `references/data-contract.md` before validating inputs or outputs.
- Read `references/workflow.md` before executing the multi-step workflow.
- Run a script in `scripts/` only when this file or `references/workflow.md` explicitly names it, the script exists inside this skill root, and the host or user approves its execution.

If an optional resource was not included in the generated package, do not invent it.

## Validate the request

Build a task record containing:

- The user objective and requested deliverables.
- Every input source and layer, band or table selector.
- The selected data domains and their input format, horizontal/vertical CRS, geometry/raster/point/mesh/3D/service type, extent, resolution or density, NoData or missing-value rules, dimensions, fields and encoding.
- The requested output format, destination, name, CRS, schema and overwrite policy.
- Whether the result must be loaded into or alter the current QGIS project.
- Any network, database, credential, Shell or device access.
- Acceptance criteria and required evidence.

Classify each material fact as `CONFIRMED`, `INFERRED` or `UNKNOWN`. Ask only about unknowns that would change the algorithm, parameters, target, side effects or acceptance criteria. Do not silently choose a materially different workflow.

## Enforce workspace and data safety

Apply all of these rules:

1. Canonicalize each input, output and temporary path immediately before access.
2. Ensure it remains inside the user-approved root after resolving symlinks, junctions and reparse points.
3. Normalize `file://localhost`, file/provider URI prefixes, container layer suffixes, dot segments, scheme/host/default port/path/query, existing reparse chains and 8.3 names to one physical endpoint identity. For existing local files also compare device plus file-index/inode so hardlinks collide. Reject relative paths and ambiguous provider strings; never skip an input/output conflict check because normalization failed.
4. Keep inputs read-only unless the workflow explicitly requires and the user approves in-place edits.
5. Default outputs to `fail-if-exists`.
6. Create intermediate files in a per-run temporary directory under an approved root.
7. Clean intermediate files after success and after handled failure.
8. Never delete a broad, computed or unresolved path.
9. Never place large GeoPackage, GeoTIFF, point-cloud or other production datasets inside the skill package.

## Verify geospatial semantics

Before the first transforming step, verify:

### Vector data

- Layer validity and selected layer identity.
- Geometry family, multipart state and Z/M dimensions.
- CRS, axis order and measurement units.
- Required fields, field types, encoding and null rules.
- Empty, invalid and self-intersecting geometries.
- Expected topology and feature count bounds.

### Raster data

- Raster validity and selected bands.
- CRS, extent, width, height and pixel size.
- Data type, scale, offset and NoData.
- Grid alignment and resampling method.
- Expected value or statistics bounds.

### Point-cloud data

- Format/version, point format, dimensions and scale/offset.
- Horizontal and vertical reference systems, bounds, point count and density.
- Classification/return flags, missing dimensions, indexing and streaming constraints.
- Expected point-count, classification-distribution and sampled-coordinate tolerances.

### Mesh and temporal simulation data

- Vertex/edge/face/volume topology and invalid or degenerate elements.
- Dataset groups, scalar/vector location, vertical levels, units and missing values.
- Time origin, step range and interpolation rules.
- Expected topology, group/time-step inventory and representative cell values.

### Three-dimensional data

- Horizontal/vertical reference, axis orientation, units and local-origin policy.
- Geometry identity, height semantics, LOD hierarchy and bounding volumes.
- Material, texture and external-resource integrity.
- Expected bounds, sampled heights, LOD completeness and renderability.

### Geospatial services

- Protocol/version, exact endpoint, layer/collection/tileset and operation.
- CRS/axis order, media type/schema, pagination or tile-matrix semantics.
- Credential reference, TLS/proxy, timeout, rate-limit, retry and cache policy.
- Expected response completeness, spatial/temporal coverage and service-error behavior.

### Tabular and spatiotemporal data

- Field types, keys, null/encoding rules and spatial join identity.
- Time zone, calendar, origin, interval boundaries, order and missing-time semantics.
- Expected row/entity counts, temporal coverage and boundary samples.

If an input has an unknown CRS, do not assign or transform it by guessing.
Apply only the sections selected by the approved `domains[]`; omit unrelated checks from the generated skill rather than claiming unsupported coverage.

## Select the execution route

Use the route recorded in `references/tool-contract.json`:

1. Use QCopilots Processing MCP tools when the task must use the live QGIS project or return project-aware results.
2. Use `qgis_process-qgis-qt6.bat` for headless and reproducible Processing workflows.
3. Use interactive tools only for visible QGIS layer, project or canvas state.
4. Use direct GDAL, OGR, PROJ, PDAL, GRASS or other CLI tools only when the approved contract explains why Processing is insufficient.

For Processing:

- Use the exact approved `provider:algorithm_id`.
- Read current algorithm details before execution.
- Reject deprecated algorithms, schema drift, or any known issue that lacks an exact current-revision acceptance record binding the capability ID, description hash, issue and rationale.
- Validate required parameters, defaults, enumerations, destination types and outputs.
- For every CLI node, use only the approved operation ID and the exact argv template in the tool contract.
- Substitute declared argv placeholders as individually validated values; never concatenate or reparse them as shell text.
- Construct the authoritative node `arguments` object only from its declared `argument_bindings`. Validate each value against its binding constraint, then validate the whole object against both the live capability input schema and the capability-reviewed, hash-bound selected-tool parameter schema. Require Processing `arguments.algorithm_id` to exactly equal the selected algorithm.
- Enumerate every terminal leaf inside each object/array argument and structured stdin. Require the capability-reviewed RFC 6901 target map, its selected-tool copy and the actual leaf set to have exactly the same keys; require an exact `nested_target_bindings` entry whose kind equals the reviewed kind and the actual value family for every leaf, including explicit `none` entries. Classify RFC 3986 schemes exactly: `file` requires a local file/path kind; `postgres`, `postgresql`, `mssql`, `mysql`, `mariadb`, `oracle` and `sqlite` require `database`; `http`, `https`, `ws`, `wss`, `ftp`, `ftps`, `s3` and every unknown absolute scheme require `network-endpoint`, which a `no-network` tool must reject. Apply this to Processing `parameters` and `qgis_process` stdin. Never permit a path-like value or any absolute URI to use `none`.
- Derive every top-level and nested target binding from the actual value: use the canonical endpoint for a non-sensitive path/URI and a canonical JSON SHA-256 for a structured or sensitive value. Refuse a caller-supplied permission target that is not this derivation.
- For every input-path/file-read, output-path/file-write or cwd binding, canonicalize the original argument and require containment in the selected tool's corresponding allowed roots. Apply this even when the display target is hashed as sensitive.
- Whenever the catalog provides an MCP invocation policy, require every schema property to be bound or explicitly omitted and enforce every policy requirement before the call; reject every R2/R3 MCP without one. Apply this to R0 file tools too, so read/glob/grep paths cannot be marked non-target or fall back to the current directory.
- For CLI operations, inspect the lexical executable, cwd and every allowed-root leaf plus each existing ancestor for symlinks, junctions or reparse points before resolution. Then re-hash the selected live resolved executable, canonicalize it inside the approved executable roots, substitute every whole-token placeholder in the exact argv template, and require the node materialized argv to match token-for-token. Canonicalize the exact cwd inside its dedicated cwd roots. Require each path placeholder to use the reviewed `argument_target_kinds` mapping before allowed-root containment.
- For `qgis_process`, validate the actual `stdin_payload` against the approved stdin contract, apply nested leaf target and containment checks to `inputs` and other fields, derive and authorize its `stdin:sha256:<canonical-json-hash>` target, and pass that same JSON object through stdin.
- Deterministically reconstruct the complete expected effect set from arguments, nested leaves, all canonical allowed roots, QGIS state, network policy, executable, full argv, cwd and stdin. Require exact set equality with the reviewed effects; reject both missing and extra self-reported effects.
- If the host offers only a general shell rather than a structured process runner, stop unless the contract explicitly approves that exact R3 invocation. Its command must be a non-empty argv array, never a shell string; executable, complete argv and exact cwd must match approved effect targets.

## Execute the workflow

Follow the approved nodes in order and honor their dependency graph.

{{workflow_steps}}

For every node:

1. Verify preconditions.
2. Resolve exact inputs and construct the authoritative `arguments`, `stdin_payload` and declared omissions from their bindings; run all binding, capability and selected-tool schema checks.
3. Resolve and state the tool, operation ID, exact algorithm or CLI executable/argv/cwd contract, every canonical effect target and expected side effect. Reconstruct the full effect set and do not invoke if any member differs.
4. Resolve the permission record that names this tool ID, workflow node ID and every exact effect target; obtain any invocation-level approval required by the host.
5. Execute only that node.
6. Capture structured result, logs and output identities.
7. Check postconditions before continuing.
8. Stop on failure unless the approved fallback condition exactly matches.

Do not reinterpret approval for one node as approval for a different target or operation.
Permission records are per-invocation audit inputs, not a global tool allowlist or a grant of host permission.

## Handle failures

On a failure:

1. Stop dependent nodes.
2. Preserve non-sensitive logs and the minimal evidence needed to reproduce the issue.
3. Classify the failure as input, environment, permission, provider, algorithm, schema drift, resource, cancellation or output validation.
4. Remove incomplete outputs only when they are known products of the current run and deletion is approved.
5. Restore reversible QGIS canvas or layer state when the contract requires it.
6. Report the failed node, exact actual result, expected result and safe recovery options.

Never hide partial success or silently switch to a lower-quality algorithm.

## Validate results

Validate content, not only file existence.

### Vector outputs

Check the output can be opened, the CRS and geometry type are correct, required fields exist, feature counts are plausible, geometries meet validity rules, extent is expected and topology constraints pass.

### Raster outputs

Check the output can be opened, CRS and extent are correct, dimensions and pixel size match the contract, bands and data types are correct, NoData is preserved, and statistics or value ranges satisfy acceptance criteria.

### QGIS state

When the workflow changes the live project, check layer count, names, sources, visibility, selection, canvas state and save state exactly as specified.

### Evidence

Record:

- Inputs and immutable identifiers where available.
- Selected data domains and their domain-specific contract results.
- Tool and exact algorithm or executable versions.
- Normalized parameters with secrets removed.
- Outputs and validation results.
- Warnings, errors and cancellation state.
- Temporary cleanup result.

## Report completion

Return a concise completion report containing:

- What was produced.
- Output locations or QGIS layer identities.
- Tools and exact algorithms used.
- Validation outcomes against every acceptance criterion.
- Any approved deviations, warnings or residual risks.
- Cleanup status.

Do not claim success if any required validation is missing or failed.

## Gotchas

- QCopilots Processing vector and raster services expose generic `list`, `details` and `run` tools. A Processing algorithm ID is a parameter, not an independent MCP tool.
- The current vector and raster classification is keyword-based and can omit or misclassify algorithms. Use the complete `qgis_process --json list` inventory when completeness matters.
- The current QCopilots algorithm details response is less complete than `qgis_process --json help`. Use the approved fallback when defaults, outputs, enum values or flags matter.
- QCopilots Builtin file and Shell tools do not provide a reliable workspace sandbox by themselves. Enforce canonical containment here.
- A successful `read_skill` after a write may still reflect a last-known-good snapshot. Check `list_skills` diagnostics and verify returned content.
- `qgis-qt6-bin.bat` is not a valid entry in the observed Qt6 package. The GUI binary is `qgis-qt6-bin.exe`, and the supported CLI wrapper is `qgis_process-qgis-qt6.bat`.
