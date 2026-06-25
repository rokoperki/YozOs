void main() {
  // create a pointer to a char and point it to first cell of video memory
  // (top-left of screen)
  char *video_memory = (char *)0xb8000;

  // at the address pointet to store char X
  *video_memory = 'X';
}
