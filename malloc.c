//
// >>>> malloc challenge! <<<<
//
// Your task is to improve utilization and speed of the following malloc
// implementation.
// Initial implementation is the same as the one implemented in simple_malloc.c.
// For the detailed explanation, please refer to simple_malloc.c.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Interfaces to get memory pages from OS
//

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

//
// Struct definitions
//

// my_metadata_t: メモリブロックの管理情報を持つ
typedef struct my_metadata_t {
  size_t size;                      // このメモリブロックのサイズ
  struct my_metadata_t *next;       // 次の空きメモリがある場所（ポインタ）
} my_metadata_t;

// my_heap_t: 空き領域全体を管理する
#define NUM_BINS 2                        // フリーリストビン実装: ビンの数を用意.
typedef struct my_heap_t {
  my_metadata_t *free_head[NUM_BINS];     // 空き領域リストの先頭要素を表すポインタ.
  my_metadata_t dummy;          // 空き領域がない場合でもエラーにならないよう常にダミー要素を設定している.
                                // 終わりを示す.
} my_heap_t;

//
// Static variables (DO NOT ADD ANOTHER STATIC VARIABLES!)
//

my_heap_t my_heap;

//
// Helper functions (feel free to add/remove/edit!)
//

// 空き領域に新しくブロックを追加.
// 与えられたインデックスのビンに繋げる.
void my_add_to_free_list(my_metadata_t *metadata, int index) {
  assert(!metadata->next);
  metadata->next = my_heap.free_head[index];
  my_heap.free_head[index] = metadata;
}

// 空き領域から特定のブロックを取り除く.
void my_remove_from_free_list(my_metadata_t *metadata, my_metadata_t *prev, int index) {
  if (prev) {
    prev->next = metadata->next;
  } else {
    my_heap.free_head[index] = metadata->next;
  }
  metadata->next = NULL;
}

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

//
// Interfaces of malloc (DO NOT RENAME FOLLOWING FUNCTIONS!)
//

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

// my_malloc() is called every time an object is allocated.
// |size| is guaranteed to be a multiple of 8 bytes and meets 8 <= |size| <=
// 4000. You are not allowed to use any library functions other than
// mmap_from_system() / munmap_to_system().

// sizeバイトのメモリを確保し, その場所（ポインタ）を返す.
void *my_malloc(size_t size) {
  // sizeから探索を開始するビンを定める.
  int start_bin_index = get_bin_index(size);

  // Best-fit: 十分な空き容量の領域のうち、最も小さいものを見つける.
  // 準備]
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
  // 探索が終わったら, 見つけたベストなブロックを元の変数に戻す.
  // これにより, 他の部分を書き換える必要がなくなる.
  metadata = best_metadata;
  prev = best_prev;

  // 十分な空き領域がなかった場合
  if (!metadata) {
    // There was no free slot available. We need to request a new memory region
    // from the system by calling mmap_from_system().
    //
    //     | metadata | free slot |
    //     ^
    //     metadata
    //     <---------------------->
    //            buffer_size
    size_t buffer_size = 4096;
    my_metadata_t *metadata = (my_metadata_t *)mmap_from_system(buffer_size);
    metadata->size = buffer_size - sizeof(my_metadata_t);
    metadata->next = NULL;
    int index = get_bin_index(metadata->size);
    // Add the memory region to the free list.
    my_add_to_free_list(metadata, index);
    // Now, try my_malloc() again. This should succeed.
    return my_malloc(size);
  }

  // |ptr| is the beginning of the allocated object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  void *ptr = metadata + 1;
  size_t remaining_size = metadata->size - size;
  int index = get_bin_index(metadata->size);
  // Remove the free slot from the free list.
  my_remove_from_free_list(metadata, prev, index);

  if (remaining_size > sizeof(my_metadata_t)) {
    // Shrink the metadata for the allocated object
    // to separate the rest of the region corresponding to remaining_size.
    // If the remaining_size is not large enough to make a new metadata,
    // this code path will not be taken and the region will be managed
    // as a part of the allocated object.
    metadata->size = size;
    // Create a new metadata for the remaining free slot.
    //
    // ... | metadata | object | metadata | free slot | ...
    //     ^          ^        ^
    //     metadata   ptr      new_metadata
    //                 <------><---------------------->
    //                   size       remaining size
    my_metadata_t *new_metadata = (my_metadata_t *)((char *)ptr + size);
    new_metadata->size = remaining_size - sizeof(my_metadata_t);
    new_metadata->next = NULL;
    // Add the remaining free slot to the free list.
    int index = get_bin_index(new_metadata->size);
    my_add_to_free_list(new_metadata, index);
  }
  return ptr;
}

// This is called every time an object is freed.  You are not allowed to
// use any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr) {
  // Look up the metadata. The metadata is placed just prior to the object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  my_metadata_t *metadata = (my_metadata_t *)ptr - 1;
  int index = get_bin_index(metadata->size);
  // Add the free slot to the free list.
  my_add_to_free_list(metadata, index);
}

// This is called at the end of each challenge.
void my_finalize() {
  // Nothing is here for now.
  // feel free to add something if you want!
}

void test() {
  // Implement here!
  assert(1 == 1); /* 1 is 1. That's always true! (You can remove this.) */
}
