/*
 * TestTypedExpressions.cpp
 *
 *  Created on: Sep 23, 2013
 *      Author: hannes
 */

#include <iostream>

#include "typed_expressions/TypedExpressions.hpp"


#define NAMED(X) ::typed_expressions::NamedExp<decltype(X)>(#X, X)

int main(int argc, char **argv) {
  using namespace tex;

  NamedExp<SimpleSpace> a("a", argc);
  SimpleSpace b(2);

//  TemplatedSpace<1> A, B;

//  auto C = A + B;

//  auto U = A + a;

//  std::cout << "NAMED(a)=" << std::endl << NAMED(a) << std::endl; // XXX: debug output of NAMED(a)

  auto c = a + b;
//  std::cout << "c=" << std::endl << c << std::endl; // XXX: debug output of c

  auto x = ("x", a + c);
  auto y = ("y", x + a);
  auto z = ("z", y + a);

  z.x();
  std::cout << "z=" << std::endl << z << "=" << z.eval().eval() << std::endl; // XXX: debug output of x.eval()

  return z.eval().eval();
}
