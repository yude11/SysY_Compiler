void init(int arr[]) {
  arr[0] = 1;
}

// int f1(int a0[], int a1[], int a2[], int a3[], int a4[], int a5[], int a6[],
//        int a7[], int a8[], int a9[]) {
//   return a0[0] + a1[1] + a2[2] + a3[3] + a4[4] + a5[5] + a6[6] + a7[7] + a8[8] +
//          a9[9];
// }

// int f2(int a0[][10], int a1[], int a2, int a3[], int a4[], int a5[][10][10],
//        int a6[], int a7[], int a8, int a9[][10]) {
//   return a0[0][9] + a1[1] + a2 + a3[3] + a4[4] + a5[5][5][5] + a6[6] + a7[7] +
//          a8 + a9[9][8];
// }

int main() {
  int arr[2], sum = 0;
  init(arr);
  // sum = sum + f1(arr[0][0], arr[1][1], arr[2][2], arr[3][3], arr[4][4],
  //                arr[5][5], arr[6][6], arr[7][7], arr[8][8], arr[9][9]);
  // sum = sum + f2(arr[0], arr[1][1], arr[2][2][2], arr[3][3], arr[4][4], arr,
  //                arr[6][6], arr[7][7], arr[8][8][8], arr[9]);
  // putint(sum);
  // putch(10);
  return sum;
}