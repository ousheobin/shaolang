-[int] ten(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j){
  return a + b + c + d + e + f + g + h + i + j;
}
-[void] shout(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j){
  output "sum=";
  output ten(a,b,c,d,e,f,g,h,i,j);
  output "\n";
}
-[int] main(){
  shout(1,2,3,4,5,6,7,8,9,10);
  output ten(10,9,8,7,6,5,4,3,2,1);
  output "\n";
  return 0;
}
