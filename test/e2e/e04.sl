-[int] main(){
  int n = 1;
  int s = 0;
  while(1){
    if(n % 7 == 0){
      n = n + 1;
      continue;
    }
    if(n > 20){
      break;
    }
    s = s + n;
    n = n + 1;
  }
  output "s=";
  output s;
  output " n=";
  output n;
  output "\n";
  return 0;
}
