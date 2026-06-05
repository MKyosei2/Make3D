# Make3D Portfolio Validation

This document is the reviewer-facing validation entry point.

## Local validation

Linux / macOS:

```bash
bash scripts/run_portfolio_validation.sh
```

Windows PowerShell:

```powershell
./scripts/run_portfolio_validation.ps1
```

The script runs:

```text
- CMake configure
- CMake build
- ctest
- production sample generation
- Make3DAdvancedCLI --benchmark
- Make3DAdvancedCLI --ablation no-shape-inference
- benchmark / ablation artifact validation
- portfolio report generation
```

Generated reports:

```text
docs/reports/portfolio_validation_report.md
docs/reports/portfolio_validation_report.json
docs/reports/make3d_generated_sample_manifest.json
docs/reports/make3d_benchmark_ablation_validation.md
docs/reports/make3d_benchmark_ablation_validation.json
```

Generated sample artifacts:

```text
build/portfolio_sample
build/cli_benchmark
build/cli_benchmark/benchmark/make3d_benchmark.json
build/cli_benchmark/benchmark/make3d_benchmark.md
build/cli_benchmark/ablations/ablation_report.csv
build/cli_benchmark/ablations/ablation_report.md
```

## Why this matters

The portfolio goal is not only to show image-to-3D generation. The goal is to show an R&D-style local pipeline with build reproducibility, tests, benchmark output, ablation output, generated assets, and artifact validation.
