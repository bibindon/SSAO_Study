# SSAO_Study

DirectX 9 ベースで SSAO(Screen Space Ambient Occlusion) を試すための学習用サンプルです。  
シーンを一度そのまま描画するだけでなく、深度、法線、背面深度、厚みといった中間情報を別テクスチャへ出力し、その結果を使って SSAO を計算して最後に合成します。

このリポジトリは、次のような人を主な読者として想定しています。

- マルチレンダーターゲット(MRT)の流れを実際のコードで追いたい人
- 「SSAO が何を見て暗くしているのか」を段階的に理解したい人
- シェーダーで深度や法線をどう扱うのかを学びたい人
- C++ 側からシェーダーへパラメータを渡す構成例を見たい人

## 概要

本サンプルは、概ね次の 2 段階で描画を行います。

1. ジオメトリを描画して、カラー、深度、法線、線形深度、背面深度のような中間情報をテクスチャへ書き出す
2. それらのテクスチャを画面全体のポストプロセスとして読み取り、厚み計算、SSAO 計算、ぼかし、最終合成を行う

「画面空間の AO」という名前の通り、SSAO は 3D モデルそのものを再度詳しく調べるのではなく、すでに描いた結果である深度バッファや法線情報を使って、画面上の各ピクセルがどれだけ周囲に遮られているかを近似します。

## 主な見どころ

- DirectX 9 + HLSL effect(.fx) で構成された比較的コンパクトなパイプライン
- MRT を使って 1 回の描画で複数の中間情報を出力
- 法線テクスチャと深度テクスチャを使った SSAO
- 背面深度から厚み(thickness)を求めて AO 判定の補助に利用
- 深度と法線を見ながら行うエッジ保持ぼかし
- 中央付近の実深度を読んで SSAO の距離パラメータを自動調整する実験機能
- デバッグ表示で深度、法線、厚み、背面深度を確認可能

## プロジェクト構成

主要ファイルは以下です。

- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/main.cpp`
  アプリケーションのエントリポイントとグローバル状態の定義です。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/app_shared.h`
  共有定数、共有状態、関数宣言をまとめたヘッダです。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/d3d_resources.cpp`
  Direct3D 初期化、エフェクト読み込み、メッシュ読み込み、レンダーターゲット生成などを担当します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/camera_input.cpp`
  カメラ移動、マウスルック、キーボード操作、簡易トグル入力を担当します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/rendering.cpp`
  描画パス全体の流れ、SSAO 自動調整、フルスクリーンクアッド描画を担当します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/tool_dialog.cpp`
  設定ダイアログの UI と各パラメータ編集処理を担当します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/simple.fx`
  ジオメトリ描画用のシェーダーです。カラー、深度、法線、背面深度を書き出します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/simple2.fx`
  ポストプロセス側のシェーダーです。厚み、SSAO、ぼかし、デバッグ表示、最終合成を担当します。

## ビルド方法

`AGENTS.md` にある通り、MSBuild 実行時には `Path` と `PATH` の二重定義があると `MSB6001` が出ることがあります。  
そのため、ビルド前に環境変数を確認し、`PATH` ではなく `Path` に統一してください。

MSBuild.exe の場所:

- `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin`

例:

```powershell
[System.Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
$msbuildDir = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin'
$env:Path = "$msbuildDir;" + $env:Path
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'C:\Users\bibindon\source\repos\bibindon\SSAO_Study\SSAO_Study\MultiPassRenderingAndMultiRenderTarget\MultiPassRendering.sln' `
  /t:Build /p:Configuration=Debug /p:Platform=x64
```

## 実行時の基本操作

- `W / A / S / D`: 前後左右移動
- `E / Q`: 上下移動
- `Mouse`: カメラ回転
- `2` または `Esc`: マウスカーソル表示の切り替え
- `3`: Lambert ライティングの ON/OFF
- `4`: 設定ダイアログの表示/非表示
- `5`: SSAO の ON/OFF
- `6`: テクスチャ表示の ON/OFF
- `F1`: 深度表示
- `F2`: 法線表示
- `F3`: 厚み表示
- `F4`: 背面深度表示

## 設定ダイアログの主な項目

### SSAO sample dist (m)

サンプリング位置を法線方向へどれくらい離すかをメートル相当の距離で指定します。  
大きくすると遠くの遮蔽も拾いやすくなりますが、ハローや不自然な影が出やすくなります。

### SSAO sample count

1 ピクセルあたり何回 SSAO 判定を行うかです。  
大きいほど滑らかになりますが、そのぶん計算コストが増えます。

### SSAO depth range (m)

深度を 0..1 に正規化するための基準距離です。  
シェーダー内ではこの値を使って「現在の深度値が何メートル相当か」を復元しています。

### Use thickness for SSAO

背面深度から推定した厚みを AO 判定に使うかどうかです。  
オンにすると、薄い形状や物体の内部感を少し自然に扱える場合があります。

### Thickness scale / Enable thickness cap / Thickness cap

厚みテクスチャの影響量や、極端な厚み値を捨てるための上限です。  
厚み推定は画面空間での近似なので、過剰な値がノイズになる場合に制限します。

### Normal bias scale / Depth bias scale / Depth compare dist / Depth bias dist

SSAO 判定時のバイアス調整です。  
自己遮蔽を減らしたり、法線方向に少し余裕を持たせたりするために使います。  
この種の値は見た目とアーティファクトのトレードオフ調整です。

### Depth scaled sample distance / Use fixed SSAO sample dist

深度に応じてサンプル距離を伸縮させるか、あるいは固定距離ベースでサンプルを並べるかを切り替えます。  
どちらがよいかは、シーンのスケール感と欲しい見た目によります。

### Auto scale SSAO by center depth / Smooth auto SSAO

画面中央近辺の深度を読み取り、その距離に応じて SSAO の深度範囲やサンプル距離を自動調整します。  
`Smooth auto SSAO` をオンにすると、値を即座に切り替えず少しずつ追従させます。

### Enable SSAO blur / 5x5 blur / 11x11 blur / 21x21 blur

SSAO のノイズを減らすぼかし設定です。  
単純なぼかしではなく、深度と法線を見てエッジをまたぎにくい重みづけを行っています。

### Shadow strength / Boost saturation in shadow / Shadow saturation

SSAO を最終カラーへどれだけ強く掛けるか、および影部分で彩度をどれだけ強めるかの設定です。  
物理ベースというより、見た目の調整用です。

### Remote Desktop

リモートデスクトップ環境でもマウスルックしやすいよう、相対移動ベースのカメラ操作へ切り替えます。

### Allow straight up/down

カメラの上下回転角の制限を少し緩め、真上・真下にかなり近い方向まで見られるようにする設定です。

## 描画の仕組み

### 1. ジオメトリ描画パス

`simple.fx` の `TechniqueMRT` でシーンを描画します。  
このとき、1 枚のカラーバッファだけでなく複数のレンダーターゲットへ同時に出力します。

書き出している主な内容:

- 通常のカラー
- 0..1 に正規化した前面深度
- ビュー空間法線
- 実メートル相当の線形深度

さらに `TechniqueBackDepth` では裏面だけを描画し、背面深度を得ています。  
前面深度と背面深度の差を見ることで、「このピクセル位置の物体はどれくらいの厚みを持つか」を近似できます。

### 2. 厚みパス

`simple2.fx` の `TechniqueThickness` では、前面深度と背面深度の差を取り、厚みテクスチャを作ります。

厚みは次のような場面で役立ちます。

- 物体が薄いのか厚いのかを AO 判定に反映したいとき
- 1 枚の深度だけでは「表面のすぐ裏に何があるか」が分かりにくいとき

### 3. SSAO パス

`TechniqueSsao` では、各ピクセルの法線方向へ少しずつサンプル点を動かし、その位置の深度や厚みを見て遮蔽されているかを判定します。

大まかな考え方:

1. 現在ピクセルの深度と法線を読む
2. 深度からビュー空間座標を復元する
3. 法線方向へ少しずつサンプル位置をずらす
4. ずらした位置を再びスクリーン座標へ投影する
5. その場所にある深度と比較し、遮蔽があるかを調べる
6. 遮蔽率を平均して AO 係数にする

### 4. SSAO ぼかしパス

SSAO はサンプル数を抑えるとノイズが出やすいため、後段でぼかします。  
ただし、ただ平均するだけだと物体の輪郭をまたいで影がにじむので、本サンプルでは深度差と法線差から重みを作っています。

つまり、

- 深度が大きく違うピクセル
- 法線の向きが大きく違うピクセル

に対しては重みを小さくし、面の連続性が高いところだけをよく混ぜるようにしています。

### 5. 最終合成パス

最後に元カラーと SSAO 結果を掛け合わせます。  
`Shadow strength` で暗くする強さを調整し、必要なら `Boost saturation in shadow` で影部分の彩度を少し強めます。

## シェーダー初学者向け補足

### クリップ空間と NDC

頂点シェーダーで `mul(position, WorldViewProj)` を行うと、頂点はクリップ空間へ変換されます。  
その後、GPU が内部で `x/w, y/w, z/w` を計算し、画面に対応する座標へ変換します。

### 線形深度を別に持つ理由

通常の深度値は投影後の値なので、カメラからの距離に対して非線形です。  
SSAO のように「実際にどれくらい離れているか」を使いたい処理では、ビュー空間 `z` をそのまま保存した線形深度のほうが扱いやすくなります。

### 法線を 0..1 にエンコードする理由

法線ベクトルの成分は普通 `-1..1` を取りますが、そのままでは一般的なカラーテクスチャに格納しづらいです。  
そのため `normal * 0.5 + 0.5` のようにして `0..1` へ写し替えて保存し、読むときに `* 2 - 1` で戻します。

### thickness とは何か

このサンプルでの thickness は「表面から裏面までの深度差」です。  
実際の物体厚を完全に表すわけではありませんが、画面空間の範囲では「この方向にどれくらい物体が詰まっているか」の近似として使えます。

## デバッグ表示の見方

- 深度: カメラに近いほど暗い/明るいのどちらで表示されるかはシェーダー実装依存ですが、距離の分布を見るための表示です
- 法線: RGB が法線の XYZ 方向を表します
- 厚み: 明るいほど前面と背面の差が大きいことを意味します
- 背面深度: 裏面側の深度を可視化したものです

これらを切り替えながら見ると、「SSAO がなぜその場所を暗くしたのか」を追いやすくなります。

## 学習の進め方の例

1. まず `simple.fx` を見て、どの情報を MRT に書き出しているか確認する
2. 次に `simple2.fx` の `ReconstructViewPosition` と `ProjectViewPositionToTexCoord` を読み、深度と座標変換の関係を理解する
3. `ComputeOcclusionSample` で、どの条件をもって「遮蔽あり」としているかを読む
4. `TechniqueThickness` と `TechniqueSsao` の結果をデバッグ表示で比較する
5. ぼかし ON/OFF とカーネルサイズ変更で、ノイズと輪郭保持の差を見る

## 備考

- 本サンプルは理解しやすさ優先の実験コードです。高速化よりも、各段階を分けて観察しやすい構成を重視しています。
- `simple.fx` / `simple2.fx` は `.fx` ファイルなので、文字コードは UTF-8 BOM なしを前提にしています。
- C++ ソースは UTF-8 BOM ありを前提にしています。
