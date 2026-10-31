-[int] down(int n){
  if(n == 0){
    return 0;
  }
  return n + down(n - 1);
}
-[int] main(){
  output down(500);
  output "\n";
  return 0;
}
