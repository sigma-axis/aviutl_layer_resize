# 可変幅レイヤー AviUtl プラグイン

拡張編集のレイヤー幅を自由に調整できるようになります．

[ダウンロードはこちら．](https://github.com/sigma-axis/aviutl_layer_resize/releases)

https://github.com/user-attachments/assets/0d2fc4ae-0f99-4e33-b695-606564b438b8

[設定ファイルを編集](#設定ファイルについて)することで，縦方向だけでなく横方向にもできます．

![上下左右4方向に配置可能](https://github.com/user-attachments/assets/bc136f5a-1c05-4be2-880c-bdf5b80f7f8d)


## 動作要件

- AviUtl 1.10 + 拡張編集 0.92

  https://spring-fragrance.mints.ne.jp/aviutl
  - 拡張編集 0.93rc1 等の他バージョンでは動作しません．

- Visual C++ 再頒布可能パッケージ（\[2015/2017/2019/2022\] の x86 対応版が必要）

  https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist

## 導入方法

以下のフォルダのいずれかに `layer_resize.auf` と `layer_resize.ini` をコピーしてください．

1. `aviutl.exe` のあるフォルダ
1. (1) のフォルダにある `plugins` フォルダ
1. (2) のフォルダにある任意のフォルダ

## 使い方

メインウィンドウの `表示` メニューから `可変幅レイヤーの表示` を選択するとレイヤー幅調整用のウィンドウが表示されます．

ウィンドウサイズは自由に変更できるので使いやすいサイズに調節してください．

### ドラッグ操作

左クリックドラッグでレイヤー幅を変更できます．

- ドラッグ中は現在のレイヤー幅のピクセル数が表示されます．

- ドラッグ中に右クリックでドラッグ前の状態に戻ります．

### ホイール操作

マウスホイールでレイヤー幅を1ピクセル単位で増減できます．

### 右クリックメニュー

右クリックでコンテキストメニューが開いて，以下の操作が行えます:

![コンテキストメニュー](https://github.com/user-attachments/assets/a221986c-6473-48c4-8a5f-96a47527e443)

1.  レイヤー幅の `大`, `中`, `小` の切り替え．
1.  レイヤー幅のリセット．

    このプラグインによる介入前の値にリセットします．

    - `リセット` で現在選択中の `大`, `中`, `小` の項目のみが戻ります．
    - `全てリセット` で `大`, `中`, `小` の項目が 3 つとも戻ります．


### レイヤー名で `Ctrl`+ホイール

タイムラインウィンドウ左にあるレイヤー名にマウスカーソルがある状態で `Ctrl`+ホイールで，レイヤー幅を1ピクセル単位で増減できます．

**初期状態では無効化されているため，利用する場合は[設定ファイルを編集](#behavior)して有効化してください．**

## メニューコマンド

メインメニューの「編集」→「可変幅レイヤー」にメニューコマンドを追加しています．ショートカットキーに割り当ててレイヤー幅の調整ができます．

![追加されるメニューコマンド](https://github.com/user-attachments/assets/18043481-303d-4a20-be95-7ed521aa6113)

1.  レイヤー幅を小さく

    レイヤー幅を `1` ピクセル小さくします．

1.  レイヤー幅を大きく

    レイヤー幅を `1` ピクセル大きくします．

1.  レイヤー幅をリセット

    レイヤー幅をこのプラグインによる変更前の値に戻します．


## 設定ファイルについて

テキストエディタで `layer_resize.ini` を編集することで挙動をカスタマイズできます．ファイル内にも説明が書いてあるためそちらもご参照ください．

### `[layout]`

レイアウトに関する項目を設定できます．

- `orientation` でゲージの向きを4方向から選べます．

  ![上下左右4方向に配置可能](https://github.com/user-attachments/assets/bc136f5a-1c05-4be2-880c-bdf5b80f7f8d)

- `margin_x`, `margin_y` で余白を設定できます．
  - 最小値は `0`, 最大値は `64` です．

初期設定だと以下の通りです:
```ini
orientation=2
margin_x=0
margin_y=0
```
- 下方向でレイヤー幅が増える縦向きで，余白なし．

### `[color]`

配色を変更できます．色は 16 進法で `0xRRGGBB` の形式で記述します．

- `fill_lt`, `fill_rb` でゲージ値より小さい領域の色を指定します．
- `back_lt`, `back_rb` でゲージ値より大きい領域の色を指定します．
- `****_lt` でグラデーションの左 or 上の色を，`****_rb` で右 or 下の色を指定します．

初期設定だと以下の通りです:
```ini
fill_lt=0x80c0ff
fill_rb=0x0080ff
back_lt=0x000040
back_rb=0x0000c0
```

### `[behavior]`

挙動に関する項目の設定です．

- `scale` でゲージの尺度を 3 通りの中から選びます．

  | 数値 | 設定 | 説明 | 備考 |
  |:---:|:---:|:---|:---|
  | `0` | 線形スケール | レイヤー幅の変化量がゲージ移動量に比例 | 小さい値の範囲で変化が急激と感じやすい |
  | `1` | 対数スケール | レイヤー幅が倍加するゲージ移動量が一定 | タイムラインの拡大率ゲージの設計に近い |
  | `2` | 逆数スケール | レイヤー個数の変化量がゲージ移動量に比例 | 大きい値の範囲で変化が急激と感じやすい |

- `size_min`, `size_max` で変更可能なレイヤー幅の範囲を指定します．
  - 最小値は `4`, 最大値は `255` です．

- `hide_cursor` を `1` にすると，ドラッグ中にマウスカーソルが非表示になります．

- `delay_ms` でゲージを操作してから実際にレイヤー幅が変化するまでの時間に遅延を与えることができます．

  - タイムラインにオブジェクトが多くて動作が重い場合に効果的です．
  - 指定はミリ秒 ($\frac{1}{1000}$ 秒) 単位です．
  - `0` を指定すると無遅延でレイヤー幅に反映されます．
  - 最小値は `10` (特別扱いの `0` を除く), 最大値は `1000` です．

- `wheel` でホイール操作を無効化したり反転したりできます．

  - `1` で通常の挙動, `0` で無効化, `-1` でホイールの回転方向が反転します．

- `wheel_on_names` で[レイヤー名上での `Ctrl`+ホイール操作](#レイヤー名で-ctrlホイール)を有効化したり，ホイール回転方向を変更したりできます．

  - `1` で通常の挙動, `0` で無効化, `-1` でホイールの回転方向が反転します．

初期設定だと以下の通りです:
```ini
scale=1
size_min=15
size_max=50
hide_cursor=0
delay_ms=50
wheel=1
wheel_on_names=0
```
- 対数スケール．
- レイヤー幅は `15` -- `50` ピクセルの範囲．
- マウスカーソルは隠さない．
- 遅延量は 50 ミリ秒 (0.05 秒).
- ホイール操作は有効で，回転方向は順方向．
- レイヤー名上での `Ctrl`+ホイール操作は無効．

### `[tooltip]`

ドラッグ中の数値表示に関する設定です．

- `show` でこの表示を有効化 / 無効化できます．
- `offset_x`, `offset_y` で数値の表示位置を調整できます．
  - 最小値は `-128`, 最大値は `127` です．
- `text_color` で数値のテキスト色を変更できます．
  - `-1` でデフォルト色の指定です．
  - ダークモード化プラグインによっては見づらいテキスト色になってしまうことがあるため，その対処のための設定です．通常は初期値の `-1` のままで問題ありません．

初期設定だと以下の通りです:
```ini
show=1
offset_x=0
offset_y=24
text_color=-1
```
- 数値表示は有効で，表示位置はマウスカーソルから下方向に 24 ピクセル．テキスト色はデフォルト．


### `[states]`

このプラグインによって変更されたレイヤー幅が記録されます．基本的にはユーザが書き換える必要のない項目です．

この項目を `0` に指定 (or 削除) することで初期値に戻ります．


## TIPS

1.  patch.aul の設定 (`patch.aul.json` ファイルの編集) でレイヤー幅の初期値を変更できます．

    この設定による影響は AviUtl 起動中に変更できませんが，[`レイヤー幅のリセット`](#メニューコマンド) などで幅を戻すときの値にもなります．

1.  ダークモード化プラグイン非導入下の高 DPI 環境で「システム（拡張）」の設定だと，レイヤー境界にゴミ画像が残る場合があります．これは[TLオブジェクト描画拡張](https://github.com/sigma-axis/aviutl_tl_obj_styler)で[レイヤー枠の色を設定](https://github.com/sigma-axis/aviutl_tl_obj_styler/blob/master/README.md#%E3%83%AC%E3%82%A4%E3%83%A4%E3%83%BC%E3%81%AE%E4%B8%8A%E4%B8%8B%E5%B7%A6%E5%8F%B3%E3%81%AE%E6%9E%A0%E7%B7%9A%E3%81%AE%E8%89%B2%E3%82%92%E5%A4%89%E6%9B%B4)することで抑制できることがあります．蛇色様の[アルティメットプラグイン](https://github.com/hebiiro/anti.aviutl.ultimate.plugin)の[『拡張編集微調整』アドイン](https://github.com/hebiiro/anti.aviutl.ultimate.plugin/wiki/exedit_tweaker)でも同様の機能・効果があります．


## 改版履歴

- **v1.10** (2024-09-23)

  - タイムラインのレイヤー名で `Ctrl`+ホイールでレイヤー幅を増減する機能を追加．

- **v1.00** (2024-09-16)

  - 初版．


## ライセンス

このプログラムの利用・改変・再頒布等に関しては MIT ライセンスに従うものとします．

---

The MIT License (MIT)

Copyright (C) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

https://mit-license.org/


#  Credits

##  aviutl_exedit_sdk

https://github.com/ePi5131/aviutl_exedit_sdk

---

1条項BSD

Copyright (c) 2022
ePi All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
THIS SOFTWARE IS PROVIDED BY ePi “AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ePi BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#  連絡・バグ報告

- GitHub: https://github.com/sigma-axis
- Twitter: https://x.com/sigma_axis
- nicovideo: https://www.nicovideo.jp/user/51492481
- Misskey.io: https://misskey.io/@sigma_axis
- Bluesky: https://bsky.app/profile/sigma-axis.bsky.social
