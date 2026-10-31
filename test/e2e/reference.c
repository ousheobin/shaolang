#include <stdio.h>
int main(){
  // e04
  int n=1,s=0;
  while(1){ if(n%7==0){n=n+1;continue;} if(n>20){break;} s=s+n; n=n+1; }
  printf("e04 s=%d n=%d\n",s,n);
  // e07
  printf("e07 %d %d %d\n", (2*2)+(3*2), (4+5)*2, (1+2)+(3+4));
  // e12
  printf("e12 %d %d %d %d\n", 0-7%3, 0-7/2, 7%3, 100/7);
  // e14
  int i=0,t=0; while(i<10){ i++; if(i==5) continue; t=t+i; }
  int k=3; k++; int a1=k; ++k; int a2=k; k--; printf("e14 %d %d %d %d\n", t, a1, a2, k);
  // e09
  char c='A'; printf("e09 %c %d", c, c+1); for(int j=0;j<5;j++){ c='a'+j; printf("%c", c);} printf("\n");
  // e02
  int a=-5; printf("e02 %d %d %d %d\n", a, -a, !0, !a);
  // e03
  int x=7; printf("e03 %d %d %d %d %d\n", x>5&&x<10, x>5||x==100, x<5&&x>0, !(x==7), (x==7||x==8)&&(x%2==1));
  return 0;
}
