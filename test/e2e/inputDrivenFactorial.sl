-[int] main(){
  int n;
  output "n?";
  input n;
  int i = 0;
  int fact = 1;
  while(i < n){
    i = i + 1;
    fact = fact * i;
  }
  output "fact=";
  output fact;
  output "\n";
  return 0;
}
