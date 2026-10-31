int counter = 0;
-[int] hit(){
  counter = counter + 1;
  return counter;
}
-[int] peek(){
  return counter;
}
-[int] main(){
  int counter = 100;
  output hit();
  output hit();
  output " ";
  output counter;
  output " ";
  output peek();
  output "\n";
  return 0;
}
