-[int] main(){
  int a[5];
  int i = 0;
  while(i < 5){
    a[i] = i * i;
    i = i + 1;
  }
  int s = 0;
  i = 0;
  while(i < 5){
    s = s + a[i];
    i = i + 1;
  }
  output s;
  output "\n";
  return 0;
}
