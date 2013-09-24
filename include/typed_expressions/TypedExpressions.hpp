/*
 * TypedExpressions.hpp
 *
 *  Created on: Sep 23, 2013
 *      Author: hannes
 */

#ifndef TYPEDEXPRESSIONS_HPP_
#define TYPEDEXPRESSIONS_HPP_

#include <memory>

namespace tex {


// **************** tools ****************

namespace internal {
  template <typename R, typename Ret = typename R::ValueType >
  Ret unwrap(R, int){ std::cout << "a=" << std::endl; // XXX: debug output of a
  }
  template <typename R>
  R unwrap(R, double){ std::cout << "b=" << std::endl; // XXX: debug output of a
  }

  template <typename R>
  struct UnwrapValueType{
    typedef decltype(unwrap(R(), 1)) ValueType;
  };


  template <template <typename, typename> class BinOp, typename A, typename B>
  struct BinOpReturnType {
    typedef typename UnwrapValueType<A>::ValueType AU;
    typedef typename UnwrapValueType<B>::ValueType BU;
    typedef typename BinOp<AU, BU>::ValueType ValueType;
  };
}

using internal::UnwrapValueType;

// **************** value base ***************

template <typename T> class OpBase {
 public:
  typedef T ValueType;

  inline ValueType eval() const { return evalImpl(); }
  inline operator ValueType() const {
    return eval();
  }

  virtual ~OpBase(){};
 private:
  virtual ValueType evalImpl() const = 0;
};

template <typename T, typename DERIVED, int Level_> class GenericOp {
 public:
  typedef DERIVED CompoundType;
  constexpr static int Level = Level_ - 1;
  typedef T ValueType;
  inline operator ValueType() const {
    return static_cast<const DERIVED&>(*this).eval();
  }
};

template <typename T>
struct HiddenPtr {
  typedef T ValueType;
  typedef std::shared_ptr<OpBase<T> > PtrType;
  HiddenPtr() = default;
  HiddenPtr(const PtrType & p) : ptr_(p){
  }
  template <typename DERIVED>
  HiddenPtr(const DERIVED & p) : ptr_(new DERIVED(p)){
  }
  operator OpBase<T> const & () {
    return *ptr_;
  }

  PtrType ptr_;
};


template <typename T, typename DERIVED>
class GenericOp<T, DERIVED, 0> : public OpBase<T> {
 public:
  typedef HiddenPtr<T> CompoundType;
  constexpr static int Level = 0;

  virtual ~GenericOp(){};
 private:
  virtual T evalImpl() const {
    static_assert(static_cast<const DERIVED&>(*this).eval != OpBase<T>::eval, "Eval must be shadowed in an Operation");
    return static_cast<DERIVED&>(*this).eval();
  };
};

constexpr int max(int a, int b) {
  return a > b ? a : b;
}
constexpr int min(int a, int b) {
  return a < b ? a : b;
}

template <typename A, typename B> struct get_level {
  constexpr static int Level = min(A::Level, B::Level);
};

// **************** operators ****************

template<typename A, typename B, typename ValueType_, typename DERIVED>
class BinOpBase : public GenericOp<ValueType_, DERIVED, get_level<A, B>::Level> {
 protected:
  BinOpBase(const A & a, const B & b) : a_(a), b_(b) {}
  const A & getA() const { return a_; }
  const B & getB() const { return b_; }
 private:
  A a_;
  B b_;
};

template <typename A, typename B, typename ValueType_ = decltype(typename UnwrapValueType<A>::ValueType().evalSum(typename UnwrapValueType<B>::ValueType()))>
class Sum : public BinOpBase<A, B, ValueType_, Sum<A, B, ValueType_> > {
 public:
  typedef BinOpBase<A, B, ValueType_, Sum<A, B, ValueType_> > Base;
  typedef ValueType_ ValueType;

  Sum(const A & a, const B & b) : Base(a, b){}

  ValueType eval() const {
    return typename UnwrapValueType<A>::ValueType(this->getA()).evalSum(typename UnwrapValueType<A>::ValueType(this->getB()));
  }

  friend
  std::ostream & operator << (std::ostream & out, const Sum & sumApp) {
    return out << "(" << sumApp.getA() << " + " << sumApp.getB() << ")";
  }

  Sum(); // compiler (bug?) requires this
};

template <typename A, typename B, typename Result = Sum<A, B> >
inline typename Result::CompoundType operator + (const A & a, const B & b){
  return Result(a, b);
}


// **************** modifier ****************

template <typename T>
class NamedExp : public T{
 public:
  constexpr static int Level = 10;

  inline NamedExp(const char * name, const T & t) : T(t), name_(name){}
  inline NamedExp();
  typedef typename UnwrapValueType<T>::ValueType ValueType;

  friend
  std::ostream & operator << (std::ostream & out, const NamedExp<T> & namedExp) {
    return out << namedExp.name_ << "(" << (static_cast<const T&>(namedExp)) << ")";
  }

 private:
  std::string name_;
};

template <typename T>
NamedExp<T> operator , (const char *name, const T & t) {
  return NamedExp<T>(name, t);
}

// **************** example spaces ****************

class SimpleSpace {
 public:
  constexpr static int Level = 10;

  SimpleSpace(int v = 0) : value(v){}

  SimpleSpace evalSum(const SimpleSpace & other) const {
    return SimpleSpace(value + other.value);
  }

  friend
  std::ostream & operator << (std::ostream & out, const SimpleSpace & ob) {
    return out << ob.value;
  }

  int eval() {
    return value;
  }

 private:
  int value;
};

template <int Dim_>
class TemplatedSpace {
 public:
  constexpr static int Dimension = Dim_;

  TemplatedSpace evalSum (const TemplatedSpace & other) const {
    TemplatedSpace r;
    for(int i = 0; i < Dimension; i ++) { r.value[i] = value[i] + other.value[i]; }
    return r;
  }

  int value[Dimension];
};

}

#endif /* TYPEDEXPRESSIONS_HPP_ */
