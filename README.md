# STEP week 7
## 概要
1. Best-fit mallocに改造しよう
2. Freelist binを実装しよう
3. 空になったページを返却しよう
4. 空き領域の右結合を実装しよう
5. (発展)空き領域の左結合を実装しよう
6. (発展)さらに様々なアイディアを試してUtilizationを上げよう！

## Best-fit mallocに改造しよう
`my_malloc`関数を編集し, First-fit malloc から Best-fit malloc に変更した.

目的: 複数の空き領域があるときに, どの領域を使うかによって Utilization を改善できる.
- First-fit malloc: 十分な容量のある空き領域のうち最初の領域を使う.
- Best-fit malloc: 十分な容量のある空き領域のうち最も小さい領域(つまり必要なサイズに最も近い領域)を使う.
- Worst-fit malloc: 十分な容量のある空き領域のうち最も大きい領域を使う.

### Best-fit malloc の実装
#### コードの変更点
- 最も適した領域のアドレス・サイズ・その直前のアドレスを保持するための変数`best_metadata`, `best_size`, `best_prev`を用意した.
- First-fit malloc では, 最初の空き領域のみ見て処理を終了していたが, Best-fit malloc では全ての空き領域を確認する必要がある. `while (metadata)`で空き領域がある限り処理を続けるよう変更した.
- 各ループ内では, 空き領域の容量が十分にあるか, `best_size`を更新するかをチェックし, 適宜更新する.
- 全ての空き領域の確認が終了したら, `best_metadata`, `best_prev`を`metadata`, `prev`とすることで元のプログラムを継承した.
``` cpp
void *my_malloc(size_t size) {
  my_metadata_t *metadata = my_heap.free_head;
  my_metadata_t *prev = NULL;
  // Best-fit: 十分な空き容量の領域のうち、最も小さいものを見つける.
  // TODO: Update this logic to Best-fit!

  // 準備
  size_t best_size = (size_t) - 1;   // best_size: ベストフィットの容量を保持する.
  my_metadata_t *best_metadata = NULL;  // best_metadata: ベストフィットの領域を保持する.
  my_metadata_t *best_prev = NULL;  // best_size: ベストフィットの領域の直前を保持する.

  // 空き領域がある限りループ処理.
  while (metadata) {
    // 容量が指定よりも大きく、bestよりもさらに小さければ
    if (size <= metadata->size && metadata->size < best_size) {
      // ベストの領域、容量、直前領域を更新する.
      best_metadata = metadata;
      best_size = best_metadata->size;
      best_prev = prev;
    }
    // 次の領域に進む.
    prev = metadata;
    metadata = metadata->next;
  }
  metadata = best_metadata;
  prev = best_prev;
```
#### 結果
Challenge #3 #4 #5でUtilizationの大幅な改善が確認できたが, 処理時間が非常に長くなってしまった.
```
====================================================
Challenge #1    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|               8 =>             971
Utilization [%] |              70 =>              70
====================================================
Challenge #2    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|               2 =>             556
Utilization [%] |              40 =>              40
====================================================
Challenge #3    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|              65 =>             647
Utilization [%] |               9 =>              51
====================================================
Challenge #4    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|            9322 =>            5493
Utilization [%] |              15 =>              72
====================================================
Challenge #5    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|            6513 =>            3634
Utilization [%] |              15 =>              75
```
## Free List Bin を実装しよう
1. フリーリストビンを表す構造体を準備する

`NUM_BINS`: ビンの数

`free_head`を配列にした.
``` cpp
// my_heap_t: 空き領域全体を管理する
#define NUM_BINS 4                        // フリーリストビン実装: ビンの数を用意.
typedef struct my_heap_t {
  my_metadata_t *free_head[NUM_BINS];     // 空き領域リストの先頭要素を表すポインタ.
  my_metadata_t dummy;          // 空き領域がない場合でもエラーにならないよう常にダミー要素を設定している.
                                // 終わりを示す.
} my_heap_t;
```
2. メモリ量とビンの対応を計算する関数

とりあえず1000バイト区切りにした.
``` cpp
// 引数でサイズを与え, これは何番のビンを探索すべきかのインデックスを返す関数.
int get_bin_index(size_t size) {
  // bins[0]: 0 <= size < 1000
  // bins[1]: 1000 <= size < 2000
  // ...
  // bins[i]: i*1000 <= size < (i+1)*1000
  // ...
  int index = size / 1000;

  if (index >= NUM_BINS) {
    index = NUM_BINS - 1;
  }
  return index;
}
```
3. 必要なビンだけ探索する
```cpp
// sizeバイトのメモリを確保し, その場所（ポインタ）を返す.
void *my_malloc(size_t size) {
  // sizeから探索を開始するビンを定める.
  int start_bin_index = get_bin_index(size);

  // Best-fit: 十分な空き容量の領域のうち、最も小さいものを見つける.
  // 準備
  my_metadata_t *metadata = NULL;
  my_metadata_t *prev = NULL;

  size_t best_size = (size_t) - 1;      // best_size: ベストフィットの容量を保持する.
  my_metadata_t *best_metadata = NULL;  // best_metadata: ベストフィットの領域を保持する.
  my_metadata_t *best_prev = NULL;      // best_size: ベストフィットの領域の直前を保持する.

  // 最初に探索するビンから, 最後のビンまで順番に見ていく.
  for (int i = start_bin_index; i < NUM_BINS; i++) {
    // 今見ているビンの先頭をセット
    metadata = my_heap.free_head[i];
    prev = NULL;

    // 選んだビンの中で今まで通りのBest-fit探索を行う.
      // 空き容量がある限りループ処理.
    while (metadata) {
      // 容量が指定よりも大きく、bestよりもさらに小さければ
      if (size <= metadata->size && metadata->size < best_size) {
        // ベストの領域、容量、直前領域を更新する.
        best_metadata = metadata;
        best_size = best_metadata->size;
        best_prev = prev;
      }
      // 次の領域に進む.
      prev = metadata;
      metadata = metadata->next;
    }

    // もし現在のビンの中でBest-fitのビンが見つかっていれば探索終了.
    if (best_metadata != NULL) {
      break;
    }    
  }
```

### dummyは一つだけでいい！
```cpp
// This is called at the beginning of each challenge.
void my_initialize() {
  // ダミーは1つだけでOK.
  my_heap.dummy.size = 0;
  my_heap.dummy.next = NULL;
  // すべてのビンをその1つのダミーに向ける.
  for (int i = 0; i < NUM_BINS; i++) {
    my_heap.free_head[i] = &my_heap.dummy;
  }
}
```
#### 結果

```
====================================================
Challenge #1    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|               5 =>             930
Utilization [%] |              70 =>              70
====================================================
Challenge #2    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|               2 =>             563
Utilization [%] |              40 =>              40
====================================================
Challenge #3    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|              67 =>             678
Utilization [%] |               9 =>              51
====================================================
Challenge #4    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|           10623 =>            5526
Utilization [%] |              15 =>              72
====================================================
Challenge #5    |   simple_malloc =>       my_malloc
--------------- + --------------- => ---------------
       Time [ms]|            7033 =>            3613
Utilization [%] |              15 =>              75
```
