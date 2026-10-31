-[int] add(int a,int b){
  return a + b;
}
-[int] dbl(int a){
  return a * 2;
}
-[int] main(){
  output add(dbl(2),dbl(3));
  output " ";
  output dbl(add(4,5));
  output " ";
  output add(add(1,2),add(3,4));
  output "\n";
  return 0;
}
