void some_function() {}

int main() {
  // create a pointer to a char and point it to first cell of video memory
  // (top-left of screen)
  int *video_memory = (int *)0xb8000;

  // at the address pointet to store char X
  *video_memory = 0x2f59;

  return 0;
}
