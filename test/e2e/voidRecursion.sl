-[void] noop(){
}
-[void] walk(int n){
  if(n > 0){
    noop();
    walk(n - 1);
  }
}
-[int] main(){
  walk(200);
  output "ok\n";
  return 0;
}
