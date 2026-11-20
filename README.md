# PPT2NX-AI

Switch 版「ぷよぷよテトリス2」で、AIが対戦相手になります。最大2体まで同時に動作できます。
より強い敵とswitch上で遊ぶことを目的としています。

> [!NOTE]
> このリポジトリは [PPT2NX-AI](https://github.com/steck0714/PPT2NX-AI)（steck0714）から派生したものです。AI の名前とリポジトリ名は今後変更する予定ですが、派生元の表記は残します。派生元の利用条件は派生元の README.md と NOTICE.md に従ってください。

> [!WARNING]
> - 改造した Switch（Atmosphere）向けの非公式ツールです。自己責任で使用してください。
> - **ネット対戦では動作しません。** オンラインで対戦しようとすると AI は止まり、ゲームを終了するまで動きません。
> - セガ、任天堂、Zetris の作者とは関係ありません。

## 必要なもの

| 名前 | 役割 |
|---|---|
| [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere/releases) | CFW |
| [nx-ovlloader](https://github.com/WerWolv/nx-ovlloader/releases)（v1.0.7） | オーバーレイ |
| [Tesla Menu](https://github.com/WerWolv/Tesla-Menu/releases)（v1.2.3） | オーバーレイ |
| ぷよぷよテトリス2 | TID `0100F5801190A000`、BID `A9C09323F3A22B54` の版のみ |

約 19MB のメモリを使用します。

## 入れ方

1. nx-ovlloader と Tesla Menu を SD カードに入れる
2. [Releases](../../releases) の `PPT2NX-AI-v1.0.0.zip` を展開し、中身を SD カードのルートにコピーする
3. 本体を再起動する

更新も同じ手順です。sysmodule とオーバーレイは同じリリースのものを使ってください。

## 使い方

1. ぷよテト2 を起動する
2. オーバーレイで設定する
3. 戦う

- 試合中以外は、手元の ZR・ZL・右スティックが AI の仮想コントローラーの A・B・十字キーになります（右スティック押し込みで AI を切り替え）。「仮想コントローラーを操作」でオフにできます

## 注意

- パフェ発見機はあくまでロマン機能なので期待しないでください。
- 落下速度が上がると追いつかない場合があります。
- sysmodule を止めるときは、オーバーレイの「sysmodule を終了」を使ってください。ovlSysmodules で止めると本体が固まることがあります
- ぷよテト2 の起動中に sysmodule を起動したときは、ゲームを起動し直すまで AI は動きません

## 利用条件

sysmodule（`atmosphere/contents/4200000050324149/`）について:

- ネット対戦で使わないでください
- 改変（ネット対戦の制限を外すことを含む）と、改変したものの配布を禁止します。改変したものはサポートしません
- 再配布するときは改変せず、README.md と NOTICE.md を含めてください

オーバーレイ（`ppt2nx-ai.ovl` と `overlay/`・`common/` のソース）は GPL-2.0-or-later です。

無保証です。使用によって起きた損害（本体の BAN を含む）について、作者は責任を負いません。

## 開発用: selftest

`selftest/` は、lemon-tea の bot が devkitA64 と libnx でビルドでき、`std::thread` が Switch 上で動くかを確かめる単体の NRO です。ゲームには触れません。結果は画面と `sdmc:/ppt2ai-selftest.txt` に出ます。GitHub Actions（`.github/workflows/build.yml`）でビルドし、Artifacts から `.nro` を取れます。

## ソースコード

オーバーレイのソースを GPL-2.0-or-later で公開しています（[COPYING](COPYING)）。

## クレジット

- [Zetris](https://github.com/ZetrisAI/Zetris)（mat1jaczyyy、BSD 3-Clause）: 立ち回りオプション等
- [MisaMino](https://github.com/misakamm/MisaMino)（Misakamm）、[MisaMinoNET](https://github.com/ZetrisAI/MisaMinoNET)（mat1jaczyyy）: テトリスの AI
- [sfinder-cpp](https://github.com/knewjade/sfinder-cpp)（knewjade、MIT）、[PerfectClearNET](https://github.com/mat1jaczyyy/PerfectClearNET)（mat1jaczyyy）: パフェ発見機
- [lemon-tea-2](https://github.com/citrus610/lemon-tea-2)（citrus610、MIT）: テトリスの AI（`third_party/lemon-tea/` に取り込み、aarch64 向けに一部改変）
- [ama](https://github.com/citrus610/ama)（citrus610、MIT）: ぷよぷよの AI
- libnx（switchbrew）、[libtesla](https://github.com/WerWolv/libtesla)（WerWolv）

ライセンスの表示は [NOTICE.md](NOTICE.md) にあります。
