-[int] main(){
  int x = 1;
  int *p;
  p = &x;
  *p = 42;
  output x;
  output "\n";
  output *p;
  output "\n";
  output p == &x;
  output "\n";
  return 0;
}
