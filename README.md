
https://github.com/user-attachments/assets/f81537c3-a208-4f8c-832a-a20ac2a77f3e
 
 # SSAO_Study

## 概要

本リポジトリは、DirectX 9 上で Screen Space Ambient Occlusion(SSAO) を試行した実装を整理したものである。  
描画は単純な単一パスではなく、カラー、前面深度、法線、実深度、背面深度といった中間情報を複数のレンダーターゲットへ出力し、それらを後段のポストプロセスで参照して SSAO を構成している。

対象プロジェクトは次の性質を持つ。

- DirectX 9 + HLSL effect(.fx) を用いた構成である
- 1 回目の描画で中間バッファ群を生成する
- 2 回目の描画で thickness、SSAO、ぼかし、最終合成を行う
- 実行中にパラメータを変更できるダイアログを備える
- デバッグ表示により深度、法線、厚み、背面深度を確認できる

本実装は、SSAO を画面空間近似として扱い、その成立条件と限界を観察しやすい構成になっている。とくに、背面深度から thickness を求めて AO 判定へ補助的に組み込んでいる点、深度と法線を用いたエッジ保持ぼかしを行っている点、中心深度に応じた自動スケーリング機構を備える点が特徴である。

## プロジェクト構成

主要ファイルは以下のとおりである。

- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/main.cpp`
  エントリポイントおよびグローバル状態の定義。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/app_shared.h`
  共有定数、共有状態、関数宣言を集約したヘッダ。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/d3d_resources.cpp`
  Direct3D 初期化、エフェクト読み込み、メッシュ読み込み、レンダーターゲット生成を担当。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/camera_input.cpp`
  カメラ移動、マウスルック、キーボード入力、簡易トグル入力を担当。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/rendering.cpp`
  描画パス全体、SSAO 自動調整、フルスクリーンクアッド描画を担当。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/tool_dialog.cpp`
  設定ダイアログの生成と各種パラメータ編集処理を担当。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/simple.fx`
  ジオメトリ描画用シェーダー。カラー、深度、法線、背面深度の生成を担当。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/simple2.fx`
  ポストプロセス用シェーダー。thickness、SSAO、ぼかし、デバッグ表示、最終合成を担当。

## ビルド方法

`AGENTS.md` に記載のとおり、MSBuild 実行時に `Path` と `PATH` が同時に存在すると `MSB6001` が発生する場合がある。  
したがって、ビルド前に環境変数を確認し、`PATH` ではなく `Path` に統一する必要がある。

MSBuild.exe の所在:

- `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin`

実行例:

```powershell
[System.Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
$msbuildDir = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin'
$env:Path = "$msbuildDir;" + $env:Path
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'C:\Users\bibindon\source\repos\bibindon\SSAO_Study\SSAO_Study\MultiPassRenderingAndMultiRenderTarget\MultiPassRendering.sln' `
  /t:Build /p:Configuration=Debug /p:Platform=x64
```

## 実行時操作

- `W / A / S / D`: 前後左右移動
- `E / Q`: 上下移動
- `Mouse`: カメラ回転
- `2` または `Esc`: マウスカーソル表示切り替え
- `3`: Lambert ライティング ON/OFF
- `4`: 設定ダイアログ表示/非表示
- `5`: SSAO ON/OFF
- `6`: テクスチャ表示 ON/OFF
- `F1`: 深度表示
- `F2`: 法線表示
- `F3`: 厚み表示
- `F4`: 背面深度表示

## 設定ダイアログ

各項目は、ほぼそのまま SSAO の評価条件または最終合成条件に対応している。

### SSAO sample dist (m)

法線方向へのサンプリング距離である。  
値を大きくすると、より広い範囲の遮蔽を拾いやすくなる一方で、ハローや過剰な陰影が生じやすくなる。

### SSAO sample count

1 ピクセルあたりの遮蔽判定回数である。  
増加させるとノイズは減少するが、計算負荷は増大する。

### SSAO depth range (m)

深度を 0..1 に正規化する際の基準距離である。  
シェーダー内では、この値を用いて深度値からビュー空間距離を復元している。

### Use thickness for SSAO

背面深度との差から得た thickness を AO 判定へ利用するかどうかを指定する。  
薄い物体や中空に近い見え方を持つ形状では、判定の安定化に寄与する場合がある。

### Thickness scale / Enable thickness cap / Thickness cap

thickness の寄与量と上限値に関する設定である。  
画面空間近似に由来する極端な厚み値を抑制し、ノイズや過剰遮蔽を軽減する目的で用いる。

### Normal bias scale / Depth bias scale / Depth compare dist / Depth bias dist

自己遮蔽や誤判定を抑えるためのバイアス群である。  
いずれも AO の安定性と陰影の強さの間にあるトレードオフ調整項目である。

### Depth scaled sample distance / Use fixed SSAO sample dist

サンプル距離を深度に応じて変化させるか、あるいは固定距離ベースで配置するかを切り替える項目である。

### Auto scale SSAO by center depth / Smooth auto SSAO

画面中央付近の実深度を読み取り、その値に応じて SSAO の深度範囲およびサンプル距離を自動調整する。  
`Smooth auto SSAO` を有効にすると、パラメータは即時切り替えではなく時間的に補間される。

### Enable SSAO blur / 5x5 blur / 11x11 blur / 21x21 blur

SSAO 結果に対する後段ぼかし設定である。  
本実装では単純平均ではなく、深度差および法線差を用いたエッジ保持型の重み付けを行う。

### Shadow strength / Boost saturation in shadow / Shadow saturation

SSAO を最終カラーへ反映する強度、および影部彩度の強調量に関する設定である。  
後者は物理的厳密性よりも見た目の調整を意図した機能である。

### Remote Desktop

リモートデスクトップ環境での操作性を考慮し、マウスルック処理を相対移動ベースへ切り替える。

### Allow straight up/down

カメラのピッチ制限を緩和し、真上・真下に近い方向まで視線を向けられるようにする。

## 描画パイプライン

### 1. ジオメトリ描画パス

`simple.fx` の `TechniqueMRT` によりシーンを描画し、複数のレンダーターゲットへ中間情報を書き出す。  
出力内容は以下である。

- 通常カラー
- 0..1 に正規化した前面深度
- ビュー空間法線
- ビュー空間の実深度

続いて `TechniqueBackDepth` を用い、背面のみを描画して背面深度を取得する。  
前面深度と背面深度の差は、同一画素位置における厚みの近似量として扱われる。

### 2. 厚みパス

`simple2.fx` の `TechniqueThickness` により、前面深度と背面深度の差から thickness テクスチャを生成する。  
これは 1 枚の深度テクスチャだけでは表現しにくい内部空間量の近似値として用いられる。

### 3. SSAO パス

`TechniqueSsao` では、現在画素の深度と法線からビュー空間位置を復元し、法線方向へ複数のサンプル点を配置する。  
各サンプル点は再投影によりスクリーン座標へ戻され、その位置に存在する深度および thickness と比較される。  
この比較結果から遮蔽率を算出し、最終的に SSAO 係数へ変換する。

処理の要点は次のとおりである。

1. 深度値からビュー空間位置を復元する
2. 法線方向へサンプル位置を移動させる
3. 移動先をスクリーン UV へ再投影する
4. 投影先の深度を参照し、想定深度との差を評価する
5. 必要に応じて thickness を加味し、遮蔽判定を行う
6. 全サンプルの結果を平均し、明度係数として出力する

### 4. SSAO ぼかしパス

SSAO はサンプルベースであるため、サンプル数が少ない条件ではノイズが残る。  
そのため、後段で 5x5、11x11、21x21 のいずれかのカーネルを用いてぼかしを行う。  
ただし、単純な平均化では輪郭をまたいで陰影がにじむため、重みは深度差と法線差に基づいて減衰される。

### 5. 最終合成パス

最終パスでは元カラーへ SSAO 結果を乗算的に反映する。  
`Shadow strength` により暗部の強さを制御し、必要に応じて影部彩度を増加させる。

## 実装上の要点

### 深度の二重保持

本実装では、前面深度を 0..1 の正規化値として保持する一方、実深度も別テクスチャへ保持している。  
前者は比較や画面空間処理に適し、後者は距離に応じたパラメータ自動調整に適する。

### 法線のエンコード

法線ベクトルは本来 `-1..1` の範囲を取るが、テクスチャへ保存するため `normal * 0.5 + 0.5` により `0..1` 範囲へ写像している。  
参照時には `* 2 - 1` により元の範囲へ戻す。

### thickness の位置付け

ここでいう thickness は、幾何学的に厳密な物体厚ではなく、前面深度と背面深度との差に基づく画面空間近似量である。  
したがって、視点依存であり、投影条件や形状配置の影響を受ける。

### エッジ保持ぼかし

SSAO ぼかしでは、深度差および法線差を重みへ反映することで、面の連続性が高い領域のみを強く混合する。  
これにより、単純平均よりも輪郭保持性の高い結果が得られる。

## デバッグ表示

デバッグ表示は中間テクスチャの内容確認を目的とする。

- 深度: 前面深度の可視化
- 法線: ビュー空間法線の可視化
- 厚み: 前面深度と背面深度の差の可視化
- 背面深度: 裏面側の深度分布の可視化

これらの表示を比較することで、SSAO の陰影がどの入力情報に依存しているかを観察できる。

## 備考

- 本実装は高速化よりも観察可能性を重視した構成である。
- `simple.fx` および `simple2.fx` は UTF-8 BOM なしを前提とする。
- C++ ソースおよび `README.md` は UTF-8 BOM ありを前提とする。
