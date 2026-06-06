============================================================
Make3D
============================================================

■ 作品概要
Make3D は、2D キャラクター画像やプロップ画像から、ゲーム開発用の proxy / hero asset を生成する C++17 / Win32 asset generation pipeline です。
単一画像から完璧な 3D モデルを自動生成するツールではなく、foreground mask、mask refinement、pseudo-depth、shape inference、local learned-shape inference、geometry generation、semantic vertex-color glTF、debug image、report、CI artifact までを含む、検証可能なローカルパイプラインとして制作しました。

■ 実行環境等を含めた実行方法
リポジトリ：MKyosei2/Make3D
実行環境：
- CMake 3.20 以上
- C++17 対応コンパイラ
- Windows 環境推奨
- Visual Studio / MSVC 推奨

補足：CMakeLists.txt では C++17 を指定し、Make3DAdvancedCore、CLI、Win32 GUI、sample generator、test target を定義しています。

基本 build 手順：
1. コマンドプロンプト、PowerShell、または Developer PowerShell を開きます。
2. Make3D のリポジトリ直下へ移動します。
3. 以下を実行します。

   cmake -S . -B build
   cmake --build build --config Release
   ctest --test-dir build -C Release --output-on-failure

Production sample 生成：
1. build 後、以下を実行します。

   build/Release/Make3DGenerateProductionPipelineSample.exe build/production_pipeline

2. 生成物を確認します。

   build/production_pipeline/OPEN_THIS_FIRST.txt
   build/production_pipeline/output/hero/make3d_hero_character_vertex_color.gltf
   build/production_pipeline/output/hero/make3d_hero_character_material.gltf
   build/production_pipeline/output/hero/make3d_hero_character.obj
   build/production_pipeline/output/production_report.md
   build/production_pipeline/output/production_report.json
   build/production_pipeline/output/debug_mask_refined.ppm
   build/production_pipeline/output/debug_depth_inferred.ppm
   build/production_pipeline/output/debug_depth_learned.ppm

学習済み shape weights を生成して使う場合：

   build/Release/Make3DTrainLearnedShapeModel.exe build/learned_shape_training
   build/Release/Make3DGenerateProductionPipelineSample.exe build/production_pipeline build/learned_shape_training/learned_shape.weights

GitHub Actions で確認する場合：
1. Actions の Make3D Advanced Core workflow を実行します。
2. artifact の中から以下を確認します。
   make3d-production-pipeline-sample
3. 最初に開くファイルは以下です。
   production_pipeline/OPEN_THIS_FIRST.txt
   production_pipeline/output/hero/make3d_hero_character_vertex_color.gltf

■ プログラムを作成する上で苦労した箇所
1. 作品のスコープを正しく制御すること
   単一画像から完璧な 3D モデルを作れると主張すると、実際の品質とズレが出ます。そのため、完璧な自動生成ではなく、debug image や report を含む inspectable な asset generation pipeline として設計しました。

2. 生成過程を段階化して検証可能にすること
   foreground extraction、mask refinement、pseudo-depth、shape inference、local learned-shape inference、hero mesh generation、semantic coloring、export、report という stage に分け、どこで何が起きたかを確認できるようにしました。

3. 見せるべき出力を絞ること
   generic な fallback mesh は見た目が弱い場合があるため、レビュワーには hero-only production sample を見てもらうようにしました。誤って壊れた fallback output が主成果物として見られないよう、hero-only regression guard を入れています。

4. C++ asset pipeline としての構成
   GUI、CLI、sample generator、trainer、test、CI artifact を分けて、単発のプログラムではなく pipeline として確認できる構成にしました。

5. OBJ / glTF / vertex color / report 出力
   生成された mesh を外部ビューアや DCC ツールで確認できるよう、OBJ と glTF を出力し、semantic vertex color や Markdown / JSON report も生成するようにしました。

■ 力を入れて作った部分 / プログラム上で特に注意して見てもらいたい箇所
1. Production pipeline
   ファイル：src/Make3DProductionPipeline.cpp
   見てほしい点：
   - 入力画像の読み込み、mask、depth、shape inference、learned shape、game asset generation、hero export、debug image、report 出力までをまとめています。
   - reviewer が最初に見る hero output を安定して生成する中心部分です。

2. Game asset generation
   ファイル：src/Make3DGameAssetPipeline.cpp
   見てほしい点：
   - mask stats を使って asset type や形状を推定し、building / prop / environment / character などの typed mesh を生成します。
   - 画像から直接きれいなモデルを作るのではなく、ゲーム開発用 proxy asset として成立する構造を目指しています。

3. Hero character geometry
   ファイル：src/Make3DHeroCharacterModel.cpp / src/Make3DHeroCharacterModel.h
   見てほしい点：
   - head、torso、arms、legs などの character-specific geometry を作る部分です。
   - 2D 入力から hero-style asset に見せるための構造的な prior を入れています。

4. Detail pass
   ファイル：src/Make3DHeroDetailEnhancer.cpp / src/Make3DHeroDetailEnhancer.h
   ファイル：src/Make3DHeroFineDetailPass.cpp / src/Make3DHeroFineDetailPass.h
   見てほしい点：
   - hair volume、clothing shell、hands、feet、face detail、hair strand hints、clothing folds などを追加する部分です。
   - 最終的な見た目を hero asset らしくするために力を入れています。

5. Semantic glTF exporter
   ファイル：src/Make3DHeroSemanticGltfExporter.cpp / src/Make3DHeroSemanticGltfExporter.h
   見てほしい点：
   - COLOR_0 を使った semantic vertex-color glTF を出力する部分です。
   - レビュワーが glTF viewer で asset の semantic coloring を確認できるようにしています。

6. Shape inference / learned shape
   ファイル：src/Make3DShapeInference.cpp
   ファイル：src/Make3DLearnedShapeModel.cpp / src/Make3DLearnedShapeModel.h
   ファイル：src/Make3DLearnedShapeTrainer.cpp / src/Make3DLearnedShapeTrainer.h
   見てほしい点：
   - silhouette、luminance、depth bias、built-in weights / external weights を使って疑似 depth を調整する部分です。
   - 外部サービスに依存せず、ローカルで動く推論 stage として作っています。

7. Regression test / CI
   ファイル：tests/Make3DHeroOnlyProductionTest.cpp
   ファイル：.github/workflows/make3d-advanced.yml
   見てほしい点：
   - hero-only production sample が expected artifact を生成することを test / CI で確認できるようにしています。
   - build、test、sample generation、artifact upload まで自動化しています。

■ 参考にしたソースファイルについて
外部の特定ソースファイルをコピーして実装したものはありません。
参考にした考え方は、C++17 の標準的な設計、Win32 GUI、CMake / CTest、OBJ / glTF の一般的なフォーマット構造、画像処理における foreground mask / depth map / shape inference、game asset pipeline における debug artifact / report / regression test の考え方です。

作品内で実装意図を確認しやすいファイル：
- README.md
- docs/REVIEWER_BRIEF.md
- CMakeLists.txt
- src/Make3DProductionPipeline.cpp
- src/Make3DGameAssetPipeline.cpp
- src/Make3DHeroCharacterModel.cpp
- src/Make3DHeroDetailEnhancer.cpp
- src/Make3DHeroFineDetailPass.cpp
- src/Make3DHeroSemanticGltfExporter.cpp
- src/Make3DShapeInference.cpp
- src/Make3DLearnedShapeModel.cpp
- src/Make3DLearnedShapeTrainer.cpp
- tests/Make3DHeroOnlyProductionTest.cpp
- .github/workflows/make3d-advanced.yml


============================================================
4. 3作品をまとめたアピール方針
============================================================

3作品は別々のツールですが、共通テーマは「ゲーム開発向け asset pipeline tooling」です。

- MAYAtoUnity：DCC データを Unity へ移し、情報の欠落や未対応を report するツール
- AssetUtility：Unity 内の asset cost を確認し、安全に最適化する Editor tool
- Make3D：2D input から reviewable な proxy / hero asset を生成し、debug artifact と report を出す C++ pipeline

共通して重視した点：
- 非破壊性
- traceability
- validation
- before / after report
- reviewer が確認しやすい出力
- CI / test / sample による再現性

書類・動画で使う説明文：
Maya / Unity / C++ を用いて、DCC データ移行、Unity アセット QA、2D-to-3D proxy generation を扱う制作支援ツール群を開発しました。各ツールで、検証ログ、before-after report、unsupported feature の明示、CI artifact 生成を重視し、アーティストとエンジニアが確認可能な asset pipeline を設計しました。
