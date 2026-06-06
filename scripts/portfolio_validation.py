#!/usr/bin/env python3
"""Repository-level portfolio validation for Make3D.

This script complements the real CMake build/ctest workflow. It checks reviewer-critical
files, inspects generated sample outputs when present, writes a dry-run rollback plan,
and stores Markdown/JSON reports with stage timings.

The README check accepts both the older English headings and the refreshed
portfolio-review headings.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import traceback
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Callable, List


@dataclass
class Stage:
    name: str
    success: bool
    milliseconds: float
    warnings: List[str]
    errors: List[str]


@dataclass
class Report:
    tool: str
    dry_run: bool
    success: bool
    stages: List[Stage]
    rollback_plan: List[str]
    generated_samples: List[str]
    limitations_status: str


class ValidationContext:
    def __init__(self, root: Path, report_dir: Path, build_dir: Path, dry_run: bool) -> None:
        self.root = root
        self.report_dir = report_dir
        self.build_dir = build_dir
        self.dry_run = dry_run
        self.stages: List[Stage] = []
        self.rollback_plan: List[str] = []
        self.generated_samples: List[str] = []
        self.limitations_status = "not checked"

    def run_stage(self, name: str, fn: Callable[[], tuple[list[str], list[str]]]) -> None:
        start = time.perf_counter()
        warnings: List[str] = []
        errors: List[str] = []
        try:
            warnings, errors = fn()
        except Exception as exc:  # pragma: no cover - CI diagnostic path
            errors.append(f"Unhandled exception: {exc}")
            errors.append(traceback.format_exc())
        elapsed = (time.perf_counter() - start) * 1000.0
        self.stages.append(Stage(name=name, success=not errors, milliseconds=elapsed, warnings=warnings, errors=errors))

    def has_errors(self) -> bool:
        return any(not s.success for s in self.stages)


def rel(ctx: ValidationContext, path: Path) -> str:
    try:
        return str(path.relative_to(ctx.root)).replace("\\", "/")
    except ValueError:
        return str(path).replace("\\", "/")


def require_paths(ctx: ValidationContext) -> tuple[list[str], list[str]]:
    warnings: List[str] = []
    errors: List[str] = []
    required = [
        "README.md",
        "CMakeLists.txt",
        "src/Make3DAdvancedCore.cpp",
        "src/Make3DBenchmark.cpp",
        "src/Make3DBenchmark.h",
        "tests/Make3DBenchmarkTest.cpp",
        "tools/GenerateProductionPipelineSample.cpp",
        "docs",
    ]
    for item in required:
        if not (ctx.root / item).exists():
            errors.append(f"Missing required path: {item}")
    return warnings, errors


def check_readme(ctx: ValidationContext) -> tuple[list[str], list[str]]:
    warnings: List[str] = []
    errors: List[str] = []
    readme = ctx.root / "README.md"
    text = readme.read_text(encoding="utf-8") if readme.exists() else ""
    lower = text.lower()

    required_groups = [
        ("portfolio summary", ["portfolio summary", "30-second overview", "ポートフォリオ要約"]),
        ("reviewer path", ["reviewer path", "レビュー手順"]),
        ("limitations", ["current limitations", "現在の制限"]),
        ("roadmap / next improvements", ["roadmap", "next improvements", "次の改善"]),
        ("portfolio wording", ["portfolio wording", "ポートフォリオ用説明文"]),
        ("honest scope note", ["scope note", "not a blender", "not a blender, zbrush", "not a blender, zbrush, photogrammetry", "代替ではありません"]),
    ]
    for label, alternatives in required_groups:
        if not any(token.lower() in lower for token in alternatives):
            errors.append(f"README is missing reviewer/limitation section: {label}; accepted tokens={alternatives}")
    ctx.limitations_status = "README includes explicit limitations" if not errors else "README limitation coverage incomplete"
    return warnings, errors


def check_cmake_test_registration(ctx: ValidationContext) -> tuple[list[str], list[str]]:
    warnings: List[str] = []
    errors: List[str] = []
    cmake = ctx.root / "CMakeLists.txt"
    text = cmake.read_text(encoding="utf-8") if cmake.exists() else ""
    required_tokens = [
        "src/Make3DBenchmark.cpp",
        "Make3DBenchmarkTest",
        "tests/Make3DBenchmarkTest.cpp",
        "add_test(NAME Make3DGenerateProductionPipelineSample",
    ]
    for token in required_tokens:
        if token not in text:
            errors.append(f"CMakeLists.txt missing required token: {token}")
    return warnings, errors


def inspect_generated_samples(ctx: ValidationContext) -> tuple[list[str], list[str]]:
    warnings: List[str] = []
    errors: List[str] = []
    candidates = [
        ctx.build_dir / "production_pipeline",
        ctx.build_dir / "portfolio_sample",
        ctx.build_dir / "generic_asset",
        ctx.build_dir / "complete_game_asset_suite",
        ctx.build_dir / "typed_assets",
        ctx.build_dir / "cli_benchmark",
    ]
    found_files: List[str] = []
    for directory in candidates:
        if directory.exists():
            for path in directory.rglob("*"):
                if path.is_file() and path.suffix.lower() in {".obj", ".gltf", ".json", ".md", ".ppm", ".bin", ".txt", ".csv"}:
                    found_files.append(rel(ctx, path))
    if not found_files:
        warnings.append("No generated sample artifacts found yet. This is acceptable for documentation-only CI runs.")
    manifest = {
        "tool": "Make3D",
        "generatedBy": "scripts/portfolio_validation.py",
        "buildDir": rel(ctx, ctx.build_dir),
        "artifactCount": len(found_files),
        "artifacts": found_files[:500],
    }
    ctx.report_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = ctx.report_dir / "make3d_generated_sample_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    ctx.generated_samples.append(rel(ctx, manifest_path))
    return warnings, errors


def generate_dry_run_plan(ctx: ValidationContext) -> tuple[list[str], list[str]]:
    warnings: List[str] = []
    errors: List[str] = []
    ctx.rollback_plan.extend([
        "Validation script is dry-run: it only reads repository/build artifacts and writes docs/reports outputs.",
        "CMake build outputs are isolated under the build directory and can be rolled back by deleting that directory.",
        "Generated sample artifacts are isolated under build/production_pipeline, build/generic_asset, build/complete_game_asset_suite, build/typed_assets, and build/cli_benchmark.",
        "Repository source files are not modified by the validation script.",
    ])
    return warnings, errors


def write_reports(ctx: ValidationContext) -> Report:
    ctx.report_dir.mkdir(parents=True, exist_ok=True)
    report = Report(
        tool="Make3D",
        dry_run=ctx.dry_run,
        success=not ctx.has_errors(),
        stages=ctx.stages,
        rollback_plan=ctx.rollback_plan,
        generated_samples=ctx.generated_samples,
        limitations_status=ctx.limitations_status,
    )
    json_path = ctx.report_dir / "portfolio_validation_report.json"
    md_path = ctx.report_dir / "portfolio_validation_report.md"
    json_path.write_text(json.dumps(asdict(report), indent=2, ensure_ascii=False), encoding="utf-8")

    lines = [
        "# Make3D Portfolio Validation Report",
        "",
        f"Dry run: `{ctx.dry_run}`",
        f"Success: `{report.success}`",
        f"Limitations: {report.limitations_status}",
        "",
        "## Stage benchmark",
        "",
        "| Stage | Result | ms | Warnings | Errors |",
        "|---|---:|---:|---:|---:|",
    ]
    for stage in report.stages:
        lines.append(f"| {stage.name} | {'PASS' if stage.success else 'FAIL'} | {stage.milliseconds:.3f} | {len(stage.warnings)} | {len(stage.errors)} |")
    lines.extend(["", "## Generated sample/report artifacts", ""])
    lines.extend([f"- `{p}`" for p in report.generated_samples] or ["- none"])
    lines.extend(["", "## Rollback / dry-run plan", ""])
    lines.extend([f"- {item}" for item in ctx.rollback_plan])
    lines.extend(["", "## Errors and warnings", ""])
    for stage in ctx.stages:
        for warning in stage.warnings:
            lines.append(f"- WARNING [{stage.name}] {warning}")
        for error in stage.errors:
            lines.append(f"- ERROR [{stage.name}] {error}")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report-dir", default="docs/reports")
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--dry-run", action="store_true", default=True)
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    ctx = ValidationContext(root=root, report_dir=root / args.report_dir, build_dir=root / args.build_dir, dry_run=args.dry_run)

    ctx.run_stage("required_paths", lambda: require_paths(ctx))
    ctx.run_stage("readme_limitations", lambda: check_readme(ctx))
    ctx.run_stage("cmake_test_registration", lambda: check_cmake_test_registration(ctx))
    ctx.run_stage("generated_sample_manifest", lambda: inspect_generated_samples(ctx))
    ctx.run_stage("dry_run_rollback_plan", lambda: generate_dry_run_plan(ctx))

    report = write_reports(ctx)
    print((ctx.report_dir / "portfolio_validation_report.md").read_text(encoding="utf-8"))
    return 0 if report.success else 1


if __name__ == "__main__":
    sys.exit(main())
