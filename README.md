# 9cc
低レイヤを知りたい人のためのCコンパイラ作成入門

https://www.sigbus.info/compilerbook/

# 目標

**セルフホスト**

## セルフホストに向けたTODOリスト

前提条件として、現在コード中に用いている記述は変更しない。コンパイルを通すためにあえて使用する構文を制限することもしない。ただし実装しなくても動作するもの（constなど）はあえて実装しなくてもよいものとする。

- 多次元配列の初期化
- `__func__`
- より厳密なC標準準拠（GCCの-std=c11 -pedantic-errors相当）
- \による文字のエスケープ
- **構造体・共用体の実装**
  - `.`
  - `->`
  - メンバのアラインメント
  - 初期値
  - 配列の初期値（構造体）
- プリプロセッサを通す
- セルフホスト用に最低限のヘッダファイルを用意する

## 実装予定なし

- ローカル配列のサイズが定数でないケース。
- [指示付きの初期化子 (Designated Initializer)](http://seclan.dll.jp/c99d/c99d07.htm#dt19991025)
- 実数

## 独自拡張
- 明示的なreturnが無い場合でも、最後に評価した値を返す。
- トップレベルの空の ; を許可する（ほとんどのコンパイラが独自拡張している）。
- typeof演算子

## その他、制限事項・バグなど

- 文字列の連結：`"ABC" "XYZ"` -> `"ABCXYZ"`
- 戻り値のない非void関数のチェックが雑。
- 文字列リテラルの途中に明示的な'\0'があると以降が無視される。
- ローカル変数の初期値として文字列リテラルを指定すると.dataにコピーが作られ、実行時にそこからコピーする動作になっている。

## [Cの仕様書](http://port70.net/~nsz/c/c11/n1570.html#A)を読むときのメモ

- `xxx-list` は `xxx` をカンマ `,` で並べたもので、9ccのBNFでは `xxx ( , xxx)*` で記載している。
- `xxx` から先頭の `pointer*` をなくしたものが `direct_xxx`。
例： `declarator = pointer* direct_declarator`
- `declarator` と　`abstract-declarator` の違いは前者には `identifier` を含むが、後者は含まないこと。
例： `int *p` と `int *`