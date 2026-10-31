-[int] main(){
  int a[4];
  a[0] = 10;
  a[1] = 20;
  a[2] = 30;
  a[3] = 0;
  int *p;
  p = a;
  int n = 0;
  while(*p){
    n = n + 1;
    p = p + 1;
  }
  output n;
  output "\n";
  output a[2];
  output "\n";
  int i = 0;
  i ++;
  a[i] ++;
  output a[1];
  output "\n";
  ++ a[i];
  output * (p - 1);
  output "\n";
  return 0;
}
