// RUN: clang-cc -fsyntax-only -verify %s

template<typename T> struct A { };

template<typename T> A<T> f0(T*);

void test_f0(int *ip, float const *cfp) {
  A<int> a0 = f0(ip);
  A<const float> a1 = f0(cfp);
}

template<typename T> void f1(T*, int);

void test_f1(int *ip, float fv) {
  f1(ip, fv);
}

template<typename T> void f2(T*, T*);

struct ConvToIntPtr {
  operator int*() const;
};

void test_f2(int *ip, float *fp) {
  f2(ip, ConvToIntPtr()); // expected-error{{no matching function}}
  f2(ip, ip); // okay
  f2(ip, fp); // expected-error{{no matching function}}
}
