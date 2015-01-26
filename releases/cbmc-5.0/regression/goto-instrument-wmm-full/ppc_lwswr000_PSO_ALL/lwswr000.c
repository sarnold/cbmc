void fence() { asm("sync"); }
void lwfence() { asm("lwsync"); }
void isync() { asm("isync"); }

int __unbuffered_cnt=0;
int __unbuffered_p0_r1=0;
int __unbuffered_p0_r3=0;
int __unbuffered_p1_r1=0;
int __unbuffered_p1_r3=0;
int __unbuffered_p1_r4=0;
int __unbuffered_p1_r5=0;
int __unbuffered_p2_r1=0;
int __unbuffered_p2_r3=0;
int __unbuffered_p3_r1=0;
int __unbuffered_p3_r3=0;
int __unbuffered_p3_r4=0;
int __unbuffered_p3_r5=0;
int x=0;
int y=0;

void * P0(void * arg) {
  __unbuffered_p0_r1 = 1;
  x = __unbuffered_p0_r1;
  lwfence();
  __unbuffered_p0_r3 = x;
  // Instrumentation for CPROVER
  fence();
  __unbuffered_cnt++;
}

void * P1(void * arg) {
  __unbuffered_p1_r1 = 2;
  x = __unbuffered_p1_r1;
  lwfence();
  __unbuffered_p1_r3 = x;
  __unbuffered_p1_r4 = __unbuffered_p1_r3 ^ __unbuffered_p1_r3;
  __unbuffered_p1_r5 = *(&y + __unbuffered_p1_r4);
  // Instrumentation for CPROVER
  fence();
  __unbuffered_cnt++;
}

void * P2(void * arg) {
  __unbuffered_p2_r1 = 1;
  y = __unbuffered_p2_r1;
  lwfence();
  __unbuffered_p2_r3 = y;
  // Instrumentation for CPROVER
  fence();
  __unbuffered_cnt++;
}

void * P3(void * arg) {
  __unbuffered_p3_r1 = 2;
  y = __unbuffered_p3_r1;
  lwfence();
  __unbuffered_p3_r3 = y;
  __unbuffered_p3_r4 = __unbuffered_p3_r3 ^ __unbuffered_p3_r3;
  __unbuffered_p3_r5 = *(&x + __unbuffered_p3_r4);
  // Instrumentation for CPROVER
  fence();
  __unbuffered_cnt++;
}

int main() {
  __CPROVER_ASYNC_0: P0(0);
  __CPROVER_ASYNC_1: P1(0);
  __CPROVER_ASYNC_2: P2(0);
  __CPROVER_ASYNC_3: P3(0);
  __CPROVER_assume(__unbuffered_cnt==4);
  fence();
  // EXPECT:exists
  __CPROVER_assert(!(x==2 && y==2 && __unbuffered_p0_r3==1 && __unbuffered_p1_r5==0 && __unbuffered_p2_r3==1 && __unbuffered_p3_r5==0), "Program proven to be relaxed for PPC, model checker says YES.");
  return 0;
}
