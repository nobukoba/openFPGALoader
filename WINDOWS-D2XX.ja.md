# WindowsでFTDIドライバーを維持してFT232H / HS3を使う

このフォークでは `USE_FTDI_D2XX=ON` を追加しています。Windows x64に入っているFTDI CDMドライバーへ、システムの `ftd2xx.dll` 経由で接続します。ドライバーのインストール・切り替え・復元は行いません。[English](WINDOWS-D2XX.md)

MSYS2 UCRT64環境でCMake、GCC、pkgconf、libftdi、libusb、zlibの開発パッケージを用意してビルドします。

```sh
cmake -S . -B build-d2xx -G Ninja -DUSE_FTDI_D2XX=ON -DENABLE_CABLE_ALL=OFF -DENABLE_FTDI_BASED_CABLE=ON -DENABLE_VENDORS_ALL=OFF -DENABLE_XILINX_SUPPORT=ON -DENABLE_UDEV=OFF -DENABLE_USB_SCAN=OFF
cmake --build build-d2xx
./build-d2xx/openFPGALoader.exe --scan-usb
```

使用前にVivado Hardware Manager / hw_serverを終了してください。スキャンはD2XXで一覧を取得するだけで、機器を開きません。FT232HのUSB IDだけではHS3と断定できないため、`digilent_hs3` を選ぶ前に実物を確認してください。

今回の対応はFT232HのインターフェイスA（0403:6014、HS3を含む）です。該当機器が複数の場合は `--usb-serial-num SERIAL` を指定します。同じシリアルが重複する場合も停止します。libusbのバス／アドレスやインデックス指定は使えません。現在WinUSBになっている機器は、利用者が別途FTDIドライバーに戻す必要があります。このプログラム自身はドライバーを変更しません。

Xilinx Platform Cable USB（03fd）は別のドライバー/APIなので、このバックエンドでは未対応です。他のFTDIチップや文字ベースの通信も対象外です。Linux/macOSの通常ビルドは従来のlibftdiを維持します。このオプションはWindows専用です。

DLLはSystem32からのみ読み込み、FTDI DLLやドライバーは同梱しません。libftdiのヘッダー・定数と通常のビルド依存関係は引き続き使用します。読み取りには3秒のドライバータイムアウトを設け、不完全な転送では自動再試行しません。フラッシュ書き込み自体は既存のopenFPGALoader処理なので、アプリ側のバックアップ・読み戻し検証を使い、終了コードだけで起動動作を保証しないでください。

Windows x64向けクロスコンパイルと模擬テストを実施しています。模擬テストでは、複数機器の選択拒否、シリアル指定、接続後の識別情報の変化、短い書き込み、ハンドル解放を確認します。`--scan-usb` では機器を開かずにインストール済みDLLの列挙を確認します。開発用Windows環境で、FTDIドライバーのままDigilent製FT232Hを2本列挙できました。シリアル未指定の検出は2本の識別情報を表示して停止し、シリアル指定のHS3 JTAG検出ではXilinx Kintex-7 XC7K160T（IDCODE 0x0364c093）を読み取り、終了コード0を確認しました。ドライバーは変更していません。続いてVivado 2023.2から同じHS3を開き、ドライバー変更なしでxc7k160t_0を確認できました。BITロードとMCS書き込み・読み戻しは未検証です。ビルドで接続機器へ書き込むことはありません。

参考: https://ftdichip.com/document/programming-guides/

