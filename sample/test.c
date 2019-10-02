#include <stdio.h>

const int U = 1, OPQRST = +123456, UVWXYZ = -7890;
const char CU = '_';
const int V = 0, W=-3,MAXS = 10000, _SEED = 391, mo = 30007;
const char CV='*', CW = '9';

int u, _abcdefg, hijklmn, opqrst01234, uvwxyz56789, ABCDEFG, HIJKLMN;
char a;
char b, s[10000];

int v[4], w[4], res[4];

int ans[4], rd[4];

int R(int v, int m) {
    return (v - v / m * m);
}

void matProd() {
    int i, j, k;
    for(i = 0; i < 2; i = i + 1) {
        for(j = 0; j < 2; j = j + 1) {
            res[i * 2 + j] = 0;
            for(k = 0; k < 2; k = k + 1) {
                res[i * 2 + j] = R(res[i * 2 + j] + v[i * 2 + k] * w[k * 2 + j], mo);
            }
        }
    }
}

void mpow(int k) {
    int i, j;

    ans[0] = 1;
    ans[1] = 0;
    ans[2] = 0;
    ans[3] = 1;
    rd[0] = 0;
    rd[1] = 1;
    rd[2] = 1;
    rd[3] = 1;

    for(i = 0; i >= 0 && k > 0; i = i + 1) {
        if(R(k, 2)) {
            for(j = 0; j < 4; j = j + 1)
                v[j] = ans[j];
            for(j = 0; j < 4; j = j + 1)
                w[j] = rd[j];
            matProd();
            for(j = 0; j < 4; j = j + 1)
                ans[j] = res[j];
        }
        for(j = 0; j < 4; j = j + 1)
            v[j] = rd[j];
        for(j = 0; j < 4; j = j + 1)
            w[j] = rd[j];
        matProd();
        for(j = 0; j < 4; j = j + 1)
            rd[j] = res[j];
        k = k / 2;
    }
}

void main() {
    int k;
    scanf("%d", &k);
    mpow(k);
    printf("%d\n", ans[1]);
}
