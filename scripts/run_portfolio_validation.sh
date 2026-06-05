#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${1:-build}"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release --parallel 2
ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure

if [ -x "$BUILD_DIR/Make3DGenerateProductionPipelineSample" ]; then
  "$BUILD_DIR/Make3DGenerateProductionPipelineSample" "$BUILD_DIR/portfolio_sample"
elif [ -x "$BUILD_DIR/Release/Make3DGenerateProductionPipelineSample" ]; then
  "$BUILD_DIR/Release/Make3DGenerateProductionPipelineSample" "$BUILD_DIR/portfolio_sample"
else
  echo "Make3DGenerateProductionPipelineSample executable not found" >&2
  find "$BUILD_DIR" -maxdepth 3 -type f -perm -111 | sort
  exit 1
fi

INPUT="$BUILD_DIR/portfolio_sample/input_noisy_character.tga"
if [ ! -f "$INPUT" ]; then
  INPUT="$(find "$BUILD_DIR/portfolio_sample" -type f \( -name '*.png' -o -name '*.tga' -o -name '*.jpg' -o -name '*.jpeg' \) | head -n 1)"
fi
if [ -z "$INPUT" ] || [ ! -f "$INPUT" ]; then
  echo "No generated image input found for CLI benchmark" >&2
  find "$BUILD_DIR/portfolio_sample" -maxdepth 4 -type f | sort
  exit 1
fi

if [ -x "$BUILD_DIR/Make3DAdvancedCLI" ]; then
  CLI="$BUILD_DIR/Make3DAdvancedCLI"
elif [ -x "$BUILD_DIR/Release/Make3DAdvancedCLI" ]; then
  CLI="$BUILD_DIR/Release/Make3DAdvancedCLI"
else
  echo "Make3DAdvancedCLI executable not found" >&2
  find "$BUILD_DIR" -maxdepth 3 -type f -perm -111 | sort
  exit 1
fi

"$CLI" --input "$INPUT" --output "$BUILD_DIR/cli_benchmark" --benchmark --ablation no-shape-inference
python3 scripts/validate_benchmark_ablation.py "$BUILD_DIR" docs/reports
python3 scripts/portfolio_validation.py --dry-run --report-dir docs/reports --build-dir "$BUILD_DIR"

echo ""
echo "Reports written to: docs/reports"
echo "Primary report: docs/reports/portfolio_validation_report.md"
echo "Benchmark/ablation report: docs/reports/make3d_benchmark_ablation_validation.md"
