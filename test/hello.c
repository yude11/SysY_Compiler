int sum(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7) {
  return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7;
}

int sum2(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
         int a9, int a10, int a11) {
  return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11;
}

int main() {
  int x = 0;
  int y = sum2(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  return x + y;
}