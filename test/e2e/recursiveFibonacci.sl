-[int] fib(int n){
  if(n < 2){
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}
-[int] main(){
  output "fib(20)=";
  output fib(20);
  output "\n";
  return 0;
}
