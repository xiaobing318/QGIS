from __future__ import annotations

import importlib
import importlib.metadata
from typing import Any, Optional

from processingmcpserver.dependency_models import RequirementEvaluation


def _parse_requirement(spec: str) -> tuple[Optional[Any], str]:
    """Parse a requirement string and return the parsed object plus any error."""
    text = spec.strip()
    if not text:
        return None, "Requirement is empty."

    requirement_parser, parser_source = _load_requirement_parser()
    if requirement_parser is None:
        return None, (
            "Requirement parser is unavailable. Install 'packaging' or ensure "
            "pip vendor modules are available."
        )

    try:
        return requirement_parser(text), ""
    except Exception as exc:
        return None, f"Invalid requirement '{text}' ({parser_source}): {exc}"


def _load_requirement_parser() -> tuple[Optional[Any], str]:
    """Load a requirement parser in priority order."""
    try:
        from packaging.requirements import Requirement

        return Requirement, "packaging.requirements"
    except Exception:
        pass

    try:
        from pip._vendor.packaging.requirements import Requirement

        return Requirement, "pip._vendor.packaging.requirements"
    except Exception:
        return None, ""


def _evaluate_requirements(
    requirements: list[str],
) -> dict[str, RequirementEvaluation]:
    """Evaluate a list of requirements."""
    evaluations: dict[str, RequirementEvaluation] = {}
    for requirement in requirements:
        evaluations[requirement] = _evaluate_requirement(requirement)
    return evaluations


def _evaluate_requirement(spec: str) -> RequirementEvaluation:
    """Evaluate one requirement for install state, version constraints, and importability."""
    text = spec.strip()
    parsed, parse_error = _parse_requirement(text)
    if parsed is None:
        return RequirementEvaluation(
            requirement=text,
            distribution_name="",
            installed_version="",
            satisfied=False,
            reason=parse_error,
        )

    distribution_name = str(parsed.name).strip()
    if not distribution_name:
        return RequirementEvaluation(
            requirement=text,
            distribution_name="",
            installed_version="",
            satisfied=False,
            reason=f"Requirement '{text}' has no distribution name.",
        )

    try:
        installed_version = importlib.metadata.version(distribution_name)
    except importlib.metadata.PackageNotFoundError:
        return RequirementEvaluation(
            requirement=text,
            distribution_name=distribution_name,
            installed_version="",
            satisfied=False,
            reason=f"Distribution '{distribution_name}' is not installed.",
        )
    except Exception as exc:
        return RequirementEvaluation(
            requirement=text,
            distribution_name=distribution_name,
            installed_version="",
            satisfied=False,
            reason=f"Failed to query version of '{distribution_name}': {exc}",
        )

    try:
        specifier = parsed.specifier
        if specifier and not specifier.contains(installed_version, prereleases=True):
            return RequirementEvaluation(
                requirement=text,
                distribution_name=distribution_name,
                installed_version=installed_version,
                satisfied=False,
                reason=(
                    f"Installed version {installed_version!r} of "
                    f"'{distribution_name}' does not satisfy '{text}'."
                ),
            )
    except Exception as exc:
        return RequirementEvaluation(
            requirement=text,
            distribution_name=distribution_name,
            installed_version=installed_version,
            satisfied=False,
            reason=f"Failed to evaluate version specifier for '{text}': {exc}",
        )

    import_names = _collect_import_names(distribution_name)
    if import_names:
        import_errors: list[str] = []
        imported = False
        for module_name in import_names:
            ok, error_text = _try_import(module_name)
            if ok:
                imported = True
                break
            import_errors.append(f"{module_name}: {error_text}")

        if not imported:
            return RequirementEvaluation(
                requirement=text,
                distribution_name=distribution_name,
                installed_version=installed_version,
                satisfied=False,
                reason=(
                    f"Import validation failed for '{distribution_name}'. "
                    + "; ".join(import_errors[:3])
                ),
            )

    return RequirementEvaluation(
        requirement=text,
        distribution_name=distribution_name,
        installed_version=installed_version,
        satisfied=True,
        reason="",
    )


def _collect_import_names(distribution_name: str) -> list[str]:
    """Derive importable module names from distribution metadata."""
    import_names: list[str] = []
    try:
        distribution = importlib.metadata.distribution(distribution_name)
    except Exception:
        distribution = None

    top_level_text = ""
    if distribution is not None:
        try:
            text = distribution.read_text("top_level.txt")
            top_level_text = text or ""
        except Exception:
            top_level_text = ""

    if top_level_text:
        for line in top_level_text.splitlines():
            module_name = line.strip()
            if module_name and module_name not in import_names:
                import_names.append(module_name)

    fallback = distribution_name.replace("-", "_").strip()
    if fallback and fallback not in import_names:
        import_names.append(fallback)

    return import_names


def _try_import(module_name: str) -> tuple[bool, str]:
    """Try importing a module and return a success flag plus error text."""
    try:
        importlib.import_module(module_name)
        return True, ""
    except Exception as exc:
        return False, str(exc)


def _extract_unsatisfied(
    evaluations: dict[str, RequirementEvaluation]
) -> tuple[list[str], dict[str, str]]:
    """Extract unmet requirements and their reasons from evaluation results."""
    unsatisfied: list[str] = []
    reasons: dict[str, str] = {}
    for requirement, evaluation in evaluations.items():
        if evaluation.satisfied:
            continue
        unsatisfied.append(requirement)
        reasons[requirement] = evaluation.reason
    return unsatisfied, reasons


def _collect_versions_from_evaluations(
    evaluations: dict[str, RequirementEvaluation]
) -> dict[str, str]:
    """Collect installed version snapshots from evaluation results."""
    versions: dict[str, str] = {}
    for requirement, evaluation in evaluations.items():
        versions[requirement] = evaluation.installed_version
    return versions
