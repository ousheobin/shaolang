-[int] sum(int *a,int n){
  int s = 0;
  int i = 0;
  while(i < n){
    s = s + a[i];
    i = i + 1;
  }
  return s;
}
-[int] main(){
  int d[4];
  d[0] = 7;
  d[1] = 8;
  d[2] = 9;
  d[3] = 10;
  int *p;
  p = d;
  output sum(p,4);
  output "\n";
  output sum(d,4);
  output "\n";
  output *(p + 2);
  output "\n";
  output d[1] + *p;
  output "\n";
  output p - d;
  output "\n";
  return 0;
}
