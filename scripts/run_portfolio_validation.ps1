$ErrorActionPreference = 'Stop'

$Root = Resolve-Path (Join-Path $PSScriptRoot '..')
Set-Location $Root

$BuildDir = if ($args.Count -gt 0) { $args[0] } else { 'build' }

cmake -S . -B $BuildDir -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --config Release --parallel 2
ctest --test-dir $BuildDir -C Release --output-on-failure

$Generator = Join-Path $BuildDir 'Make3DGenerateProductionPipelineSample'
$GeneratorRelease = Join-Path $BuildDir 'Release/Make3DGenerateProductionPipelineSample.exe'
if (Test-Path $Generator) {
    & $Generator (Join-Path $BuildDir 'portfolio_sample')
} elseif (Test-Path $GeneratorRelease) {
    & $GeneratorRelease (Join-Path $BuildDir 'portfolio_sample')
} else {
    throw 'Make3DGenerateProductionPipelineSample executable not found.'
}

$Input = Join-Path $BuildDir 'portfolio_sample/input_noisy_character.tga'
if (!(Test-Path $Input)) {
    $Input = Get-ChildItem (Join-Path $BuildDir 'portfolio_sample') -Recurse -Include *.png,*.tga,*.jpg,*.jpeg | Select-Object -First 1 -ExpandProperty FullName
}
if (!(Test-Path $Input)) {
    throw 'No generated image input found for CLI benchmark.'
}

$Cli = Join-Path $BuildDir 'Make3DAdvancedCLI'
$CliExe = Join-Path $BuildDir 'Make3DAdvancedCLI.exe'
$CliRelease = Join-Path $BuildDir 'Release/Make3DAdvancedCLI.exe'
if (Test-Path $Cli) {
    & $Cli --input $Input --output (Join-Path $BuildDir 'cli_benchmark') --benchmark --ablation no-shape-inference
} elseif (Test-Path $CliExe) {
    & $CliExe --input $Input --output (Join-Path $BuildDir 'cli_benchmark') --benchmark --ablation no-shape-inference
} elseif (Test-Path $CliRelease) {
    & $CliRelease --input $Input --output (Join-Path $BuildDir 'cli_benchmark') --benchmark --ablation no-shape-inference
} else {
    throw 'Make3DAdvancedCLI executable not found.'
}

python scripts/validate_benchmark_ablation.py $BuildDir docs/reports
python scripts/portfolio_validation.py --dry-run --report-dir docs/reports --build-dir $BuildDir

Write-Host ""
Write-Host "Reports written to: docs/reports"
Write-Host "Primary report: docs/reports/portfolio_validation_report.md"
Write-Host "Benchmark/ablation report: docs/reports/make3d_benchmark_ablation_validation.md"
