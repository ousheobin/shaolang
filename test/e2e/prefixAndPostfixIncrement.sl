-[void] nothing(){
  output "nothing\n";
}
-[int] main(){
  nothing();
  int i = 0;
  int s = 0;
  while(i < 10){
    i ++;
    if(i == 5){
      continue;
    }
    s = s + i;
  }
  output s;
  output " ";
  int k = 3;
  k ++;
  output k;
  output " ";
  ++k;
  output k;
  output " ";
  k --;
  output k;
  output "\n";
  return 0;
}
