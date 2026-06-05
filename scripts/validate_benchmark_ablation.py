#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import sys
from pathlib import Path


def main() -> int:
    build_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("build")
    out_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("docs/reports")
    out_dir.mkdir(parents=True, exist_ok=True)

    errors: list[str] = []
    warnings: list[str] = []

    benchmark_json = build_dir / "cli_benchmark" / "benchmark" / "make3d_benchmark.json"
    benchmark_md = build_dir / "cli_benchmark" / "benchmark" / "make3d_benchmark.md"
    ablation_csv = build_dir / "cli_benchmark" / "ablations" / "ablation_report.csv"
    ablation_md = build_dir / "cli_benchmark" / "ablations" / "ablation_report.md"

    benchmark_summary = {}
    if benchmark_json.exists():
        data = json.loads(benchmark_json.read_text(encoding="utf-8"))
        stages = data.get("stages") or []
        benchmark_summary = {
            "previewTriangles": data.get("previewTriangles"),
            "finalTriangles": data.get("finalTriangles"),
            "stageCount": len(stages),
            "stageNames": [stage.get("name") for stage in stages],
        }
        if not stages:
            errors.append("make3d_benchmark.json contains no stages")
        if int(data.get("previewTriangles", 0) or 0) <= 0:
            errors.append("previewTriangles must be positive")
        if int(data.get("finalTriangles", 0) or 0) <= 0:
            errors.append("finalTriangles must be positive")
    else:
        errors.append("Missing benchmark JSON: " + str(benchmark_json))

    if not benchmark_md.exists():
        errors.append("Missing benchmark Markdown: " + str(benchmark_md))

    ablation_rows = []
    if ablation_csv.exists():
        with ablation_csv.open("r", encoding="utf-8", newline="") as f:
            ablation_rows = list(csv.DictReader(f))
        if not ablation_rows:
            errors.append("ablation_report.csv contains no rows")
        for row in ablation_rows:
            if row.get("ok") != "true":
                errors.append("Ablation row failed: " + json.dumps(row, ensure_ascii=False))
    else:
        errors.append("Missing ablation CSV: " + str(ablation_csv))

    if not ablation_md.exists():
        errors.append("Missing ablation Markdown: " + str(ablation_md))

    report = {
        "success": not errors,
        "warnings": warnings,
        "errors": errors,
        "benchmark": benchmark_summary,
        "ablationRows": ablation_rows,
        "files": {
            "benchmarkJson": str(benchmark_json),
            "benchmarkMarkdown": str(benchmark_md),
            "ablationCsv": str(ablation_csv),
            "ablationMarkdown": str(ablation_md),
        },
    }
    (out_dir / "make3d_benchmark_ablation_validation.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

    lines = [
        "# Make3D Benchmark / Ablation Validation",
        "",
        f"Success: `{not errors}`",
        "",
        "## Benchmark",
        "",
        f"- Preview triangles: `{benchmark_summary.get('previewTriangles')}`",
        f"- Final triangles: `{benchmark_summary.get('finalTriangles')}`",
        f"- Stage count: `{benchmark_summary.get('stageCount')}`",
        "",
        "## Ablations",
        "",
        "| Name | OK | Hero tris | Game asset tris |",
        "|---|---:|---:|---:|",
    ]
    for row in ablation_rows:
        lines.append(f"| {row.get('name')} | {row.get('ok')} | {row.get('hero_triangles')} | {row.get('game_asset_triangles')} |")
    lines.extend(["", "## Errors", ""])
    lines.extend([f"- {error}" for error in errors] or ["- none"])
    (out_dir / "make3d_benchmark_ablation_validation.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    print("\n".join(lines))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
