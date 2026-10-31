-[int] classify(int n){
  if(n < 0){
    output "neg";
  }
  else{
    if(n == 0){
      output "zero";
    }
    else{
      if(n % 2 == 0){
        output "even";
      }
      else{
        output "odd";
      }
    }
  }
  return 0;
}
-[int] main(){
  classify(0 - 3);
  output " ";
  classify(0);
  output " ";
  classify(8);
  output " ";
  classify(7);
  output "\n";
  return 0;
}
