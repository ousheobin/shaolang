-[int] swap(int *a,int *b){
  int t;
  t = *a;
  *a = *b;
  *b = t;
  return 0;
}
-[int] main(){
  int x = 1;
  int y = 2;
  swap(&x,&y);
  output x;
  output " ";
  output y;
  output "\n";
  int d[2];
  d[0] = 5;
  d[1] = 6;
  swap(&d[0],&d[1]);
  output d[0];
  output " ";
  output d[1];
  output "\n";
  return 0;
}
