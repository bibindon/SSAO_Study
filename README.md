# SSAO_Study

## 概要

このリポジトリは、DirectX 9 で SSAO(Screen Space Ambient Occlusion) を試した実装をまとめたものです。  
1 回目の描画でカラーや深度、法線、背面深度といった中間情報を作り、2 回目の描画でそれらを使って thickness、SSAO、ぼかし、最終合成を行います。

いわゆる「画面空間 AO」なので、3D 空間全体を厳密に解くというより、すでに描画した結果を使って「このピクセルは周囲にどれくらい遮られていそうか」を近似的に求める構成です。  
その中で、この実装では次の点を観察しやすいようにしています。

- MRT を使って中間情報を一度に出力していること
- 背面深度から thickness を作って AO 判定の補助に使っていること
- 深度と法線を見ながらエッジ保持ぼかしをかけていること
- 画面中央付近の実深度を読んで、SSAO の距離パラメータを自動調整していること

## プロジェクト構成

主なファイルは以下です。

- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/main.cpp`
  エントリポイントとグローバル状態の定義です。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/app_shared.h`
  定数、共有状態、関数宣言をまとめたヘッダです。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/d3d_resources.cpp`
  Direct3D 初期化、メッシュ読み込み、エフェクト読み込み、レンダーターゲット生成を担当します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/camera_input.cpp`
  カメラ操作とキーボード/マウス入力を担当します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/rendering.cpp`
  描画パス全体、SSAO 自動調整、フルスクリーンクアッド描画を担当します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/tool_dialog.cpp`
  設定ダイアログの生成と、各種パラメータ編集処理を担当します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/simple.fx`
  ジオメトリ描画用シェーダーです。カラー、深度、法線、背面深度などを出力します。
- `SSAO_Study/MultiPassRenderingAndMultiRenderTarget/MultiPassRendering/simple2.fx`
  ポストプロセス用シェーダーです。thickness、SSAO、ぼかし、デバッグ表示、最終合成を担当します。

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

設定項目はほとんどが SSAO の評価条件か、最終合成の見た目に直接つながっています。

### SSAO sample dist (m)

法線方向へどれくらい離れた位置をサンプルするかを決める値です。  
大きくすると広い範囲の遮蔽を拾いやすくなりますが、そのぶんハローや不自然な影も出やすくなります。

### SSAO sample count

1 ピクセルあたりの遮蔽判定回数です。  
増やすほどノイズは減りますが、当然コストは上がります。

### SSAO depth range (m)

深度を 0..1 に正規化するときの基準距離です。  
シェーダー側では、この値を使って深度値からビュー空間距離を復元しています。

### Use thickness for SSAO

背面深度との差から作った thickness を AO 判定に使うかどうかの設定です。  
薄い形状や内部空間を少し意識した見え方にしたいときに効いてきます。

### Thickness scale / Enable thickness cap / Thickness cap

thickness の効き方と、極端な値を切るための上限設定です。  
画面空間で作った厚みなので、条件によっては大きすぎる値がノイズ源になります。

### Normal bias scale / Depth bias scale / Depth compare dist / Depth bias dist

自己遮蔽や誤判定を抑えるためのバイアス群です。  
このあたりは、きれいな陰影と安定性のバランスを取るための調整項目だと思っておくと分かりやすいです。

### Depth scaled sample distance / Use fixed SSAO sample dist

サンプル距離を深度に応じて変えるか、固定距離ベースで並べるかの切り替えです。

### Auto scale SSAO by center depth / Smooth auto SSAO

画面中央付近の実深度を読んで、その距離に応じて SSAO の深度範囲やサンプル距離を自動調整します。  
`Smooth auto SSAO` を有効にすると、値は即座に切り替わらず、少しずつ追従します。

### Enable SSAO blur / 5x5 blur / 11x11 blur / 21x21 blur

SSAO 結果に対するぼかし設定です。  
単純な平均ではなく、深度差と法線差を見てエッジをまたぎにくい重み付けをしています。

### Shadow strength / Boost saturation in shadow / Shadow saturation

SSAO を最終カラーへどれだけ強く掛けるか、影部分の彩度をどれだけ強めるかの設定です。  
後者は物理ベースというより、見た目を調整するための機能です。

### Remote Desktop

リモートデスクトップ環境でも扱いやすいように、マウスルック処理を相対移動ベースへ切り替えます。

### Allow straight up/down

カメラの上下回転制限を少し緩めて、真上・真下にかなり近い方向まで見られるようにします。

## 描画の流れ

### 1. ジオメトリ描画パス

`simple.fx` の `TechniqueMRT` でシーンを描画し、複数のレンダーターゲットへ中間情報を書き出します。  
出力している主な内容は以下です。

- 通常カラー
- 0..1 に正規化した前面深度
- ビュー空間法線
- ビュー空間の実深度

そのあと `TechniqueBackDepth` で背面だけを描画し、背面深度を作っています。  
前面深度と背面深度の差を見ると、そのピクセル位置で物体がどれくらいの厚みを持っているかを近似できます。

### 2. 厚みパス

`simple2.fx` の `TechniqueThickness` では、前面深度と背面深度の差から thickness テクスチャを作ります。  
これは厳密な物体厚ではありませんが、「表面のすぐ裏にどれくらい空間があるか」を見るには十分役立ちます。

### 3. SSAO パス

`TechniqueSsao` では、現在ピクセルの深度と法線からビュー空間位置を復元し、法線方向へ複数のサンプル点を飛ばします。  
そのサンプル点を再びスクリーン座標へ戻して、そこに存在する深度や thickness と比較し、遮蔽されているかを判定します。

大まかな流れは次の通りです。

1. 深度値からビュー空間位置を復元する
2. 法線方向へサンプル位置を動かす
3. その位置をスクリーン UV へ再投影する
4. 投影先の深度を読む
5. 必要なら thickness も加味して遮蔽判定する
6. 複数サンプルの結果を平均して SSAO 係数にする

### 4. SSAO ぼかしパス

SSAO はサンプル数を抑えるとどうしてもノイズが残ります。  
そこで後段でぼかしをかけますが、単純に平均してしまうと輪郭をまたいで影がにじむので、深度差と法線差に応じて重みを下げています。

その結果、平坦な面の中ではよく混ざり、形状の境界では混ざりにくいぼかしになっています。

### 5. 最終合成パス

最後に元カラーへ SSAO 結果を掛け合わせます。  
`Shadow strength` で暗さの強さを調整し、必要なら影部分の彩度も少し持ち上げます。

## 実装上のポイント

### 深度を 2 種類持っている理由

この実装では、前面深度を 0..1 の正規化値として持つ一方、実深度も別テクスチャへ保存しています。  
前者は比較しやすく、後者は「実際にどれくらいの距離か」を扱いやすいので、用途を分けています。

### 法線を 0..1 にして保存する理由

法線ベクトルは本来 `-1..1` の範囲を取りますが、そのままでは普通のカラーテクスチャへ入れづらいので、`normal * 0.5 + 0.5` で `0..1` へ写して保存しています。  
読み戻すときは `* 2 - 1` で元の範囲へ戻します。

### thickness の意味

ここでの thickness は、前面深度と背面深度の差から作った画面空間近似です。  
なので、厳密な物体厚というより「この画素位置で表面の裏側がどれくらい離れているか」の目安として使っています。

### エッジ保持ぼかし

ぼかしでは深度差と法線差を両方見ています。  
深度が大きく違うピクセルや、法線の向きが大きく違うピクセルは重みを小さくして、輪郭が崩れにくいようにしています。

## デバッグ表示

デバッグ表示では中間テクスチャの内容を確認できます。

- 深度: 前面深度の可視化
- 法線: ビュー空間法線の可視化
- 厚み: 前面深度と背面深度の差の可視化
- 背面深度: 背面側の深度分布の可視化

これらを見比べると、SSAO がどの情報をもとに暗さを作っているのかが追いやすくなります。


